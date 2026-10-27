"""Proof direction, watch resets, eviction, witnesses, and certificate regressions."""
from dataclasses import replace
import random
import unittest
from proofs import NogoodIndex, ProofStore
from sampler import Sampler, CONFIGS, Node
from workloads import synthetic
from oracle import Oracle
from prior import matches
from run_experiments import audit_row, exact_distribution


class ProofIndexTests(unittest.TestCase):
    def test_watches_match_linear_for_independent_partial_queries_and_eviction(self):
        rng=random.Random(20261021)
        for long_only in (False,True):
            index=NogoodIndex()
            regions=[]
            for step in range(1200):
                if step%19==0:
                    regions=[tuple((i,rng.randrange(3)) for i in sorted(rng.sample(range(9),rng.randint(3 if long_only else 1,9))))
                             for _ in range(rng.randrange(1,30))]
                    index.replace(regions)
                query=tuple((i,rng.randrange(3)) for i in sorted(rng.sample(range(9),rng.randrange(10))))
                found=index.find(query)
                possible=[r for r in regions if set(r).issubset(query)]
                self.assertEqual(found is not None,bool(possible))
                if found is not None:self.assertIn(found,possible)
            if long_only:self.assertGreater(index.watch_moves,0)
        index.replace([()]);self.assertEqual(index.find(()),())
        index.replace([]);self.assertIsNone(index.find(((0,1),)))

    def test_sat_proofs_only_imply_weaker_queries(self):
        store=ProofStore()
        store.remember_sat(((0,0),(1,1)),{'y':1})
        self.assertIsNotNone(store.find_sat(((0,0),)))
        self.assertIsNone(store.find_sat(((0,0),(1,0))))
        self.assertIsNone(store.find_sat(((0,0),(1,1),(2,1))))
        store.remember_sat(((0,0),),{},(0,1,2))
        proof=store.find_sat(((2,2),))
        self.assertEqual(proof.completion,(0,1,2))
        self.assertIsNone(store.find_sat(((2,1),)))

    def test_removed_nogood_does_not_survive_index_rebuild(self):
        store=ProofStore()
        store.nogoods.replace([((0,1),),((1,1),(2,1))])
        self.assertEqual(store.nogoods.find(((0,1),)),((0,1),))
        store.nogoods.replace([((1,1),(2,1))])
        self.assertIsNone(store.nogoods.find(((0,1),)))


class ReuseTests(unittest.TestCase):
    def test_sat_completion_is_certified_for_original_partial_query(self):
        for p in synthetic():
            answer=Oracle(p).query(())
            self.assertEqual(answer.status,'sat')
            self.assertTrue(p.feasible(answer.completion))
            self.assertTrue(p.check_witness(answer.completion,answer.witness))

    def test_unsat_subsumption_and_sat_superset_skip_real_queries(self):
        p=synthetic()[0];s=Sampler(p,CONFIGS['mass_b4'])
        answer=s.query(((0,1),(1,1)))
        s.insert(answer.core)
        task=Node((1,1,0,0,0,0),frozenset([0,1,2]),frozenset([3,4,5]))
        s.refine(task,((0,1),(1,1),(2,0)))
        self.assertEqual(s.core_calls,0)
        self.assertEqual(s.inference_hits['unsat'],1)
        s.query(((0,0),(1,0),(2,0),(3,0),(4,0),(5,0)))
        task=Node((1,1,0,0,0,0),frozenset([3]),frozenset([0,1,2,4,5]))
        s.refine(task,((3,0),))
        self.assertEqual(s.core_calls,0)
        self.assertEqual(s.inference_hits['sat'],1)

    def test_complete_witness_reuse_keeps_duplicate_samples_and_positions(self):
        p=synthetic()[-1]
        original=Sampler(p,replace(CONFIGS['raw'],proof_reuse=False,sat_memo=False,proof_index='linear')).run(128,11)
        reused=Sampler(p,CONFIGS['raw']).run(128,11)
        self.assertEqual(original['samples'],reused['samples'])
        self.assertEqual(original['positions'],reused['positions'])
        self.assertEqual(len(reused['samples']),128)
        self.assertEqual(reused['sat_cache_hits'],128-len(set(reused['samples'])))
        self.assertEqual(reused['business_calls'],len(set(reused['samples']))+1)
        self.assertLess(reused['total_calls'],original['total_calls'])
        audit_row(p,reused,exact_distribution(p,4096))

    def test_eager_inference_respects_logical_budget_before_scoring(self):
        p=synthetic()[0];s=Sampler(p,CONFIGS['mass_b4'])
        s.query(((0,0),(1,0),(2,0),(3,0),(4,0),(5,0)))
        s.add_task((1,1,0,0,0,0));s.discovery_budget=1
        self.assertIsNone(s.choose())
        self.assertEqual(s.memo_hits,1)
        self.assertEqual(s.core_calls,0)
        self.assertEqual(s.times['mass'],0)
        s=Sampler(p,CONFIGS['mass_b4'])
        s.query(tuple(enumerate((0,0,0,0,0,0))))
        s.nodes=[Node((1,1,0,0,0,0),frozenset([0]),frozenset([1,2,3,4,5])),
                 Node((1,1,0,0,0,0),frozenset(),frozenset(range(6)))]
        s.discovery_budget=1
        self.assertIsNone(s.choose())
        self.assertEqual(s.memo_hits,1)
        self.assertEqual(s.times['mass'],0)  # First unknown node was not scored needlessly.

    def test_partial_sat_never_admits_incompatible_complete_input(self):
        p=synthetic()[0];s=Sampler(p,CONFIGS['raw'])
        s.query(())
        self.assertIsNone(s.proofs.find_sat(tuple(enumerate((1,1,0,0,0,0)))))

    def test_forged_inference_or_witness_is_rejected(self):
        p=synthetic()[-1];r=Sampler(p,CONFIGS['mass_b4']).run(32,11)
        exact=exact_distribution(p,4096);audit_row(p,r,exact)
        memo=next(e for e in r['events'] if e['kind']=='memo' and e['status']=='sat')
        memo['evidence']=((0,999),)
        with self.assertRaises(AssertionError):audit_row(p,r,exact)
        r=Sampler(p,CONFIGS['raw']).run(32,11)
        cached=next(e for e in r['events'] if e['kind']=='sat_cache')
        cached['witness']={'completion':999}
        with self.assertRaises(AssertionError):audit_row(p,r,exact)

    def test_index_modes_and_finite_cache_preserve_stream_and_certificates(self):
        for p in synthetic():
            ref=Sampler(p,CONFIGS['rejection']).run(24,29)
            for name in ('raw','mass_b4','bounded_mass','adaptive_b4'):
                for mode in ('linear','watched'):
                    for eager,defer in ((False,True),(True,False),(True,True)):
                        config=replace(CONFIGS[name],proof_index=mode,eager_proofs=eager,defer_scores=defer)
                        r=Sampler(p,config).run(24,29)
                        self.assertEqual((r['samples'],r['positions']),(ref['samples'],ref['positions']))
                        if config.capacity:self.assertLessEqual(r['proof_store']['nogood_entries'],config.capacity)
                        audit_row(p,r,exact_distribution(p,4096))


if __name__=='__main__':unittest.main()
