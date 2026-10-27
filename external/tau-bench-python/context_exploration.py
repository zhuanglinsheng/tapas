"""Online context redundancy as a spending signal, never a feasibility proof.

A shared literal intersection of distinct native business cores is only a query
hypothesis. No model metadata, offline feasibility mass, or future input is used.
The opportunity score is a screening heuristic, not an unbiased gain estimator.
"""
from collections import OrderedDict, deque
from math import prod
from time import perf_counter
from exploration import CoreSampler


class ContextSignals:
    def __init__(self,prior,window=32,limit=256):
        self.prior=prior;self.window=window;self.limit=limit
        self.recent=deque(maxlen=window)
        self.hypotheses=OrderedDict()
        self.observed=0;self.generated=0

    def mass(self,bindings):
        return prod(self.prior.maps[i].get(v,0) for i,v in bindings)

    def observe(self,core,candidate):
        core=tuple(sorted(core));self.observed+=1
        if len(core)<=1 or any(old==core for _,old in self.recent):return []
        created=[];literals=set(core)
        for age,old in self.recent:
            shared=tuple(sorted(literals.intersection(old)))
            if not shared or len(shared)>=min(len(core),len(old)):continue
            if shared not in self.hypotheses:
                self.hypotheses[shared]=dict(region=shared,parents={},tested=False)
                self.generated+=1;created.append(shared)
            h=self.hypotheses[shared]
            # Bounded actual observations; duplicate kernels are not diversity.
            h['parents'][old]=age;h['parents'][core]=candidate
            while len(h['parents'])>self.window:
                oldest=min(h['parents'],key=h['parents'].get);del h['parents'][oldest]
        self.recent.append((candidate,core))
        while len(self.hypotheses)>self.limit:self.hypotheses.popitem(last=False)
        return created

    def score(self,h,candidate,horizon):
        # Windowed support decays even when the old hypothesis remains retained.
        earliest=self.recent[0][0] if self.recent else candidate
        parents={core:age for core,age in h['parents'].items() if age>=earliest}
        support=len(parents)
        if support<2:return dict(opportunity=0.,support=support)
        mass=self.mass(h['region']);average=sum(self.mass(c) for c in parents)/support
        compression=mass/average if average else 0.
        # Overlapping contexts need not partition the region. This proxy may
        # under/overestimate their count, so only a native answer can certify it.
        unseen=max(0.,compression-support)
        arrival=(support-1)/max(1,candidate-min(parents.values()))
        opportunity=min(unseen,horizon*arrival)
        return dict(opportunity=opportunity,support=support,compression=compression,
                    novelty_per_candidate=arrival,remaining_candidates=horizon,mass=mass)


class ContextSampler(CoreSampler):
    """Redundancy-gated deletion, or direct certified intersection probing.

    Default safety rules: need >2 forecast avoidable queries; 3 consecutive
    unproductive paid queries stop; a 10ms soft learning allowance. Thresholds
    are deliberately explicit and are not a break-even guarantee.
    """
    def __init__(self,problem,config,*,direct=True,oracle=None,time_budget=.01):
        super().__init__(problem,config,stop_after=3,time_budget=time_budget,oracle=oracle)
        self.direct=direct;self.signals=ContextSignals(self.prior)
        self.gates=dict(no_signal=0,opportunity=0,cost=0,allowed=0,known=0)
        self.business_unsat_seconds=0.;self.business_unsat_count=0
        self.aux_seconds=0.;self.aux_count=0
        self.signal_seconds=0.;self.gate_seconds=0.
        self._gate_state=None

    def query(self,r,discovery=False):
        answer=super().query(r,discovery)
        seconds=answer.build_seconds+answer.solve_seconds+answer.extract_seconds
        if discovery:self.aux_seconds+=seconds;self.aux_count+=1
        elif answer.status=='unsat':
            self.business_unsat_count+=1;self.business_unsat_seconds+=seconds
            # Recording stays outside CoreSampler's measured learning interval,
            # but inside the sampler's online CPU/wall time.
            if not self.stopped:
                started=perf_counter();self.signals.observe(answer.core,self.candidates)
                self.signal_seconds+=perf_counter()-started
        return answer

    def add_task(self,x):
        if not self.direct:return super().add_task(x)
        # Signals contain hypotheses only; business insertion has already run.
        # Task creation is deferred to choose, bounding live tasks by the window.
        self.pending_tasks=int(any(not h['tested'] for h in self.signals.hypotheses.values()))

    def horizon(self):
        return min(self.candidate_limit-self.candidates,
            (self.requested-self.accepted_count)*(self.candidates+1)/(self.accepted_count+1))

    def eligible(self):
        horizon=self.horizon();best=None
        for h in self.signals.hypotheses.values():
            if h['tested']:continue
            # Proof lookup is sound; the score is not. Do not spend on a region
            # whose feasibility is already implied by available evidence.
            inferred=self.lookup_proof(h['region'])
            if inferred[0] is not None:
                h['tested']=True;self.gates['known']+=1;continue
            score=self.signals.score(h,self.candidates,horizon)
            if best is None or score['opportunity']>best[1]['opportunity']:best=h,score
        return best

    def allow_learning(self):
        if self.time_budget and self.learning_seconds+self.signal_seconds+self.gate_seconds>=self.time_budget:
            self.stopped=True;self.probe_stats['stopped']=True;self.probe_stats['time_stopped']=True
            return False
        # Direct hypotheses have no persistent CoreSampler tasks to count.
        if self.direct:self.pending_tasks=int(any(not h['tested'] for h in self.signals.hypotheses.values()))
        if not super().allow_learning():return False
        started=perf_counter();best=self.eligible()
        business=self.business_unsat_seconds/max(1,self.business_unsat_count)
        aux=self.aux_seconds/self.aux_count if self.aux_count else business
        cost_ratio=aux/max(business,1e-12)
        reason=('no_signal' if best is None else 'opportunity' if best[1]['opportunity']<=2
                else 'cost' if best[1]['opportunity']<=2*cost_ratio else 'allowed')
        self.gates[reason]+=1
        state=(reason,best[0]['region'] if best else None)
        if state!=self._gate_state:
            self.events.append(dict(kind='context_gate',candidate=self.candidates,decision=reason,
                region=state[1],score=best[1] if best else None,cost_ratio=cost_ratio))
            self._gate_state=state
        self._eligible=best
        self.gate_seconds+=perf_counter()-started
        return reason=='allowed'

    def choose(self):
        if not self.direct:return super().choose()
        h,score=self._eligible;h['tested']=True
        # CoreSampler.refine handles certificates and stop counters. This direct
        # task always closes after its one query; SAT never authorizes rejection.
        core=next(iter(h['parents']))
        task=dict(core=core,essential=set(),closed=False,testing=None)
        self.tasks.append(task);self.pending_tasks=1
        self.events.append(dict(kind='context_probe',candidate=self.candidates,region=h['region'],score=score))
        return task,h['region'],score['mass']

    def refine(self,task,bindings,inferred=None):
        result=super().refine(task,bindings,inferred)
        if self.direct:self.close(task)
        return result

    def run(self,*args,**kwargs):
        row=super().run(*args,**kwargs)
        row['context_adaptation']=dict(direct=self.direct,window=self.signals.window,
            hypothesis_limit=self.signals.limit,minimum_opportunity=2,cost_safety_factor=2,
            observed_business_cores=self.signals.observed,generated_hypotheses=self.signals.generated,
            retained_hypotheses=len(self.signals.hypotheses),gates=self.gates,
            signal_seconds=self.signal_seconds,gate_seconds=self.gate_seconds)
        return row
