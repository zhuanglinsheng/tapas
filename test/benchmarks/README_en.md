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
| `recursive_fibonacci` | [Tapas](recursive_fibonacci.tap) · [Python](recursive_fibonacci.py) · [Lua](recursive_fibonacci.lua) | 3,612.0 µs | 3,280.0 µs | 1,636.0 µs | 1.101× | 2.208× |
| `sieve` | [Tapas](sieve.tap) · [Python](sieve.py) · [Lua](sieve.lua) | 812.0 µs | 783.0 µs | 435.0 µs | 1.037× | 1.867× |
| `merge_sort` | [Tapas](merge_sort.tap) · [Python](merge_sort.py) · [Lua](merge_sort.lua) | 3,958.0 µs | 3,678.0 µs | 5,398.0 µs | 1.076× | 0.733× |
| `n_queens` | [Tapas](n_queens.tap) · [Python](n_queens.py) · [Lua](n_queens.lua) | 117,427.0 µs | 100,047.0 µs | 51,187.0 µs | 1.174× | 2.294× |
| `longest_common_subsequence` | [Tapas](longest_common_subsequence.tap) · [Python](longest_common_subsequence.py) · [Lua](longest_common_subsequence.lua) | 12,874.0 µs | 10,241.0 µs | 15,659.0 µs | 1.257× | 0.822× |
| `matrix_multiply` | [Tapas](matrix_multiply.tap) · [Python](matrix_multiply.py) · [Lua](matrix_multiply.lua) | 2,608.0 µs | 3,624.0 µs | 1,229.0 µs | 0.720× | 2.122× |
| `sudoku` | [Tapas](sudoku.tap) · [Python](sudoku.py) · [Lua](sudoku.lua) | 315,528.0 µs | 252,733.0 µs | 139,117.0 µs | 1.248× | 2.268× |
| `dijkstra` | [Tapas](dijkstra.tap) · [Python](dijkstra.py) · [Lua](dijkstra.lua) | 17,982.0 µs | 14,426.0 µs | 8,405.0 µs | 1.246× | 2.139× |
| `optimize` | [Tapas](optimize.tap) · [Python](optimize.py) · [Lua](optimize.lua) | 16,290.0 µs | 16,736.0 µs | 17,469.0 µs | 0.973× | 0.933× |
| `kmp` | [Tapas](kmp.tap) · [Python](kmp.py) · [Lua](kmp.lua) | 20,226.0 µs | 16,844.0 µs | 41,933.0 µs | 1.201× | 0.482× |
| `k_nucleotide` | [Tapas](k_nucleotide.tap) · [Python](k_nucleotide.py) · [Lua](k_nucleotide.lua) | 151,358.0 µs | 125,156.0 µs | 71,769.0 µs | 1.209× | 2.109× |
| `binary_trees` | [Tapas](binary_trees.tap) · [Python](binary_trees.py) · [Lua](binary_trees.lua) | 613,388.0 µs | 466,537.0 µs | 766,672.0 µs | 1.315× | 0.800× |
| `lru_cache` | [Tapas](lru_cache.tap) · [Python](lru_cache.py) · [Lua](lru_cache.lua) | 37,083.0 µs | 53,487.0 µs | 44,044.0 µs | 0.693× | 0.842× |
| `huffman` | [Tapas](huffman.tap) · [Python](huffman.py) · [Lua](huffman.lua) | 1,780.0 µs | 1,411.0 µs | 1,726.0 µs | 1.262× | 1.031× |
| `fannkuch` | [Tapas](fannkuch.tap) · [Python](fannkuch.py) · [Lua](fannkuch.lua) | 34,679.0 µs | 26,973.0 µs | 25,121.0 µs | 1.286× | 1.380× |
| `n_body` | [Tapas](n_body.tap) · [Python](n_body.py) · [Lua](n_body.lua) | 19,037.0 µs | 16,117.0 µs | 9,381.0 µs | 1.181× | 2.029× |

The geometric mean ratio for this group: Tapas/Python **1.106×**，Tapas/Lua **1.340×**.

### VM Hot Paths

Each program times a loop inside a function and keeps only the measured VM operation or container access pattern in the loop body, to help isolate interpreter overhead. Modular index arithmetic, growing-integer accumulation, and module-scope variable access are kept out, so one language's unrelated strengths or weaknesses cannot leak into a single-item result. The loop bodies of `vm_hot_paths` and `float_arithmetic` are themselves the measured arithmetic workload; `function_call_baseline` shares the loop body of `function_calls`, so subtracting the two estimates direct-call overhead, and `function_callbacks` adds a higher-order callback layer.

| Benchmark | Source | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | [Tapas](vm_hot_paths.tap) · [Python](vm_hot_paths.py) · [Lua](vm_hot_paths.lua) | 65,587.0 µs | 103,733.0 µs | 37,721.0 µs | 0.632× | 1.739× |
| `float_arithmetic` | [Tapas](float_arithmetic.tap) · [Python](float_arithmetic.py) · [Lua](float_arithmetic.lua) | 21,243.0 µs | 29,506.0 µs | 9,429.0 µs | 0.720× | 2.253× |
| `function_call_baseline` | [Tapas](function_call_baseline.tap) · [Python](function_call_baseline.py) · [Lua](function_call_baseline.lua) | 2,359.0 µs | 4,316.0 µs | 983.0 µs | 0.547× | 2.400× |
| `function_calls` | [Tapas](function_calls.tap) · [Python](function_calls.py) · [Lua](function_calls.lua) | 6,379.0 µs | 6,361.0 µs | 2,911.0 µs | 1.003× | 2.191× |
| `function_callbacks` | [Tapas](function_callbacks.tap) · [Python](function_callbacks.py) · [Lua](function_callbacks.lua) | 11,254.0 µs | 9,381.0 µs | 5,994.0 µs | 1.200× | 1.878× |
| `list_access` | [Tapas](list_access.tap) · [Python](list_access.py) · [Lua](list_access.lua) | 3,555.0 µs | 4,773.0 µs | 1,140.0 µs | 0.745× | 3.118× |
| `list_append` | [Tapas](list_append.tap) · [Python](list_append.py) · [Lua](list_append.lua) | 5,966.0 µs | 6,086.0 µs | 5,559.0 µs | 0.980× | 1.073× |
| `list_update` | [Tapas](list_update.tap) · [Python](list_update.py) · [Lua](list_update.lua) | 3,515.0 µs | 4,147.0 µs | 1,740.0 µs | 0.848× | 2.020× |
| `list_copy` | [Tapas](list_copy.tap) · [Python](list_copy.py) · [Lua](list_copy.lua) | 4,481.0 µs | 18,594.0 µs | 122,742.0 µs | 0.241× | 0.037× |
| `list_from_iterable` | [Tapas](list_from_iterable.tap) · [Python](list_from_iterable.py) · [Lua](list_from_iterable.lua) | 9,836.0 µs | 157,859.0 µs | 188,823.0 µs | 0.062× | 0.052× |
| `list_replicate` | [Tapas](list_replicate.tap) · [Python](list_replicate.py) · [Lua](list_replicate.lua) | 4,626.0 µs | 11,618.0 µs | 322,481.0 µs | 0.398× | 0.014× |
| `dict_insert` | [Tapas](dict_insert.tap) · [Python](dict_insert.py) · [Lua](dict_insert.lua) | 5,131.0 µs | 5,836.0 µs | 1,159.0 µs | 0.879× | 4.427× |
| `dict_lookup` | [Tapas](dict_lookup.tap) · [Python](dict_lookup.py) · [Lua](dict_lookup.lua) | 3,808.0 µs | 9,906.0 µs | 1,146.0 µs | 0.384× | 3.323× |
| `dict_update` | [Tapas](dict_update.tap) · [Python](dict_update.py) · [Lua](dict_update.lua) | 4,183.0 µs | 11,413.0 µs | 1,747.0 µs | 0.367× | 2.394× |
| `dict_delete` | [Tapas](dict_delete.tap) · [Python](dict_delete.py) · [Lua](dict_delete.lua) | 3,027.0 µs | 6,018.0 µs | 531.0 µs | 0.503× | 5.701× |
| `dict_literal` | [Tapas](dict_literal.tap) · [Python](dict_literal.py) · [Lua](dict_literal.lua) | 16,023.0 µs | 17,364.0 µs | 27,299.0 µs | 0.923× | 0.587× |
| `dict_counting` | [Tapas](dict_counting.tap) · [Python](dict_counting.py) · [Lua](dict_counting.lua) | 15,444.0 µs | 25,293.0 µs | 5,924.0 µs | 0.611× | 2.607× |
| `tail_recursion` | [Tapas](tail_recursion.tap) · [Python](tail_recursion.py) · [Lua](tail_recursion.lua) | 1,383.0 µs | 2,955.0 µs | 471.0 µs | 0.468× | 2.936× |
| `branch_logic` | [Tapas](branch_logic.tap) · [Python](branch_logic.py) · [Lua](branch_logic.lua) | 17,892.0 µs | 19,132.0 µs | 7,249.0 µs | 0.935× | 2.468× |

The geometric mean ratio for this group: Tapas/Python **0.561×**，Tapas/Lua **1.157×**.

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
