"""Native-core active learning with explicit positive and negative controls."""
import unittest
from dataclasses import replace
from exploration_workloads import allocation,packing,multiguard,partial_unsat
from itertools import combinations,product
from exploration import CoreSampler,SafeAuxMassSampler
from oracle import Oracle,Answer
from sampler import Sampler,CONFIGS
from run_experiments import audit_row,exact_distribution


class ExplorationTests(unittest.TestCase):
    def test_native_context_core_can_hide_global_guard_conflict(self):
        p=allocation();o=Oracle(p)
        full=o.query(tuple(enumerate((1,0,1,2,0))))
        partial=o.query(((0,1),))
        self.assertEqual(full.status,'unsat');self.assertEqual(partial.status,'unsat')
        self.assertGreater(len(full.core),len(partial.core))
        self.assertEqual(partial.core,((0,1),))

    def test_feasible_capacity_is_a_real_negative_control(self):
        p=allocation('feasible');o=Oracle(p)
        self.assertEqual(o.query(((0,1),)).status,'sat')
        self.assertEqual(o.query(tuple(enumerate((1,0,1,2)))).status,'sat')
        self.assertEqual(o.query(tuple(enumerate((1,0,0,2)))).status,'unsat')

    def test_refinement_preserves_stream_and_has_net_call_advantage(self):
        for pos in (0,4):
            p=allocation(guard_position=pos)
            raw=Sampler(p,CONFIGS['raw']).run(128,11)
            active=CoreSampler(p,replace(CONFIGS['mus'],name='core_scout'),stop_after=3).run(128,11)
            self.assertEqual((raw['samples'],raw['positions']),(active['samples'],active['positions']))
            audit_row(p,active,exact_distribution(p,8192))
            self.assertLess(active['total_calls'],raw['total_calls'])

    def test_simplified_equivalent_encoding_needs_no_scouting(self):
        p=allocation(encoding='simplified')
        raw=Sampler(p,CONFIGS['raw']).run(32,29)
        active=CoreSampler(p,replace(CONFIGS['mus'],name='core_scout'),stop_after=3).run(32,29)
        self.assertEqual(active['discovery_calls'],0)
        self.assertEqual(active['total_calls'],raw['total_calls'])
        self.assertEqual(active['samples'],raw['samples'])

    def test_failed_scouting_stops_and_keeps_certificates_valid(self):
        p=allocation('feasible')
        active=CoreSampler(p,replace(CONFIGS['mus'],name='core_scout'),stop_after=3).run(128,47)
        raw=Sampler(p,CONFIGS['raw']).run(128,47)
        self.assertEqual(active['samples'],raw['samples'])
        self.assertLessEqual(active['discovery_calls'],3)
        audit_row(p,active,exact_distribution(p,8192))

    def test_auxiliary_unknown_is_abandoned_but_business_unknown_stops(self):
        p=allocation();cfg=replace(CONFIGS['mus'],name='core_scout')
        class PartialUnknown(Oracle):
            def query(self,r):
                return Answer('unknown',{},()) if len(r)<len(p.xvars) else super().query(r)
        active=CoreSampler(p,cfg,stop_after=3,oracle=PartialUnknown(p)).run(32,11)
        raw=Sampler(p,CONFIGS['raw']).run(32,11)
        self.assertEqual(active['status'],'complete');self.assertEqual(active['samples'],raw['samples'])
        self.assertGreater(active['core_refinement']['unknown'],0)
        audit_row(p,active,exact_distribution(p,8192))
        safe_mass=SafeAuxMassSampler(p,replace(CONFIGS['mass_b4'],name='mass_safe'),PartialUnknown(p)).run(32,11)
        self.assertEqual(safe_mass['status'],'complete');self.assertEqual(safe_mass['samples'],raw['samples'])
        audit_row(p,safe_mass,exact_distribution(p,8192))
        class AllUnknown:
            def query(self,r):return Answer('unknown',{},())
        stopped=CoreSampler(p,cfg,oracle=AllUnknown()).run(32,11)
        self.assertEqual(stopped['status'],'unknown');self.assertEqual(stopped['candidates'],1)

    def test_independent_partial_checker_matches_exhaustive_completions(self):
        for p in (allocation(),allocation('feasible')):
            points=list(p.prior.points());feasible=[x for x in points if p.feasible(x)]
            for width in (0,1,2,3):
                for coords in combinations(range(len(p.xvars)),width):
                    for values in product(*(p.prior.domains[i] for i in coords)):
                        r=tuple(zip(coords,values))
                        brute=not any(all(x[i]==v for i,v in r) for x in feasible)
                        self.assertEqual(partial_unsat(p,r),brute)

    def test_packing_has_exact_witnesses_and_feasible_capacity_control(self):
        for bins in (3,4):
            p=packing(bins);o=Oracle(p)
            answer=o.query(((0,1),))
            self.assertEqual(answer.status,'unsat' if bins==3 else 'sat')
            if bins==4:
                self.assertTrue(p.feasible(answer.completion))
                self.assertTrue(p.check_witness(answer.completion,answer.witness))
            row=CoreSampler(p,replace(CONFIGS['mus'],name='core_scout'),stop_after=3).run(32,11)
            audit_row(p,row,exact_distribution(p,8192))

    def test_empty_frontier_gate_keeps_query_trace(self):
        for p in (allocation(),allocation('feasible'),allocation(encoding='simplified')):
            rows=[CoreSampler(p,replace(CONFIGS['mus'],name='core_scout'),stop_after=3,skip_empty=flag).run(64,101)
                  for flag in (True,False)]
            def trace(r):return [{k:v for k,v in e.items() if k!='seconds'} for e in r['events']]
            self.assertEqual(trace(rows[0]),trace(rows[1]))

    def test_delayed_scout_never_probes_before_its_warmup(self):
        p=allocation();cfg=replace(CONFIGS['mus'],name='core_delayed')
        row=CoreSampler(p,cfg,stop_after=3,start_after=32).run(128,101)
        self.assertTrue(all(e['candidate']>=32 for e in row['events'] if e['kind']=='discovery'))
        raw=Sampler(p,CONFIGS['raw']).run(128,101)
        self.assertEqual((row['samples'],row['positions']),(raw['samples'],raw['positions']))

    def test_bounded_mass_is_an_upper_bound_and_never_a_feasibility_proof(self):
        from prior import ProductPrior
        p=ProductPrior(['a','b','c'],[(0,1)]*3)
        cache=(((0,1),(1,1)),((1,0),(2,0)))
        exact=p.uncovered((),cache);p._residual_mass.cache_clear();p.mass_node_limit=1
        bound=p.uncovered((),cache)
        self.assertGreaterEqual(bound,exact);self.assertLessEqual(bound,1)
        self.assertGreater(p.coverage_fallbacks,0)
        p.mass_node_limit=0
        self.assertAlmostEqual(p.uncovered((),cache),exact)  # No aborted value was memoized.
        problem=allocation();raw=Sampler(problem,CONFIGS['raw']).run(32,101)
        problem.prior.mass_node_limit=1
        active=CoreSampler(problem,replace(CONFIGS['mus'],name='core_scout'),stop_after=3).run(32,101)
        self.assertEqual((raw['samples'],raw['positions']),(active['samples'],active['positions']))
        audit_row(problem,active,exact_distribution(problem,8192))
        self.assertIn(active['cache_mass_kind'],('exact','lower_bound'))
        self.assertGreaterEqual(active['diagnostics']['cpu_seconds'],0)

    def test_tiny_learning_time_budget_limits_spending_without_changing_samples(self):
        p=allocation();cfg=replace(CONFIGS['mus'],name='core_timed')
        active=CoreSampler(p,cfg,stop_after=3,time_budget=1e-12).run(32,101)
        raw=Sampler(p,CONFIGS['raw']).run(32,101)
        self.assertEqual((active['samples'],active['positions']),(raw['samples'],raw['positions']))
        self.assertLessEqual(active['discovery_calls'],1)
        audit_row(p,active,exact_distribution(p,8192))

    def test_multiguard_core_is_a_conjunction_and_preserves_sampling(self):
        for width in (2,4):
            p=multiguard(width);oracle=Oracle(p)
            full=tuple((i,1) for i in p.metadata['guard_positions'])
            self.assertEqual(oracle.query(full).status,'unsat')
            self.assertEqual(oracle.query(full[:-1]).status,'sat')
            raw=Sampler(p,CONFIGS['raw']).run(32,201)
            active=CoreSampler(p,replace(CONFIGS['mus'],name='core_scout'),stop_after=3).run(32,201)
            self.assertEqual((raw['samples'],raw['positions']),(active['samples'],active['positions']))
            audit_row(p,active,exact_distribution(p,8192))


if __name__=='__main__':unittest.main()
