"""Distribution-preserving rejection with certified, budgeted online region learning.

The MassCore interval search follows the paper/C implementation design. This is
an independent Python implementation, not an ABI wrapper or bitwise C port.
"""
from __future__ import annotations
from dataclasses import dataclass, asdict
from itertools import combinations
from math import floor, sqrt
import random
from time import perf_counter,process_time
from oracle import Oracle, Answer
from prior import contains, matches
from proofs import ProofStore


@dataclass(frozen=True)
class Config:
    name: str
    mode: str = 'mass'
    beta: float = 4.0
    scheduler: str = 'mass_fair'
    capacity: int = 0
    eviction: str = 'mass'
    fairness: int = 4
    core_limit: int = 100000
    gain_horizon: int = 0
    adaptive: bool = False
    learning_fraction: float = 0.1
    warmup: int = 32
    charge_memo: bool = True
    proof_reuse: bool = True
    sat_memo: bool = True
    proof_index: str = 'watched'
    eager_proofs: bool = True
    defer_scores: bool = True
    memoize_misses: bool = True


CONFIGS = {
    c.name: c for c in [Config('rejection', 'none', 0), Config('full', 'full', 0),
    Config('raw', 'raw', 0), Config('mus', 'mus'), Config('minimum', 'minimum'),
    Config('mass_b1', beta=1), Config('mass_b4'), Config('fifo_b4', scheduler='fifo'),
    Config('width_b4', scheduler='width'), Config('bounded_mass', capacity=4),
    Config('bounded_fifo', capacity=4, eviction='fifo'),
    Config('bounded_width', capacity=4, eviction='width'),
    Config('gain_b4', gain_horizon=1000), Config('adaptive_b4', adaptive=True)]}


@dataclass
class Node:
    x: tuple
    lower: frozenset
    undecided: frozenset
    minimum: str = ''
    maximum: str = 'unsat'
    closed: bool = False

    def bindings(self, indices):
        return tuple((i, self.x[i]) for i in sorted(indices))


class Sampler:
    def __init__(self, problem, config, oracle=None):
        self.problem, self.prior, self.config = problem, problem.prior, config
        self.prior._residual_mass.cache_clear()  # No cross-strategy warm-cache advantage.
        self.prior.coverage_fallbacks=0
        self.oracle = oracle or Oracle(problem)
        self.cache = []
        self._mass_cache = {}
        self.nodes, self.tasks = [], []
        self.seen_tasks = set()
        self.events = []
        self.business_calls = self.core_calls = self.cache_hits = 0
        self.candidates = 0
        self.known = {}  # Exact partial-query statuses; never used to accept inputs.
        self.proofs = ProofStore(config.proof_index)
        self._proof_misses = set()
        self.proof_miss_hits = 0
        self.sat_cache_hits = 0
        self.inference_hits = dict(sat=0,unsat=0,exact=0)
        self.discovery_budget=0
        self.memo_hits = 0
        self.learning_seconds = self.business_seconds = 0.0
        self.last_raw_failure = 0
        self._last_gate_reason = None
        self.adaptive_decisions = dict(warmup=0, covered=0, cost=0, horizon=0, allowed=0)
        self.times = dict(query_build=0.0, solve=0.0, extract=0.0, mass=0.0, match=0.0, selection=0.0,
                          proof_lookup=0.0,proof_update=0.0)

    def gain(self, r, cache=None):
        t = perf_counter()
        if cache is None and r in self._mass_cache:
            value = self._mass_cache[r]
        else:
            value = self.prior.uncovered(r, tuple(self.cache if cache is None else cache))
            if cache is None: self._mass_cache[r] = value
        self.times['mass'] += perf_counter() - t
        return value

    def insert(self, r):
        # Only callers with an UNSAT answer/proof may reach this method.
        r = tuple(sorted(r))
        if any(contains(old, r) for old in self.cache):
            return
        self.events.append(dict(kind='proof', candidate=self.candidates, region=r))
        self.cache = [old for old in self.cache if not contains(r, old)]
        self.cache.append(r)
        self._mass_cache.clear()
        k = self.config.capacity
        if k and len(self.cache) > k:
            if self.config.eviction == 'fifo':
                victim = 0
            elif self.config.eviction == 'width':
                victim = max(range(len(self.cache)), key=lambda i: (len(self.cache[i]), i))
            else:
                # Equal loss evicts the newcomer, retaining incumbent coverage.
                victim = min(reversed(range(len(self.cache))),
                             key=lambda i: self.gain(self.cache[i], self.cache[:i] + self.cache[i+1:]))
            removed = self.cache.pop(victim)
            self._mass_cache.clear()
            self.events.append(dict(kind='evict', candidate=self.candidates, region=removed))
        t=perf_counter()
        self.proofs.nogoods.replace(self.cache)
        self._proof_misses.clear()
        self.times['proof_update']+=perf_counter()-t

    def query(self, r, discovery=False):
        if discovery:
            self.core_calls += 1
        else:
            self.business_calls += 1
        answer = self.oracle.query(r)
        if not self.config.proof_reuse and self.config.mode in ('mass', 'mus', 'minimum') and answer.status in ('sat', 'unsat'):
            self.known[tuple(r)] = answer
        if not discovery:
            self.business_seconds += answer.build_seconds + answer.solve_seconds + answer.extract_seconds
        self.times['query_build'] += answer.build_seconds
        self.times['solve'] += answer.solve_seconds
        self.times['extract'] += answer.extract_seconds
        completion=getattr(answer,'completion',None)
        if answer.status=='sat' and completion is None and tuple(i for i,_ in r)==tuple(range(len(self.prior.names))):
            completion=tuple(v for _,v in r)
        self.events.append(dict(kind='discovery' if discovery else 'business', candidate=self.candidates,
                                region=r, status=answer.status, core=answer.core,
                                completion=completion,witness=answer.witness,
                                seconds=dict(build=answer.build_seconds,solve=answer.solve_seconds,extract=answer.extract_seconds)))
        if answer.status=='sat' and self.config.mode!='none' and (self.config.proof_reuse or self.config.sat_memo):
            t=perf_counter()
            if completion is not None:
                if (len(completion)!=len(self.prior.names) or any(completion[i]!=v for i,v in r)
                    or not self.problem.feasible(completion) or not self.problem.check_witness(completion,answer.witness)):
                    raise AssertionError('Invalid SAT completion or witness')
            before = len(self.proofs.sat)
            self.proofs.remember_sat(r,answer.witness,completion)
            if len(self.proofs.sat) != before:
                self._proof_misses.clear()
            self.times['proof_update']+=perf_counter()-t
        return answer

    def add_task(self, x):
        if x in self.seen_tasks:
            return
        self.seen_tasks.add(x)
        if self.config.mode == 'mass':
            self.nodes.append(Node(x, frozenset(), frozenset(range(len(x)))))
        else:
            candidates = (iter(combinations(range(len(x)), k)) for k in range(len(x)))
            self.tasks.append(dict(x=x, working=set(range(len(x))), cursor=0, closed=False,
                                   combinations=(c for group in candidates for c in group)))

    def prepare_nodes(self):
        cursor=0
        while cursor<len(self.nodes):
            node=self.nodes[cursor]
            if node.closed:
                cursor+=1
                continue
            if (self.config.eager_proofs and self.config.proof_reuse and self.config.charge_memo
                and self.core_calls+self.memo_hits>=self.discovery_budget):
                break
            if not node.undecided:
                node.minimum = node.minimum or node.maximum
                node.maximum = node.maximum or node.minimum
            if node.maximum == 'sat':
                node.closed = True
            elif node.minimum == 'unsat':
                self.insert(node.bindings(node.lower)); node.closed = True
            elif node.minimum and node.maximum:
                selected = min(node.undecided)
                rest = node.undecided - {selected}
                self.nodes.extend([Node(node.x, node.lower | {selected}, rest, '', node.maximum),
                                   Node(node.x, node.lower, rest, node.minimum, '')])
                node.closed = True
            elif (self.config.eager_proofs and self.config.proof_reuse and self.config.charge_memo and
                  self.core_calls+self.memo_hits<self.discovery_budget):
                indices=node.lower if node.maximum=='unsat' else node.lower|node.undecided
                bindings=node.bindings(indices);inferred=self.lookup_proof(bindings)
                if inferred[0] is not None:
                    self.refine(node,bindings,inferred)
                    continue  # Apply endpoints/split before paying for scores.
            defer=self.config.defer_scores and self.config.eager_proofs and self.config.proof_reuse and self.config.charge_memo
            if not defer and not node.closed and self.gain(node.bindings(node.lower))<=0 and not self.config.capacity:
                node.closed=True
            cursor+=1
        # Preserve FIFO age order without rescanning all historical closed nodes.
        self.nodes = [n for n in self.nodes if not n.closed]

    def choose(self):
        if self.config.mode == 'mass':
            self.prepare_nodes()
            if self.config.eager_proofs and self.config.proof_reuse and self.config.charge_memo and self.core_calls+self.memo_hits>=self.discovery_budget:
                return None
            available=[]
            for i,node in enumerate(self.nodes):
                if node.closed:continue
                lower=node.bindings(node.lower)
                if self.gain(lower)>0:available.append((i,node,lower))
                elif not self.config.capacity:node.closed=True
        else:
            available = []
            for i, task in enumerate(self.tasks):
                if task['closed']:
                    continue
                if self.config.mode == 'mus':
                    while task['cursor'] < len(task['x']) and task['cursor'] not in task['working']:
                        task['cursor'] += 1
                    if task['cursor'] == len(task['x']):
                        task['closed'] = True; continue
                    indices = sorted(task['working'] - {task['cursor']})
                else:
                    if 'next' not in task:
                        task['next'] = next(task['combinations'], None)
                    if task['next'] is None:
                        task['closed'] = True; continue
                    indices = task['next']
                available.append((i, task, tuple((j, task['x'][j]) for j in indices)))
        if not available:
            return None
        steps = self.core_calls + (self.memo_hits if self.config.charge_memo else 0)
        oldest = self.config.scheduler == 'fifo' or (self.config.scheduler == 'mass_fair'
                                                    and (steps + 1) % self.config.fairness == 0)
        if oldest:
            chosen = min(available, key=lambda row: row[0])
        elif self.config.scheduler == 'width':
            chosen = min(available, key=lambda row: (len(row[2]), row[0]))
        else:
            chosen = max(available, key=lambda row: (self.gain(row[2]), -row[0]))
        _, task, lower = chosen
        if self.config.mode == 'mass':
            indices = task.lower if task.maximum == 'unsat' else task.lower | task.undecided
            return task, task.bindings(indices), self.gain(lower)
        return task, lower, self.gain(lower)

    def lookup_proof(self, bindings):
        t=perf_counter();answer=None;evidence=None;method='exact'
        if self.config.proof_reuse:
            if self.config.memoize_misses and bindings in self._proof_misses:
                self.proof_miss_hits += 1
                self.times['proof_lookup'] += perf_counter()-t
                return None,None,method
            core=self.proofs.nogoods.find(bindings)
            if core is not None:
                answer=Answer('unsat',{},core);evidence=core;method='unsat'
            else:
                proof=self.proofs.find_sat(bindings)
                if proof is not None:
                    answer=Answer('sat',dict(proof.witness),(),completion=proof.completion)
                    evidence=proof.bindings;method='sat'
            if answer is None and self.config.memoize_misses:
                # A miss stays a miss until either proof store changes. This
                # caches no solver status and spends no logical search step.
                if len(self._proof_misses)>=32768:self._proof_misses.clear()
                self._proof_misses.add(bindings)
        else:
            answer = self.known.get(bindings)
        self.times['proof_lookup']+=perf_counter()-t
        return answer,evidence,method

    def refine(self, task, bindings, inferred=None):
        answer,evidence,method=self.lookup_proof(bindings) if inferred is None else inferred
        if answer is None:
            answer = self.query(bindings, True)
        else:
            self.memo_hits += 1
            self.inference_hits[method]+=1
            self.events.append(dict(kind='memo',candidate=self.candidates,region=bindings,status=answer.status,
                                    method=method,evidence=evidence,core=answer.core))
        if answer.status not in ('sat', 'unsat'):
            return answer.status
        if answer.status == 'unsat':
            self.insert(answer.core)
        if self.config.mode == 'mass':
            if task.maximum == 'unsat':
                task.minimum = answer.status
                if answer.status == 'unsat': task.closed = True
            else:
                task.maximum = answer.status
                if answer.status == 'sat': task.closed = True
        elif self.config.mode == 'mus':
            if answer.status == 'unsat': task['working'].discard(task['cursor'])
            task['cursor'] += 1
        else:
            if answer.status == 'unsat': task['closed'] = True
            else: task.pop('next', None)
        return ''

    def allow_learning(self):
        """Conservative online spending gate, not a break-even guarantee.

        No future candidates or offline feasible probability are consulted.
        One probe may overrun its estimate; actual cost is debited afterwards.
        """
        if not self.config.adaptive:
            return True
        if self.candidates < self.config.warmup:
            reason = 'warmup'
        elif self.candidates - self.last_raw_failure >= self.config.warmup:
            reason = 'covered'
        else:
            calls = self.business_calls + self.core_calls
            query_seconds = sum(self.times[k] for k in ('query_build', 'solve', 'extract'))
            estimate = query_seconds / max(1, calls)
            credit = self.config.learning_fraction * self.business_seconds - self.learning_seconds
            reason = 'allowed' if credit >= 2 * estimate else 'cost'
        self.adaptive_decisions[reason] += 1
        if reason != self._last_gate_reason:
            self.events.append(dict(kind='adaptive',candidate=self.candidates,decision=reason,
                                    business_seconds=self.business_seconds,learning_seconds=self.learning_seconds))
            self._last_gate_reason = reason
        return reason == 'allowed'

    def run(self, requested, seed, candidate_limit=100000,wall_limit=None):
        if requested < 0 or candidate_limit < 0:
            raise ValueError('Nonnegative counts required')
        if wall_limit is not None and wall_limit<=0:raise ValueError('Positive wall limit required')
        started = perf_counter()
        # Public online progress for spending policies; never future samples.
        self.requested=requested;self.accepted_count=0;self.candidate_limit=candidate_limit
        rng = random.Random(seed)  # Never consumed by discovery, scheduling or CP.
        accepted, positions, witnesses = [], [], []
        status = 'complete'
        while len(accepted) < requested and self.candidates < candidate_limit:
            # Censor between candidates, never skip an undecided input. A query
            # or learning step already in flight can overrun this soft limit.
            if wall_limit is not None and perf_counter()-started>=wall_limit:
                status='wall_limit';break
            x = self.prior.draw(rng); self.candidates += 1
            bindings=tuple(enumerate(x))
            t = perf_counter()
            hit = (self.proofs.nogoods.find(bindings) is not None if self.config.proof_index=='watched'
                   else any(matches(r,x) for r in self.cache))
            self.times['match'] += perf_counter() - t
            if hit:
                self.cache_hits += 1
            else:
                t=perf_counter();proof=None
                if self.config.mode!='none' and self.config.sat_memo:
                    index=self.proofs.sat_exact.get(bindings)
                    if index is not None:proof=self.proofs.sat[index]
                self.times['proof_lookup']+=perf_counter()-t
                if proof is not None and proof.completion is not None:
                    self.sat_cache_hits+=1
                    answer=Answer('sat',dict(proof.witness),(),completion=proof.completion)
                    self.events.append(dict(kind='sat_cache',candidate=self.candidates,region=bindings,
                                            evidence=proof.bindings,witness=answer.witness))
                else:
                    answer = self.query(bindings)
                if answer.status == 'sat':
                    if not self.problem.feasible(x) or not self.problem.check_witness(x, answer.witness):
                        raise AssertionError('Solver/model disagrees with independent business checks')
                    accepted.append(x); positions.append(self.candidates); witnesses.append(answer.witness)
                    self.accepted_count=len(accepted)
                elif answer.status == 'unsat':
                    self.last_raw_failure = self.candidates
                    if self.problem.feasible(x):
                        raise AssertionError('CP rejected a business-feasible input')
                    if self.config.mode != 'none':
                        proof = tuple(enumerate(x)) if self.config.mode == 'full' else answer.core
                        self.insert(proof)
                    if self.config.mode in ('mass', 'mus', 'minimum'):
                        self.add_task(x)
                else:
                    status = answer.status; break  # Never skip an undecided candidate.
            if len(accepted) >= requested:
                break
            budget = min(self.config.core_limit, floor(self.config.beta * sqrt(self.candidates)))
            self.discovery_budget=budget
            # Reused answers consume a logical search step. Otherwise saved
            # solver calls are immediately reinvested in further exploration,
            # hiding the intended latency saving under the same call budget.
            while (self.config.mode in ('mass', 'mus', 'minimum') and
                   self.core_calls + (self.memo_hits if self.config.charge_memo else 0) < budget):
                if wall_limit is not None and perf_counter()-started>=wall_limit:break
                if not self.allow_learning():
                    break
                learning_started = perf_counter()
                t = perf_counter(); choice = self.choose()
                self.times['selection'] += perf_counter() - t
                if choice is None:
                    self.learning_seconds += perf_counter() - learning_started
                    break
                task, bindings, bound = choice
                if self.config.adaptive:
                    remaining = min(candidate_limit - self.candidates,
                                    (requested-len(accepted)) * (self.candidates+1) / (len(accepted)+1))
                    business_cost = self.business_seconds / max(1, self.business_calls)
                    query_cost = sum(self.times[k] for k in ('query_build','solve','extract')) / max(1,self.business_calls+self.core_calls)
                    # This is an optimistic screening estimate, NOT an expected
                    # discovered gain or a guarantee that a probe will pay off.
                    if remaining * bound * business_cost <= query_cost + (perf_counter()-learning_started):
                        self.adaptive_decisions['horizon'] += 1
                        self.learning_seconds += perf_counter()-learning_started
                        break
                if self.config.gain_horizon and max(0, self.config.gain_horizon - self.candidates) * bound <= 1:
                    self.learning_seconds += perf_counter() - learning_started
                    break
                failure = self.refine(task, bindings)
                self.learning_seconds += perf_counter() - learning_started
                if failure:
                    status = failure; break
            if status != 'complete': break
        if len(accepted) < requested and status == 'complete':
            status = 'candidate_limit'
        elapsed = perf_counter() - started
        # Final coverage is a report, not work needed to emit the last sample.
        # Keep its wall and CPU cost separate for every strategy, including raw.
        online_mass=self.times['mass'];online_fallbacks=self.prior.coverage_fallbacks
        diagnostic_started=perf_counter();diagnostic_cpu=process_time()
        final_mass = 1 - self.gain(())
        diagnostic_cpu=process_time()-diagnostic_cpu
        diagnostic_wall=perf_counter()-diagnostic_started
        diagnostic_mass=self.times['mass']-online_mass
        self.times['mass']=online_mass
        return dict(status=status, requested=requested, seed=seed, config=asdict(self.config),wall_limit=wall_limit,
                    candidates=self.candidates, samples=accepted, positions=positions, witnesses=witnesses,
                    business_calls=self.business_calls, discovery_calls=self.core_calls,
                    total_calls=self.business_calls + self.core_calls, cache_hits=self.cache_hits,
                    cache=list(self.cache), cache_mass=final_mass,
                    cache_mass_kind='lower_bound' if self.prior.coverage_fallbacks else 'exact',
                    coverage=dict(node_limit=self.prior.mass_node_limit,online_fallbacks=online_fallbacks,
                                  total_fallbacks=self.prior.coverage_fallbacks),
                    diagnostics=dict(cpu_seconds=diagnostic_cpu,wall_seconds=diagnostic_wall,mass_seconds=diagnostic_mass),
                    active_searches=sum(not n.closed for n in self.nodes) + sum(not t['closed'] for t in self.tasks),
                    memo_hits=self.memo_hits, learning_seconds=self.learning_seconds,
                    sat_cache_hits=self.sat_cache_hits,inference_hits=self.inference_hits,proof_store=self.proofs.stats(),
                    proof_miss_hits=self.proof_miss_hits,
                    search_steps=self.core_calls+self.memo_hits,
                    adaptive_decisions=self.adaptive_decisions,
                    wall_seconds=elapsed, timings=self.times.copy(), events=self.events)
