# Tapas, Python, and Lua Performance Comparison

[简体中文](README.md) | English | [Project Home](../../README_en.md)

Test date: 2026-10-09

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
| `recursive_fibonacci` | [Tapas](recursive_fibonacci.tap) · [Python](recursive_fibonacci.py) · [Lua](recursive_fibonacci.lua) | 3,151.0 µs | 3,193.0 µs | 1,575.0 µs | 0.987× | 2.001× |
| `sieve` | [Tapas](sieve.tap) · [Python](sieve.py) · [Lua](sieve.lua) | 810.0 µs | 746.0 µs | 414.0 µs | 1.086× | 1.957× |
| `merge_sort` | [Tapas](merge_sort.tap) · [Python](merge_sort.py) · [Lua](merge_sort.lua) | 4,022.0 µs | 3,561.0 µs | 5,138.0 µs | 1.129× | 0.783× |
| `n_queens` | [Tapas](n_queens.tap) · [Python](n_queens.py) · [Lua](n_queens.lua) | 119,539.0 µs | 98,467.0 µs | 49,790.0 µs | 1.214× | 2.401× |
| `longest_common_subsequence` | [Tapas](longest_common_subsequence.tap) · [Python](longest_common_subsequence.py) · [Lua](longest_common_subsequence.lua) | 12,049.0 µs | 10,188.0 µs | 15,953.0 µs | 1.183× | 0.755× |
| `matrix_multiply` | [Tapas](matrix_multiply.tap) · [Python](matrix_multiply.py) · [Lua](matrix_multiply.lua) | 2,576.0 µs | 3,610.0 µs | 1,196.0 µs | 0.714× | 2.154× |
| `sudoku` | [Tapas](sudoku.tap) · [Python](sudoku.py) · [Lua](sudoku.lua) | 339,426.0 µs | 254,848.0 µs | 125,152.0 µs | 1.332× | 2.712× |
| `dijkstra` | [Tapas](dijkstra.tap) · [Python](dijkstra.py) · [Lua](dijkstra.lua) | 15,751.0 µs | 13,550.0 µs | 7,872.0 µs | 1.162× | 2.001× |
| `optimize` | [Tapas](optimize.tap) · [Python](optimize.py) · [Lua](optimize.lua) | 16,243.0 µs | 16,087.0 µs | 16,267.0 µs | 1.010× | 0.999× |
| `kmp` | [Tapas](kmp.tap) · [Python](kmp.py) · [Lua](kmp.lua) | 19,545.0 µs | 16,420.0 µs | 41,124.0 µs | 1.190× | 0.475× |
| `k_nucleotide` | [Tapas](k_nucleotide.tap) · [Python](k_nucleotide.py) · [Lua](k_nucleotide.lua) | 148,404.0 µs | 123,846.0 µs | 71,264.0 µs | 1.198× | 2.082× |
| `binary_trees` | [Tapas](binary_trees.tap) · [Python](binary_trees.py) · [Lua](binary_trees.lua) | 558,445.0 µs | 431,646.0 µs | 754,560.0 µs | 1.294× | 0.740× |
| `lru_cache` | [Tapas](lru_cache.tap) · [Python](lru_cache.py) · [Lua](lru_cache.lua) | 36,987.0 µs | 53,117.0 µs | 44,197.0 µs | 0.696× | 0.837× |
| `huffman` | [Tapas](huffman.tap) · [Python](huffman.py) · [Lua](huffman.lua) | 1,771.0 µs | 1,350.0 µs | 1,667.0 µs | 1.312× | 1.062× |
| `fannkuch` | [Tapas](fannkuch.tap) · [Python](fannkuch.py) · [Lua](fannkuch.lua) | 34,527.0 µs | 26,448.0 µs | 24,468.0 µs | 1.305× | 1.411× |
| `n_body` | [Tapas](n_body.tap) · [Python](n_body.py) · [Lua](n_body.lua) | 18,773.0 µs | 16,361.0 µs | 9,190.0 µs | 1.147× | 2.043× |

The geometric mean ratio for this group: Tapas/Python **1.104×**，Tapas/Lua **1.350×**.

### VM Hot Paths

Each program times a loop inside a function and keeps only the measured VM operation or container access pattern in the loop body, to help isolate interpreter overhead. Modular index arithmetic, growing-integer accumulation, and module-scope variable access are kept out, so one language's unrelated strengths or weaknesses cannot leak into a single-item result. The loop bodies of `vm_hot_paths` and `float_arithmetic` are themselves the measured arithmetic workload; `function_call_baseline` shares the loop body of `function_calls`, so subtracting the two estimates direct-call overhead, and `function_callbacks` adds a higher-order callback layer.

| Benchmark | Source | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | [Tapas](vm_hot_paths.tap) · [Python](vm_hot_paths.py) · [Lua](vm_hot_paths.lua) | 69,094.0 µs | 99,681.0 µs | 35,105.0 µs | 0.693× | 1.968× |
| `float_arithmetic` | [Tapas](float_arithmetic.tap) · [Python](float_arithmetic.py) · [Lua](float_arithmetic.lua) | 19,931.0 µs | 29,526.0 µs | 7,628.0 µs | 0.675× | 2.613× |
| `function_call_baseline` | [Tapas](function_call_baseline.tap) · [Python](function_call_baseline.py) · [Lua](function_call_baseline.lua) | 2,627.0 µs | 4,243.0 µs | 941.0 µs | 0.619× | 2.792× |
| `function_calls` | [Tapas](function_calls.tap) · [Python](function_calls.py) · [Lua](function_calls.lua) | 6,269.0 µs | 6,074.0 µs | 2,808.0 µs | 1.032× | 2.233× |
| `function_callbacks` | [Tapas](function_callbacks.tap) · [Python](function_callbacks.py) · [Lua](function_callbacks.lua) | 11,419.0 µs | 9,368.0 µs | 6,089.0 µs | 1.219× | 1.875× |
| `list_access` | [Tapas](list_access.tap) · [Python](list_access.py) · [Lua](list_access.lua) | 3,585.0 µs | 4,777.0 µs | 1,134.0 µs | 0.750× | 3.161× |
| `list_append` | [Tapas](list_append.tap) · [Python](list_append.py) · [Lua](list_append.lua) | 5,974.0 µs | 6,056.0 µs | 5,297.0 µs | 0.986× | 1.128× |
| `list_update` | [Tapas](list_update.tap) · [Python](list_update.py) · [Lua](list_update.lua) | 3,275.0 µs | 3,852.0 µs | 1,715.0 µs | 0.850× | 1.910× |
| `list_copy` | [Tapas](list_copy.tap) · [Python](list_copy.py) · [Lua](list_copy.lua) | 4,474.0 µs | 18,822.0 µs | 120,442.0 µs | 0.238× | 0.037× |
| `list_from_iterable` | [Tapas](list_from_iterable.tap) · [Python](list_from_iterable.py) · [Lua](list_from_iterable.lua) | 9,881.0 µs | 170,049.0 µs | 186,695.0 µs | 0.058× | 0.053× |
| `list_replicate` | [Tapas](list_replicate.tap) · [Python](list_replicate.py) · [Lua](list_replicate.lua) | 4,586.0 µs | 11,390.0 µs | 319,469.0 µs | 0.403× | 0.014× |
| `dict_insert` | [Tapas](dict_insert.tap) · [Python](dict_insert.py) · [Lua](dict_insert.lua) | 5,136.0 µs | 5,581.0 µs | 1,024.0 µs | 0.920× | 5.016× |
| `dict_lookup` | [Tapas](dict_lookup.tap) · [Python](dict_lookup.py) · [Lua](dict_lookup.lua) | 3,814.0 µs | 9,702.0 µs | 1,157.0 µs | 0.393× | 3.296× |
| `dict_update` | [Tapas](dict_update.tap) · [Python](dict_update.py) · [Lua](dict_update.lua) | 4,175.0 µs | 11,188.0 µs | 1,723.0 µs | 0.373× | 2.423× |
| `dict_delete` | [Tapas](dict_delete.tap) · [Python](dict_delete.py) · [Lua](dict_delete.lua) | 2,969.0 µs | 5,964.0 µs | 531.0 µs | 0.498× | 5.591× |
| `dict_literal` | [Tapas](dict_literal.tap) · [Python](dict_literal.py) · [Lua](dict_literal.lua) | 15,206.0 µs | 16,859.0 µs | 26,884.0 µs | 0.902× | 0.566× |
| `dict_counting` | [Tapas](dict_counting.tap) · [Python](dict_counting.py) · [Lua](dict_counting.lua) | 15,915.0 µs | 24,998.0 µs | 5,845.0 µs | 0.637× | 2.723× |
| `tail_recursion` | [Tapas](tail_recursion.tap) · [Python](tail_recursion.py) · [Lua](tail_recursion.lua) | 1,365.0 µs | 2,924.0 µs | 428.0 µs | 0.467× | 3.189× |
| `branch_logic` | [Tapas](branch_logic.tap) · [Python](branch_logic.py) · [Lua](branch_logic.lua) | 18,606.0 µs | 19,060.0 µs | 7,028.0 µs | 0.976× | 2.647× |

The geometric mean ratio for this group: Tapas/Python **0.569×**，Tapas/Lua **1.203×**.

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
