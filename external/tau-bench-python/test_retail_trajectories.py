"""Workflow transitions must match unmodified original tools, including quirks."""
import json
from itertools import product
import unittest
from retail_trajectories import catalogs,trajectory_problem,OPS
from oracle import Oracle
from upstream import tools
from project import DEFAULT_SOURCE
from benchmark_exploration import strategy
from run_experiments import audit_row,exact_distribution


class RetailTrajectoryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.catalogs=catalogs();cls.original=tools(DEFAULT_SOURCE)
        if not cls.catalogs:raise AssertionError('Pinned data contains no workflow catalogs')

    def test_all_two_step_requests_match_original_acceptance_and_state(self):
        c=self.catalogs[0];p=trajectory_problem(c,2);o=Oracle(p)
        for x in p.prior.points():
            a=o.query(tuple(enumerate(x)));ok,n=p.replay(x,self.original)
            self.assertEqual(a.status=='sat',ok,(x,a.status))
            self.assertEqual(ok,p.feasible(x),x)
            if ok:self.assertTrue(p.check_witness(x,a.witness),x)
        # One-step checks cover every catalog and both real price selections.
        for style in ('tight','nearest'):
            for c in catalogs(upgrade_style=style):
                p=trajectory_problem(c,1)
                for x in p.prior.points():self.assertEqual(p.replay(x,self.original)[0],p.feasible(x))

    def test_joint_gift_depletion_differs_from_independent_action_mixture(self):
        for c in self.catalogs:
            self.assertTrue(c.metadata['tight_two_upgrade_pair'])
            x=(3,0,3,1)
            mixed=trajectory_problem(c,2,mode='mixed');coupled=trajectory_problem(c,2)
            self.assertTrue(mixed.feasible(x));self.assertFalse(coupled.feasible(x))
            self.assertEqual(Oracle(mixed).query(((0,3),(2,3))).status,'sat')
            self.assertEqual(Oracle(coupled).query(((0,3),(2,3))).status,'unsat')
            self.assertTrue(mixed.replay(x,self.original)[0]);self.assertFalse(coupled.replay(x,self.original)[0])

    def test_payment_then_cancellation_preserves_original_history_refund_quirk(self):
        c=self.catalogs[0];p=trajectory_problem(c,2)
        q=next(i for i,o in enumerate(c.orders[:2]) if o['payment_history'][0]['payment_method_id'] in c.gifts)
        x=(2,q,0,q);credit=c.initial.credit[q*len(c.gifts)]
        self.assertTrue(p.replay(x,self.original)[0])
        y=p.simulate(x);self.assertEqual(y[f'history_2_{q}'],6)
        self.assertEqual(y[f'credit_2_{q*len(c.gifts)}'],4*credit)
        self.assertEqual(y['balance_2_0'],c.initial.balance[0]+3*credit)
        a=Oracle(p).query(tuple(enumerate(x)));self.assertTrue(p.check_witness(x,a.witness))

    def test_refund_can_enable_a_later_previously_unaffordable_payment(self):
        c=self.catalogs[0]
        q=next(i for i,o in enumerate(c.orders[:2]) if o['payment_history'][0]['payment_method_id'] in c.gifts)
        x=(0,q,2,1-q)
        coupled=trajectory_problem(c,2);mixed=trajectory_problem(c,2,mode='mixed')
        self.assertTrue(coupled.feasible(x));self.assertFalse(mixed.feasible(x))
        self.assertTrue(coupled.replay(x,self.original)[0]);self.assertFalse(mixed.replay(x,self.original)[0])

    def test_exchange_and_return_requests_do_not_change_gift_balance(self):
        c=self.catalogs[0]
        for x in ((5,0,5,1),(4,0,4,1)):
            p=trajectory_problem(c,2);y=p.simulate(x)
            self.assertIsNotNone(y);self.assertEqual(y['balance_2_0'],c.initial.balance[0])
            self.assertTrue(p.replay(x,self.original)[0])

    def test_independent_partial_checker_uses_reachable_states(self):
        c=self.catalogs[0];p=trajectory_problem(c,2)
        feasible=[x for x in p.prior.points() if p.feasible(x)]
        for bindings in ((),((0,3),),((0,3),(2,3)),((0,0),(1,0),(2,2),(3,1)),((0,6),(2,6))):
            brute=any(all(x[i]==v for i,v in bindings) for x in feasible)
            self.assertEqual(p.partial_feasible(bindings),brute)
        flags=trajectory_problem(c,1,policy_flags=True)
        auth=flags.prior.names.index('auth_0')
        self.assertFalse(flags.partial_feasible(((auth,0),)))
        self.assertEqual(Oracle(flags).query(((auth,0),)).status,'unsat')

    def test_weighted_state_mass_matches_full_enumeration(self):
        from benchmark_retail_trajectories import feasible_mass
        for mode in ('single','mixed','coupled'):
            p=trajectory_problem(self.catalogs[0],2,mode=mode,policy_flags=True)
            exact=exact_distribution(p,8192)
            self.assertEqual(exact['status'],'exact')
            self.assertAlmostEqual(feasible_mass(p)['z'],exact['z'],places=12)

    def test_longer_history_and_balance_path_matches_original_steps(self):
        c=self.catalogs[0]
        q=next(i for i,o in enumerate(c.orders[:2]) if o['payment_history'][0]['payment_method_id'] in c.gifts)
        x=(2,q,0,q,2,1-q,6,0);p=trajectory_problem(c,4)
        self.assertTrue(p.replay(x,self.original)[0])
        a=Oracle(p).query(tuple(enumerate(x)))
        self.assertEqual(a.status,'sat');self.assertTrue(p.check_witness(x,a.witness))

    def test_coupling_acceptance_effects_match_exhaustive_joint_classification(self):
        from analyze_retail_trajectories import coupling_effects
        p=trajectory_problem(self.catalogs[0],2,policy_flags=True)
        mixed=trajectory_problem(self.catalogs[0],2,mode='mixed',policy_flags=True)
        expected=dict(both_feasible=0.,coupling_enabled=0.,coupling_forbidden=0.,both_infeasible=0.)
        for x in p.prior.points():
            a=p.feasible(x);b=mixed.feasible(x)
            key=('both_feasible' if b else 'coupling_enabled') if a else ('coupling_forbidden' if b else 'both_infeasible')
            expected[key]+=p.prior.pmf(x)
        actual=coupling_effects(p)
        for key in expected:self.assertAlmostEqual(actual[key],expected[key],places=12)

    def test_samplers_share_output_stream_and_leave_original_snapshot_unchanged(self):
        c=self.catalogs[0];before=json.dumps(c.db,sort_keys=True)
        for mode in ('single','mixed','coupled'):
            p=trajectory_problem(c,2,mode=mode)
            rows=[strategy(p,s).run(8,11) for s in ('rejection','raw','mass_b4','context_gate','context_direct')]
            for row in rows:
                self.assertEqual((row['samples'],row['positions']),(rows[0]['samples'],rows[0]['positions']))
                audit_row(p,row,exact_distribution(p,8192))
                for x in row['samples']:self.assertTrue(p.replay(x,self.original)[0])
        self.assertEqual(json.dumps(c.db,sort_keys=True),before)


if __name__=='__main__':unittest.main()
