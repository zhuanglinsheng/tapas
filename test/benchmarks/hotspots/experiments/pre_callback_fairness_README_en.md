# Tapas, Python, and Lua Performance Comparison

[简体中文](README.md) | English | [Project Home](../../README_en.md)

Test date: 2026-10-18

## Test Environment

- System: `macOS-27.0.1-arm64-arm-64bit-Mach-O`
- Architecture: `arm64`
- Python: `3.14.2`
- Tapas: `Tapas 0.2.1 Copyright (C) 2020-2026`
- Lua interpreter: `Lua 5.5.1  Copyright (C) 1994-2026 Lua.org, PUC-Rio`
- Tapas executable: `build-release/bin/tapas`
- Measured runs: 3 per benchmark, after 1 warm-up run
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
| `recursive_fibonacci` | [Tapas](recursive_fibonacci.tap) · [Python](recursive_fibonacci.py) · [Lua](recursive_fibonacci.lua) | 5,338.0 µs | 5,833.0 µs | 2,726.0 µs | 0.915× | 1.958× |
| `sieve` | [Tapas](sieve.tap) · [Python](sieve.py) · [Lua](sieve.lua) | 42.0 µs | 53.0 µs | 625.0 µs | 0.792× | 0.067× |
| `merge_sort` | [Tapas](merge_sort.tap) · [Python](merge_sort.py) · [Lua](merge_sort.lua) | 4,255.0 µs | 4,048.0 µs | 4,862.0 µs | 1.051× | 0.875× |
| `n_queens` | [Tapas](n_queens.tap) · [Python](n_queens.py) · [Lua](n_queens.lua) | 68,333.0 µs | 79,408.0 µs | 47,984.0 µs | 0.861× | 1.424× |
| `longest_common_subsequence` | [Tapas](longest_common_subsequence.tap) · [Python](longest_common_subsequence.py) · [Lua](longest_common_subsequence.lua) | 12,869.0 µs | 9,506.0 µs | 11,295.0 µs | 1.354× | 1.139× |
| `matrix_multiply` | [Tapas](matrix_multiply.tap) · [Python](matrix_multiply.py) · [Lua](matrix_multiply.lua) | 3,062.0 µs | 3,065.0 µs | 1,215.0 µs | 0.999× | 2.520× |
| `sudoku` | [Tapas](sudoku.tap) · [Python](sudoku.py) · [Lua](sudoku.lua) | 259,206.0 µs | 271,860.0 µs | 126,855.0 µs | 0.953× | 2.043× |
| `dijkstra` | [Tapas](dijkstra.tap) · [Python](dijkstra.py) · [Lua](dijkstra.lua) | 3,900.0 µs | 3,368.0 µs | 6,153.0 µs | 1.158× | 0.634× |
| `optimize` | [Tapas](optimize.tap) · [Python](optimize.py) · [Lua](optimize.lua) | 18,269.0 µs | 20,322.0 µs | 14,046.0 µs | 0.899× | 1.301× |
| `kmp` | [Tapas](kmp.tap) · [Python](kmp.py) · [Lua](kmp.lua) | 17,795.0 µs | 18,418.0 µs | 28,074.0 µs | 0.966× | 0.634× |
| `k_nucleotide` | [Tapas](k_nucleotide.tap) · [Python](k_nucleotide.py) · [Lua](k_nucleotide.lua) | 91,238.0 µs | 120,350.0 µs | 68,238.0 µs | 0.758× | 1.337× |
| `binary_trees` | [Tapas](binary_trees.tap) · [Python](binary_trees.py) · [Lua](binary_trees.lua) | 536,859.0 µs | 527,728.0 µs | 643,764.0 µs | 1.017× | 0.834× |
| `lru_cache` | [Tapas](lru_cache.tap) · [Python](lru_cache.py) · [Lua](lru_cache.lua) | 41,567.0 µs | 70,513.0 µs | 34,895.0 µs | 0.589× | 1.191× |
| `huffman` | [Tapas](huffman.tap) · [Python](huffman.py) · [Lua](huffman.lua) | 73.0 µs | 80.0 µs | 1,511.0 µs | 0.912× | 0.048× |
| `fannkuch` | [Tapas](fannkuch.tap) · [Python](fannkuch.py) · [Lua](fannkuch.lua) | 39,015.0 µs | 38,091.0 µs | 28,456.0 µs | 1.024× | 1.371× |
| `n_body` | [Tapas](n_body.tap) · [Python](n_body.py) · [Lua](n_body.lua) | 23,938.0 µs | 20,554.0 µs | 10,105.0 µs | 1.165× | 2.369× |

The geometric mean ratio for this group: Tapas/Python **0.947×** (16 cases), Tapas/Lua **0.869×** (16 cases).

### VM Hot Paths

Includes basic operations, algorithm-derived workloads, and negative controls from `hotspots/`, using semantically equivalent idiomatic implementations. Missing Lua implementations are shown as — and excluded from the Lua geometric mean.

| Benchmark | Source | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | [Tapas](hotspots/vm_hot_paths.tap) · [Python](hotspots/vm_hot_paths.py) · [Lua](hotspots/vm_hot_paths.lua) | 75,390.0 µs | 139,834.0 µs | 43,843.0 µs | 0.539× | 1.720× |
| `float_arithmetic` | [Tapas](hotspots/float_arithmetic.tap) · [Python](hotspots/float_arithmetic.py) · [Lua](hotspots/float_arithmetic.lua) | 21,914.0 µs | 41,584.0 µs | 10,483.0 µs | 0.527× | 2.090× |
| `function_call_baseline` | [Tapas](hotspots/function_call_baseline.tap) · [Python](hotspots/function_call_baseline.py) · [Lua](hotspots/function_call_baseline.lua) | 2,756.0 µs | 6,001.0 µs | 1,193.0 µs | 0.459× | 2.310× |
| `function_calls` | [Tapas](hotspots/function_calls.tap) · [Python](hotspots/function_calls.py) · [Lua](hotspots/function_calls.lua) | 6,546.0 µs | 8,954.0 µs | 3,532.0 µs | 0.731× | 1.853× |
| `function_callbacks` | [Tapas](hotspots/function_callbacks.tap) · [Python](hotspots/function_callbacks.py) · [Lua](hotspots/function_callbacks.lua) | 12,223.0 µs | 12,589.0 µs | 5,767.0 µs | 0.971× | 2.119× |
| `list_access` | [Tapas](hotspots/list_access.tap) · [Python](hotspots/list_access.py) · [Lua](hotspots/list_access.lua) | 6,187.0 µs | 6,858.0 µs | 1,825.0 µs | 0.902× | 3.390× |
| `list_append` | [Tapas](hotspots/list_append.tap) · [Python](hotspots/list_append.py) · [Lua](hotspots/list_append.lua) | 7,649.0 µs | 9,405.0 µs | 6,456.0 µs | 0.813× | 1.185× |
| `list_update` | [Tapas](hotspots/list_update.tap) · [Python](hotspots/list_update.py) · [Lua](hotspots/list_update.lua) | 4,868.0 µs | 6,040.0 µs | 2,481.0 µs | 0.806× | 1.962× |
| `list_copy` | [Tapas](hotspots/list_copy.tap) · [Python](hotspots/list_copy.py) · [Lua](hotspots/list_copy.lua) | 4,902.0 µs | 21,406.0 µs | 166,027.0 µs | 0.229× | 0.030× |
| `list_from_iterable` | [Tapas](hotspots/list_from_iterable.tap) · [Python](hotspots/list_from_iterable.py) · [Lua](hotspots/list_from_iterable.lua) | 10,852.0 µs | 281,315.0 µs | 179,265.0 µs | 0.039× | 0.061× |
| `list_replicate` | [Tapas](hotspots/list_replicate.tap) · [Python](hotspots/list_replicate.py) · [Lua](hotspots/list_replicate.lua) | 5,284.0 µs | 13,761.0 µs | 175,206.0 µs | 0.384× | 0.030× |
| `list_sort` | [Tapas](hotspots/list_sort.tap) · [Python](hotspots/list_sort.py) · [Lua](hotspots/list_sort.lua) | 3,526.0 µs | 8,335.0 µs | 19,212.0 µs | 0.423× | 0.184× |
| `dict_insert` | [Tapas](hotspots/dict_insert.tap) · [Python](hotspots/dict_insert.py) · [Lua](hotspots/dict_insert.lua) | 7,802.0 µs | 8,612.0 µs | 1,296.0 µs | 0.906× | 6.020× |
| `dict_lookup` | [Tapas](hotspots/dict_lookup.tap) · [Python](hotspots/dict_lookup.py) · [Lua](hotspots/dict_lookup.lua) | 5,991.0 µs | 15,627.0 µs | 1,842.0 µs | 0.383× | 3.252× |
| `dict_update` | [Tapas](hotspots/dict_update.tap) · [Python](hotspots/dict_update.py) · [Lua](hotspots/dict_update.lua) | 4,856.0 µs | 19,800.0 µs | 1,917.0 µs | 0.245× | 2.533× |
| `dict_delete` | [Tapas](hotspots/dict_delete.tap) · [Python](hotspots/dict_delete.py) · [Lua](hotspots/dict_delete.lua) | 3,564.0 µs | 7,944.0 µs | 769.0 µs | 0.449× | 4.635× |
| `dict_literal` | [Tapas](hotspots/dict_literal.tap) · [Python](hotspots/dict_literal.py) · [Lua](hotspots/dict_literal.lua) | 16,623.0 µs | 22,694.0 µs | 29,118.0 µs | 0.732× | 0.571× |
| `dict_counting` | [Tapas](hotspots/dict_counting.tap) · [Python](hotspots/dict_counting.py) · [Lua](hotspots/dict_counting.lua) | 21,313.0 µs | 30,997.0 µs | 5,843.0 µs | 0.688× | 3.648× |
| `tail_recursion` | [Tapas](hotspots/tail_recursion.tap) · [Python](hotspots/tail_recursion.py) · [Lua](hotspots/tail_recursion.lua) | 1,758.0 µs | 4,030.0 µs | 682.0 µs | 0.436× | 2.578× |
| `branch_logic` | [Tapas](hotspots/branch_logic.tap) · [Python](hotspots/branch_logic.py) · [Lua](hotspots/branch_logic.lua) | 23,148.0 µs | 28,155.0 µs | 10,903.0 µs | 0.822× | 2.123× |
| `constraint_probe` | [Tapas](hotspots/constraint_probe.tap) · [Python](hotspots/constraint_probe.py) · [Lua](hotspots/constraint_probe.lua) | 68,637.0 µs | 79,389.0 µs | 33,448.0 µs | 0.865× | 2.052× |
| `dp_row` | [Tapas](hotspots/dp_row.tap) · [Python](hotspots/dp_row.py) · [Lua](hotspots/dp_row.lua) | 34,297.0 µs | 28,316.0 µs | 28,631.0 µs | 1.211× | 1.198× |
| `heap_callback` | [Tapas](hotspots/heap_callback.tap) · [Python](hotspots/heap_callback.py) · [Lua](hotspots/heap_callback.lua) | 8,703.0 µs | 3,946.0 µs | 7,893.0 µs | 2.206× | 1.103× |
| `heap_order` | [Tapas](hotspots/heap_order.tap) · [Python](hotspots/heap_order.py) · [Lua](hotspots/heap_order.lua) | 2,798.0 µs | 4,194.0 µs | 9,089.0 µs | 0.667× | 0.308× |
| `record_update` | [Tapas](hotspots/record_update.tap) · [Python](hotspots/record_update.py) · [Lua](hotspots/record_update.lua) | 10,500.0 µs | 12,520.0 µs | 8,022.0 µs | 0.839× | 1.309× |
| `recursive_loops` | [Tapas](hotspots/recursive_loops.tap) · [Python](hotspots/recursive_loops.py) · [Lua](hotspots/recursive_loops.lua) | 15,147.0 µs | 11,773.0 µs | 8,257.0 µs | 1.287× | 1.834× |
| `string_count` | [Tapas](hotspots/string_count.tap) · [Python](hotspots/string_count.py) · [Lua](hotspots/string_count.lua) | 23,935.0 µs | 13,581.0 µs | 20,073.0 µs | 1.762× | 1.192× |
| `substring_count` | [Tapas](hotspots/substring_count.tap) · [Python](hotspots/substring_count.py) · [Lua](hotspots/substring_count.lua) | 17,583.0 µs | 20,688.0 µs | 15,883.0 µs | 0.850× | 1.107× |
| `tree_build` | [Tapas](hotspots/tree_build.tap) · [Python](hotspots/tree_build.py) · [Lua](hotspots/tree_build.lua) | 21,722.0 µs | 24,715.0 µs | 51,175.0 µs | 0.879× | 0.424× |
| `while_search` | [Tapas](hotspots/while_search.tap) · [Python](hotspots/while_search.py) · [Lua](hotspots/while_search.lua) | 52,897.0 µs | 27,755.0 µs | 27,686.0 µs | 1.906× | 1.911× |

The geometric mean ratio for this group: Tapas/Python **0.648×** (30 cases), Tapas/Lua **1.093×** (30 cases).

Kernels, tasks, representations and semantic cases measure different layers; this group mean is not a pure VM score. See the [case audit](hotspots/AUDIT.md).

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
Algorithm-derived deficits are covered in the [diagnostic analysis](hotspots/ANALYSIS.md), with optimized Python references, matched controls, and ablations.
Run `python3.14 test/benchmarks/diagnose_script.py build-release/bin/tapas --algorithms --ablations --controls`.
See the [implementation notes](hotspots/OPTIMIZATION.md) and [A/B results](hotspots/AB_RESULTS.md). Use `ab_script.py BASELINE CANDIDATE --rounds 3 --runs 8` for alternating rounds on identical sources; pass the same binary twice for an A/A noise control.
See the implementation notes for hotspot-only PGO training and build commands; `profile_script.py` records training inputs, binary hashes, and the profile hash.
See [List ordering experiments](hotspots/LIST_ORDER.md) for native lexicographic heaps and source A/B results.
See the [heap experiments](hotspots/HEAP.md) for Dijkstra and Huffman source A/B tests. `source_ab_script.py` compares two equivalent Tapas programs on the same runtime.
See the [bulk-operation experiments](hotspots/BULK.md) for sieve fill/count source A/B tests and the Python count reference choice.
See [sort experiments](hotspots/SORT.md) for native and runtime comparisons against libc qsort, and [compact sequences and sorting](hotspots/COMPACT_AND_SORT.md) for the latest optimization round.
See [code cleanup validation](hotspots/CODE_CLEANUP.md) for Type comparison, extension capability validation, and performance retention decisions.
See [performance recovery validation](hotspots/PERFORMANCE_RECOVERY.md) for restored membership gains and sorting regression analysis.
See [LCS source optimization](hotspots/LCS.md) for the current implementation, historical hoisting experiments, and source A/B results.
See [value-path optimization](hotspots/VALUE_PATHS.md) for generic character comparison and local initialization improvements, plus scope-cleanup experiments.
See [generic heap optimization](hotspots/HEAP_OPTIMIZATION.md) for heap strategy and callback-entry A/B experiments.
See [remaining algorithm gaps](hotspots/REMAINING_GAPS.md) for source A/B evidence separating syntax, native operations, and object representation in six complete algorithms.
See [idiomatic source optimization](hotspots/IDIOMATIC.md) for candidate implementations and source A/B tests across all three languages.
After retaining a runtime or benchmark-source optimization, remeasure the final build and update both README reports, raw samples, and validation records together.
Performance thresholds are not part of the regular test suite because CPU load, power settings, compiler version, and Python version affect the results; published comparisons should always include the runtime environment.

## Notes

These programs compare semantically equivalent tasks across the available implementations; they do not represent complete application performance.
Each language uses an idiomatic implementation with equivalent semantics; advantages from built-in bulk operations and data structures are part of the comparison.
The comparison checks fixed-workload results; verify_semantics.py additionally checks small kernel inputs and complete output sequences.
Results depend on system load, power settings, compiler version, Python version, and Lua version.
Regenerate the reports with the commands in the Usage section after changing the VM or runtime environment.
