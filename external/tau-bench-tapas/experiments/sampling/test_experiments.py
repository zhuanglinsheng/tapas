"""Check research oracles and detect invalid paired-experiment evidence."""
import copy
import unittest

from run_experiments import CONFIGS, exact_oracle, validate, workloads


class ExperimentChecks(unittest.TestCase):
    def test_equal_width_mass_separation(self):
        o = exact_oracle(workloads()[0])
        self.assertAlmostEqual(o["feasible_mass"], .36*.96)
        self.assertEqual([c["width"] for c in o["mus"]], [2, 2])
        self.assertAlmostEqual(o["mus"][0]["mass"] / o["mus"][1]["mass"], 16)

    def test_overlap_reverses_selection(self):
        low, high = [exact_oracle(w) for w in workloads()[1:3]]
        self.assertEqual(low["largest_total"]["core"], low["largest_marginal"]["core"])
        self.assertNotEqual(high["largest_total"]["core"], high["largest_marginal"]["core"])
        self.assertAlmostEqual(high["largest_total"]["marginal_mass"], .064)
        self.assertAlmostEqual(high["largest_marginal"]["marginal_mass"], .1008)

    def test_cardinality_is_not_mass_optimal(self):
        o = exact_oracle(workloads()[3])
        self.assertEqual(min(c["width"] for c in o["mus"]), 1)
        self.assertEqual(o["largest_marginal"]["width"], 2)
        self.assertAlmostEqual(o["largest_marginal"]["mass"], .25)

    def fixture(self):
        xs = [dict((f"x{i}", v) for i, v in enumerate(x))
              for x in ((0, 0, 0, 0, 0, 0), (0, 0, 0, 0, 0, 1))]
        rows = []
        for seed in (11, 29):
            for label, _, _, beta in CONFIGS:
                for traced in (True, False):
                    rows.append(dict(seed=seed, n=2, label=label, beta=beta, traced=traced,
                                     status="complete", samples=copy.deepcopy(xs), candidates=3,
                                     solver_calls=3, core_calls=0, cache_hits=0,
                                     trace=[dict(candidate=i+1, outcome=outcome, cache_mass=0)
                                            for i, outcome in enumerate(("sat", "unsat", "sat"))]
                                     if traced else []))
        return rows

    def test_sequence_check_catches_same_count_different_order(self):
        rows = self.fixture()
        validate(rows, workloads()[0], [11, 29], [2])
        rows[2]["samples"].reverse()
        with self.assertRaises(AssertionError):
            validate(rows, workloads()[0], [11, 29], [2])

    def test_positions_check_catches_same_sequence_different_stream(self):
        rows = self.fixture()
        rows[2]["trace"][0]["outcome"] = "unsat"
        rows[2]["trace"][1]["outcome"] = "sat"
        with self.assertRaises(AssertionError):
            validate(rows, workloads()[0], [11, 29], [2])

    def test_cache_cannot_cover_feasible_mass(self):
        rows = self.fixture()
        rows[2]["trace"][2]["cache_mass"] = 1.0
        with self.assertRaises(AssertionError):
            validate(rows, workloads()[0], [11, 29], [2])


if __name__ == "__main__":
    unittest.main()
