# Tapas, Python, and Lua Performance Comparison

[简体中文](README.md) | English | [Project Home](../../README_en.md)

Test date: 2026-09-27

## Test Environment

- System: `macOS-26.6.2-arm64-arm-64bit-Mach-O`
- Architecture: `arm64`
- Python: `3.14.7`
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
| `recursive_fibonacci` | [Tapas](recursive_fibonacci.tap) · [Python](recursive_fibonacci.py) · [Lua](recursive_fibonacci.lua) | 4,974.0 µs | 3,434.0 µs | 1,660.0 µs | 1.448× | 2.996× |
| `sieve` | [Tapas](sieve.tap) · [Python](sieve.py) · [Lua](sieve.lua) | 1,648.0 µs | 1,031.0 µs | 425.0 µs | 1.598× | 3.878× |
| `merge_sort` | [Tapas](merge_sort.tap) · [Python](merge_sort.py) · [Lua](merge_sort.lua) | 6,079.0 µs | 3,716.0 µs | 5,703.0 µs | 1.636× | 1.066× |
| `n_queens` | [Tapas](n_queens.tap) · [Python](n_queens.py) · [Lua](n_queens.lua) | 163,594.0 µs | 98,895.0 µs | 51,218.0 µs | 1.654× | 3.194× |
| `longest_common_subsequence` | [Tapas](longest_common_subsequence.tap) · [Python](longest_common_subsequence.py) · [Lua](longest_common_subsequence.lua) | 19,786.0 µs | 10,220.0 µs | 15,996.0 µs | 1.936× | 1.237× |
| `matrix_multiply` | [Tapas](matrix_multiply.tap) · [Python](matrix_multiply.py) · [Lua](matrix_multiply.lua) | 4,712.0 µs | 3,644.0 µs | 1,256.0 µs | 1.293× | 3.752× |
| `sudoku` | [Tapas](sudoku.tap) · [Python](sudoku.py) · [Lua](sudoku.lua) | 386,062.0 µs | 254,639.0 µs | 131,054.0 µs | 1.516× | 2.946× |
| `dijkstra` | [Tapas](dijkstra.tap) · [Python](dijkstra.py) · [Lua](dijkstra.lua) | 27,904.0 µs | 13,681.0 µs | 8,279.0 µs | 2.040× | 3.370× |
| `optimize` | [Tapas](optimize.tap) · [Python](optimize.py) · [Lua](optimize.lua) | 21,424.0 µs | 16,395.0 µs | 16,773.0 µs | 1.307× | 1.277× |
| `kmp` | [Tapas](kmp.tap) · [Python](kmp.py) · [Lua](kmp.lua) | 28,395.0 µs | 16,584.0 µs | 41,960.0 µs | 1.712× | 0.677× |
| `k_nucleotide` | [Tapas](k_nucleotide.tap) · [Python](k_nucleotide.py) · [Lua](k_nucleotide.lua) | 143,033.0 µs | 123,795.0 µs | 72,015.0 µs | 1.155× | 1.986× |
| `binary_trees` | [Tapas](binary_trees.tap) · [Python](binary_trees.py) · [Lua](binary_trees.lua) | 807,155.0 µs | 442,519.0 µs | 772,585.0 µs | 1.824× | 1.045× |
| `lru_cache` | [Tapas](lru_cache.tap) · [Python](lru_cache.py) · [Lua](lru_cache.lua) | 41,295.0 µs | 54,608.0 µs | 44,152.0 µs | 0.756× | 0.935× |
| `huffman` | [Tapas](huffman.tap) · [Python](huffman.py) · [Lua](huffman.lua) | 2,890.0 µs | 1,432.0 µs | 1,698.0 µs | 2.018× | 1.702× |
| `fannkuch` | [Tapas](fannkuch.tap) · [Python](fannkuch.py) · [Lua](fannkuch.lua) | 60,713.0 µs | 27,062.0 µs | 24,884.0 µs | 2.243× | 2.440× |
| `n_body` | [Tapas](n_body.tap) · [Python](n_body.py) · [Lua](n_body.lua) | 24,910.0 µs | 16,260.0 µs | 9,508.0 µs | 1.532× | 2.620× |

The geometric mean ratio for this group: Tapas/Python **1.558×**，Tapas/Lua **1.917×**.

### VM Hot Paths

Each program amplifies one common VM operation to help isolate interpreter overhead.

| Benchmark | Source | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | [Tapas](vm_hot_paths.tap) · [Python](vm_hot_paths.py) · [Lua](vm_hot_paths.lua) | 73,949.0 µs | 219,705.0 µs | 36,834.0 µs | 0.337× | 2.008× |
| `float_arithmetic` | [Tapas](float_arithmetic.tap) · [Python](float_arithmetic.py) · [Lua](float_arithmetic.lua) | 24,367.0 µs | 64,929.0 µs | 8,685.0 µs | 0.375× | 2.806× |
| `function_calls` | [Tapas](function_calls.tap) · [Python](function_calls.py) · [Lua](function_calls.lua) | 9,262.0 µs | 13,932.0 µs | 2,978.0 µs | 0.665× | 3.110× |
| `list_access` | [Tapas](list_access.tap) · [Python](list_access.py) · [Lua](list_access.lua) | 11,250.0 µs | 29,913.0 µs | 4,691.0 µs | 0.376× | 2.398× |
| `tail_recursion` | [Tapas](tail_recursion.tap) · [Python](tail_recursion.py) · [Lua](tail_recursion.lua) | 1,919.0 µs | 2,956.0 µs | 458.0 µs | 0.649× | 4.190× |
| `branch_logic` | [Tapas](branch_logic.tap) · [Python](branch_logic.py) · [Lua](branch_logic.lua) | 76,824.0 µs | 158,893.0 µs | 28,742.0 µs | 0.483× | 2.673× |

The geometric mean ratio for this group: Tapas/Python **0.463×**，Tapas/Lua **2.789×**.

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
