#!/usr/bin/env python3
"""Enumerated mechanism oracles and paired experiments against real solve::sample.

No sampler is reimplemented here. Python enumerates the target and checks Tapas
outputs; all measured solver calls come from the production Tapas executable.
"""
from __future__ import annotations

import argparse
import gzip
import hashlib
import itertools
import json
import math
import os
from pathlib import Path
import random
import statistics
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[4]
CONFIGS = [
    ("none", "none", "mass_fair", 1.0),
    ("full", "full", "mass_fair", 1.0),
    ("raw", "raw_core", "mass_fair", 1.0),
    ("mus", "deletion_mus", "mass_fair", 4.0),
    ("minimum", "minimum_core", "mass_fair", 4.0),
    ("mass_b1", "online_mass_core", "mass_fair", 1.0),
    ("mass_b4", "online_mass_core", "mass_fair", 4.0),
    ("fifo_b4", "online_mass_core", "fifo", 4.0),
    ("size_b4", "online_mass_core", "binding_count", 4.0),
]

CONTROL_CONFIGS = [
    ("none", "none", "mass_fair", 1.0),
    ("raw", "raw_core", "mass_fair", 1.0),
    ("mass_b1", "online_mass_core", "mass_fair", 1.0),
    ("bounded_mass", "online_mass_core", "mass_fair", 1.0),
    ("bounded_fifo", "online_mass_core", "mass_fair", 1.0),
    ("bounded_width", "online_mass_core", "mass_fair", 1.0),
    ("capped", "online_mass_core", "mass_fair", 1.0),
    ("gain_capped", "online_mass_core", "mass_fair", 1.0),
    ("off", "online_mass_core", "mass_fair", 1.0),
]
CONTROL_OPTIONS = {
    "bounded_mass": "cache_limit = 1",
    "bounded_fifo": "cache_limit = 1, cache_policy = 'fifo'",
    "bounded_width": "cache_limit = 1, cache_policy = 'binding_count'",
    "capped": "cache_limit = 1, core_call_limit = 8",
    "gain_capped": "cache_limit = 1, core_call_limit = 8, exploration = 'gain'",
    "off": "cache_limit = 1, exploration = 'off'",
}


def workloads():
    # A clause lists the coordinates whose simultaneous value 1 is forbidden.
    return [
        dict(name="equal_width", probabilities=[.8, .8, .2, .2, .5, .5],
             clauses=[(0, 1), (2, 3)]),
        dict(name="overlap_low", probabilities=[.8, .8, .6, .6, .1, .5],
             clauses=[(0, 1), (2, 3), (0, 4)]),
        dict(name="overlap_high", probabilities=[.8, .8, .6, .6, .9, .5],
             clauses=[(0, 1), (2, 3), (0, 4)]),
        dict(name="cardinality", probabilities=[.02, .5, .5, .5, .5, .5],
             clauses=[(0,), (1, 2)]),
    ]


def feasible(x, w):
    return not any(all(x[i] == 1 for i in c) for c in w["clauses"])


def enumerate_prior(w):
    return {x: math.prod(p if v else 1-p for v, p in zip(x, w["probabilities"]))
            for x in itertools.product((0, 1), repeat=len(w["probabilities"]))}


def region(core, prior):
    return {x for x in prior if all(x[i] == v for i, v in core)}


def exact_oracle(w):
    """Exhaust all subsets of a failed all-ones input, with exact finite sums."""
    prior = enumerate_prior(w)
    d = len(w["probabilities"])
    safe = []
    for k in range(d + 1):
        for indices in itertools.combinations(range(d), k):
            c = tuple((i, 1) for i in indices)
            g = region(c, prior)
            if all(not feasible(x, w) for x in g):
                safe.append(c)
    mus = [c for c in safe if not any(set(s) < set(c) for s in safe)]
    mass = lambda g: sum(prior[x] for x in g)
    cached = region(((0, 1), (4, 1)), prior) if w["name"].startswith("overlap") else set()
    candidates = []
    for c in mus:
        g = region(c, prior)
        candidates.append(dict(core=list(c), width=len(c), mass=mass(g),
                               marginal_mass=mass(g-cached)))
    useful = [c for c in candidates if c["marginal_mass"] > 1e-12]
    return dict(name=w["name"], feasible_mass=sum(p for x, p in prior.items() if feasible(x, w)),
                cached_mass=mass(cached), mus=candidates,
                largest_total=max(useful, key=lambda c: c["mass"]),
                largest_marginal=max(useful, key=lambda c: c["marginal_mass"]))


def source_for(w, seeds, horizons, option_overrides=None):
    names = [f"x{i}" for i in range(len(w["probabilities"]))]
    conditions = ["not (" + " and ".join(f"{names[i]} == 1" for i in c) + ")"
                  for c in w["clauses"]]
    text = ["let Model = rule (\n" + "\n".join(f"        {n}: Int," for n in names) + "\n) {\n"
            + "\n".join("    " + condition for condition in conditions) + "\n}",
            "let space = {\n" + "\n".join(f"    '{n}': [0, 1]," for n in names) + "\n}",
            "let distributions = {\n" + "\n".join(
                f"    '{n}': finite::categorical([{1-p!r}, {p!r}]),"
                for n, p in zip(names, w["probabilities"])) + "\n}"]
    # Warm the persistent Python worker before recording elapsed time.
    text.append("solve::sample(Model, 1, space, distributions = distributions, generalization = 'none')")
    for seed in seeds:
        configs = list(CONFIGS)
        random.Random(seed).shuffle(configs)
        for n in horizons:
            for label, mode, scheduler, beta in configs:
                extra = (option_overrides or {}).get(label, "")
                # Traced and untraced calls have identical RNG state, and are
                # checked for exact agreement. Only untraced time is compared.
                for tracing in (True, False):
                    text.append("if (true) {\n    let r = solve::sample(Model, " + str(n) + ", space,\n"
                                "        distributions = distributions,\n"
                                f"        rng = random::generator(random::pcg32_xsh_rr, {seed}),\n"
                                f"        candidate_limit = 100000, time_limit = 120.0,\n"
                                f"        generalization = '{mode}', scheduler = '{scheduler}',\n"
                                f"        core_budget_factor = {beta}, fairness_interval = 4,\n"
                                f"        trace = {str(tracing).lower()}" + (", " + extra if extra else "") + ")\n\n"
                                "    print('ROW|', json::stringify({\n"
                                f"        'seed': {seed},\n        'n': {n},\n"
                                f"        'label': '{label}',\n        'beta': {beta},\n"
                                f"        'traced': {str(tracing).lower()},\n"
                                "        'status': r::status,\n        'samples': r::samples,\n"
                                "        'candidates': r::candidates,\n        'solver_calls': r::solver_calls,\n"
                                "        'core_calls': r::core_calls,\n        'cache_hits': r::cache_hits,\n"
                                "        'elapsed': r::elapsed,\n        'trace': r::trace,\n"
                                "    }))\n}")
    return "\n\n".join(text) + "\n"


def validate(rows, w, seeds, horizons):
    expected = len(seeds) * len(horizons) * len(CONFIGS) * 2
    if len(rows) != expected:
        raise AssertionError(f"{w['name']}: expected {expected} rows, got {len(rows)}")
    groups = {}
    prior = enumerate_prior(w)
    z = sum(p for x, p in prior.items() if feasible(x, w))
    for r in rows:
        assert r["status"] == "complete", r
        assert len(r["samples"]) == r["n"]
        xs = [tuple(s[f"x{i}"] for i in range(len(w["probabilities"]))) for s in r["samples"]]
        assert all(x in prior and feasible(x, w) for x in xs)
        r["input_sequence"] = xs
        assert r["solver_calls"] + r["cache_hits"] == r["candidates"]
        assert r["core_calls"] <= math.floor(r["beta"] * math.sqrt(r["candidates"]))
        if r["label"] in ("capped", "gain_capped"):
            assert r["core_calls"] <= 8
        if r["label"] == "off":
            assert r["core_calls"] == 0
        if r["traced"]:
            assert len(r["trace"]) == r["candidates"]
            if r["label"] in CONTROL_OPTIONS:
                assert all(t["cache_regions"] <= 1 for t in r["trace"])
            assert sum(t["outcome"] == "sat" for t in r["trace"]) == r["n"]
            masses = [t["cache_mass"] for t in r["trace"]]
            assert all(-1e-9 <= m <= 1-z+1e-9 for m in masses)
            if r["label"] not in ("bounded_fifo", "bounded_width"):
                assert all(a <= b+1e-9 for a, b in zip(masses, masses[1:]))
        key = (r["seed"], r["n"])
        groups.setdefault(key, []).append(r)
    for group in groups.values():
        baseline = next(r for r in group if r["label"] == "none" and r["traced"])
        sat_positions = [t["candidate"] for t in baseline["trace"] if t["outcome"] == "sat"]
        for r in group:
            assert r["input_sequence"] == baseline["input_sequence"], (w["name"], r["label"])
            assert r["candidates"] == baseline["candidates"]
            if r["traced"]:
                assert [t["candidate"] for t in r["trace"] if t["outcome"] == "sat"] == sat_positions
            else:
                traced = next(t for t in group if t["label"] == r["label"] and t["traced"])
                assert all(r[k] == traced[k] for k in ("solver_calls", "core_calls", "cache_hits"))
    # Runs with different requested batch sizes must share their accepted prefix.
    for seed in seeds:
        baselines = [r for r in rows if r["seed"] == seed and r["label"] == "none" and r["traced"]]
        longest = max(baselines, key=lambda r: r["n"])["input_sequence"]
        assert all(r["input_sequence"] == longest[:r["n"]] for r in baselines)
    return z


def interval(values):
    n = len(values)
    mean = statistics.mean(values)
    # Fixed supported seed counts; do not silently substitute normal quantiles.
    critical = {2: 12.706, 3: 4.303, 5: 2.776, 10: 2.262}
    half = critical[n] * statistics.stdev(values) / math.sqrt(n) if n in critical else None
    return dict(mean=mean, ci95_half=half, n=n)


def summarize(all_rows, ws):
    summary = []
    for w in ws:
        rows = [r for r in all_rows if r["workload"] == w["name"]]
        prior = enumerate_prior(w)
        z = sum(p for x, p in prior.items() if feasible(x, w))
        for n in sorted({r["n"] for r in rows}):
            for label, *_ in CONFIGS:
                group = [r for r in rows if r["n"] == n and r["label"] == label and not r["traced"]]
                delta, time_delta = [], []
                for r in group:
                    raw = next(t for t in rows if t["seed"] == r["seed"] and t["n"] == n
                               and t["label"] == "raw" and not t["traced"])
                    delta.append(r["solver_calls"] + r["core_calls"] - raw["solver_calls"])
                    time_delta.append(r["elapsed"] - raw["elapsed"])
                summary.append(dict(workload=w["name"], n=n, label=label, feasible_mass=z,
                                    calls_per_sample=interval([(r["solver_calls"]+r["core_calls"])/n for r in group]),
                                    extra_calls_vs_raw=interval(delta),
                                    elapsed=interval([r["elapsed"] for r in group]),
                                    extra_seconds_vs_raw=interval(time_delta)))
    return summary


def plot(summary, ws, destination):
    os.environ.setdefault("MPLCONFIGDIR", str(Path(tempfile.gettempdir()) / "tapas-sampling-mpl"))
    os.environ.setdefault("XDG_CACHE_HOME", str(Path(tempfile.gettempdir()) / "tapas-sampling-cache"))
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    fig, axes = plt.subplots(2, 2, figsize=(11, 7), constrained_layout=True)
    for ax, w in zip(axes.flat, ws):
        for label, *_ in CONFIGS:
            rs = [r for r in summary if r["workload"] == w["name"] and r["label"] == label]
            ax.errorbar([r["n"] for r in rs],
                        [r["calls_per_sample"]["mean"]*r["n"] for r in rs],
                        yerr=[(r["calls_per_sample"]["ci95_half"] or 0)*r["n"] for r in rs],
                        marker=".", label=label, capsize=2)
        ax.set(title=w["name"], xlabel="Accepted samples", ylabel="Total solver calls")
        ax.set_xscale("log", base=2)
        ax.grid(alpha=.2)
    axes.flat[0].legend(fontsize=8)
    fig.savefig(destination / "total_calls.png", dpi=160)
    fig.savefig(destination / "total_calls.pdf")
    plt.close(fig)
    fig, axes = plt.subplots(2, 2, figsize=(11, 7), constrained_layout=True)
    for ax, w in zip(axes.flat, ws):
        for label, *_ in CONFIGS:
            if label in ("none", "full", "raw"):
                continue
            rs = [r for r in summary if r["workload"] == w["name"] and r["label"] == label]
            ax.errorbar([r["n"] for r in rs],
                        [r["extra_calls_vs_raw"]["mean"] for r in rs],
                        yerr=[r["extra_calls_vs_raw"]["ci95_half"] or 0 for r in rs],
                        marker=".", label=label, capsize=2)
        ax.axhline(0, color="black", linewidth=.8)
        ax.set(title=w["name"], xlabel="Accepted samples", ylabel="Extra total calls vs raw")
        ax.set_xscale("log", base=2)
        ax.grid(alpha=.2)
    axes.flat[0].legend(fontsize=8)
    fig.savefig(destination / "extra_calls.png", dpi=160)
    fig.savefig(destination / "extra_calls.pdf")
    plt.close(fig)


def report(summary, oracles, rows, destination, controls=False):
    def fmt(s):
        return f"{s['mean']:.3f}" + (f" ± {s['ci95_half']:.3f}" if s['ci95_half'] is not None else "")
    lines = ["# Production sampling controls" if controls else "# Synthetic sampling pilot", "",
             "All calls use production `solve::sample`; intervals are two-sided 95% Student intervals across seeds.",
             "Paired differences subtract raw-core on the same seed. Positive differences mean more work.",
             "Elapsed times exclude trace and include in-process sampling costs after worker warm-up.", "",
             "## Exact local selection oracles", "",
             "These freeze a failed all-ones input and a known safe cache. They do not predict online speedup.", "",
             "| Workload | Z | Cached mass | Core | Width | Total mass | Uncovered mass |",
             "|---|---:|---:|---|---:|---:|---:|"]
    for o in oracles:
        for c in o["mus"]:
            lines.append(f"| {o['name']} | {o['feasible_mass']:.4f} | {o['cached_mass']:.4f} | "
                         f"{c['core']} | {c['width']} | {c['mass']:.4f} | {c['marginal_mass']:.4f} |")
    if controls:
        lines += ["", "Control options (all calls execute production solve::sample):", "",
                  "```json", json.dumps(CONTROL_OPTIONS, indent=2), "```", "",
                  "The raw comparator has unlimited cache; bounded policies have capacity one. "
                  "Compare bounded_mass vs bounded_fifo/width for equal-capacity selection, and "
                  "capped vs bounded_mass for a discovery-budget change. Events are disabled in both passes; "
                  "the dedicated integration tests separately replay and check detailed events."]
    lines += ["", "## Measured sampler results", "",
              "| Workload | Samples | Strategy | Calls/sample | Paired extra calls vs raw | Seconds (trace off) |",
              "|---|---:|---|---:|---:|---:|"]
    for r in summary:
        lines.append(f"| {r['workload']} | {r['n']} | {r['label']} | {fmt(r['calls_per_sample'])} | "
                     f"{fmt(r['extra_calls_vs_raw'])} | {fmt(r['elapsed'])} |")
    lines += ["", "## Distribution implementation check", "",
              "All strategies, trace settings, and batch prefixes produced exactly the same accepted input sequence "
              "and acceptance positions as rejection on each seed. All returned inputs were checked against the "
              "enumerated feasible set; trace cache mass was bounded by infeasible mass and monotone "
              "for policies that promise monotone total coverage (FIFO/width eviction are excluded). "
              "This is an implementation check, not a proof of iid output or a statistical acceptance threshold.", "",
              "Only the largest batch per seed is used below; strategies and nested batch prefixes are not pooled as independent samples.", "",
              "| Workload | Unique draws counted | Empirical TV vs enumerated conditional target |",
              "|---|---:|---:|"]
    for w in workloads():
        subset = [r for r in rows if r["workload"] == w["name"] and r["label"] == "none" and not r["traced"]]
        n = max(r["n"] for r in subset)
        xs = [tuple(x) for r in subset if r["n"] == n for x in r["input_sequence"]]
        prior = enumerate_prior(w)
        z = sum(p for x, p in prior.items() if feasible(x, w))
        tv = .5 * sum(abs(xs.count(x)/len(xs)-p/z) for x, p in prior.items() if feasible(x, w))
        lines.append(f"| {w['name']} | {len(xs)} | {tv:.5f} |")
    lines += [""]
    if (destination / "total_calls.png").exists():
        lines += ["![Cumulative calls](total_calls.png)", ""]
    if (destination / "extra_calls.png").exists():
        lines += ["![Paired extra calls](extra_calls.png)", ""]
    lines += [
              "Limitations: independent finite priors, small synthetic rules; "
              "no SampleSearch/weighted projected sampler comparison, no large-domain timing claim. "
              "Frozen-cache oracle selection is distinct from the production online scheduler. "
              "No production algorithm was tuned using these results."]
    (destination / "report.md").write_text("\n".join(lines)+"\n")


def main():
    global CONFIGS
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tapas", type=Path, default=ROOT / "build/bin/tapas")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--controls", action="store_true", help="exercise native cache/exploration controls")
    parser.add_argument("--seeds", type=int, nargs="+", default=[11, 29, 47, 71, 97])
    parser.add_argument("--horizons", type=int, nargs="+", default=[8, 32, 128])
    parser.add_argument("--no-plot", action="store_true")
    args = parser.parse_args()
    if args.controls:
        CONFIGS = CONTROL_CONFIGS
    if args.output is None:
        args.output = ROOT / "tmp/external/tau-bench-tapas/experiments/sampling/results" / ("production_controls" if args.controls else "pilot")
    if len(set(args.seeds)) != len(args.seeds) or len(args.seeds) not in (2, 3, 5, 10):
        parser.error("use 2, 3, 5 or 10 distinct seeds for Student intervals")
    if not args.horizons or min(args.horizons) < 1 or len(set(args.horizons)) != len(args.horizons):
        parser.error("horizons must be distinct positive integers")
    args.output.mkdir(parents=True, exist_ok=True)
    ws = workloads()
    oracles = [exact_oracle(w) for w in ws]
    high = next(o for o in oracles if o["name"] == "overlap_high")
    assert high["largest_total"]["core"] != high["largest_marginal"]["core"]
    rows = []
    for w in ws:
        print(f"Running {w['name']} ...", flush=True)
        source = source_for(w, args.seeds, sorted(args.horizons), CONTROL_OPTIONS if args.controls else None)
        source_path = args.output / f"{w['name']}.tap"
        source_path.write_text(source)
        completed = subprocess.run([str(args.tapas.resolve()), str(source_path.resolve())],
                                   cwd=ROOT, text=True, capture_output=True, timeout=1800)
        (args.output / f"{w['name']}.stderr.txt").write_text(completed.stderr)
        if completed.returncode:
            raise RuntimeError(completed.stderr + completed.stdout[-4000:])
        batch = [json.loads(line[4:]) for line in completed.stdout.splitlines() if line.startswith("ROW|")]
        validate(batch, w, args.seeds, args.horizons)
        for r in batch:
            r["workload"] = w["name"]
        rows.extend(batch)
        (args.output / "rows.json.gz").write_bytes(gzip.compress(
            (json.dumps(rows, separators=(",", ":"))+"\n").encode(), mtime=0))
        print(f"  checked {len(batch)} runs", flush=True)
    summary = summarize(rows, ws)
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2)+"\n")
    metadata = dict(seeds=args.seeds, horizons=sorted(args.horizons), workloads=ws,
                    controls=args.controls, option_overrides=CONTROL_OPTIONS if args.controls else {},
                    executable=str(args.tapas.resolve()),
                    executable_sha256=hashlib.sha256(args.tapas.read_bytes()).hexdigest(),
                    git_head=subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
                    git_status=subprocess.check_output(["git", "status", "--short"], cwd=ROOT, text=True),
                    oracles=oracles)
    (args.output / "metadata.json").write_text(json.dumps(metadata, indent=2)+"\n")
    if not args.no_plot:
        plot(summary, ws, args.output)
    report(summary, oracles, rows, args.output, controls=args.controls)
    print(f"Report: {args.output / 'report.md'}", flush=True)


if __name__ == "__main__":
    main()
