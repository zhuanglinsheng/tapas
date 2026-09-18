# Tapas, Python, and Lua Performance Comparison

[简体中文](README.md) | English | [Project Home](../../README_en.md)

Test date: 2026-09-18

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
| `recursive_fibonacci` | [Tapas](recursive_fibonacci.tap) · [Python](recursive_fibonacci.py) · [Lua](recursive_fibonacci.lua) | 5,168.0 µs | 5,232.0 µs | 1,704.0 µs | 0.988× | 3.033× |
| `sieve` | [Tapas](sieve.tap) · [Python](sieve.py) · [Lua](sieve.lua) | 1,932.0 µs | 1,951.0 µs | 463.0 µs | 0.990× | 4.173× |
| `merge_sort` | [Tapas](merge_sort.tap) · [Python](merge_sort.py) · [Lua](merge_sort.lua) | 6,385.0 µs | 5,702.0 µs | 5,753.0 µs | 1.120× | 1.110× |
| `n_queens` | [Tapas](n_queens.tap) · [Python](n_queens.py) · [Lua](n_queens.lua) | 174,636.0 µs | 152,214.0 µs | 52,698.0 µs | 1.147× | 3.314× |
| `longest_common_subsequence` | [Tapas](longest_common_subsequence.tap) · [Python](longest_common_subsequence.py) · [Lua](longest_common_subsequence.lua) | 21,006.0 µs | 16,754.0 µs | 16,216.0 µs | 1.254× | 1.295× |
| `matrix_multiply` | [Tapas](matrix_multiply.tap) · [Python](matrix_multiply.py) · [Lua](matrix_multiply.lua) | 4,880.0 µs | 5,218.0 µs | 1,311.0 µs | 0.935× | 3.722× |
| `sudoku` | [Tapas](sudoku.tap) · [Python](sudoku.py) · [Lua](sudoku.lua) | 405,558.0 µs | 439,180.0 µs | 131,898.0 µs | 0.923× | 3.075× |
| `dijkstra` | [Tapas](dijkstra.tap) · [Python](dijkstra.py) · [Lua](dijkstra.lua) | 29,740.0 µs | 24,372.0 µs | 8,353.0 µs | 1.220× | 3.560× |
| `optimize` | [Tapas](optimize.tap) · [Python](optimize.py) · [Lua](optimize.lua) | 24,064.0 µs | 19,100.0 µs | 16,937.0 µs | 1.260× | 1.421× |
| `kmp` | [Tapas](kmp.tap) · [Python](kmp.py) · [Lua](kmp.lua) | 31,086.0 µs | 27,941.0 µs | 42,984.0 µs | 1.113× | 0.723× |
| `k_nucleotide` | [Tapas](k_nucleotide.tap) · [Python](k_nucleotide.py) · [Lua](k_nucleotide.lua) | 146,285.0 µs | 153,400.0 µs | 71,174.0 µs | 0.954× | 2.055× |
| `binary_trees` | [Tapas](binary_trees.tap) · [Python](binary_trees.py) · [Lua](binary_trees.lua) | 865,066.0 µs | 611,519.0 µs | 775,906.0 µs | 1.415× | 1.115× |
| `lru_cache` | [Tapas](lru_cache.tap) · [Python](lru_cache.py) · [Lua](lru_cache.lua) | 44,789.0 µs | 55,772.0 µs | 44,076.0 µs | 0.803× | 1.016× |
| `huffman` | [Tapas](huffman.tap) · [Python](huffman.py) · [Lua](huffman.lua) | 3,031.0 µs | 2,059.0 µs | 1,689.0 µs | 1.472× | 1.795× |
| `fannkuch` | [Tapas](fannkuch.tap) · [Python](fannkuch.py) · [Lua](fannkuch.lua) | 62,991.0 µs | 40,373.0 µs | 24,789.0 µs | 1.560× | 2.541× |
| `n_body` | [Tapas](n_body.tap) · [Python](n_body.py) · [Lua](n_body.lua) | 26,740.0 µs | 19,152.0 µs | 9,469.0 µs | 1.396× | 2.824× |

The geometric mean ratio for this group: Tapas/Python **1.140×**，Tapas/Lua **2.018×**.

### VM Hot Paths

Each program amplifies one common VM operation to help isolate interpreter overhead.

| Benchmark | Source | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | [Tapas](vm_hot_paths.tap) · [Python](vm_hot_paths.py) · [Lua](vm_hot_paths.lua) | 99,193.0 µs | 294,549.0 µs | 36,613.0 µs | 0.337× | 2.709× |
| `float_arithmetic` | [Tapas](float_arithmetic.tap) · [Python](float_arithmetic.py) · [Lua](float_arithmetic.lua) | 30,430.0 µs | 72,838.0 µs | 9,006.0 µs | 0.418× | 3.379× |
| `function_calls` | [Tapas](function_calls.tap) · [Python](function_calls.py) · [Lua](function_calls.lua) | 10,560.0 µs | 18,328.0 µs | 2,938.0 µs | 0.576× | 3.594× |
| `list_access` | [Tapas](list_access.tap) · [Python](list_access.py) · [Lua](list_access.lua) | 12,783.0 µs | 38,002.0 µs | 4,817.0 µs | 0.336× | 2.654× |
| `tail_recursion` | [Tapas](tail_recursion.tap) · [Python](tail_recursion.py) · [Lua](tail_recursion.lua) | 1,892.0 µs | 3,183.0 µs | 458.0 µs | 0.594× | 4.131× |
| `branch_logic` | [Tapas](branch_logic.tap) · [Python](branch_logic.py) · [Lua](branch_logic.lua) | 99,571.0 µs | 186,342.0 µs | 28,869.0 µs | 0.534× | 3.449× |

The geometric mean ratio for this group: Tapas/Python **0.453×**，Tapas/Lua **3.279×**.

## Usage

Use a Release build and a `lua` executable on `PATH` (override with `--lua`).

```sh
python3 test/benchmarks/compare_script.py build-release/bin/tapas
```

The script performs one warm-up and compares the median of three measured runs per benchmark (tune with `--runs`), prints a summary, and updates both README documents in this directory.
Add `--no-markdown` for console output only, or pass `--markdown PATH` to write a report elsewhere.
Repeating `--markdown` writes multiple reports from the same measurements, so their numbers cannot diverge because of a second run.
To require Tapas to be faster than Python in every benchmark, use `--require-faster`.
Performance thresholds are not part of the regular test suite because CPU load, power settings, compiler version, and Python version affect the results; published comparisons should always include the runtime environment.

## Notes

These programs compare interpreter overhead while all implementations execute the same algorithms; they do not represent complete application performance.
Results depend on system load, power settings, compiler version, Python version, and Lua version.
Regenerate the reports with the commands in the Usage section after changing the VM or runtime environment.
