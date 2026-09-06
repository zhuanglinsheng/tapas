#!/usr/bin/env python3
"""Run and summarize the reproducible retail sampling experiment."""

from __future__ import annotations

import argparse
import math
import statistics
import subprocess
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SOURCE = Path(__file__).with_name("sample_experiment.tap")
FIELDS = (
    "mode", "scheduler", "seed", "status", "samples", "candidates",
    "solver_calls", "core_calls", "cache_hits", "cache_regions",
    "cache_mass", "elapsed",
)


def parse_rows(output: str) -> list[dict[str, object]]:
    rows = []
    for line in output.splitlines():
        if not line.startswith("RESULT|"):
            continue
        values = line.split("|")[1:]
        if len(values) != len(FIELDS):
            raise ValueError(f"invalid result row: {line}")
        row = dict(zip(FIELDS, values))
        for name in ("seed", "samples", "candidates", "solver_calls",
                     "core_calls", "cache_hits", "cache_regions"):
            row[name] = int(row[name])
        for name in ("cache_mass", "elapsed"):
            row[name] = float(row[name])
        if row["status"] != "complete" or row["samples"] != 10:
            raise RuntimeError(f"incomplete experiment: {row}")
        row["total_calls"] = row["solver_calls"] + row["core_calls"]
        row["calls_per_sample"] = row["total_calls"] / row["samples"]
        rows.append(row)
    if len(rows) != 45:
        raise RuntimeError(f"expected 45 result rows, got {len(rows)}")
    candidates_by_seed: dict[int, set[int]] = defaultdict(set)
    for row in rows:
        candidates_by_seed[int(row["seed"])].add(int(row["candidates"]))
    if any(len(values) != 1 for values in candidates_by_seed.values()):
        raise RuntimeError("strategies did not preserve paired candidate streams")
    return rows


def distribution_check(output: str) -> tuple[float, str]:
    groups: dict[int, list[int]] = defaultdict(list)
    for line in output.splitlines():
        if line.startswith("DISTRIBUTION|"):
            _, seed, capacity = line.split("|")
            groups[int(seed)].append(int(capacity))
    if len(groups) != 5 or any(len(values) != 200 for values in groups.values()):
        raise RuntimeError("expected 1,000 conditional-distribution samples")
    def tv(values: list[int]) -> float:
        if any(value < 100 or value > 109 for value in values):
            raise RuntimeError("sample outside the enumerated feasible support")
        return 0.5 * sum(abs(values.count(value) / len(values) - 0.1)
                         for value in range(100, 110))
    all_values = [value for values in groups.values() for value in values]
    return tv(all_values), interval([tv(values) for values in groups.values()])


def interval(values: list[float]) -> str:
    mean = statistics.mean(values)
    if len(values) < 2:
        return f"{mean:.3f}"
    # Two-sided 95% Student interval for the fixed five-seed experiment.
    half = 2.776 * statistics.stdev(values) / math.sqrt(len(values))
    return f"{mean:.3f} ± {half:.3f}"


def table(rows: list[dict[str, object]], keys: list[tuple[str, str]]) -> str:
    groups: dict[tuple[str, str], list[dict[str, object]]] = defaultdict(list)
    for row in rows:
        groups[(str(row["mode"]), str(row["scheduler"]))].append(row)
    lines = [
        "| generalization | scheduler | candidates | business calls | "
        "core calls | total/sample | cache hits | cache mass | elapsed (s) |",
        "|---|---|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for key in keys:
        group = groups[key]
        cells = [key[0], key[1]]
        for field in ("candidates", "solver_calls", "core_calls",
                      "calls_per_sample", "cache_hits", "cache_mass",
                      "elapsed"):
            cells.append(interval([float(row[field]) for row in group]))
        lines.append("| " + " | ".join(cells) + " |")
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tapas", type=Path,
                        default=ROOT / "build/bin/tapas")
    args = parser.parse_args()
    completed = subprocess.run(
        [str(args.tapas), str(SOURCE)], cwd=ROOT, text=True,
        capture_output=True, check=True,
    )
    rows = parse_rows(completed.stdout)
    aggregate_tv, seed_tv = distribution_check(completed.stdout)
    generalizations = [
        (name, "mass_fair") for name in (
            "none", "full", "raw_core", "deletion_mus",
            "minimum_core", "online_mass_core",
        )
    ]
    schedulers = [
        ("online_mass_core", name) for name in (
            "mass_fair", "mass", "fifo", "binding_count",
        )
    ]
    print("## Generalization ablation\n")
    print(table(rows, generalizations))
    print("\n## Online MassCore scheduler ablation\n")
    print(table(rows, schedulers))
    print("\n## Conditional-distribution check\n")
    print(f"1,000 accepted samples; aggregate TV distance = {aggregate_tv:.4f}; "
          f"per-seed TV distance = {seed_tv} (mean ± 95% CI).")
    print("Paired candidate counts are identical across all strategies.")


if __name__ == "__main__":
    main()
