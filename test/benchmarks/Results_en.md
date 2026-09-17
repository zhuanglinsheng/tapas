# Tapas and Python Performance Comparison

[简体中文](Results_zh.md) | English | [Project Home](../../README_en.md)

Test date: 2026-09-17

## Test Environment

- System: `macOS-26.6.2-arm64-arm-64bit-Mach-O`
- Architecture: `arm64`
- Python: `3.13.3`
- Tapas: `Tapas 0.1.0 Copyright (C) 2020-2026`
- Tapas executable: `build-release/bin/tapas`
- Measured runs: 11 per benchmark, after 1 warm-up run

## Results

Times are median process CPU times and exclude process startup, source loading, and compilation.
A `Tapas/Python` ratio below 1 means Tapas is faster.

### Complete Algorithms

These programs combine recursion, branching, containers, indexing, and allocation to approximate complete algorithm workloads.

| Benchmark | Source | Result | Tapas | Python | Tapas/Python |
| --- | --- | ---: | ---: | ---: | ---: |
| `recursive_fibonacci` | [Tapas](recursive_fibonacci.tap) · [Python](recursive_fibonacci.py) | 46368 | 6,010,000 ns | 5,144,000 ns | 1.168× |
| `sieve` | [Tapas](sieve.tap) · [Python](sieve.py) | 2262 | 1,780,000 ns | 1,916,000 ns | 0.929× |
| `merge_sort` | [Tapas](merge_sort.tap) · [Python](merge_sort.py) | 5638402144 | 6,898,000 ns | 5,618,000 ns | 1.228× |
| `n_queens` | [Tapas](n_queens.tap) · [Python](n_queens.py) | 724 | 175,513,000 ns | 151,226,000 ns | 1.161× |
| `longest_common_subsequence` | [Tapas](longest_common_subsequence.tap) · [Python](longest_common_subsequence.py) | 1480 | 20,779,000 ns | 16,531,000 ns | 1.257× |
| `matrix_multiply` | [Tapas](matrix_multiply.tap) · [Python](matrix_multiply.py) | 817318963 | 4,347,000 ns | 5,236,000 ns | 0.830× |

The geometric mean ratio for this group is **1.083×**.

### VM Hot Paths

Each program amplifies one common VM operation to help isolate interpreter overhead.

| Benchmark | Source | Result | Tapas | Python | Tapas/Python |
| --- | --- | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | [Tapas](vm_hot_paths.tap) · [Python](vm_hot_paths.py) | 8999994 | 104,667,000 ns | 294,751,000 ns | 0.355× |
| `float_arithmetic` | [Tapas](float_arithmetic.tap) · [Python](float_arithmetic.py) | 125627 | 32,871,000 ns | 75,281,000 ns | 0.437× |
| `function_calls` | [Tapas](function_calls.tap) · [Python](function_calls.py) | 599994 | 10,454,000 ns | 19,433,000 ns | 0.538× |
| `list_access` | [Tapas](list_access.tap) · [Python](list_access.py) | 1937500 | 13,534,000 ns | 38,818,000 ns | 0.349× |
| `tail_recursion` | [Tapas](tail_recursion.tap) · [Python](tail_recursion.py) | 50000 | 1,828,000 ns | 3,332,000 ns | 0.549× |
| `branch_logic` | [Tapas](branch_logic.tap) · [Python](branch_logic.py) | -2626268 | 102,258,000 ns | 184,042,000 ns | 0.556× |

The geometric mean ratio for this group is **0.455×**.

The geometric mean ratio across all benchmarks is **0.702×**.

## Notes

These programs compare interpreter overhead while both implementations execute the same algorithms; they do not represent complete application performance.
Results depend on system load, power settings, compiler version, and Python version.
Regenerate the reports with the commands in this directory's README after changing the VM or runtime environment.
