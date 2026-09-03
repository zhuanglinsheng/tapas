# Tapas and Python Performance Comparison

[简体中文](Results_zh.md) | English | [Project Home](../../README_en.md)

Test date: 2026-09-02

## Test Environment

- System: `macOS-26.5.2-arm64-arm-64bit-Mach-O`
- Architecture: `arm64`
- Python: `3.13.3`
- Tapas: `Tapas 0.1.0 Copyright (C) 2020-2026`
- Tapas executable: `build-release/bin/tapas`
- Measured runs: 11 per benchmark, after 1 warm-up run

## Results

Times are median process CPU times and exclude process startup, source loading,
and compilation. A `Tapas/Python` ratio below 1 means Tapas is faster.

### Complete Algorithms

These programs combine recursion, branching, containers, indexing, and memory
allocation to approximate complete algorithm workloads.

| Benchmark | Source | Result | Tapas | Python | Tapas/Python |
| --- | --- | ---: | ---: | ---: | ---: |
| `recursive_fibonacci` | [Tapas](recursive_fibonacci.tap) · [Python](recursive_fibonacci.py) | 46368 | 6,902,000 ns | 5,301,000 ns | 1.302× |
| `sieve` | [Tapas](sieve.tap) · [Python](sieve.py) | 2262 | 2,287,000 ns | 1,991,000 ns | 1.149× |
| `merge_sort` | [Tapas](merge_sort.tap) · [Python](merge_sort.py) | 5638402144 | 9,359,000 ns | 5,706,000 ns | 1.640× |
| `n_queens` | [Tapas](n_queens.tap) · [Python](n_queens.py) | 724 | 226,331,000 ns | 152,538,000 ns | 1.484× |
| `longest_common_subsequence` | [Tapas](longest_common_subsequence.tap) · [Python](longest_common_subsequence.py) | 1480 | 43,013,000 ns | 16,870,000 ns | 2.550× |
| `matrix_multiply` | [Tapas](matrix_multiply.tap) · [Python](matrix_multiply.py) | 817318963 | 5,645,000 ns | 5,254,000 ns | 1.074× |

The geometric mean ratio for this group is **1.467×**.

### VM Hot Paths

Each program amplifies one common VM operation to help isolate interpreter
overhead.

| Benchmark | Source | Result | Tapas | Python | Tapas/Python |
| --- | --- | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | [Tapas](vm_hot_paths.tap) · [Python](vm_hot_paths.py) | 8999994 | 161,430,000 ns | 280,901,000 ns | 0.575× |
| `float_arithmetic` | [Tapas](float_arithmetic.tap) · [Python](float_arithmetic.py) | 125627 | 42,606,000 ns | 73,943,000 ns | 0.576× |
| `function_calls` | [Tapas](function_calls.tap) · [Python](function_calls.py) | 599994 | 11,964,000 ns | 18,487,000 ns | 0.647× |
| `list_access` | [Tapas](list_access.tap) · [Python](list_access.py) | 1937500 | 18,422,000 ns | 38,664,000 ns | 0.476× |
| `tail_recursion` | [Tapas](tail_recursion.tap) · [Python](tail_recursion.py) | 50000 | 2,589,000 ns | 3,454,000 ns | 0.750× |
| `branch_logic` | [Tapas](branch_logic.tap) · [Python](branch_logic.py) | -2626268 | 127,680,000 ns | 184,419,000 ns | 0.692× |

The geometric mean ratio for this group is **0.613×**.

The geometric mean ratio across all benchmarks is **0.948×**.

## Notes

These programs compare interpreter overhead while both implementations execute
the same algorithms; they do not represent complete application performance.
Results depend on system load, power settings, compiler version, and Python
version. Regenerate the reports with the commands in this directory's README
after changing the VM or runtime environment.
