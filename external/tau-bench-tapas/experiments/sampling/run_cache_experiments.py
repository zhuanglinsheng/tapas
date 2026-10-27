#!/usr/bin/env python3
"""Controlled finite-cache oracle-cost experiment, not production sampler timing.

Tapas certifies a public region dictionary (including SAT distractors). Python
then replays paired iid streams and charges each logical discovery/business
query. Discovery decisions see only the prior, horizon and already queried
regions. The held-out streams never enter the discovery decision function.
"""
from __future__ import annotations

import argparse
import functools
import gzip
import hashlib
import itertools
import json
from pathlib import Path
import random
import subprocess
import os
import tempfile

from run_experiments import ROOT, enumerate_prior, feasible, interval, workloads

SEEDS = [101, 137, 173, 211, 257, 307, 353, 401, 449, 503]
HORIZONS = [32, 128, 512, 2048]
CAPACITIES = [1, 2, 4]
POLICIES = ["minimum_width", "minimum_reverse", "total_mass", "marginal_mass", "exact_mass", "gated_exact"]


def cache_workloads():
    ws = workloads()
    # Low mass comes first in declaration order; the reverse tie-break baseline
    # exposes how dependent a width-only comparison is on ordering.
    ws[0] = dict(ws[0], probabilities=[.2, .2, .8, .8, .5, .5])
    return ws


class Dictionary:
    def __init__(self, w):
        self.workload = w
        self.prior = enumerate_prior(w)
        self.points = list(self.prior)
        self.index = {x: i for i, x in enumerate(self.points)}
        self.weights = list(self.prior.values())
        self.cores = [c for k in (1, 2) for c in itertools.combinations(range(len(w["probabilities"])), k)]
        self.masks = [sum(1 << j for j, x in enumerate(self.points) if all(x[i] == 1 for i in c))
                      for c in self.cores]
        self.infeasible = sum(1 << j for j, x in enumerate(self.points) if not feasible(x, w))
        self.safe = [m & ~self.infeasible == 0 for m in self.masks]

    @functools.lru_cache(maxsize=None)
    def mass(self, mask):
        return sum(p for i, p in enumerate(self.weights) if mask & (1 << i))

    def union(self, ids):
        mask = 0
        for i in ids:
            mask |= self.masks[i]
        return mask

    def prune(self, ids):
        # All policies get identical removal of contained/redundant regions.
        return tuple(i for i in sorted(ids) if not any(
            i != j and self.masks[i] & ~self.masks[j] == 0 for j in ids))

    def select(self, ids, capacity, policy):
        ids = self.prune(ids)
        if policy in ("exact_mass", "gated_exact"):
            choices = (c for size in range(min(capacity, len(ids))+1)
                       for c in itertools.combinations(ids, size))
            return max(choices, key=lambda c: (self.mass(self.union(c)), -len(c), tuple(-i for i in c)))
        if policy.startswith("minimum"):
            return tuple(sorted(ids, key=lambda i: (len(self.cores[i]),
                         -i if policy == "minimum_reverse" else i))[:capacity])
        if policy == "total_mass":
            return tuple(sorted(ids, key=lambda i: (-self.mass(self.masks[i]), i))[:capacity])
        if policy != "marginal_mass":
            raise ValueError(policy)
        selected, mask = [], 0
        for _ in range(min(capacity, len(ids))):
            i = max((i for i in ids if i not in selected),
                    key=lambda i: (self.mass(self.masks[i] & ~mask), -i))
            selected.append(i)
            mask |= self.masks[i]
        return tuple(selected)


def discover(d, capacity, horizon, policy, answers):
    """Only queried answers are inspected. No stream or true-safe set is read."""
    verified, pending, history = [], set(range(len(d.cores))), []
    while pending:
        selected = d.select(verified, capacity, "exact_mass" if policy == "gated_exact" else policy)
        if policy == "gated_exact":
            current = d.mass(d.union(selected))
            # Optimistic *single-query* gain: temporarily treat that region as
            # certified. It is an upper bound on this query's immediate gain,
            # not a bound on the value of a sequence of future queries.
            bounds = {i: max(0., d.mass(d.union(d.select(verified+[i], capacity, "exact_mass")))-current)
                      for i in pending}
            candidate = max(pending, key=lambda i: (bounds[i], -i))
            benefit = horizon * bounds[candidate]
            if benefit <= 1.0 + 1e-12:
                break
        else:
            candidate = min(pending)
            benefit = None
        pending.remove(candidate)
        safe = answers[candidate]
        history.append(dict(candidate=candidate, unsat=safe, optimistic_saved_calls=benefit))
        if safe:
            verified.append(candidate)
    selected = d.select(verified, capacity, "exact_mass" if policy == "gated_exact" else policy)
    mask = d.union(selected)
    # Independent check of the final certificates (not used to choose them).
    assert mask & ~d.infeasible == 0
    return dict(selected=list(selected), covered_mass=d.mass(mask), mask=mask,
                discovery_calls=len(history), discovery_sat=sum(not h["unsat"] for h in history),
                history=history, unqueried=len(pending))


def model_source(d):
    names = [f"x{i}" for i in range(len(d.workload["probabilities"]))]
    conditions = ["not (" + " and ".join(f"x{i} == 1" for i in c) + ")"
                  for c in d.workload["clauses"]]
    return ["let Model = rule (\n" + "\n".join(f"        {n}: Int," for n in names) + "\n) {\n" +
              "\n".join("    " + condition for condition in conditions) + "\n}",
              "let bounded = rules::restrict(Model, "+", ".join(f"'{n}': rules::range(0, 1)" for n in names)+")"]


def certify(d, tapas, destination):
    source = model_source(d)
    for i, c in enumerate(d.cores):
        source.append("if (true) {\n    let r = solve::hold(rules::restrict(bounded, "+
                      ", ".join(f"'x{j}': 1" for j in c)+"))\n"+
                      f"\n    print('CERT|{i}|', r::status)\n}}")
    path = destination / f"{d.workload['name']}_certify.tap"
    path.write_text("\n\n".join(source)+"\n")
    result = subprocess.run([str(tapas.resolve()), str(path.resolve())], cwd=ROOT,
                            text=True, capture_output=True, timeout=120)
    (destination / f"{d.workload['name']}_certify.log").write_text(result.stdout+result.stderr)
    if result.returncode:
        raise RuntimeError(result.stdout+result.stderr)
    answers = {}
    for line in result.stdout.splitlines():
        if line.startswith("CERT|"):
            _, index, status = line.split("|")
            if status not in ("sat", "unsat"):
                raise AssertionError(line)
            answers[int(index)] = status == "unsat"
    assert len(answers) == len(d.cores)
    assert [answers[i] for i in range(len(d.cores))] == d.safe
    return answers


def audit_actual_queries(d, plans, tapas, destination):
    """Execute planned discovery and every cache-miss query in actual Tapas.

    Schedules are scripted from the controlled planner, not a new implementation
    of a production sampler or a dynamically adaptive Tapas scheduler.
    """
    source = model_source(d)
    expected = {}
    audit_rows = []
    for seed, horizon, policy in itertools.product((101, 503), (32, 128),
                                                   ("total_mass", "marginal_mass", "gated_exact")):
        gid = f"{seed}_{horizon}_{policy}"
        plan = plans[d.workload["name"], 2, horizon, policy]
        for h in plan["history"]:
            index = h["candidate"]
            query = ", ".join(f"'x{i}': 1" for i in d.cores[index])
            source.append(f"if (true) {{\n    let r = solve::hold(rules::restrict(bounded, {query}))\n\n"
                          f"    print('AUDIT|{gid}|D|{index}|', r::status)\n}}")
            expected[gid, "D", index] = "unsat" if h["unsat"] else "sat"
        stream = stream_for(d, seed, horizon)
        checks = ["("+" and ".join(f"input[{j}] == 1" for j in d.cores[i])+")" for i in plan["selected"]]
        expression = " or ".join(checks) if checks else "false"
        query = ", ".join(f"'x{i}': input[{i}]" for i in range(len(d.workload["probabilities"])))
        source.append("if (true) {\n    let index = 0\n\n    for (let input in "+json.dumps(stream)+") {\n"
                      f"        if ({expression}) {{\n            print('AUDIT|{gid}|B|', index, '|cache')\n        }} else {{\n"
                      f"            let r = solve::hold(rules::restrict(bounded, {query}))\n\n"
                      f"            print('AUDIT|{gid}|B|', index, '|', r::status)\n        }}\n\n"
                      "        index = index + 1\n    }\n}")
        for i, x in enumerate(stream):
            hit = bool(plan["mask"] & (1 << d.index[x]))
            expected[gid, "B", i] = "cache" if hit else "sat" if feasible(x, d.workload) else "unsat"
        audit_rows.append(dict(workload=d.workload["name"], seed=seed, horizon=horizon, capacity=2,
                               policy=policy, discovery_calls=plan["discovery_calls"], **replay(d, stream, plan)))
    path = destination / f"{d.workload['name']}_audit.tap"
    path.write_text("\n\n".join(source)+"\n")
    result = subprocess.run([str(tapas.resolve()), str(path.resolve())], cwd=ROOT,
                            text=True, capture_output=True, timeout=180)
    (destination / f"{d.workload['name']}_audit.log").write_text(result.stdout+result.stderr)
    if result.returncode:
        raise RuntimeError(result.stdout+result.stderr)
    actual = {}
    for line in result.stdout.splitlines():
        if line.startswith("AUDIT|"):
            _, gid, kind, index, status = line.split("|")
            key = (gid, kind, int(index))
            assert key not in actual
            actual[key] = status
    assert actual == expected
    actual_queries = sum(status != "cache" for status in actual.values())
    assert actual_queries == sum(r["total_calls"] for r in audit_rows)
    print(f"{d.workload['name']}: audited {len(audit_rows)} replays, {actual_queries} actual solve::hold calls", flush=True)
    return audit_rows


def stream_for(d, seed, length):
    rng = random.Random(seed)
    return [tuple(int(rng.random() < p) for p in d.workload["probabilities"]) for _ in range(length)]


def replay(d, stream, plan):
    mask = plan["mask"]
    hits = sum(bool(mask & (1 << d.index[x])) for x in stream)
    accepted = [x for x in stream if feasible(x, d.workload)]
    # Reconstruct acceptance after cached rejections and explicit oracle checks.
    observed = [x for x in stream if not (mask & (1 << d.index[x])) and feasible(x, d.workload)]
    assert observed == accepted
    return dict(business_calls=len(stream)-hits, cache_hits=hits,
                total_calls=len(stream)-hits+plan["discovery_calls"], accepted=len(accepted))


def write_report(summary, dictionaries, plans, destination, audits):
    def fmt(v):
        return f"{v['mean']:.2f} ± {v['ci95_half']:.2f}"
    lines = ["# Finite-cache and discovery-cost experiment", "",
             "Controlled oracle-call model, not production `solve::sample` timing or its raw-core baseline. "
             "Tapas SAT/UNSAT certificates match exhaustive finite enumeration for every dictionary query. "
             "Each replay is charged all discovery queries from a cold start, including SAT failures.", "",
             "10 held-out seeds; 21 public candidate regions per workload; capacities 1/2/4; "
             "32/128/512/2048 raw proposals. Intervals are paired two-sided 95% Student intervals.", "",
             "Discovery runs before proposals. Full policies query the same dictionary; gated_exact may stop early. "
             "All policies remove contained regions. minimum_reverse tests the sensitivity of width-only ties. "
             "exact_mass knows the prior, not the future stream. Gating uses H times optimistic single-query "
             "coverage improvement > 1; multi-query interactions are outside this myopic rule.", "",
             "## Full-discovery coverage", "",
             "| Workload | Capacity | Policy | Covered prior mass | Discovery queries |",
             "|---|---:|---|---:|---:|"]
    for name in dictionaries:
        for capacity in CAPACITIES:
            for policy in POLICIES[:-1]:
                p = plans[name, capacity, HORIZONS[-1], policy]
                lines.append(f"| {name} | {capacity} | {policy} | {p['covered_mass']:.4f} | {p['discovery_calls']} |")
    lines += ["", "## Paired accounting", "",
              "Negative extra calls mean fewer logical oracle calls. The full-discovery exact policy is "
              "a comparator with identical dictionary-query cost; the no-cache baseline makes H business calls.", "",
              "| Workload | Capacity | H | Policy | Total calls | Extra vs no cache | Extra vs full exact | Discovery queries |",
              "|---|---:|---:|---|---:|---:|---:|---:|"]
    for r in summary:
        lines.append(f"| {r['workload']} | {r['capacity']} | {r['horizon']} | {r['policy']} | "
                     f"{fmt(r['total_calls'])} | {fmt(r['extra_vs_none'])} | {fmt(r['extra_vs_full_exact'])} | {r['discovery_calls']} |")
    lines += ["", "![Paired query costs](cache_costs.png)", "", "## Scope", "",
              "The 21-query dictionary is small and artificial. Supplied priors are exact independent finite distributions. "
              "Cache selection and exact mass calculations are free in this call-count model; no wall-clock speedup "
              "is claimed. The gated policy is evaluated on seeds distinct from the first pilot, but its fixed "
              "workload family is known; this is not evidence of generalization to new domains. "
              "No claim that stopping is globally optimal or that gated selection always improves total calls. "
              "The fixed proposal horizon differs from a fixed number of accepted samples."]
    lines += ["", "## Actual-query audit", "",
              f"Executed {len(audits)} scripted Tapas replays, including "
              f"{sum(r['total_calls'] for r in audits)} actual `solve::hold` queries. "
              "Every discovery result, cached rejection and fully bound business result matched the controlled model. "
              "See the workload audit sources/logs and metadata for all schedules. These queries are additional "
              "to the 84 dictionary certification queries. The full 2,880-row study remains a replay study."]
    (destination / "report.md").write_text("\n".join(lines)+"\n")


def plot(summary, destination):
    os.environ.setdefault("MPLCONFIGDIR", str(Path(tempfile.gettempdir()) / "tapas-sampling-mpl"))
    os.environ.setdefault("XDG_CACHE_HOME", str(Path(tempfile.gettempdir()) / "tapas-sampling-cache"))
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    fig, axes = plt.subplots(2, 2, figsize=(11, 7), constrained_layout=True)
    for ax, (name, capacity) in zip(axes.flat, [("equal_width", 1), ("overlap_high", 2),
                                               ("overlap_low", 2), ("cardinality", 2)]):
        for policy in ("minimum_width", "total_mass", "marginal_mass", "gated_exact"):
            group = [r for r in summary if r["workload"] == name and r["capacity"] == capacity and r["policy"] == policy]
            ax.errorbar([r["horizon"] for r in group],
                        [r["extra_vs_full_exact"]["mean"] for r in group],
                        yerr=[r["extra_vs_full_exact"]["ci95_half"] for r in group],
                        label=policy, marker=".", capsize=2)
        ax.axhline(0, color="black", linewidth=.8)
        ax.set(title=f"{name}, capacity={capacity}", xlabel="Raw proposals (H)",
               ylabel="Extra calls vs full-discovery exact")
        ax.set_xscale("log", base=2)
        ax.grid(alpha=.2)
    axes.flat[0].legend(fontsize=8)
    fig.savefig(destination / "cache_costs.png", dpi=160)
    fig.savefig(destination / "cache_costs.pdf")
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tapas", type=Path, default=ROOT / "build/bin/tapas")
    parser.add_argument("--output", type=Path, default=ROOT / "tmp/external/tau-bench-tapas/experiments/sampling/results/finite_cache")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    rows, plans, dictionaries, audits = [], {}, {}, []
    for w in cache_workloads():
        d = Dictionary(w)
        dictionaries[w["name"]] = d
        answers = certify(d, args.tapas, args.output)
        print(f"{w['name']}: {len(answers)} Tapas certificates checked", flush=True)
        for capacity in CAPACITIES:
            for horizon in HORIZONS:
                for policy in POLICIES:
                    plans[w["name"], capacity, horizon, policy] = discover(d, capacity, horizon, policy, answers)
        for seed in SEEDS:
            stream = stream_for(d, seed, max(HORIZONS))
            for capacity in CAPACITIES:
                for horizon in HORIZONS:
                    for policy in POLICIES:
                        plan = plans[w["name"], capacity, horizon, policy]
                        counts = replay(d, stream[:horizon], plan)
                        rows.append(dict(workload=w["name"], seed=seed, capacity=capacity, horizon=horizon,
                                         policy=policy, discovery_calls=plan["discovery_calls"], **counts))
        if w["name"] in ("overlap_high", "cardinality"):
            audits.extend(audit_actual_queries(d, plans, args.tapas, args.output))
    summary = []
    for key, plan in plans.items():
        name, capacity, horizon, policy = key
        group = [r for r in rows if (r["workload"], r["capacity"], r["horizon"], r["policy"]) == key]
        differences = []
        for r in group:
            exact = next(x for x in rows if x["workload"] == name and x["capacity"] == capacity and
                         x["horizon"] == horizon and x["seed"] == r["seed"] and x["policy"] == "exact_mass")
            differences.append(r["total_calls"]-exact["total_calls"])
        summary.append(dict(workload=name, capacity=capacity, horizon=horizon, policy=policy,
                            discovery_calls=plan["discovery_calls"], covered_mass=plan["covered_mass"],
                            total_calls=interval([r["total_calls"] for r in group]),
                            extra_vs_none=interval([r["total_calls"]-horizon for r in group]),
                            extra_vs_full_exact=interval(differences),
                            expected_total=horizon*(1-plan["covered_mass"])+plan["discovery_calls"]))
    assert len(rows) == 4*len(SEEDS)*len(CAPACITIES)*len(HORIZONS)*len(POLICIES)
    metadata = dict(seeds=SEEDS, horizons=HORIZONS, capacities=CAPACITIES,
                    model="controlled oracle-call replay, not production sampler timing",
                    executable_sha256=hashlib.sha256(args.tapas.read_bytes()).hexdigest(),
                    actual_query_audits=audits,
                    workloads=cache_workloads(), dictionaries={name:[dict(id=i, core=c, unsat=d.safe[i], mass=d.mass(d.masks[i]))
                        for i, c in enumerate(d.cores)] for name,d in dictionaries.items()},
                    plans=[dict(workload=k[0], capacity=k[1], horizon=k[2], policy=k[3], **p) for k,p in plans.items()])
    (args.output / "metadata.json").write_text(json.dumps(metadata, indent=2)+"\n")
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2)+"\n")
    (args.output / "rows.json.gz").write_bytes(gzip.compress(json.dumps(rows).encode(), mtime=0))
    plot(summary, args.output)
    write_report(summary, dictionaries, plans, args.output, audits)
    print(f"Checked {len(rows)} paired replays; report: {args.output / 'report.md'}")


if __name__ == "__main__":
    main()
