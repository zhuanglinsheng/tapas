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
| `recursive_fibonacci` | [Tapas](recursive_fibonacci.tap) · [Python](recursive_fibonacci.py) · [Lua](recursive_fibonacci.lua) | 3,308.0 µs | 3,204.0 µs | 1,614.0 µs | 1.032× | 2.050× |
| `sieve` | [Tapas](sieve.tap) · [Python](sieve.py) · [Lua](sieve.lua) | 844.0 µs | 745.0 µs | 419.0 µs | 1.133× | 2.014× |
| `merge_sort` | [Tapas](merge_sort.tap) · [Python](merge_sort.py) · [Lua](merge_sort.lua) | 3,813.0 µs | 3,842.0 µs | 5,740.0 µs | 0.992× | 0.664× |
| `n_queens` | [Tapas](n_queens.tap) · [Python](n_queens.py) · [Lua](n_queens.lua) | 117,598.0 µs | 98,876.0 µs | 48,159.0 µs | 1.189× | 2.442× |
| `longest_common_subsequence` | [Tapas](longest_common_subsequence.tap) · [Python](longest_common_subsequence.py) · [Lua](longest_common_subsequence.lua) | 12,249.0 µs | 10,145.0 µs | 15,710.0 µs | 1.207× | 0.780× |
| `matrix_multiply` | [Tapas](matrix_multiply.tap) · [Python](matrix_multiply.py) · [Lua](matrix_multiply.lua) | 2,571.0 µs | 3,589.0 µs | 1,304.0 µs | 0.716× | 1.972× |
| `sudoku` | [Tapas](sudoku.tap) · [Python](sudoku.py) · [Lua](sudoku.lua) | 312,263.0 µs | 248,064.0 µs | 123,435.0 µs | 1.259× | 2.530× |
| `dijkstra` | [Tapas](dijkstra.tap) · [Python](dijkstra.py) · [Lua](dijkstra.lua) | 15,872.0 µs | 13,514.0 µs | 7,823.0 µs | 1.174× | 2.029× |
| `optimize` | [Tapas](optimize.tap) · [Python](optimize.py) · [Lua](optimize.lua) | 16,446.0 µs | 15,884.0 µs | 17,281.0 µs | 1.035× | 0.952× |
| `kmp` | [Tapas](kmp.tap) · [Python](kmp.py) · [Lua](kmp.lua) | 20,031.0 µs | 16,519.0 µs | 40,702.0 µs | 1.213× | 0.492× |
| `k_nucleotide` | [Tapas](k_nucleotide.tap) · [Python](k_nucleotide.py) · [Lua](k_nucleotide.lua) | 144,844.0 µs | 127,703.0 µs | 70,636.0 µs | 1.134× | 2.051× |
| `binary_trees` | [Tapas](binary_trees.tap) · [Python](binary_trees.py) · [Lua](binary_trees.lua) | 565,062.0 µs | 439,061.0 µs | 759,778.0 µs | 1.287× | 0.744× |
| `lru_cache` | [Tapas](lru_cache.tap) · [Python](lru_cache.py) · [Lua](lru_cache.lua) | 35,905.0 µs | 53,683.0 µs | 45,713.0 µs | 0.669× | 0.785× |
| `huffman` | [Tapas](huffman.tap) · [Python](huffman.py) · [Lua](huffman.lua) | 1,908.0 µs | 1,411.0 µs | 1,695.0 µs | 1.352× | 1.126× |
| `fannkuch` | [Tapas](fannkuch.tap) · [Python](fannkuch.py) · [Lua](fannkuch.lua) | 34,070.0 µs | 26,538.0 µs | 24,062.0 µs | 1.284× | 1.416× |
| `n_body` | [Tapas](n_body.tap) · [Python](n_body.py) · [Lua](n_body.lua) | 18,698.0 µs | 16,071.0 µs | 8,714.0 µs | 1.163× | 2.146× |

The geometric mean ratio for this group: Tapas/Python **1.097×**，Tapas/Lua **1.335×**.

### VM Hot Paths

Each program times a loop inside a function and keeps only the measured VM operation or container access pattern in the loop body, to help isolate interpreter overhead. Modular index arithmetic, growing-integer accumulation, and module-scope variable access are kept out, so one language's unrelated strengths or weaknesses cannot leak into a single-item result. The loop bodies of `vm_hot_paths` and `float_arithmetic` are themselves the measured arithmetic workload; `function_call_baseline` shares the loop body of `function_calls`, so subtracting the two estimates direct-call overhead, and `function_callbacks` adds a higher-order callback layer.

| Benchmark | Source | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | [Tapas](vm_hot_paths.tap) · [Python](vm_hot_paths.py) · [Lua](vm_hot_paths.lua) | 66,664.0 µs | 102,687.0 µs | 36,355.0 µs | 0.649× | 1.834× |
| `float_arithmetic` | [Tapas](float_arithmetic.tap) · [Python](float_arithmetic.py) · [Lua](float_arithmetic.lua) | 20,227.0 µs | 30,513.0 µs | 8,269.0 µs | 0.663× | 2.446× |
| `function_call_baseline` | [Tapas](function_call_baseline.tap) · [Python](function_call_baseline.py) · [Lua](function_call_baseline.lua) | 2,445.0 µs | 4,292.0 µs | 1,014.0 µs | 0.570× | 2.411× |
| `function_calls` | [Tapas](function_calls.tap) · [Python](function_calls.py) · [Lua](function_calls.lua) | 5,782.0 µs | 6,203.0 µs | 2,957.0 µs | 0.932× | 1.955× |
| `function_callbacks` | [Tapas](function_callbacks.tap) · [Python](function_callbacks.py) · [Lua](function_callbacks.lua) | 11,074.0 µs | 9,484.0 µs | 5,977.0 µs | 1.168× | 1.853× |
| `list_access` | [Tapas](list_access.tap) · [Python](list_access.py) · [Lua](list_access.lua) | 3,837.0 µs | 4,913.0 µs | 1,195.0 µs | 0.781× | 3.211× |
| `list_append` | [Tapas](list_append.tap) · [Python](list_append.py) · [Lua](list_append.lua) | 8,238.0 µs | 6,331.0 µs | 5,507.0 µs | 1.301× | 1.496× |
| `list_update` | [Tapas](list_update.tap) · [Python](list_update.py) · [Lua](list_update.lua) | 3,401.0 µs | 3,890.0 µs | 1,723.0 µs | 0.874× | 1.974× |
| `list_copy` | [Tapas](list_copy.tap) · [Python](list_copy.py) · [Lua](list_copy.lua) | 4,715.0 µs | 18,852.0 µs | 122,006.0 µs | 0.250× | 0.039× |
| `list_from_iterable` | [Tapas](list_from_iterable.tap) · [Python](list_from_iterable.py) · [Lua](list_from_iterable.lua) | 9,851.0 µs | 160,860.0 µs | 188,792.0 µs | 0.061× | 0.052× |
| `list_replicate` | [Tapas](list_replicate.tap) · [Python](list_replicate.py) · [Lua](list_replicate.lua) | 4,531.0 µs | 11,419.0 µs | 323,755.0 µs | 0.397× | 0.014× |
| `dict_insert` | [Tapas](dict_insert.tap) · [Python](dict_insert.py) · [Lua](dict_insert.lua) | 4,822.0 µs | 5,932.0 µs | 1,152.0 µs | 0.813× | 4.186× |
| `dict_lookup` | [Tapas](dict_lookup.tap) · [Python](dict_lookup.py) · [Lua](dict_lookup.lua) | 3,862.0 µs | 9,969.0 µs | 1,152.0 µs | 0.387× | 3.352× |
| `dict_update` | [Tapas](dict_update.tap) · [Python](dict_update.py) · [Lua](dict_update.lua) | 4,158.0 µs | 11,214.0 µs | 1,712.0 µs | 0.371× | 2.429× |
| `dict_delete` | [Tapas](dict_delete.tap) · [Python](dict_delete.py) · [Lua](dict_delete.lua) | 3,008.0 µs | 5,979.0 µs | 514.0 µs | 0.503× | 5.852× |
| `dict_literal` | [Tapas](dict_literal.tap) · [Python](dict_literal.py) · [Lua](dict_literal.lua) | 16,029.0 µs | 17,702.0 µs | 27,535.0 µs | 0.905× | 0.582× |
| `dict_counting` | [Tapas](dict_counting.tap) · [Python](dict_counting.py) · [Lua](dict_counting.lua) | 15,577.0 µs | 25,112.0 µs | 5,907.0 µs | 0.620× | 2.637× |
| `tail_recursion` | [Tapas](tail_recursion.tap) · [Python](tail_recursion.py) · [Lua](tail_recursion.lua) | 1,395.0 µs | 2,998.0 µs | 449.0 µs | 0.465× | 3.107× |
| `branch_logic` | [Tapas](branch_logic.tap) · [Python](branch_logic.py) · [Lua](branch_logic.lua) | 19,400.0 µs | 19,485.0 µs | 7,223.0 µs | 0.996× | 2.686× |

The geometric mean ratio for this group: Tapas/Python **0.569×**，Tapas/Lua **1.189×**.

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
