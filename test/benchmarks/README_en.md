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
- Measured runs: 3 per benchmark, after 1 warm-up run

## Results

Times are median process CPU times in microseconds and exclude process startup, source loading, and compilation.
A ratio below 1 means Tapas is faster (against Python and Lua respectively).

### Complete Algorithms

These programs combine recursion, branching, containers, indexing, and allocation to approximate complete algorithm workloads.

| Benchmark | Source | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `recursive_fibonacci` | [Tapas](recursive_fibonacci.tap) · [Python](recursive_fibonacci.py) · [Lua](recursive_fibonacci.lua) | 3,752.0 µs | 3,435.0 µs | 1,558.0 µs | 1.092× | 2.408× |
| `sieve` | [Tapas](sieve.tap) · [Python](sieve.py) · [Lua](sieve.lua) | 1,198.0 µs | 754.0 µs | 421.0 µs | 1.589× | 2.846× |
| `merge_sort` | [Tapas](merge_sort.tap) · [Python](merge_sort.py) · [Lua](merge_sort.lua) | 5,806.0 µs | 3,643.0 µs | 5,290.0 µs | 1.594× | 1.098× |
| `n_queens` | [Tapas](n_queens.tap) · [Python](n_queens.py) · [Lua](n_queens.lua) | 169,751.0 µs | 98,152.0 µs | 52,115.0 µs | 1.729× | 3.257× |
| `longest_common_subsequence` | [Tapas](longest_common_subsequence.tap) · [Python](longest_common_subsequence.py) · [Lua](longest_common_subsequence.lua) | 20,093.0 µs | 10,057.0 µs | 15,565.0 µs | 1.998× | 1.291× |
| `matrix_multiply` | [Tapas](matrix_multiply.tap) · [Python](matrix_multiply.py) · [Lua](matrix_multiply.lua) | 4,663.0 µs | 3,587.0 µs | 1,236.0 µs | 1.300× | 3.773× |
| `sudoku` | [Tapas](sudoku.tap) · [Python](sudoku.py) · [Lua](sudoku.lua) | 385,978.0 µs | 251,246.0 µs | 129,172.0 µs | 1.536× | 2.988× |
| `dijkstra` | [Tapas](dijkstra.tap) · [Python](dijkstra.py) · [Lua](dijkstra.lua) | 27,414.0 µs | 13,582.0 µs | 7,980.0 µs | 2.018× | 3.435× |
| `optimize` | [Tapas](optimize.tap) · [Python](optimize.py) · [Lua](optimize.lua) | 19,480.0 µs | 16,159.0 µs | 16,737.0 µs | 1.206× | 1.164× |
| `kmp` | [Tapas](kmp.tap) · [Python](kmp.py) · [Lua](kmp.lua) | 28,492.0 µs | 16,374.0 µs | 40,782.0 µs | 1.740× | 0.699× |
| `k_nucleotide` | [Tapas](k_nucleotide.tap) · [Python](k_nucleotide.py) · [Lua](k_nucleotide.lua) | 140,563.0 µs | 121,422.0 µs | 71,711.0 µs | 1.158× | 1.960× |
| `binary_trees` | [Tapas](binary_trees.tap) · [Python](binary_trees.py) · [Lua](binary_trees.lua) | 709,173.0 µs | 436,037.0 µs | 755,461.0 µs | 1.626× | 0.939× |
| `lru_cache` | [Tapas](lru_cache.tap) · [Python](lru_cache.py) · [Lua](lru_cache.lua) | 40,768.0 µs | 53,204.0 µs | 44,083.0 µs | 0.766× | 0.925× |
| `huffman` | [Tapas](huffman.tap) · [Python](huffman.py) · [Lua](huffman.lua) | 1,827.0 µs | 1,387.0 µs | 1,659.0 µs | 1.317× | 1.101× |
| `fannkuch` | [Tapas](fannkuch.tap) · [Python](fannkuch.py) · [Lua](fannkuch.lua) | 50,023.0 µs | 26,726.0 µs | 24,314.0 µs | 1.872× | 2.057× |
| `n_body` | [Tapas](n_body.tap) · [Python](n_body.py) · [Lua](n_body.lua) | 25,091.0 µs | 16,173.0 µs | 8,561.0 µs | 1.551× | 2.931× |

The geometric mean ratio for this group: Tapas/Python **1.464×**，Tapas/Lua **1.791×**.

### VM Hot Paths

Each program amplifies a common VM operation or container usage pattern to help isolate interpreter overhead. `function_call_baseline` is the equivalent loop without a call; subtracting it from `function_calls` estimates direct-call overhead, while `function_callbacks` adds a higher-order callback layer.

| Benchmark | Source | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | [Tapas](vm_hot_paths.tap) · [Python](vm_hot_paths.py) · [Lua](vm_hot_paths.lua) | 78,076.0 µs | 214,828.0 µs | 35,403.0 µs | 0.363× | 2.205× |
| `float_arithmetic` | [Tapas](float_arithmetic.tap) · [Python](float_arithmetic.py) · [Lua](float_arithmetic.lua) | 25,662.0 µs | 68,076.0 µs | 8,860.0 µs | 0.377× | 2.896× |
| `function_call_baseline` | [Tapas](function_call_baseline.tap) · [Python](function_call_baseline.py) · [Lua](function_call_baseline.lua) | 3,498.0 µs | 10,180.0 µs | 946.0 µs | 0.344× | 3.698× |
| `function_calls` | [Tapas](function_calls.tap) · [Python](function_calls.py) · [Lua](function_calls.lua) | 6,587.0 µs | 13,340.0 µs | 2,896.0 µs | 0.494× | 2.275× |
| `function_callbacks` | [Tapas](function_callbacks.tap) · [Python](function_callbacks.py) · [Lua](function_callbacks.lua) | 11,608.0 µs | 18,508.0 µs | 5,594.0 µs | 0.627× | 2.075× |
| `list_access` | [Tapas](list_access.tap) · [Python](list_access.py) · [Lua](list_access.lua) | 11,460.0 µs | 32,877.0 µs | 4,722.0 µs | 0.349× | 2.427× |
| `list_append` | [Tapas](list_append.tap) · [Python](list_append.py) · [Lua](list_append.lua) | 12,155.0 µs | 19,198.0 µs | 5,904.0 µs | 0.633× | 2.059× |
| `list_update` | [Tapas](list_update.tap) · [Python](list_update.py) · [Lua](list_update.lua) | 21,852.0 µs | 50,863.0 µs | 6,799.0 µs | 0.430× | 3.214× |
| `list_copy` | [Tapas](list_copy.tap) · [Python](list_copy.py) · [Lua](list_copy.lua) | 21,907.0 µs | 19,970.0 µs | 121,653.0 µs | 1.097× | 0.180× |
| `list_from_iterable` | [Tapas](list_from_iterable.tap) · [Python](list_from_iterable.py) · [Lua](list_from_iterable.lua) | 26,208.0 µs | 159,836.0 µs | 187,668.0 µs | 0.164× | 0.140× |
| `list_replicate` | [Tapas](list_replicate.tap) · [Python](list_replicate.py) · [Lua](list_replicate.lua) | 24,479.0 µs | 13,271.0 µs | 360,414.0 µs | 1.845× | 0.068× |
| `dict_insert` | [Tapas](dict_insert.tap) · [Python](dict_insert.py) · [Lua](dict_insert.lua) | 14,027.0 µs | 13,755.0 µs | 1,259.0 µs | 1.020× | 11.141× |
| `dict_lookup` | [Tapas](dict_lookup.tap) · [Python](dict_lookup.py) · [Lua](dict_lookup.lua) | 29,543.0 µs | 76,155.0 µs | 6,064.0 µs | 0.388× | 4.872× |
| `dict_update` | [Tapas](dict_update.tap) · [Python](dict_update.py) · [Lua](dict_update.lua) | 24,034.0 µs | 46,874.0 µs | 4,182.0 µs | 0.513× | 5.747× |
| `dict_delete` | [Tapas](dict_delete.tap) · [Python](dict_delete.py) · [Lua](dict_delete.lua) | 10,411.0 µs | 10,862.0 µs | 508.0 µs | 0.958× | 20.494× |
| `dict_literal` | [Tapas](dict_literal.tap) · [Python](dict_literal.py) · [Lua](dict_literal.lua) | 38,726.0 µs | 48,686.0 µs | 30,316.0 µs | 0.795× | 1.277× |
| `dict_counting` | [Tapas](dict_counting.tap) · [Python](dict_counting.py) · [Lua](dict_counting.lua) | 37,986.0 µs | 65,490.0 µs | 6,947.0 µs | 0.580× | 5.468× |
| `tail_recursion` | [Tapas](tail_recursion.tap) · [Python](tail_recursion.py) · [Lua](tail_recursion.lua) | 1,542.0 µs | 3,163.0 µs | 455.0 µs | 0.488× | 3.389× |
| `branch_logic` | [Tapas](branch_logic.tap) · [Python](branch_logic.py) · [Lua](branch_logic.lua) | 76,796.0 µs | 157,893.0 µs | 28,339.0 µs | 0.486× | 2.710× |

The geometric mean ratio for this group: Tapas/Python **0.545×**，Tapas/Lua **2.094×**.

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
Each language uses an idiomatic implementation with equivalent semantics; advantages from built-in bulk operations and data structures are part of the comparison.
Results depend on system load, power settings, compiler version, Python version, and Lua version.
Regenerate the reports with the commands in the Usage section after changing the VM or runtime environment.
