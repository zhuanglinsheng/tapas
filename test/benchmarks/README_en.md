# Tapas, Python, and Lua Performance Comparison

[简体中文](README.md) | English | [Project Home](../../README_en.md)

Test date: 2026-10-08

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
| `recursive_fibonacci` | [Tapas](recursive_fibonacci.tap) · [Python](recursive_fibonacci.py) · [Lua](recursive_fibonacci.lua) | 4,117.0 µs | 5,498.0 µs | 4,358.0 µs | 0.749× | 0.945× |
| `sieve` | [Tapas](sieve.tap) · [Python](sieve.py) · [Lua](sieve.lua) | 990.0 µs | 2,263.0 µs | 1,012.0 µs | 0.437× | 0.978× |
| `merge_sort` | [Tapas](merge_sort.tap) · [Python](merge_sort.py) · [Lua](merge_sort.lua) | 7,212.0 µs | 5,187.0 µs | 8,737.0 µs | 1.390× | 0.825× |
| `n_queens` | [Tapas](n_queens.tap) · [Python](n_queens.py) · [Lua](n_queens.lua) | 175,247.0 µs | 147,560.0 µs | 68,256.0 µs | 1.188× | 2.567× |
| `longest_common_subsequence` | [Tapas](longest_common_subsequence.tap) · [Python](longest_common_subsequence.py) · [Lua](longest_common_subsequence.lua) | 17,204.0 µs | 12,494.0 µs | 24,752.0 µs | 1.377× | 0.695× |
| `matrix_multiply` | [Tapas](matrix_multiply.tap) · [Python](matrix_multiply.py) · [Lua](matrix_multiply.lua) | 6,081.0 µs | 5,246.0 µs | 3,372.0 µs | 1.159× | 1.803× |
| `sudoku` | [Tapas](sudoku.tap) · [Python](sudoku.py) · [Lua](sudoku.lua) | 449,432.0 µs | 387,177.0 µs | 171,619.0 µs | 1.161× | 2.619× |
| `dijkstra` | [Tapas](dijkstra.tap) · [Python](dijkstra.py) · [Lua](dijkstra.lua) | 24,248.0 µs | 20,002.0 µs | 11,943.0 µs | 1.212× | 2.030× |
| `optimize` | [Tapas](optimize.tap) · [Python](optimize.py) · [Lua](optimize.lua) | 25,175.0 µs | 21,993.0 µs | 22,727.0 µs | 1.145× | 1.108× |
| `kmp` | [Tapas](kmp.tap) · [Python](kmp.py) · [Lua](kmp.lua) | 30,440.0 µs | 22,799.0 µs | 59,432.0 µs | 1.335× | 0.512× |
| `k_nucleotide` | [Tapas](k_nucleotide.tap) · [Python](k_nucleotide.py) · [Lua](k_nucleotide.lua) | 206,894.0 µs | 183,256.0 µs | 96,980.0 µs | 1.129× | 2.133× |
| `binary_trees` | [Tapas](binary_trees.tap) · [Python](binary_trees.py) · [Lua](binary_trees.lua) | 852,199.0 µs | 626,822.0 µs | 1,017,804.0 µs | 1.360× | 0.837× |
| `lru_cache` | [Tapas](lru_cache.tap) · [Python](lru_cache.py) · [Lua](lru_cache.lua) | 46,897.0 µs | 68,794.0 µs | 59,117.0 µs | 0.682× | 0.793× |
| `huffman` | [Tapas](huffman.tap) · [Python](huffman.py) · [Lua](huffman.lua) | 3,408.0 µs | 3,188.0 µs | 2,480.0 µs | 1.069× | 1.374× |
| `fannkuch` | [Tapas](fannkuch.tap) · [Python](fannkuch.py) · [Lua](fannkuch.lua) | 47,325.0 µs | 33,590.0 µs | 32,991.0 µs | 1.409× | 1.434× |
| `n_body` | [Tapas](n_body.tap) · [Python](n_body.py) · [Lua](n_body.lua) | 25,982.0 µs | 26,492.0 µs | 10,875.0 µs | 0.981× | 2.389× |

The geometric mean ratio for this group: Tapas/Python **1.068×**，Tapas/Lua **1.276×**.

### VM Hot Paths

Each program times a loop inside a function and keeps only the measured VM operation or container access pattern in the loop body, to help isolate interpreter overhead. Modular index arithmetic, growing-integer accumulation, and module-scope variable access are kept out, so one language's unrelated strengths or weaknesses cannot leak into a single-item result. The loop bodies of `vm_hot_paths` and `float_arithmetic` are themselves the measured arithmetic workload; `function_call_baseline` shares the loop body of `function_calls`, so subtracting the two estimates direct-call overhead, and `function_callbacks` adds a higher-order callback layer.

| Benchmark | Source | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | [Tapas](vm_hot_paths.tap) · [Python](vm_hot_paths.py) · [Lua](vm_hot_paths.lua) | 93,839.0 µs | 146,973.0 µs | 42,528.0 µs | 0.638× | 2.207× |
| `float_arithmetic` | [Tapas](float_arithmetic.tap) · [Python](float_arithmetic.py) · [Lua](float_arithmetic.lua) | 29,203.0 µs | 41,863.0 µs | 11,213.0 µs | 0.698× | 2.604× |
| `function_call_baseline` | [Tapas](function_call_baseline.tap) · [Python](function_call_baseline.py) · [Lua](function_call_baseline.lua) | 2,948.0 µs | 4,567.0 µs | 2,168.0 µs | 0.646× | 1.360× |
| `function_calls` | [Tapas](function_calls.tap) · [Python](function_calls.py) · [Lua](function_calls.lua) | 10,084.0 µs | 6,783.0 µs | 6,059.0 µs | 1.487× | 1.664× |
| `function_callbacks` | [Tapas](function_callbacks.tap) · [Python](function_callbacks.py) · [Lua](function_callbacks.lua) | 15,721.0 µs | 11,902.0 µs | 8,806.0 µs | 1.321× | 1.785× |
| `list_access` | [Tapas](list_access.tap) · [Python](list_access.py) · [Lua](list_access.lua) | 7,438.0 µs | 7,062.0 µs | 1,212.0 µs | 1.053× | 6.137× |
| `list_append` | [Tapas](list_append.tap) · [Python](list_append.py) · [Lua](list_append.lua) | 9,997.0 µs | 6,621.0 µs | 8,898.0 µs | 1.510× | 1.124× |
| `list_update` | [Tapas](list_update.tap) · [Python](list_update.py) · [Lua](list_update.lua) | 7,398.0 µs | 5,532.0 µs | 4,536.0 µs | 1.337× | 1.631× |
| `list_copy` | [Tapas](list_copy.tap) · [Python](list_copy.py) · [Lua](list_copy.lua) | 7,720.0 µs | 25,250.0 µs | 175,539.0 µs | 0.306× | 0.044× |
| `list_from_iterable` | [Tapas](list_from_iterable.tap) · [Python](list_from_iterable.py) · [Lua](list_from_iterable.lua) | 13,345.0 µs | 212,673.0 µs | 236,438.0 µs | 0.063× | 0.056× |
| `list_replicate` | [Tapas](list_replicate.tap) · [Python](list_replicate.py) · [Lua](list_replicate.lua) | 6,689.0 µs | 12,819.0 µs | 402,398.0 µs | 0.522× | 0.017× |
| `dict_insert` | [Tapas](dict_insert.tap) · [Python](dict_insert.py) · [Lua](dict_insert.lua) | 8,973.0 µs | 6,212.0 µs | 2,516.0 µs | 1.444× | 3.566× |
| `dict_lookup` | [Tapas](dict_lookup.tap) · [Python](dict_lookup.py) · [Lua](dict_lookup.lua) | 5,926.0 µs | 13,734.0 µs | 1,486.0 µs | 0.431× | 3.988× |
| `dict_update` | [Tapas](dict_update.tap) · [Python](dict_update.py) · [Lua](dict_update.lua) | 4,452.0 µs | 17,718.0 µs | 1,794.0 µs | 0.251× | 2.482× |
| `dict_delete` | [Tapas](dict_delete.tap) · [Python](dict_delete.py) · [Lua](dict_delete.lua) | 4,310.0 µs | 6,127.0 µs | 1,361.0 µs | 0.703× | 3.167× |
| `dict_literal` | [Tapas](dict_literal.tap) · [Python](dict_literal.py) · [Lua](dict_literal.lua) | 19,059.0 µs | 19,862.0 µs | 34,133.0 µs | 0.960× | 0.558× |
| `dict_counting` | [Tapas](dict_counting.tap) · [Python](dict_counting.py) · [Lua](dict_counting.lua) | 19,653.0 µs | 31,174.0 µs | 6,468.0 µs | 0.630× | 3.038× |
| `tail_recursion` | [Tapas](tail_recursion.tap) · [Python](tail_recursion.py) · [Lua](tail_recursion.lua) | 1,551.0 µs | 3,222.0 µs | 1,135.0 µs | 0.481× | 1.367× |
| `branch_logic` | [Tapas](branch_logic.tap) · [Python](branch_logic.py) · [Lua](branch_logic.lua) | 23,207.0 µs | 22,780.0 µs | 11,489.0 µs | 1.019× | 2.020× |

The geometric mean ratio for this group: Tapas/Python **0.662×**，Tapas/Lua **1.095×**.

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
Timed regions live inside functions in all three languages, and the comparison script verifies that all implementations print identical results.
Results depend on system load, power settings, compiler version, Python version, and Lua version.
Regenerate the reports with the commands in the Usage section after changing the VM or runtime environment.
