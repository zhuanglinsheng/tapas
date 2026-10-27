"""Online-only redundancy signals must never become unverified proofs."""
import unittest
import random
from dataclasses import replace
from context_exploration import ContextSignals,ContextSampler
from exploration_workloads import allocation,multiguard
from oracle import Oracle,Answer
from sampler import Sampler,CONFIGS
from run_experiments import audit_row,exact_distribution


class ContextTests(unittest.TestCase):
    def test_diverse_native_kernels_form_hypothesis_not_certificate(self):
        p=allocation();s=ContextSignals(p.prior)
        first=((0,1),(1,0),(2,0));second=((0,1),(3,1),(4,1))
        s.observe(first,2);s.observe(first,3)
        self.assertFalse(s.hypotheses)
        s.observe(second,4)
        h=s.hypotheses[((0,1),)]
        score=s.score(h,4,100)
        self.assertEqual(score['support'],2)
        self.assertGreater(score['opportunity'],2)
        self.assertEqual(s.score(h,4,0)['opportunity'],0)
        self.assertFalse(h['tested'])

    def test_context_overlap_can_be_sat_and_never_rejects_candidates(self):
        p=allocation('feasible');o=Oracle(p);s=ContextSignals(p.prior)
        s.observe(((0,1),(1,0),(2,0)),1)
        s.observe(((0,1),(2,1),(3,1)),2)
        shared=((0,1),)
        self.assertIn(shared,s.hypotheses)
        self.assertEqual(o.query(shared).status,'sat')
        row=ContextSampler(p,replace(CONFIGS['mus'],name='context_direct')).run(128,11)
        raw=Sampler(p,CONFIGS['raw']).run(128,11)
        self.assertEqual((row['samples'],row['positions']),(raw['samples'],raw['positions']))
        audit_row(p,row,exact_distribution(p,8192))

    def test_gating_and_direct_probes_preserve_distribution_and_save_calls(self):
        for p in (allocation(),allocation(guard_position=4),multiguard(4)):
            raw=Sampler(p,CONFIGS['raw']).run(128,101)
            for direct in (False,True):
                row=ContextSampler(p,replace(CONFIGS['mus'],name='context'),direct=direct,time_budget=0).run(128,101)
                self.assertEqual((row['samples'],row['positions']),(raw['samples'],raw['positions']))
                self.assertLess(row['total_calls'],raw['total_calls'])
                audit_row(p,row,exact_distribution(p,8192))
                discoveries=[e for e in row['events'] if e['kind']=='discovery']
                business=[e for e in row['events'] if e['kind']=='business' and e['status']=='unsat']
                self.assertTrue(all(sum(b['candidate']<=d['candidate'] for b in business)>=2 for d in discoveries))

    def test_strong_native_core_never_triggers_exploration(self):
        p=allocation(encoding='simplified')
        row=ContextSampler(p,replace(CONFIGS['mus'],name='context')).run(32,11)
        self.assertEqual(row['discovery_calls'],0)
        self.assertEqual(row['context_adaptation']['generated_hypotheses'],0)

    def test_auxiliary_unknown_is_not_a_proof_and_does_not_stop_business(self):
        p=allocation()
        class PartialUnknown(Oracle):
            def query(self,r):
                if len(r)<len(p.xvars):return Answer('unknown',{},())
                return super().query(r)
        row=ContextSampler(p,replace(CONFIGS['mus'],name='context'),oracle=PartialUnknown(p)).run(32,11)
        raw=Sampler(p,CONFIGS['raw']).run(32,11)
        self.assertEqual(row['status'],'complete')
        self.assertEqual(row['samples'],raw['samples'])
        self.assertLessEqual(row['discovery_calls'],3)
        audit_row(p,row,exact_distribution(p,8192))

    def test_signal_memory_is_bounded(self):
        p=allocation();s=ContextSignals(p.prior,window=3,limit=2)
        for n in range(20):
            s.observe(((0,1),(1,n%3),(2,(n//3)%3)),n+1)
            self.assertLessEqual(len(s.recent),3)
            self.assertLessEqual(len(s.hypotheses),2)
            self.assertTrue(all(len(h['parents'])<=3 for h in s.hypotheses.values()))
        # Evicted parents can both be reintroduced in one update; truncating
        # just once would leave a hypothesis above its configured bound.
        from prior import ProductPrior
        prior=ProductPrior([str(i) for i in range(12)],[(0,1)]*12)
        s=ContextSignals(prior,window=3,limit=16);rng=random.Random(41)
        for n in range(200):
            s.observe(((0,1),)+tuple(sorted((i,rng.randrange(2)) for i in rng.sample(range(1,12),3))),n+1)
            self.assertTrue(all(len(h['parents'])<=3 for h in s.hypotheses.values()))


if __name__=='__main__':unittest.main()
