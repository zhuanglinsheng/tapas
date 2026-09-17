# VM Performance Benchmarks

[简体中文](README.md) | English | [Project Home](../../README_en.md)

This directory compares complete algorithms and basic VM hot paths through
matching Tapas, Python, and Lua programs.

- Complete algorithms include recursive Fibonacci, the sieve of Eratosthenes,
  merge sort, N-Queens, longest common subsequence, and matrix multiplication.
  Together they cover recursion, backtracking, dynamic programming, container
  access, and nested loops.
- Hot-path programs isolate integer control flow, floating-point arithmetic,
  ordinary calls, tail recursion, nested branching, and list access to help
  locate specific VM overhead.

All three implementations use the same algorithms and inputs. Measurements record
process CPU time for the workload and exclude process startup, source loading,
and compilation. Each program also prints its computed result, and comparison
stops immediately if the results differ.

Use a Release build and a `lua` executable on `PATH` (override with `--lua`).
The script performs one warm-up and compares the median of
eleven measured runs per benchmark (tune with `--runs`), prints a summary, and
updates both result documents:

```sh
python3 test/benchmarks/compare_script.py build-release/bin/tapas
```

Add `--no-markdown` for console output only, or pass `--markdown PATH` to write
a report elsewhere. Repeating `--markdown` writes multiple reports from the
same measurements, so their numbers cannot diverge because of a second run.

To require Tapas to be faster in every benchmark, run:

```sh
python3 test/benchmarks/compare_script.py \
    build-release/bin/tapas --require-faster
```

See the current [Chinese](Results_zh.md) and [English](Results_en.md) reports.
Performance thresholds are not part of the regular test suite because CPU
load, power settings, compiler version, and Python version affect the results.
Published comparisons should always include the runtime environment.
