import itertools
import math
import random
import unittest
from dataclasses import replace
from prior import ProductPrior, JointPrior, matches
from oracle import Oracle, Answer
from sampler import Sampler, CONFIGS, Config, Node
from workloads import synthetic, retail_problem, load_retail, OPERATIONS
from upstream import tools, check
from project import DEFAULT_SOURCE


class PriorTests(unittest.TestCase):
    def test_union_mass_against_enumeration(self):
        prior=ProductPrior(['a','b','c'],[(0,1,2),(0,1),(0,1)],[[1,3,6],[7,3],[2,8]])
        rng=random.Random(9);points=list(prior.points())
        for _ in range(100):
            def draw_region():
                x=rng.choice(points)
                return tuple((i,x[i]) for i in range(3) if rng.random()<.6)
            candidate=draw_region();cache=tuple(draw_region() for _ in range(rng.randrange(6)))
            expected=sum(prior.pmf(x) for x in points if matches(candidate,x) and not any(matches(c,x) for c in cache))
            self.assertAlmostEqual(prior.uncovered(candidate,cache),expected,places=13)

    def test_joint_and_product_same_pmf(self):
        p=ProductPrior(['a','b'],[(0,1)]*2,[[1,9],[4,6]])
        q=JointPrior(p.names,p.domains,{x:p.pmf(x) for x in p.points()})
        cache=(((0,1),(1,1)),)
        self.assertAlmostEqual(p.uncovered((),cache),q.uncovered((),cache))
        self.assertEqual(q.draw(random.Random(7)),q.draw(random.Random(7)))


class ModelTests(unittest.TestCase):
    def test_synthetic_exact_projection_and_cores(self):
        for problem in synthetic():
            oracle=Oracle(problem)
            for x in problem.prior.points():
                answer=oracle.query(tuple(enumerate(x)))
                self.assertEqual(answer.status=='sat',problem.feasible(x))
                if answer.status=='sat':self.assertTrue(problem.check_witness(x,answer.witness))
                else:
                    self.assertTrue(all(not problem.feasible(z) for z in problem.prior.points() if matches(answer.core,z)))

    def test_retail_micro_exact_projection(self):
        data=load_retail()
        for op in OPERATIONS:
            p=retail_problem(data,op,order_count=2,micro=True)
            oracle=Oracle(p)
            for x in p.prior.points():
                answer=oracle.query(tuple(enumerate(x)))
                self.assertEqual(answer.status=='sat',p.feasible(x),(p.name,x))
                if answer.status=='sat':self.assertTrue(p.check_witness(x,answer.witness))

    def test_retail_multislot_original_tools(self):
        data=load_retail();original=tools(DEFAULT_SOURCE)
        for op in OPERATIONS:
            p=retail_problem(data,op,order_count=8,slots=2 if op in ('modify_items','exchange','return') else 1)
            r=Sampler(p,CONFIGS['raw']).run(6,29,candidate_limit=20000)
            self.assertEqual(r['status'],'complete',p.name)
            for x,y in zip(r['samples'],r['witnesses']):check(p,x,y,data,original)


class SamplingTests(unittest.TestCase):
    def test_partial_status_reuse_never_repeats_oracle_call(self):
        p=synthetic()[0];s=Sampler(p,CONFIGS['mass_b4'])
        s.query(())  # SAT; lower endpoint of a genuinely UNSAT full assignment.
        task=Node((1,1,1,1,1,1),frozenset(),frozenset(range(6)))
        self.assertEqual(s.refine(task,()),'')
        self.assertEqual(task.minimum,'sat')
        self.assertEqual(s.core_calls,0)
        self.assertEqual(s.memo_hits,1)
        self.assertFalse(s.cache)

    def test_discovery_uses_native_core_not_whole_query(self):
        p=synthetic()[0];s=Sampler(p,CONFIGS['mass_b4'])
        bindings=((0,1),(1,1),(4,1))
        task=Node((1,1,0,0,1,0),frozenset([0,1,4]),frozenset([2,3,5]))
        self.assertEqual(s.refine(task,bindings),'')
        self.assertEqual(s.cache,[((0,1),(1,1))])
        for x in p.prior.points():
            if matches(s.cache[0],x):self.assertFalse(p.feasible(x))

    def test_adaptive_gate_uses_observed_cost_and_recovers_credit(self):
        s=Sampler(synthetic()[0],CONFIGS['adaptive_b4'])
        s.business_calls=10;s.business_seconds=1;s.times['solve']=1
        s.candidates=31;s.last_raw_failure=31
        self.assertFalse(s.allow_learning())  # warmup
        s.candidates=32;s.last_raw_failure=32
        self.assertFalse(s.allow_learning())  # insufficient credit for a probe
        s.business_calls=100
        self.assertTrue(s.allow_learning())
        s.learning_seconds=.1
        self.assertFalse(s.allow_learning())  # actual learning cost was debited
        s.business_seconds=2
        self.assertTrue(s.allow_learning())  # new business work replenishes credit
        s.candidates=64
        self.assertFalse(s.allow_learning())  # no recently uncovered failure

    def test_tampered_certificate_is_rejected(self):
        from run_experiments import audit_row, exact_distribution
        p=synthetic()[0]
        r=Sampler(p,CONFIGS['raw']).run(4,11)
        exact=exact_distribution(p,4096)
        audit_row(p,r,exact)
        r['events'].append(dict(kind='proof',region=(),candidate=1))
        with self.assertRaises(AssertionError):audit_row(p,r,exact)

    def test_paired_stream_and_all_certificates(self):
        from run_experiments import audit_row, exact_distribution
        for p in synthetic():
            reference=None
            for name in CONFIGS:
                config=CONFIGS[name];r=Sampler(p,config).run(24,11)
                audit_row(p,r,exact_distribution(p,4096))
                self.assertEqual(r['status'],'complete')
                path=(r['samples'],r['positions'])
                if reference is None:reference=path
                self.assertEqual(reference,path,(p.name,name))
                self.assertLessEqual(r['discovery_calls'],math.floor(config.beta*math.sqrt(r['candidates'])))
                self.assertLessEqual(r['search_steps'],math.floor(config.beta*math.sqrt(r['candidates'])))
                self.assertEqual(r['memo_hits'],sum(e['kind']=='memo' for e in r['events']))
                if config.capacity:self.assertLessEqual(len(r['cache']),config.capacity)
                for event in r['events']:
                    if event['kind']=='proof':
                        self.assertTrue(all(not p.feasible(x) for x in p.prior.points() if matches(event['region'],x)))

    def test_mass_memo_is_invalidated_by_eviction(self):
        s=Sampler(synthetic()[3],replace(CONFIGS['bounded_mass'],capacity=1))
        old=((0,1),);new=((1,1),(2,1))
        s.insert(old);self.assertAlmostEqual(s.gain(old),0)
        s.insert(new);self.assertAlmostEqual(s.gain(old),.015)

    def test_unknown_stops_without_acceptance_or_learning(self):
        p=synthetic()[0]
        class Unknown:
            def query(self,bindings):return Answer('unknown',{},())
        r=Sampler(p,CONFIGS['mass_b4'],Unknown()).run(2,11)
        self.assertEqual(r['status'],'unknown');self.assertFalse(r['samples']);self.assertFalse(r['cache'])
        self.assertFalse(any(e['kind']=='proof' for e in r['events']))

    def test_unknown_discovery_cannot_admit_candidate_region(self):
        p=synthetic()[0]
        class FailThenUnknown:
            calls=0
            def query(self,bindings):
                self.calls+=1
                return Answer('unsat',{},tuple(bindings)) if self.calls==1 else Answer('unknown',{},())
        # Seed 11 begins with x0=x1=1 and is genuinely infeasible.
        r=Sampler(p,CONFIGS['mass_b4'],FailThenUnknown()).run(2,11)
        self.assertEqual(r['status'],'unknown')
        self.assertEqual(sum(e['kind']=='proof' for e in r['events']),1)
        self.assertEqual(len(r['cache'][0]),len(p.prior.names))

    def test_candidate_cap_is_not_success(self):
        p=synthetic()[0];r=Sampler(p,CONFIGS['raw']).run(100,11,candidate_limit=1)
        self.assertEqual(r['status'],'candidate_limit')

    def test_auxiliary_multiplicity_changes_no_input_weight(self):
        p=synthetic()[-1]
        target={x:p.prior.pmf(x) for x in p.prior.points() if p.feasible(x)}
        z=sum(target.values());target={x:q/z for x,q in target.items()}
        self.assertAlmostEqual(sum(q for x,q in target.items() if x[0]==1),.75)
        # Weighting by witness multiplicity would incorrectly raise this to 6/7.
        self.assertNotAlmostEqual(.75,6/7)


if __name__=='__main__':unittest.main()
