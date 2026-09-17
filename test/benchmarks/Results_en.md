# Tapas, Python, and Lua Performance Comparison

[简体中文](Results_zh.md) | English | [Project Home](../../README_en.md)

Test date: 2026-09-17

## Test Environment

- System: `macOS-26.6.2-arm64-arm-64bit-Mach-O`
- Architecture: `arm64`
- Python: `3.13.3`
- Tapas: `Tapas 0.1.0 Copyright (C) 2020-2026`
- Lua interpreter: `Lua 5.5.1  Copyright (C) 1994-2026 Lua.org, PUC-Rio`
- Tapas executable: `build-release/bin/tapas`
- Measured runs: 11 per benchmark, after 1 warm-up run

## Results

Times are median process CPU times in microseconds and exclude process startup, source loading, and compilation.
A ratio below 1 means Tapas is faster (against Python and Lua respectively).

### Complete Algorithms

These programs combine recursion, branching, containers, indexing, and allocation to approximate complete algorithm workloads.

| Benchmark | Source | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `recursive_fibonacci` | [Tapas](recursive_fibonacci.tap) · [Python](recursive_fibonacci.py) · [Lua](recursive_fibonacci.lua) | 5,140.0 µs | 5,358.0 µs | 1,668.0 µs | 0.959× | 3.082× |
| `sieve` | [Tapas](sieve.tap) · [Python](sieve.py) · [Lua](sieve.lua) | 1,878.0 µs | 1,911.0 µs | 472.0 µs | 0.983× | 3.979× |
| `merge_sort` | [Tapas](merge_sort.tap) · [Python](merge_sort.py) · [Lua](merge_sort.lua) | 6,304.0 µs | 5,595.0 µs | 5,676.0 µs | 1.127× | 1.111× |
| `n_queens` | [Tapas](n_queens.tap) · [Python](n_queens.py) · [Lua](n_queens.lua) | 173,902.0 µs | 150,263.0 µs | 52,340.0 µs | 1.157× | 3.323× |
| `longest_common_subsequence` | [Tapas](longest_common_subsequence.tap) · [Python](longest_common_subsequence.py) · [Lua](longest_common_subsequence.lua) | 20,562.0 µs | 16,648.0 µs | 16,064.0 µs | 1.235× | 1.280× |
| `matrix_multiply` | [Tapas](matrix_multiply.tap) · [Python](matrix_multiply.py) · [Lua](matrix_multiply.lua) | 4,815.0 µs | 5,158.0 µs | 1,315.0 µs | 0.934× | 3.662× |

The geometric mean ratio for this group: Tapas/Python **1.060×**，Tapas/Lua **2.442×**.

### VM Hot Paths

Each program amplifies one common VM operation to help isolate interpreter overhead.

| Benchmark | Source | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | [Tapas](vm_hot_paths.tap) · [Python](vm_hot_paths.py) · [Lua](vm_hot_paths.lua) | 98,231.0 µs | 295,007.0 µs | 37,208.0 µs | 0.333× | 2.640× |
| `float_arithmetic` | [Tapas](float_arithmetic.tap) · [Python](float_arithmetic.py) · [Lua](float_arithmetic.lua) | 30,167.0 µs | 73,752.0 µs | 9,293.0 µs | 0.409× | 3.246× |
| `function_calls` | [Tapas](function_calls.tap) · [Python](function_calls.py) · [Lua](function_calls.lua) | 10,656.0 µs | 18,112.0 µs | 2,991.0 µs | 0.588× | 3.563× |
| `list_access` | [Tapas](list_access.tap) · [Python](list_access.py) · [Lua](list_access.lua) | 12,722.0 µs | 38,565.0 µs | 4,694.0 µs | 0.330× | 2.710× |
| `tail_recursion` | [Tapas](tail_recursion.tap) · [Python](tail_recursion.py) · [Lua](tail_recursion.lua) | 1,925.0 µs | 3,351.0 µs | 460.0 µs | 0.574× | 4.185× |
| `branch_logic` | [Tapas](branch_logic.tap) · [Python](branch_logic.py) · [Lua](branch_logic.lua) | 101,725.0 µs | 179,922.0 µs | 28,958.0 µs | 0.565× | 3.513× |

The geometric mean ratio for this group: Tapas/Python **0.453×**，Tapas/Lua **3.267×**.

The geometric mean ratio across all benchmarks: Tapas/Python **0.693×**，Tapas/Lua **2.825×**.

## Notes

These programs compare interpreter overhead while all implementations execute the same algorithms; they do not represent complete application performance.
Results depend on system load, power settings, compiler version, Python version, and Lua version.
Regenerate the reports with the commands in this directory's README after changing the VM or runtime environment.
