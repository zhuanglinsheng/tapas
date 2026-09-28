# Tapas, Python, and Lua Performance Comparison

[简体中文](README.md) | English | [Project Home](../../README_en.md)

Test date: 2026-09-28

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
| `recursive_fibonacci` | [Tapas](recursive_fibonacci.tap) · [Python](recursive_fibonacci.py) · [Lua](recursive_fibonacci.lua) | 4,500.0 µs | 3,343.0 µs | 1,725.0 µs | 1.346× | 2.609× |
| `sieve` | [Tapas](sieve.tap) · [Python](sieve.py) · [Lua](sieve.lua) | 1,543.0 µs | 832.0 µs | 449.0 µs | 1.855× | 3.437× |
| `merge_sort` | [Tapas](merge_sort.tap) · [Python](merge_sort.py) · [Lua](merge_sort.lua) | 5,616.0 µs | 3,804.0 µs | 5,780.0 µs | 1.476× | 0.972× |
| `n_queens` | [Tapas](n_queens.tap) · [Python](n_queens.py) · [Lua](n_queens.lua) | 153,193.0 µs | 99,967.0 µs | 51,749.0 µs | 1.532× | 2.960× |
| `longest_common_subsequence` | [Tapas](longest_common_subsequence.tap) · [Python](longest_common_subsequence.py) · [Lua](longest_common_subsequence.lua) | 17,449.0 µs | 10,074.0 µs | 15,775.0 µs | 1.732× | 1.106× |
| `matrix_multiply` | [Tapas](matrix_multiply.tap) · [Python](matrix_multiply.py) · [Lua](matrix_multiply.lua) | 5,132.0 µs | 3,655.0 µs | 1,225.0 µs | 1.404× | 4.189× |
| `sudoku` | [Tapas](sudoku.tap) · [Python](sudoku.py) · [Lua](sudoku.lua) | 381,909.0 µs | 252,845.0 µs | 128,815.0 µs | 1.510× | 2.965× |
| `dijkstra` | [Tapas](dijkstra.tap) · [Python](dijkstra.py) · [Lua](dijkstra.lua) | 26,983.0 µs | 13,604.0 µs | 8,023.0 µs | 1.983× | 3.363× |
| `optimize` | [Tapas](optimize.tap) · [Python](optimize.py) · [Lua](optimize.lua) | 16,751.0 µs | 16,121.0 µs | 16,694.0 µs | 1.039× | 1.003× |
| `kmp` | [Tapas](kmp.tap) · [Python](kmp.py) · [Lua](kmp.lua) | 27,158.0 µs | 16,228.0 µs | 41,684.0 µs | 1.674× | 0.652× |
| `k_nucleotide` | [Tapas](k_nucleotide.tap) · [Python](k_nucleotide.py) · [Lua](k_nucleotide.lua) | 146,791.0 µs | 127,826.0 µs | 68,201.0 µs | 1.148× | 2.152× |
| `binary_trees` | [Tapas](binary_trees.tap) · [Python](binary_trees.py) · [Lua](binary_trees.lua) | 776,779.0 µs | 429,244.0 µs | 761,426.0 µs | 1.810× | 1.020× |
| `lru_cache` | [Tapas](lru_cache.tap) · [Python](lru_cache.py) · [Lua](lru_cache.lua) | 36,116.0 µs | 53,686.0 µs | 43,861.0 µs | 0.673× | 0.823× |
| `huffman` | [Tapas](huffman.tap) · [Python](huffman.py) · [Lua](huffman.lua) | 2,093.0 µs | 1,386.0 µs | 1,665.0 µs | 1.510× | 1.257× |
| `fannkuch` | [Tapas](fannkuch.tap) · [Python](fannkuch.py) · [Lua](fannkuch.lua) | 43,211.0 µs | 26,768.0 µs | 25,115.0 µs | 1.614× | 1.721× |
| `n_body` | [Tapas](n_body.tap) · [Python](n_body.py) · [Lua](n_body.lua) | 26,892.0 µs | 16,277.0 µs | 8,464.0 µs | 1.652× | 3.177× |

The geometric mean ratio for this group: Tapas/Python **1.455×**，Tapas/Lua **1.775×**.

### VM Hot Paths

Each program times a loop inside a function and keeps only the measured VM operation or container access pattern in the loop body, to help isolate interpreter overhead. Modular index arithmetic, growing-integer accumulation, and module-scope variable access are kept out, so one language's unrelated strengths or weaknesses cannot leak into a single-item result. The loop bodies of `vm_hot_paths` and `float_arithmetic` are themselves the measured arithmetic workload; `function_call_baseline` shares the loop body of `function_calls`, so subtracting the two estimates direct-call overhead, and `function_callbacks` adds a higher-order callback layer.

| Benchmark | Source | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | [Tapas](vm_hot_paths.tap) · [Python](vm_hot_paths.py) · [Lua](vm_hot_paths.lua) | 84,224.0 µs | 101,088.0 µs | 35,372.0 µs | 0.833× | 2.381× |
| `float_arithmetic` | [Tapas](float_arithmetic.tap) · [Python](float_arithmetic.py) · [Lua](float_arithmetic.lua) | 25,732.0 µs | 29,649.0 µs | 9,159.0 µs | 0.868× | 2.809× |
| `function_call_baseline` | [Tapas](function_call_baseline.tap) · [Python](function_call_baseline.py) · [Lua](function_call_baseline.lua) | 3,012.0 µs | 4,074.0 µs | 932.0 µs | 0.739× | 3.232× |
| `function_calls` | [Tapas](function_calls.tap) · [Python](function_calls.py) · [Lua](function_calls.lua) | 6,952.0 µs | 6,641.0 µs | 2,881.0 µs | 1.047× | 2.413× |
| `function_callbacks` | [Tapas](function_callbacks.tap) · [Python](function_callbacks.py) · [Lua](function_callbacks.lua) | 12,519.0 µs | 9,351.0 µs | 5,840.0 µs | 1.339× | 2.144× |
| `list_access` | [Tapas](list_access.tap) · [Python](list_access.py) · [Lua](list_access.lua) | 3,464.0 µs | 4,745.0 µs | 1,130.0 µs | 0.730× | 3.065× |
| `list_append` | [Tapas](list_append.tap) · [Python](list_append.py) · [Lua](list_append.lua) | 5,578.0 µs | 6,070.0 µs | 5,355.0 µs | 0.919× | 1.042× |
| `list_update` | [Tapas](list_update.tap) · [Python](list_update.py) · [Lua](list_update.lua) | 4,661.0 µs | 3,659.0 µs | 1,716.0 µs | 1.274× | 2.716× |
| `list_copy` | [Tapas](list_copy.tap) · [Python](list_copy.py) · [Lua](list_copy.lua) | 4,625.0 µs | 18,705.0 µs | 121,427.0 µs | 0.247× | 0.038× |
| `list_from_iterable` | [Tapas](list_from_iterable.tap) · [Python](list_from_iterable.py) · [Lua](list_from_iterable.lua) | 9,887.0 µs | 156,414.0 µs | 187,323.0 µs | 0.063× | 0.053× |
| `list_replicate` | [Tapas](list_replicate.tap) · [Python](list_replicate.py) · [Lua](list_replicate.lua) | 4,602.0 µs | 11,599.0 µs | 320,036.0 µs | 0.397× | 0.014× |
| `dict_insert` | [Tapas](dict_insert.tap) · [Python](dict_insert.py) · [Lua](dict_insert.lua) | 4,692.0 µs | 5,696.0 µs | 1,044.0 µs | 0.824× | 4.494× |
| `dict_lookup` | [Tapas](dict_lookup.tap) · [Python](dict_lookup.py) · [Lua](dict_lookup.lua) | 3,826.0 µs | 9,937.0 µs | 1,158.0 µs | 0.385× | 3.304× |
| `dict_update` | [Tapas](dict_update.tap) · [Python](dict_update.py) · [Lua](dict_update.lua) | 3,970.0 µs | 11,181.0 µs | 1,734.0 µs | 0.355× | 2.290× |
| `dict_delete` | [Tapas](dict_delete.tap) · [Python](dict_delete.py) · [Lua](dict_delete.lua) | 2,709.0 µs | 5,911.0 µs | 530.0 µs | 0.458× | 5.111× |
| `dict_literal` | [Tapas](dict_literal.tap) · [Python](dict_literal.py) · [Lua](dict_literal.lua) | 26,966.0 µs | 27,850.0 µs | 30,910.0 µs | 0.968× | 0.872× |
| `dict_counting` | [Tapas](dict_counting.tap) · [Python](dict_counting.py) · [Lua](dict_counting.lua) | 23,786.0 µs | 26,332.0 µs | 6,274.0 µs | 0.903× | 3.791× |
| `tail_recursion` | [Tapas](tail_recursion.tap) · [Python](tail_recursion.py) · [Lua](tail_recursion.lua) | 1,681.0 µs | 2,925.0 µs | 439.0 µs | 0.575× | 3.829× |
| `branch_logic` | [Tapas](branch_logic.tap) · [Python](branch_logic.py) · [Lua](branch_logic.lua) | 31,192.0 µs | 19,318.0 µs | 7,628.0 µs | 1.615× | 4.089× |

The geometric mean ratio for this group: Tapas/Python **0.633×**，Tapas/Lua **1.338×**.

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
