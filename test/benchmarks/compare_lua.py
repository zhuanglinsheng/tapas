#!/usr/bin/env python3
import argparse
import statistics
import subprocess
import sys
from pathlib import Path


HERE = Path(__file__).resolve().parent
ALGORITHM_BENCHMARKS = (
    "recursive_fibonacci",
    "sieve",
    "merge_sort",
    "n_queens",
    "longest_common_subsequence",
    "matrix_multiply",
)

HOT_PATH_BENCHMARKS = (
    "vm_hot_paths",
    "float_arithmetic",
    "function_calls",
    "list_access",
    "tail_recursion",
    "branch_logic",
)

BENCHMARKS = ALGORITHM_BENCHMARKS + HOT_PATH_BENCHMARKS


def measure(command: list[str], runs: int) -> tuple[int, list[int]]:
    totals: list[int] = []
    elapsed: list[int] = []
    for _ in range(runs + 1):
        result = subprocess.run(
            command, check=True, capture_output=True, text=True
        )
        lines = result.stdout.splitlines()
        if len(lines) != 2:
            raise RuntimeError(
                f"unexpected output from {' '.join(command)}: {result.stdout!r}"
            )
        totals.append(int(lines[0]))
        elapsed.append(int(lines[1]))
    if len(set(totals)) != 1:
        raise RuntimeError(f"result changed between runs: {totals}")
    return totals[0], elapsed[1:]


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare matching Tapas and Lua benchmark programs."
    )
    parser.add_argument("tapas", type=Path, help="Release Tapas executable")
    parser.add_argument(
        "--lua",
        type=str,
        default="lua",
        help="Lua interpreter executable (default: lua from PATH)",
    )
    parser.add_argument("--runs", type=int, default=7)
    parser.add_argument(
        "--require-faster",
        action="store_true",
        help="fail when the Tapas median is not lower than Lua's",
    )
    args = parser.parse_args()
    if args.runs < 3:
        parser.error("--runs must be at least 3")

    lua_version = subprocess.run(
        [args.lua, "-v"], check=True, capture_output=True, text=True
    ).stdout.splitlines()[0]
    print(f"Lua interpreter: {lua_version}")

    failed = False
    ratios: list[float] = []
    ratios_by_name: dict[str, float] = {}
    for name in BENCHMARKS:
        tapas_total, tapas_times = measure(
            [str(args.tapas), str(HERE / f"{name}.tap")], args.runs
        )
        lua_total, lua_times = measure(
            [args.lua, str(HERE / f"{name}.lua")], args.runs
        )
        if tapas_total != lua_total:
            raise RuntimeError(
                f"{name}: different results: "
                f"Tapas={tapas_total}, Lua={lua_total}"
            )

        tapas_median = int(statistics.median(tapas_times))
        lua_median = int(statistics.median(lua_times))
        ratio = tapas_median / lua_median
        ratios.append(ratio)
        ratios_by_name[name] = ratio
        print(name)
        print(f"  result:        {tapas_total}")
        print(f"  Tapas median:  {tapas_median:,} ns")
        print(f"  Lua median:    {lua_median:,} ns")
        print(f"  Tapas/Lua:     {ratio:.3f}x")
        if args.require_faster and ratio >= 1.0:
            failed = True

    geometric_mean = statistics.geometric_mean(ratios)
    algorithm_mean = statistics.geometric_mean(
        ratios_by_name[name] for name in ALGORITHM_BENCHMARKS
    )
    hot_path_mean = statistics.geometric_mean(
        ratios_by_name[name] for name in HOT_PATH_BENCHMARKS
    )
    print(f"algorithm geometric mean Tapas/Lua: {algorithm_mean:.3f}x")
    print(f"hot-path geometric mean Tapas/Lua: {hot_path_mean:.3f}x")
    print(f"geometric mean Tapas/Lua: {geometric_mean:.3f}x")
    if failed:
        print(
            "Tapas is not faster than Lua in every benchmark.",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
