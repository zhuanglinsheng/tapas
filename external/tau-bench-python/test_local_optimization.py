"""Optimizations must retain the RNG stream and actual CP query semantics."""
from dataclasses import replace
import random
import unittest
from prior import ProductPrior, JointPrior
from sampler import Sampler, CONFIGS
from oracle import Oracle
from workloads import synthetic
from run_experiments import audit_row, exact_distribution
import native_sampling


class LocalOptimizationTests(unittest.TestCase):
    def test_weighted_draws_exactly_match_choices_and_rng_state(self):
        # Zeros, nonbinary/negative/large domain values, skew and one-point
        # domains exercise bisect boundaries and cumulative rounding.
        p=ProductPrior(['a','b','c'],[(-99,2,10**30),(7,),(-5,0,2,9)],
                       [[0,1e-12,1],[1],[1,0,2,9]])
        capsule=p._native_draw
        for backend in (None,capsule):
            p._native_draw=backend
            a,b=random.Random(20261021),random.Random(20261021)
            for _ in range(12000):
                expected=tuple(b.choices(d,weights=w,k=1)[0] for d,w in zip(p.domains,p.probabilities))
                self.assertEqual(p.draw(a),expected)
            self.assertEqual(a.getstate(),b.getstate())
        j=JointPrior(['x'],[(0,1,2)],{(0,):0,(1,):1,(2,):9})
        a,b=random.Random(97),random.Random(97)
        for _ in range(12000):
            self.assertEqual(j.draw(a),b.choices(j.population,weights=j.weights,k=1)[0])
        self.assertEqual(a.getstate(),b.getstate())

    @unittest.skipUnless(native_sampling.BACKEND=='c','Optional extension not built/enabled')
    def test_native_boundaries_and_invalid_arguments(self):
        from _sampling_native import compile_draw, draw
        specs=(((1,2,3),(0.0,0.5,1.0)),)
        compiled=compile_draw(specs)
        for value in (0.0,0.5,1.0):
            a=draw(compiled,lambda:value)
            from bisect import bisect_right
            self.assertEqual(a,(specs[0][0][bisect_right(specs[0][1],value,0,2)],))
        for bad in ([],(((),()),),(((1,),(float('nan'),)),),(((1,2),(1.0,0.5)),)):
            with self.assertRaises((ValueError,TypeError)):compile_draw(bad)
        for value in (-1,float('nan'),2):
            with self.assertRaises(ValueError):draw(compiled,lambda:value)
        with self.assertRaises((TypeError,ValueError)):draw(None,random.Random().random)
        with self.assertRaises(ZeroDivisionError):draw(compiled,lambda:1/0)

    def test_proof_miss_cache_invalidates_on_both_proof_sources(self):
        p=synthetic()[0];s=Sampler(p,CONFIGS['mass_b4'])
        self.assertIsNone(s.lookup_proof(())[0]);self.assertIsNone(s.lookup_proof(())[0])
        self.assertEqual(s.proof_miss_hits,1)
        s.query(())
        self.assertEqual(s.lookup_proof(())[0].status,'sat')
        r=((0,1),(1,1))
        self.assertIsNone(s.lookup_proof(r)[0]);s.insert(r)
        self.assertEqual(s.lookup_proof(r)[0].status,'unsat')

    def test_miss_cache_keeps_query_trace_and_certificates(self):
        for p in synthetic():
            a=Sampler(p,CONFIGS['mass_b4']).run(128,11)
            b=Sampler(p,replace(CONFIGS['mass_b4'],memoize_misses=False)).run(128,11)
            for key in ('samples','positions','total_calls','discovery_calls','memo_hits','cache'):
                self.assertEqual(a[key],b[key],(p.name,key))
            def trace(row):
                return [(e['kind'],e['region'],e['status'],e.get('core')) for e in row['events'] if e['kind'] in ('business','discovery','memo')]
            self.assertEqual(trace(a),trace(b))
            audit_row(p,a,exact_distribution(p,4096))

    def test_oracle_reuse_matches_fresh_wrapper_without_model_mutation(self):
        for p in synthetic():
            original=str(p.model.proto);oracle=Oracle(p)
            for r in ((),((0,0),),((0,1),),tuple(enumerate(next(p.prior.points()))),()):
                a,b=oracle.query(r),Oracle(p).query(r)
                self.assertEqual((a.status,a.core,a.completion,a.witness),(b.status,b.core,b.completion,b.witness))
                self.assertEqual(str(p.model.proto),original)


if __name__=='__main__':unittest.main()
