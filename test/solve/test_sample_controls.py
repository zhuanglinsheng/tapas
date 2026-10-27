#!/usr/bin/env python3
"""End-to-end control/eviction certificates in source and saved bytecode modes."""
import itertools
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
TAPAS = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else ROOT / "build/bin/tapas"
SEEDS = (11, 29, 47)
CONFIGS = {
    "none": "generalization = 'none'",
    "default": "",
    "mass1": "cache_limit = 1",
    "fifo1": "cache_limit = 1, cache_policy = 'fifo'",
    "width1": "cache_limit = 1, cache_policy = 'binding_count'",
    "mass2": "cache_limit = 2",
    "gain1": "cache_limit = 1, exploration = 'gain', discovery_horizon = 1",
    "gain_auto": "cache_limit = 1, exploration = 'gain'",
    "cap0": "cache_limit = 1, core_call_limit = 0",
    "cap3": "cache_limit = 1, core_call_limit = 3",
    "off": "cache_limit = 1, exploration = 'off'",
    "raw": "cache_limit = 1, generalization = 'raw_core'",
    "mus": "cache_limit = 1, generalization = 'deletion_mus', core_call_limit = 3",
    "minimum": "cache_limit = 1, generalization = 'minimum_core', core_call_limit = 3",
    "events_off": "cache_limit = 1",
    "trace_off": "cache_limit = 1",
    "all_options": "time_limit = 60.0, fairness_interval = 4, generalization = 'online_mass_core', "
                   "scheduler = 'mass_fair', cache_limit = 1, cache_policy = 'mass', exploration = 'budget', "
                   "core_call_limit = 3, discovery_horizon = 128",
}
POINTS = list(itertools.product((0, 1), repeat=4))
JOINT_POINTS = [(0, 0, 0, 0), (0, 1, 0, 0), (1, 0, 0, 0),
                (1, 1, 0, 0), (0, 0, 1, 1), (1, 1, 1, 1)]
JOINT_WEIGHTS = [2, 1, 1, 3, 2, 1]
PARAM_TO_DIMENSION = {0: 0, 2: 1, 3: 2, 4: 3}  # y is an existential parameter in the middle.


def feasible(x):
    return not (x[0] == x[1] == 1 or x[2] == x[3] == 1)


def matches(x, bindings):
    return all(x[PARAM_TO_DIMENSION[b["parameter"]]] == b["coordinate"] for b in bindings)


def program():
    parts = ["let R = rule(a: Int, y: Int, b: Int, c: Int, d: Int) {\n"
             "not (a == 1 and b == 1)\nnot (c == 1 and d == 1)\ny == a + c\n}",
             "let space = {'a': [0, 1], 'b': [0, 1], 'c': [0, 1], 'd': [0, 1]}",
             "let independent = {'a': finite::categorical([1.0, 9.0]), 'b': finite::categorical([1.0, 9.0]), "
             "'c': finite::categorical([3.0, 7.0]), 'd': finite::categorical([3.0, 7.0])}",
             "var joint_points = " + json.dumps(JOINT_POINTS),
             "let joint = finite::map(finite::categorical([2.0, 1.0, 1.0, 3.0, 2.0, 1.0]), "
             "finite::index([2, 2, 2, 2]), (source) { return finite::index(joint_points[source[0]]) })"]
    for distribution in ("independent", "joint"):
        for seed in SEEDS:
            for label, options in CONFIGS.items():
                # All fifteen named options fit the native descriptor when used together.
                parts.append("if (true) {\nlet r = solve::sample(R, 12, space, "
                             f"distributions = {distribution}, rng = random::generator(random::pcg32_xsh_rr, {seed}), "
                             "candidate_limit = 3000, core_budget_factor = 4.0, "
                             f"trace = {str(label != 'trace_off').lower()}, events = {str(label != 'events_off').lower()}"
                             + (", " + options if options else "") + ")\n"
                             f"print('ROW|{distribution}|{seed}|{label}|', json::stringify(r))\n}}")
    return "\n".join(parts)+"\n"


class SampleControls(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="tapas-sample-controls-")
        path = Path(cls.temporary.name) / "controls.tap"
        path.write_text(program())
        cls.results = {}
        compiled = subprocess.run([str(TAPAS), "-c", str(path)], capture_output=True, text=True, timeout=60)
        if compiled.returncode:
            raise AssertionError(compiled.stdout+compiled.stderr)
        for mode, command in (("source", [str(path)]), ("bytecode", ["-e", str(path)+"c"])):
            run = subprocess.run([str(TAPAS), *command], capture_output=True, text=True, timeout=90)
            if run.returncode:
                raise AssertionError(run.stdout[-3000:]+run.stderr)
            rows = {}
            for line in run.stdout.splitlines():
                if line.startswith("ROW|"):
                    _, distribution, seed, label, data = line.split("|", 4)
                    key = (distribution, int(seed), label)
                    if key in rows:
                        raise AssertionError(key)
                    rows[key] = json.loads(data)
            if len(rows) != 2*len(SEEDS)*len(CONFIGS):
                raise AssertionError(f"missing rows: {len(rows)}")
            cls.results[mode] = rows

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def test_paired_sequences_and_witnesses(self):
        for rows in self.results.values():
            for (distribution, seed, label), r in rows.items():
                with self.subTest(distribution=distribution, seed=seed, label=label):
                    baseline = rows[distribution, seed, "none"]
                    self.assertEqual(r["status"], "complete")
                    self.assertEqual(r["samples"], baseline["samples"])
                    self.assertEqual(r["candidates"], baseline["candidates"])
                    self.assertEqual(len(r["samples"]), 12)
                    self.assertEqual(r["solver_calls"]+r["cache_hits"], r["candidates"])
                    for s in r["samples"]:
                        self.assertTrue(feasible(tuple(s[k] for k in "abcd")))
                        self.assertEqual(s["y"], s["a"]+s["c"])
                    if label != "trace_off":
                        self.assertEqual([t["candidate"] for t in r["trace"] if t["outcome"] == "sat"],
                                         [t["candidate"] for t in baseline["trace"] if t["outcome"] == "sat"])

    def test_eviction_certificates_and_actual_coverage(self):
        removals = 0
        for rows in self.results.values():
            for (distribution, _, label), r in rows.items():
                if label == "events_off":
                    self.assertEqual(r["events"], [])
                    continue
                active, masses, subsumed = {}, {}, []
                weights = dict(zip(JOINT_POINTS, [w/10 for w in JOINT_WEIGHTS])) if distribution == "joint" else {
                    x: math.prod(p if v else 1-p for v, p in zip(x, (.9, .9, .7, .7))) for x in POINTS}
                for e in r["events"]:
                    kind = e["kind"]
                    if kind in ("business_query", "core_query"):
                        possible = any(feasible(x) and matches(x, e["bindings"]) for x in POINTS)
                        self.assertEqual(e["status"], "sat" if possible else "unsat")
                    if kind == "cache_add":
                        self.assertTrue(all(not feasible(x) for x in POINTS if matches(x, e["bindings"])))
                        self.assertNotIn(e["region_id"], active)
                        previous = list(active.values())+subsumed
                        expected_gain = sum(p for x,p in weights.items() if matches(x, e["bindings"])
                                            and not any(matches(x, b) for b in previous))
                        self.assertAlmostEqual(e["uncovered_mass"], expected_gain, places=10)
                        active[e["region_id"]] = e["bindings"]
                        subsumed = []
                    if kind == "cache_remove":
                        self.assertIn(e["region_id"], active)
                        if e["reason"] == "subsumed":
                            subsumed.append(active[e["region_id"]])
                        del active[e["region_id"]]
                        removals += e["reason"].startswith("capacity_")
                    if kind == "cache_hit":
                        self.assertIn(e["region_id"], active)
                        point = tuple(b["coordinate"] for b in e["bindings"])
                        self.assertTrue(matches(point, active[e["region_id"]]))
                    self.assertEqual(e["cache_regions"], len(active))
                    masses[e["candidate"]] = sum(p for x, p in weights.items() if any(matches(x, b) for b in active.values()))
                for t in r["trace"]:
                    self.assertAlmostEqual(t["cache_mass"], masses[t["candidate"]], places=10)
                    limit = 2 if label == "mass2" else 0 if label in ("none", "default") else 1
                    if limit:
                        self.assertLessEqual(t["cache_regions"], limit)
                if label not in ("fifo1", "width1"):
                    self.assertTrue(all(a["cache_mass"] <= b["cache_mass"]+1e-10
                                        for a, b in zip(r["trace"], r["trace"][1:])))
        self.assertGreater(removals, 0)

    def test_limits_pauses_and_events_do_not_change_search(self):
        for rows in self.results.values():
            for (distribution, seed, label), r in rows.items():
                self.assertLessEqual(r["core_calls"], math.floor(4*math.sqrt(r["candidates"])))
                if label in ("cap0", "off", "gain1", "raw", "none"):
                    self.assertEqual(r["core_calls"], 0)
                if label in ("cap3", "mus", "minimum", "all_options"):
                    self.assertLessEqual(r["core_calls"], 3)
                if label in ("events_off", "trace_off"):
                    ref = rows[distribution, seed, "mass1"]
                    for key in ("solver_calls", "core_calls", "cache_hits", "cache_regions"):
                        self.assertEqual(r[key], ref[key])
                if label != "events_off":
                    self.assertEqual(sum(e["kind"] == "business_query" for e in r["events"]), r["solver_calls"])
                    self.assertEqual(sum(e["kind"] == "core_query" for e in r["events"]), r["core_calls"])
                    self.assertEqual(sum(e["kind"] == "cache_hit" for e in r["events"]), r["cache_hits"])
                    self.assertAlmostEqual(sum(e["seconds"] for e in r["events"] if e["kind"].endswith("_query")),
                                           r["timings"]["solver_seconds"], places=8)
                    self.assertEqual(r["events"][-1]["kind"], "sample_end")
                self.assertTrue(all(t >= 0 for t in r["timings"].values()))
                if label in ("off", "cap0", "gain1"):
                    reason = {"off": "disabled", "cap0": "call_limit", "gain1": "gain_bound"}[label]
                    self.assertTrue(any(e["kind"] == "exploration_pause" and e["reason"] == reason for e in r["events"]))

    def test_source_bytecode_decisions_agree(self):
        for key, a in self.results["source"].items():
            b = self.results["bytecode"][key]
            for field in ("status", "samples", "candidates", "solver_calls", "core_calls", "cache_hits", "trace"):
                self.assertEqual(a[field], b[field])
            self.assertEqual([{k:v for k,v in e.items() if k != "seconds"} for e in a["events"]],
                             [{k:v for k,v in e.items() if k != "seconds"} for e in b["events"]])

    def test_invalid_control_arguments_fail(self):
        for argument in ("cache_limit = -1", "cache_limit = true", "cache_policy = 'lru'",
                         "exploration = 'foresee'", "core_call_limit = -1", "discovery_horizon = -1",
                         "events = 1", "cache_limit = 1, cache_limit = 2"):
            with self.subTest(argument=argument):
                path = Path(self.temporary.name) / "invalid.tap"
                path.write_text("let R = rule(x: Int) { x == 0 }\n"
                                "solve::sample(R, 1, {'x': [0, 1]}, "+argument+")\n")
                result = subprocess.run([str(TAPAS), str(path)], capture_output=True, text=True, timeout=10)
                self.assertNotEqual(result.returncode, 0)

    def test_nondecisions_during_discovery_never_learn_a_region(self):
        path = Path(self.temporary.name) / "unknown.tap"
        path.write_text("let R = rule(x: Int, b: Int) { x == 0 }\n"
                        "let r = solve::sample(R, 1, {'x': [1], 'b': [0, 1]}, "
                        "generalization = 'deletion_mus', cache_limit = 1, core_budget_factor = 4.0, "
                        "events = true)\nprint(json::stringify(r))\n")
        fake = Path(self.temporary.name) / "nondecision_backend.py"
        for status in ("unknown", "unsupported", "error"):
            with self.subTest(status=status):
                fake.write_text(f"#!{sys.executable}\nimport sys\n"
                                f"for status in ('unsat', '{status}'):\n"
                                "    size = int.from_bytes(sys.stdin.buffer.read(4), 'big')\n"
                                "    sys.stdin.buffer.read(size)\n"
                                "    reply = ('TAPAS_SOLVE_3\\n'+status+'\\nconfigured\\nfixture\\n0\\n0\\n').encode()\n"
                                "    sys.stdout.buffer.write(len(reply).to_bytes(4, 'big')+reply)\n"
                                "    sys.stdout.buffer.flush()\n")
                fake.chmod(0o700)
                env = dict(os.environ, TAPAS_SOLVE_PYTHON=str(fake))
                run = subprocess.run([str(TAPAS), str(path)], capture_output=True, text=True, env=env, timeout=10)
                self.assertEqual(run.returncode, 0, run.stderr+run.stdout)
                r = json.loads(run.stdout)
                self.assertEqual(r["status"], status)
                self.assertEqual(r["samples"], [])
                self.assertEqual(r["core_calls"], 1)
                self.assertEqual(r["solver_calls"], 1)
                self.assertEqual(r["cache_regions"], 1)
                queries = [e for e in r["events"] if e["kind"] == "core_query"]
                self.assertEqual([e["status"] for e in queries], [status])
                self.assertEqual(sum(e["kind"] == "cache_add" for e in r["events"]), 1)


if __name__ == "__main__":
    unittest.main()
