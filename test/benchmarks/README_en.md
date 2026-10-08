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
| `recursive_fibonacci` | [Tapas](recursive_fibonacci.tap) · [Python](recursive_fibonacci.py) · [Lua](recursive_fibonacci.lua) | 3,833.0 µs | 3,438.0 µs | 1,712.0 µs | 1.115× | 2.239× |
| `sieve` | [Tapas](sieve.tap) · [Python](sieve.py) · [Lua](sieve.lua) | 927.0 µs | 828.0 µs | 469.0 µs | 1.120× | 1.977× |
| `merge_sort` | [Tapas](merge_sort.tap) · [Python](merge_sort.py) · [Lua](merge_sort.lua) | 4,428.0 µs | 3,742.0 µs | 5,628.0 µs | 1.183× | 0.787× |
| `n_queens` | [Tapas](n_queens.tap) · [Python](n_queens.py) · [Lua](n_queens.lua) | 124,235.0 µs | 102,229.0 µs | 53,244.0 µs | 1.215× | 2.333× |
| `longest_common_subsequence` | [Tapas](longest_common_subsequence.tap) · [Python](longest_common_subsequence.py) · [Lua](longest_common_subsequence.lua) | 14,203.0 µs | 10,462.0 µs | 16,120.0 µs | 1.358× | 0.881× |
| `matrix_multiply` | [Tapas](matrix_multiply.tap) · [Python](matrix_multiply.py) · [Lua](matrix_multiply.lua) | 2,703.0 µs | 3,817.0 µs | 1,333.0 µs | 0.708× | 2.028× |
| `sudoku` | [Tapas](sudoku.tap) · [Python](sudoku.py) · [Lua](sudoku.lua) | 333,065.0 µs | 257,030.0 µs | 132,499.0 µs | 1.296× | 2.514× |
| `dijkstra` | [Tapas](dijkstra.tap) · [Python](dijkstra.py) · [Lua](dijkstra.lua) | 17,887.0 µs | 13,846.0 µs | 8,285.0 µs | 1.292× | 2.159× |
| `optimize` | [Tapas](optimize.tap) · [Python](optimize.py) · [Lua](optimize.lua) | 16,633.0 µs | 16,409.0 µs | 16,981.0 µs | 1.014× | 0.980× |
| `kmp` | [Tapas](kmp.tap) · [Python](kmp.py) · [Lua](kmp.lua) | 27,055.0 µs | 19,090.0 µs | 51,458.0 µs | 1.417× | 0.526× |
| `k_nucleotide` | [Tapas](k_nucleotide.tap) · [Python](k_nucleotide.py) · [Lua](k_nucleotide.lua) | 186,076.0 µs | 160,602.0 µs | 89,326.0 µs | 1.159× | 2.083× |
| `binary_trees` | [Tapas](binary_trees.tap) · [Python](binary_trees.py) · [Lua](binary_trees.lua) | 721,546.0 µs | 509,567.0 µs | 816,692.0 µs | 1.416× | 0.883× |
| `lru_cache` | [Tapas](lru_cache.tap) · [Python](lru_cache.py) · [Lua](lru_cache.lua) | 45,673.0 µs | 63,972.0 µs | 62,641.0 µs | 0.714× | 0.729× |
| `huffman` | [Tapas](huffman.tap) · [Python](huffman.py) · [Lua](huffman.lua) | 1,964.0 µs | 1,601.0 µs | 2,477.0 µs | 1.227× | 0.793× |
| `fannkuch` | [Tapas](fannkuch.tap) · [Python](fannkuch.py) · [Lua](fannkuch.lua) | 42,976.0 µs | 30,687.0 µs | 28,289.0 µs | 1.400× | 1.519× |
| `n_body` | [Tapas](n_body.tap) · [Python](n_body.py) · [Lua](n_body.lua) | 19,885.0 µs | 21,324.0 µs | 11,431.0 µs | 0.933× | 1.740× |

The geometric mean ratio for this group: Tapas/Python **1.137×**，Tapas/Lua **1.345×**.

### VM Hot Paths

Each program times a loop inside a function and keeps only the measured VM operation or container access pattern in the loop body, to help isolate interpreter overhead. Modular index arithmetic, growing-integer accumulation, and module-scope variable access are kept out, so one language's unrelated strengths or weaknesses cannot leak into a single-item result. The loop bodies of `vm_hot_paths` and `float_arithmetic` are themselves the measured arithmetic workload; `function_call_baseline` shares the loop body of `function_calls`, so subtracting the two estimates direct-call overhead, and `function_callbacks` adds a higher-order callback layer.

| Benchmark | Source | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | [Tapas](vm_hot_paths.tap) · [Python](vm_hot_paths.py) · [Lua](vm_hot_paths.lua) | 75,126.0 µs | 123,425.0 µs | 47,291.0 µs | 0.609× | 1.589× |
| `float_arithmetic` | [Tapas](float_arithmetic.tap) · [Python](float_arithmetic.py) · [Lua](float_arithmetic.lua) | 26,404.0 µs | 35,467.0 µs | 10,997.0 µs | 0.744× | 2.401× |
| `function_call_baseline` | [Tapas](function_call_baseline.tap) · [Python](function_call_baseline.py) · [Lua](function_call_baseline.lua) | 3,804.0 µs | 4,515.0 µs | 1,042.0 µs | 0.843× | 3.651× |
| `function_calls` | [Tapas](function_calls.tap) · [Python](function_calls.py) · [Lua](function_calls.lua) | 6,797.0 µs | 6,784.0 µs | 3,067.0 µs | 1.002× | 2.216× |
| `function_callbacks` | [Tapas](function_callbacks.tap) · [Python](function_callbacks.py) · [Lua](function_callbacks.lua) | 17,740.0 µs | 13,451.0 µs | 6,873.0 µs | 1.319× | 2.581× |
| `list_access` | [Tapas](list_access.tap) · [Python](list_access.py) · [Lua](list_access.lua) | 5,456.0 µs | 4,971.0 µs | 1,186.0 µs | 1.098× | 4.600× |
| `list_append` | [Tapas](list_append.tap) · [Python](list_append.py) · [Lua](list_append.lua) | 9,031.0 µs | 8,818.0 µs | 7,791.0 µs | 1.024× | 1.159× |
| `list_update` | [Tapas](list_update.tap) · [Python](list_update.py) · [Lua](list_update.lua) | 5,832.0 µs | 5,566.0 µs | 1,811.0 µs | 1.048× | 3.220× |
| `list_copy` | [Tapas](list_copy.tap) · [Python](list_copy.py) · [Lua](list_copy.lua) | 5,373.0 µs | 23,387.0 µs | 155,718.0 µs | 0.230× | 0.035× |
| `list_from_iterable` | [Tapas](list_from_iterable.tap) · [Python](list_from_iterable.py) · [Lua](list_from_iterable.lua) | 10,880.0 µs | 189,594.0 µs | 222,134.0 µs | 0.057× | 0.049× |
| `list_replicate` | [Tapas](list_replicate.tap) · [Python](list_replicate.py) · [Lua](list_replicate.lua) | 9,526.0 µs | 16,225.0 µs | 391,620.0 µs | 0.587× | 0.024× |
| `dict_insert` | [Tapas](dict_insert.tap) · [Python](dict_insert.py) · [Lua](dict_insert.lua) | 7,093.0 µs | 7,400.0 µs | 1,486.0 µs | 0.959× | 4.773× |
| `dict_lookup` | [Tapas](dict_lookup.tap) · [Python](dict_lookup.py) · [Lua](dict_lookup.lua) | 5,723.0 µs | 11,198.0 µs | 1,532.0 µs | 0.511× | 3.736× |
| `dict_update` | [Tapas](dict_update.tap) · [Python](dict_update.py) · [Lua](dict_update.lua) | 5,078.0 µs | 14,263.0 µs | 1,826.0 µs | 0.356× | 2.781× |
| `dict_delete` | [Tapas](dict_delete.tap) · [Python](dict_delete.py) · [Lua](dict_delete.lua) | 3,101.0 µs | 6,167.0 µs | 878.0 µs | 0.503× | 3.532× |
| `dict_literal` | [Tapas](dict_literal.tap) · [Python](dict_literal.py) · [Lua](dict_literal.lua) | 18,303.0 µs | 18,131.0 µs | 32,262.0 µs | 1.009× | 0.567× |
| `dict_counting` | [Tapas](dict_counting.tap) · [Python](dict_counting.py) · [Lua](dict_counting.lua) | 17,774.0 µs | 31,532.0 µs | 6,124.0 µs | 0.564× | 2.902× |
| `tail_recursion` | [Tapas](tail_recursion.tap) · [Python](tail_recursion.py) · [Lua](tail_recursion.lua) | 1,446.0 µs | 3,305.0 µs | 483.0 µs | 0.438× | 2.994× |
| `branch_logic` | [Tapas](branch_logic.tap) · [Python](branch_logic.py) · [Lua](branch_logic.lua) | 19,059.0 µs | 21,107.0 µs | 10,410.0 µs | 0.903× | 1.831× |

The geometric mean ratio for this group: Tapas/Python **0.613×**，Tapas/Lua **1.266×**.

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
