# Tapas, Python, and Lua Performance Comparison

[简体中文](README.md) | English | [Project Home](../../README_en.md)

Test date: 2026-10-07

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
| `recursive_fibonacci` | [Tapas](recursive_fibonacci.tap) · [Python](recursive_fibonacci.py) · [Lua](recursive_fibonacci.lua) | 3,357.0 µs | 3,082.0 µs | 1,563.0 µs | 1.089× | 2.148× |
| `sieve` | [Tapas](sieve.tap) · [Python](sieve.py) · [Lua](sieve.lua) | 926.0 µs | 767.0 µs | 413.0 µs | 1.207× | 2.242× |
| `merge_sort` | [Tapas](merge_sort.tap) · [Python](merge_sort.py) · [Lua](merge_sort.lua) | 4,442.0 µs | 3,421.0 µs | 5,185.0 µs | 1.298× | 0.857× |
| `n_queens` | [Tapas](n_queens.tap) · [Python](n_queens.py) · [Lua](n_queens.lua) | 120,986.0 µs | 95,744.0 µs | 49,106.0 µs | 1.264× | 2.464× |
| `longest_common_subsequence` | [Tapas](longest_common_subsequence.tap) · [Python](longest_common_subsequence.py) · [Lua](longest_common_subsequence.lua) | 13,809.0 µs | 9,845.0 µs | 14,986.0 µs | 1.403× | 0.921× |
| `matrix_multiply` | [Tapas](matrix_multiply.tap) · [Python](matrix_multiply.py) · [Lua](matrix_multiply.lua) | 2,888.0 µs | 3,541.0 µs | 1,197.0 µs | 0.816× | 2.413× |
| `sudoku` | [Tapas](sudoku.tap) · [Python](sudoku.py) · [Lua](sudoku.lua) | 327,230.0 µs | 246,378.0 µs | 120,187.0 µs | 1.328× | 2.723× |
| `dijkstra` | [Tapas](dijkstra.tap) · [Python](dijkstra.py) · [Lua](dijkstra.lua) | 19,315.0 µs | 13,347.0 µs | 7,634.0 µs | 1.447× | 2.530× |
| `optimize` | [Tapas](optimize.tap) · [Python](optimize.py) · [Lua](optimize.lua) | 16,607.0 µs | 15,794.0 µs | 15,929.0 µs | 1.051× | 1.043× |
| `kmp` | [Tapas](kmp.tap) · [Python](kmp.py) · [Lua](kmp.lua) | 22,079.0 µs | 15,931.0 µs | 40,518.0 µs | 1.386× | 0.545× |
| `k_nucleotide` | [Tapas](k_nucleotide.tap) · [Python](k_nucleotide.py) · [Lua](k_nucleotide.lua) | 162,339.0 µs | 122,261.0 µs | 67,198.0 µs | 1.328× | 2.416× |
| `binary_trees` | [Tapas](binary_trees.tap) · [Python](binary_trees.py) · [Lua](binary_trees.lua) | 597,933.0 µs | 432,490.0 µs | 742,094.0 µs | 1.383× | 0.806× |
| `lru_cache` | [Tapas](lru_cache.tap) · [Python](lru_cache.py) · [Lua](lru_cache.lua) | 39,019.0 µs | 52,677.0 µs | 42,737.0 µs | 0.741× | 0.913× |
| `huffman` | [Tapas](huffman.tap) · [Python](huffman.py) · [Lua](huffman.lua) | 1,942.0 µs | 1,370.0 µs | 1,544.0 µs | 1.418× | 1.258× |
| `fannkuch` | [Tapas](fannkuch.tap) · [Python](fannkuch.py) · [Lua](fannkuch.lua) | 39,556.0 µs | 26,123.0 µs | 23,116.0 µs | 1.514× | 1.711× |
| `n_body` | [Tapas](n_body.tap) · [Python](n_body.py) · [Lua](n_body.lua) | 20,843.0 µs | 16,047.0 µs | 9,050.0 µs | 1.299× | 2.303× |

The geometric mean ratio for this group: Tapas/Python **1.226×**，Tapas/Lua **1.516×**.

### VM Hot Paths

Each program times a loop inside a function and keeps only the measured VM operation or container access pattern in the loop body, to help isolate interpreter overhead. Modular index arithmetic, growing-integer accumulation, and module-scope variable access are kept out, so one language's unrelated strengths or weaknesses cannot leak into a single-item result. The loop bodies of `vm_hot_paths` and `float_arithmetic` are themselves the measured arithmetic workload; `function_call_baseline` shares the loop body of `function_calls`, so subtracting the two estimates direct-call overhead, and `function_callbacks` adds a higher-order callback layer.

| Benchmark | Source | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | [Tapas](vm_hot_paths.tap) · [Python](vm_hot_paths.py) · [Lua](vm_hot_paths.lua) | 81,924.0 µs | 99,391.0 µs | 34,000.0 µs | 0.824× | 2.410× |
| `float_arithmetic` | [Tapas](float_arithmetic.tap) · [Python](float_arithmetic.py) · [Lua](float_arithmetic.lua) | 19,168.0 µs | 29,976.0 µs | 8,878.0 µs | 0.639× | 2.159× |
| `function_call_baseline` | [Tapas](function_call_baseline.tap) · [Python](function_call_baseline.py) · [Lua](function_call_baseline.lua) | 2,568.0 µs | 4,204.0 µs | 931.0 µs | 0.611× | 2.758× |
| `function_calls` | [Tapas](function_calls.tap) · [Python](function_calls.py) · [Lua](function_calls.lua) | 6,188.0 µs | 6,149.0 µs | 2,721.0 µs | 1.006× | 2.274× |
| `function_callbacks` | [Tapas](function_callbacks.tap) · [Python](function_callbacks.py) · [Lua](function_callbacks.lua) | 12,973.0 µs | 9,192.0 µs | 5,679.0 µs | 1.411× | 2.284× |
| `list_access` | [Tapas](list_access.tap) · [Python](list_access.py) · [Lua](list_access.lua) | 4,026.0 µs | 4,687.0 µs | 1,096.0 µs | 0.859× | 3.673× |
| `list_append` | [Tapas](list_append.tap) · [Python](list_append.py) · [Lua](list_append.lua) | 5,944.0 µs | 5,979.0 µs | 5,078.0 µs | 0.994× | 1.171× |
| `list_update` | [Tapas](list_update.tap) · [Python](list_update.py) · [Lua](list_update.lua) | 4,167.0 µs | 3,759.0 µs | 1,621.0 µs | 1.109× | 2.571× |
| `list_copy` | [Tapas](list_copy.tap) · [Python](list_copy.py) · [Lua](list_copy.lua) | 4,481.0 µs | 18,146.0 µs | 119,854.0 µs | 0.247× | 0.037× |
| `list_from_iterable` | [Tapas](list_from_iterable.tap) · [Python](list_from_iterable.py) · [Lua](list_from_iterable.lua) | 10,809.0 µs | 156,373.0 µs | 186,015.0 µs | 0.069× | 0.058× |
| `list_replicate` | [Tapas](list_replicate.tap) · [Python](list_replicate.py) · [Lua](list_replicate.lua) | 4,332.0 µs | 10,872.0 µs | 315,689.0 µs | 0.398× | 0.014× |
| `dict_insert` | [Tapas](dict_insert.tap) · [Python](dict_insert.py) · [Lua](dict_insert.lua) | 5,638.0 µs | 5,408.0 µs | 1,022.0 µs | 1.043× | 5.517× |
| `dict_lookup` | [Tapas](dict_lookup.tap) · [Python](dict_lookup.py) · [Lua](dict_lookup.lua) | 4,647.0 µs | 9,809.0 µs | 1,090.0 µs | 0.474× | 4.263× |
| `dict_update` | [Tapas](dict_update.tap) · [Python](dict_update.py) · [Lua](dict_update.lua) | 5,198.0 µs | 11,208.0 µs | 1,661.0 µs | 0.464× | 3.129× |
| `dict_delete` | [Tapas](dict_delete.tap) · [Python](dict_delete.py) · [Lua](dict_delete.lua) | 2,942.0 µs | 6,100.0 µs | 557.0 µs | 0.482× | 5.282× |
| `dict_literal` | [Tapas](dict_literal.tap) · [Python](dict_literal.py) · [Lua](dict_literal.lua) | 17,504.0 µs | 16,305.0 µs | 26,047.0 µs | 1.074× | 0.672× |
| `dict_counting` | [Tapas](dict_counting.tap) · [Python](dict_counting.py) · [Lua](dict_counting.lua) | 23,198.0 µs | 24,785.0 µs | 5,737.0 µs | 0.936× | 4.044× |
| `tail_recursion` | [Tapas](tail_recursion.tap) · [Python](tail_recursion.py) · [Lua](tail_recursion.lua) | 1,227.0 µs | 2,799.0 µs | 435.0 µs | 0.438× | 2.821× |
| `branch_logic` | [Tapas](branch_logic.tap) · [Python](branch_logic.py) · [Lua](branch_logic.lua) | 16,709.0 µs | 18,869.0 µs | 6,955.0 µs | 0.886× | 2.402× |

The geometric mean ratio for this group: Tapas/Python **0.624×**，Tapas/Lua **1.313×**.

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
