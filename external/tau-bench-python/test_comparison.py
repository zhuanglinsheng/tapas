"""Accounting includes every CP call; censored prefixes are never speed wins."""
import unittest
from unittest.mock import patch
from copy import deepcopy
from compare_sampling_algorithms import metrics
from exploration_workloads import allocation
from oracle import Oracle
from sampler import Sampler,CONFIGS


class ComparisonTests(unittest.TestCase):
    def test_rejection_counts_every_candidate_and_never_explores(self):
        p=allocation();row=Sampler(p,CONFIGS['rejection']).run(4,11)
        row['strategy']='rejection';row['cpu_seconds']=0
        m=metrics(row)
        self.assertEqual(m['total_calls'],row['candidates'])
        self.assertEqual(m['business_sat'],4)
        self.assertEqual(m['discovery_calls'],0)
        broken=deepcopy(row);broken['total_calls']+=1
        with self.assertRaises(AssertionError):metrics(broken)

    def test_wall_limit_keeps_a_decided_prefix_without_skipping_input(self):
        p=allocation()
        for mode in ('rejection','raw'):
            clock=[0.]
            class ClockOracle(Oracle):
                def query(self,bindings):
                    answer=super().query(bindings);clock[0]+=1.;return answer
            with patch('sampler.perf_counter',side_effect=lambda:clock[0]):
                row=Sampler(p,CONFIGS[mode],ClockOracle(p)).run(32,11,wall_limit=2.5)
            full=Sampler(p,CONFIGS[mode]).run(32,11)
            self.assertEqual(row['status'],'wall_limit')
            self.assertEqual(row['total_calls'],3)
            n=len(row['samples'])
            self.assertEqual((row['samples'],row['positions']),(full['samples'][:n],full['positions'][:n]))
            row['strategy']=mode;row['cpu_seconds']=0;metrics(row)
        with self.assertRaises(ValueError):Sampler(p,CONFIGS['raw']).run(1,11,wall_limit=0)


if __name__=='__main__':unittest.main()
