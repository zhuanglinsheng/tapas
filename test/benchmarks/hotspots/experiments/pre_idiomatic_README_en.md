# Tapas, Python, and Lua Performance Comparison

[简体中文](../../README.md) | English | [Project Home](../../../../README_en.md)

Test date: 2026-10-17

## Test Environment

- System: `macOS-27.0.1-arm64-arm-64bit-Mach-O`
- Architecture: `arm64`
- Python: `3.14.2`
- Tapas: `Tapas 0.1.0 Copyright (C) 2020-2026`
- Lua interpreter: `Lua 5.5.1  Copyright (C) 1994-2026 Lua.org, PUC-Rio`
- Tapas executable: `/Users/zhuanglinsheng/Documents/Github/tapas/build-release/bin/tapas`
- Measured runs: 7 per benchmark, after 1 warm-up run
- Build options: `CMAKE_BUILD_TYPE:STRING=Release`
- Build options: `CMAKE_C_COMPILER:FILEPATH=/usr/bin/cc`
- Build options: `CMAKE_C_FLAGS:STRING=`
- Build options: `CMAKE_C_FLAGS_RELEASE:STRING=-O3 -DNDEBUG`
- Build options: `CMAKE_EXE_LINKER_FLAGS:STRING=`
- Build options: `TAPAS_ENABLE_IPO:BOOL=ON`

## Results

Times are median process CPU times in microseconds and exclude process startup, source loading, and compilation.
A ratio below 1 means Tapas is faster (against Python and Lua respectively).

### Complete Algorithms

Keep the specified algorithm while allowing idiomatic containers and standard-library bulk operations. Inputs, repetitions, outputs, and timing boundaries are equivalent; line-by-line translations are not required. See SEMANTICS.md for the contracts.

| Benchmark | Source | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `recursive_fibonacci` | [Tapas](../../recursive_fibonacci.tap) · [Python](../../recursive_fibonacci.py) · [Lua](../../recursive_fibonacci.lua) | 4,211.0 µs | 5,399.0 µs | 2,626.0 µs | 0.780× | 1.604× |
| `sieve` | [Tapas](../../sieve.tap) · [Python](../../sieve.py) · [Lua](../../sieve.lua) | 85.0 µs | 51.0 µs | 592.0 µs | 1.667× | 0.144× |
| `merge_sort` | [Tapas](../../merge_sort.tap) · [Python](../../merge_sort.py) · [Lua](../../merge_sort.lua) | 4,788.0 µs | 4,548.0 µs | 5,429.0 µs | 1.053× | 0.882× |
| `n_queens` | [Tapas](../../n_queens.tap) · [Python](../../n_queens.py) · [Lua](../../n_queens.lua) | 67,979.0 µs | 79,288.0 µs | 47,760.0 µs | 0.857× | 1.423× |
| `longest_common_subsequence` | [Tapas](../../longest_common_subsequence.tap) · [Python](../../longest_common_subsequence.py) · [Lua](../../longest_common_subsequence.lua) | 14,574.0 µs | 10,456.0 µs | 12,024.0 µs | 1.394× | 1.212× |
| `matrix_multiply` | [Tapas](../../matrix_multiply.tap) · [Python](../../matrix_multiply.py) · [Lua](../../matrix_multiply.lua) | 3,033.0 µs | 2,987.0 µs | 1,212.0 µs | 1.015× | 2.502× |
| `sudoku` | [Tapas](../../sudoku.tap) · [Python](../../sudoku.py) · [Lua](../../sudoku.lua) | 359,834.0 µs | 297,171.0 µs | 154,726.0 µs | 1.211× | 2.326× |
| `dijkstra` | [Tapas](../../dijkstra.tap) · [Python](../../dijkstra.py) · [Lua](../../dijkstra.lua) | 6,948.0 µs | 3,353.0 µs | 5,855.0 µs | 2.072× | 1.187× |
| `optimize` | [Tapas](../../optimize.tap) · [Python](../../optimize.py) · [Lua](../../optimize.lua) | 18,310.0 µs | 20,175.0 µs | 13,996.0 µs | 0.908× | 1.308× |
| `kmp` | [Tapas](../../kmp.tap) · [Python](../../kmp.py) · [Lua](../../kmp.lua) | 20,850.0 µs | 18,953.0 µs | 28,188.0 µs | 1.100× | 0.740× |
| `k_nucleotide` | [Tapas](../../k_nucleotide.tap) · [Python](../../k_nucleotide.py) · [Lua](../../k_nucleotide.lua) | 108,093.0 µs | 125,495.0 µs | 68,536.0 µs | 0.861× | 1.577× |
| `binary_trees` | [Tapas](../../binary_trees.tap) · [Python](../../binary_trees.py) · [Lua](../../binary_trees.lua) | 726,909.0 µs | 554,558.0 µs | 858,053.0 µs | 1.311× | 0.847× |
| `lru_cache` | [Tapas](../../lru_cache.tap) · [Python](../../lru_cache.py) · [Lua](../../lru_cache.lua) | 42,292.0 µs | 70,769.0 µs | 35,107.0 µs | 0.598× | 1.205× |
| `huffman` | [Tapas](../../huffman.tap) · [Python](../../huffman.py) · [Lua](../../huffman.lua) | 2,083.0 µs | 1,143.0 µs | 1,567.0 µs | 1.822× | 1.329× |
| `fannkuch` | [Tapas](../../fannkuch.tap) · [Python](../../fannkuch.py) · [Lua](../../fannkuch.lua) | 39,583.0 µs | 38,017.0 µs | 28,079.0 µs | 1.041× | 1.410× |
| `n_body` | [Tapas](../../n_body.tap) · [Python](../../n_body.py) · [Lua](../../n_body.lua) | 24,064.0 µs | 20,593.0 µs | 10,423.0 µs | 1.169× | 2.309× |

The geometric mean ratio for this group: Tapas/Python **1.121×** (16 cases), Tapas/Lua **1.191×** (16 cases).

### VM Hot Paths

This table automatically includes every Tapas/Python case in `hotspots/`: basic VM operations, algorithm-derived composite workloads, and negative controls. The Python column runs each file's default implementation; new diagnostic cases use idiomatic optimized references. Eight Python structural controls are reported separately below. Missing Lua implementations are shown as — and excluded from the Lua geometric mean; each mean states its actual sample count. `function_call_baseline` and `function_calls` share a loop body to estimate direct-call overhead; `function_callbacks` adds a callback layer.

| Benchmark | Measured workload | Source | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | kernel: integer loop, modulo and branches | [Tapas](../../hotspots/vm_hot_paths.tap) · [Python](../../hotspots/vm_hot_paths.py) · [Lua](../../hotspots/vm_hot_paths.lua) | 74,879.0 µs | 139,434.0 µs | 43,829.0 µs | 0.537× | 1.708× |
| `float_arithmetic` | kernel: dependent floating-point arithmetic | [Tapas](../../hotspots/float_arithmetic.tap) · [Python](../../hotspots/float_arithmetic.py) · [Lua](../../hotspots/float_arithmetic.lua) | 21,993.0 µs | 42,066.0 µs | 10,545.0 µs | 0.523× | 2.086× |
| `function_call_baseline` | kernel: call control: loop, modulo and addition | [Tapas](../../hotspots/function_call_baseline.tap) · [Python](../../hotspots/function_call_baseline.py) · [Lua](../../hotspots/function_call_baseline.lua) | 2,686.0 µs | 6,087.0 µs | 1,200.0 µs | 0.441× | 2.238× |
| `function_calls` | kernel: direct calls to a fixed scalar function | [Tapas](../../hotspots/function_calls.tap) · [Python](../../hotspots/function_calls.py) · [Lua](../../hotspots/function_calls.lua) | 6,733.0 µs | 9,100.0 µs | 3,621.0 µs | 0.740× | 1.859× |
| `function_callbacks` | kernel: a fixed callback through a wrapper | [Tapas](../../hotspots/function_callbacks.tap) · [Python](../../hotspots/function_callbacks.py) · [Lua](../../hotspots/function_callbacks.lua) | 12,336.0 µs | 12,601.0 µs | 5,798.0 µs | 0.979× | 2.128× |
| `list_access` | kernel: sequential list reads and loop overhead | [Tapas](../../hotspots/list_access.tap) · [Python](../../hotspots/list_access.py) · [Lua](../../hotspots/list_access.lua) | 6,316.0 µs | 6,928.0 µs | 1,852.0 µs | 0.912× | 3.410× |
| `list_append` | kernel: element-wise append and growth | [Tapas](../../hotspots/list_append.tap) · [Python](../../hotspots/list_append.py) · [Lua](../../hotspots/list_append.lua) | 7,567.0 µs | 9,449.0 µs | 6,776.0 µs | 0.801× | 1.117× |
| `list_update` | kernel: overwriting existing list slots | [Tapas](../../hotspots/list_update.tap) · [Python](../../hotspots/list_update.py) · [Lua](../../hotspots/list_update.lua) | 4,859.0 µs | 6,123.0 µs | 2,406.0 µs | 0.794× | 2.020× |
| `list_copy` | task: full shallow copy, allocation and reclamation | [Tapas](../../hotspots/list_copy.tap) · [Python](../../hotspots/list_copy.py) · [Lua](../../hotspots/list_copy.lua) | 4,839.0 µs | 21,497.0 µs | 152,789.0 µs | 0.225× | 0.032× |
| `list_from_iterable` | task: materializing an integer range | [Tapas](../../hotspots/list_from_iterable.tap) · [Python](../../hotspots/list_from_iterable.py) · [Lua](../../hotspots/list_from_iterable.lua) | 10,225.0 µs | 215,308.0 µs | 176,555.0 µs | 0.047× | 0.058× |
| `list_replicate` | task: constructing a repeated sequence | [Tapas](../../hotspots/list_replicate.tap) · [Python](../../hotspots/list_replicate.py) · [Lua](../../hotspots/list_replicate.lua) | 5,428.0 µs | 13,848.0 µs | 163,716.0 µs | 0.392× | 0.033× |
| `dict_insert` | kernel: sequential integer-key insertion and growth | [Tapas](../../hotspots/dict_insert.tap) · [Python](../../hotspots/dict_insert.py) · [Lua](../../hotspots/dict_insert.lua) | 7,355.0 µs | 8,137.0 µs | 1,243.0 µs | 0.904× | 5.917× |
| `dict_lookup` | kernel: sequential existing integer-key lookup | [Tapas](../../hotspots/dict_lookup.tap) · [Python](../../hotspots/dict_lookup.py) · [Lua](../../hotspots/dict_lookup.lua) | 5,867.0 µs | 15,598.0 µs | 1,848.0 µs | 0.376× | 3.175× |
| `dict_update` | kernel: scalar overwrite at existing integer keys | [Tapas](../../hotspots/dict_update.tap) · [Python](../../hotspots/dict_update.py) · [Lua](../../hotspots/dict_update.lua) | 4,736.0 µs | 19,827.0 µs | 1,952.0 µs | 0.239× | 2.426× |
| `dict_delete` | kernel: element-wise deletion of existing integer keys | [Tapas](../../hotspots/dict_delete.tap) · [Python](../../hotspots/dict_delete.py) · [Lua](../../hotspots/dict_delete.lua) | 3,490.0 µs | 8,042.0 µs | 771.0 µs | 0.434× | 4.527× |
| `dict_literal` | kernel: small dictionary construction, read and reclamation | [Tapas](../../hotspots/dict_literal.tap) · [Python](../../hotspots/dict_literal.py) · [Lua](../../hotspots/dict_literal.lua) | 16,894.0 µs | 22,800.0 µs | 29,042.0 µs | 0.741× | 0.582× |
| `dict_counting` | kernel: explicit loop: default lookup, increment and write | [Tapas](../../hotspots/dict_counting.tap) · [Python](../../hotspots/dict_counting.py) · [Lua](../../hotspots/dict_counting.lua) | 19,907.0 µs | 31,192.0 µs | 5,911.0 µs | 0.638× | 3.368× |
| `tail_recursion` | semantics: tail-call behavior and frame management | [Tapas](../../hotspots/tail_recursion.tap) · [Python](../../hotspots/tail_recursion.py) · [Lua](../../hotspots/tail_recursion.lua) | 1,782.0 µs | 3,973.0 µs | 701.0 µs | 0.449× | 2.542× |
| `branch_logic` | kernel: short-circuit branches over a fixed state sequence | [Tapas](../../hotspots/branch_logic.tap) · [Python](../../hotspots/branch_logic.py) · [Lua](../../hotspots/branch_logic.lua) | 21,861.0 µs | 27,470.0 µs | 10,862.0 µs | 0.796× | 2.013× |
| `constraint_probe` | kernel: Sudoku predicate indexing, comparison and early exit | [Tapas](../../hotspots/constraint_probe.tap) · [Python](../../hotspots/constraint_probe.py) · [Lua](../../hotspots/constraint_probe.lua) | 69,561.0 µs | 79,094.0 µs | 33,325.0 µs | 0.879× | 2.087× |
| `dp_row` | kernel: one LCS row update from a valid predecessor row | [Tapas](../../hotspots/dp_row.tap) · [Python](../../hotspots/dp_row.py) · [Lua](../../hotspots/dp_row.lua) | 40,786.0 µs | 30,194.0 µs | 30,844.0 µs | 1.351× | 1.322× |
| `heap_callback` | task: priority-record heap comparator callbacks, including copying, heapify and ordered draining | [Tapas](../../hotspots/heap_callback.tap) · [Python](../../hotspots/heap_callback.py) · [Lua](../../hotspots/heap_callback.lua) | 8,759.0 µs | 4,050.0 µs | 8,083.0 µs | 2.163× | 1.084× |
| `record_update` | representation: fixed-field updates and object representation | [Tapas](../../hotspots/record_update.tap) · [Python](../../hotspots/record_update.py) · [Lua](../../hotspots/record_update.lua) | 10,673.0 µs | 12,490.0 µs | 7,838.0 µs | 0.855× | 1.362× |
| `recursive_loops` | task: short-sequence sums in a recursion tree | [Tapas](../../hotspots/recursive_loops.tap) · [Python](../../hotspots/recursive_loops.py) · [Lua](../../hotspots/recursive_loops.lua) | 15,315.0 µs | 11,924.0 µs | 8,511.0 µs | 1.284× | 1.799× |
| `string_count` | task: complete character histogram | [Tapas](../../hotspots/string_count.tap) · [Python](../../hotspots/string_count.py) · [Lua](../../hotspots/string_count.lua) | 24,426.0 µs | 12,918.0 µs | 20,601.0 µs | 1.891× | 1.186× |
| `substring_count` | task: slicing, hashing and overlapping substring counts | [Tapas](../../hotspots/substring_count.tap) · [Python](../../hotspots/substring_count.py) · [Lua](../../hotspots/substring_count.lua) | 18,871.0 µs | 21,615.0 µs | 16,418.0 µs | 0.873× | 1.149× |
| `tree_build` | representation: recursive read-only tree construction and lifetimes | [Tapas](../../hotspots/tree_build.tap) · [Python](../../hotspots/tree_build.py) · [Lua](../../hotspots/tree_build.lua) | 29,378.0 µs | 24,736.0 µs | 69,868.0 µs | 1.188× | 0.420× |
| `while_search` | task: linear search for the first matching position | [Tapas](../../hotspots/while_search.tap) · [Python](../../hotspots/while_search.py) · [Lua](../../hotspots/while_search.lua) | 52,936.0 µs | 27,893.0 µs | 28,182.0 µs | 1.898× | 1.878× |

The geometric mean ratio for this group: Tapas/Python **0.670×** (28 cases), Tapas/Lua **1.216×** (28 cases).

Kernels, tasks, representations and semantic cases measure different layers; this group mean is not a pure VM score. See the [case audit](../../hotspots/AUDIT.md).

### Python structural controls

The main table keeps idiomatic defaults. These additional explicit-loop or conventional-object controls expose bulk-operation, iteration and layout effects; they do not isolate a VM instruction cost. Both Python variants must agree. Controls are excluded from all geometric means.

| Case | Python default | Python structural control | Tapas/default | Tapas/control |
| --- | ---: | ---: | ---: | ---: |
| `dp_row` | 30,194.0 µs | 32,385.0 µs | 1.351× | 1.259× |
| `heap_callback` | 4,050.0 µs | 15,751.0 µs | 2.163× | 0.556× |
| `record_update` | 12,490.0 µs | 13,384.0 µs | 0.855× | 0.797× |
| `recursive_loops` | 11,924.0 µs | 13,938.0 µs | 1.284× | 1.099× |
| `string_count` | 12,918.0 µs | 23,680.0 µs | 1.891× | 1.032× |
| `substring_count` | 21,615.0 µs | 22,487.0 µs | 0.873× | 0.839× |
| `tree_build` | 24,736.0 µs | 45,518.0 µs | 1.188× | 0.645× |
| `while_search` | 27,893.0 µs | 59,862.0 µs | 1.898× | 0.884× |

## Usage

Use a Release build and a `lua` executable on `PATH` (override with `--lua`).

```sh
python3 test/benchmarks/compare_script.py build-release/bin/tapas
```

The script remeasures all complete algorithms and `hotspots/` cases, performs one warm-up, and compares the median of three measured runs per benchmark (tune with `--runs`), prints a summary, and updates both README documents in this directory.
Add `--no-markdown` for console output only, or pass `--markdown PATH` to write a report elsewhere.
Repeating `--markdown` writes multiple reports from the same measurements, so their numbers cannot diverge because of a second run.
To require Tapas to be faster than Python in every benchmark, use `--require-faster`.
Run under Python 3.14 with `--require-algorithm-target` to require a geometric mean Tapas/Python time ratio ≤ 0.90 across the fixed 16 complete algorithms; hotspots are excluded.
Complete algorithm sources live here; basic and algorithm-derived hotspots live in `hotspots/`. Runner scripts remain here.
Algorithm-derived deficits are covered in the [diagnostic analysis](../../hotspots/ANALYSIS.md), with optimized Python references, matched controls, and ablations.
Run `python3.14 test/benchmarks/diagnose_script.py build-release/bin/tapas --algorithms --ablations --controls`.
See the [implementation notes](../../hotspots/OPTIMIZATION.md) and [A/B results](../../hotspots/AB_RESULTS.md). Use `ab_script.py BASELINE CANDIDATE --rounds 3 --runs 8` for alternating rounds on identical sources; pass the same binary twice for an A/A noise control.
See the implementation notes for hotspot-only PGO training and build commands; `profile_script.py` records training inputs, binary hashes, and the profile hash.
See the [heap experiments](../../hotspots/HEAP.md) for Dijkstra and Huffman source A/B tests. `source_ab_script.py` compares two equivalent Tapas programs on the same runtime.
See the [bulk-operation experiments](../../hotspots/BULK.md) for sieve fill/count source A/B tests and the Python count reference choice.
See [generic heap optimization](../../hotspots/HEAP_OPTIMIZATION.md) for heap strategy and callback-entry A/B experiments.
See [remaining algorithm gaps](../../hotspots/REMAINING_GAPS.md) for source A/B evidence separating syntax, native operations, and object representation in six complete algorithms.
After retaining a runtime or benchmark-source optimization, remeasure the final build and update both README reports, raw samples, and validation records together.
Performance thresholds are not part of the regular test suite because CPU load, power settings, compiler version, and Python version affect the results; published comparisons should always include the runtime environment.

## Notes

These programs compare semantically equivalent tasks across the available implementations; they do not represent complete application performance.
Each language uses an idiomatic implementation with equivalent semantics; advantages from built-in bulk operations and data structures are part of the comparison.
The comparison checks fixed-workload results; verify_semantics.py additionally checks small kernel inputs and complete output sequences.
Results depend on system load, power settings, compiler version, Python version, and Lua version.
Regenerate the reports with the commands in the Usage section after changing the VM or runtime environment.
