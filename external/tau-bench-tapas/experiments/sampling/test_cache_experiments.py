"""Verify finite-cache selection, explicit query charging and causal discovery."""
import itertools
import unittest

from run_cache_experiments import Dictionary, cache_workloads, discover, replay, stream_for


class AccessLog(dict):
    def __init__(self, answers):
        super().__init__(answers)
        self.reads = []

    def __getitem__(self, key):
        self.reads.append(key)
        return super().__getitem__(key)


class CacheExperimentChecks(unittest.TestCase):
    def test_overlap_selection_has_exact_coverage_gain(self):
        d = Dictionary(cache_workloads()[2])
        safe = [i for i, answer in enumerate(d.safe) if answer]
        total = d.select(safe, 2, "total_mass")
        marginal = d.select(safe, 2, "marginal_mass")
        self.assertAlmostEqual(d.mass(d.union(total)), .784)
        self.assertAlmostEqual(d.mass(d.union(marginal)), .8208)
        self.assertAlmostEqual(d.mass(d.union(marginal))-d.mass(d.union(total)), .0368)

    def test_pruning_is_fair_to_width_baseline(self):
        d = Dictionary(cache_workloads()[3])
        safe = [i for i, answer in enumerate(d.safe) if answer]
        self.assertGreater(len(safe), 2)
        self.assertEqual(len(d.prune(safe)), 2)
        for policy in ("minimum_width", "minimum_reverse", "total_mass", "marginal_mass"):
            self.assertAlmostEqual(d.mass(d.union(d.select(safe, 4, policy))), .265)

    def test_exact_selector_matches_independent_exhaustion(self):
        d = Dictionary(cache_workloads()[2])
        safe = [i for i, answer in enumerate(d.safe) if answer]
        for capacity in (1, 2, 4):
            reference = max(d.mass(d.union(c)) for k in range(min(len(safe), capacity)+1)
                            for c in itertools.combinations(safe, k))
            self.assertAlmostEqual(d.mass(d.union(d.select(safe, capacity, "exact_mass"))), reference)

    def test_only_queried_answers_are_read_and_sat_costs_count(self):
        d = Dictionary(cache_workloads()[0])
        answers = AccessLog(enumerate(d.safe))
        plan = discover(d, 1, 32, "gated_exact", answers)
        self.assertEqual(answers.reads, [h["candidate"] for h in plan["history"]])
        self.assertEqual(plan["discovery_calls"], len(answers.reads))
        self.assertEqual(plan["discovery_calls"], 3)
        self.assertEqual(plan["discovery_sat"], 2)

    def test_one_proposal_cannot_repay_a_query(self):
        d = Dictionary(cache_workloads()[2])
        answers = AccessLog(enumerate(d.safe))
        plan = discover(d, 4, 1, "gated_exact", answers)
        self.assertEqual(plan["discovery_calls"], 0)
        self.assertEqual(answers.reads, [])
        self.assertEqual(plan["selected"], [])

    def test_replay_does_not_change_discovery_or_accepted_inputs(self):
        d = Dictionary(cache_workloads()[0])
        plan = discover(d, 1, 32, "gated_exact", dict(enumerate(d.safe)))
        history = [h.copy() for h in plan["history"]]
        for seed in (101, 503):
            counts = replay(d, stream_for(d, seed, 32), plan)
            self.assertEqual(counts["total_calls"], counts["business_calls"]+3)
            self.assertEqual(plan["history"], history)


if __name__ == "__main__":
    unittest.main()
