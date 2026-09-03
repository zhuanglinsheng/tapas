# VM Performance Benchmarks

[简体中文](README.md) | English | [Project Home](../../README_en.md)

This directory compares complete algorithms and basic VM hot paths through
matching Tapas and Python programs.

- Complete algorithms include recursive Fibonacci, the sieve of Eratosthenes,
  merge sort, N-Queens, longest common subsequence, and matrix multiplication.
  Together they cover recursion, backtracking, dynamic programming, container
  access, and nested loops.
- Hot-path programs isolate integer control flow, floating-point arithmetic,
  ordinary calls, tail recursion, nested branching, and list access to help
  locate specific VM overhead.

Both implementations use the same algorithms and inputs. Measurements record
process CPU time for the workload and exclude process startup, source loading,
and compilation. Each program also prints its computed result, and comparison
stops immediately if the results differ.

Use a Release build. The script performs one warm-up and compares the median of
seven measured runs by default:

```sh
python3 test/benchmarks/compare_python.py build-release/bin/tapas
```

To require Tapas to be faster in every benchmark, run:

```sh
python3 test/benchmarks/compare_python.py \
    build-release/bin/tapas --require-faster
```

Generate or update both reports with 11 measured runs per benchmark:

```sh
python3 test/benchmarks/compare_python.py \
    build-release/bin/tapas --runs 11 \
    --markdown test/benchmarks/Results_zh.md

python3 test/benchmarks/compare_python.py \
    build-release/bin/tapas --runs 11 \
    --markdown test/benchmarks/Results_en.md
```

See the current [Chinese](Results_zh.md) and [English](Results_en.md) reports.
Performance thresholds are not part of the regular test suite because CPU
load, power settings, compiler version, and Python version affect the results.
Published comparisons should always include the runtime environment.
