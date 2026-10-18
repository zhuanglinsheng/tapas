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
| `recursive_fibonacci` | [Tapas](recursive_fibonacci.tap) · [Python](recursive_fibonacci.py) · [Lua](recursive_fibonacci.lua) | 4,184.0 µs | 5,362.0 µs | 2,618.0 µs | 0.780× | 1.598× |
| `sieve` | [Tapas](sieve.tap) · [Python](sieve.py) · [Lua](sieve.lua) | 38.0 µs | 45.0 µs | 550.0 µs | 0.844× | 0.069× |
| `merge_sort` | [Tapas](merge_sort.tap) · [Python](merge_sort.py) · [Lua](merge_sort.lua) | 4,012.0 µs | 3,800.0 µs | 1,831.0 µs | 1.056× | 2.191× |
| `n_queens` | [Tapas](n_queens.tap) · [Python](n_queens.py) · [Lua](n_queens.lua) | 67,200.0 µs | 79,618.0 µs | 33,315.0 µs | 0.844× | 2.017× |
| `longest_common_subsequence` | [Tapas](longest_common_subsequence.tap) · [Python](longest_common_subsequence.py) · [Lua](longest_common_subsequence.lua) | 12,478.0 µs | 9,445.0 µs | 5,406.0 µs | 1.321× | 2.308× |
| `matrix_multiply` | [Tapas](matrix_multiply.tap) · [Python](matrix_multiply.py) · [Lua](matrix_multiply.lua) | 2,998.0 µs | 3,081.0 µs | 1,212.0 µs | 0.973× | 2.474× |
| `sudoku` | [Tapas](sudoku.tap) · [Python](sudoku.py) · [Lua](sudoku.lua) | 256,699.0 µs | 272,829.0 µs | 97,308.0 µs | 0.941× | 2.638× |
| `dijkstra` | [Tapas](dijkstra.tap) · [Python](dijkstra.py) · [Lua](dijkstra.lua) | 3,894.0 µs | 3,380.0 µs | 5,125.0 µs | 1.152× | 0.760× |
| `optimize` | [Tapas](optimize.tap) · [Python](optimize.py) · [Lua](optimize.lua) | 18,320.0 µs | 20,303.0 µs | 9,489.0 µs | 0.902× | 1.931× |
| `kmp` | [Tapas](kmp.tap) · [Python](kmp.py) · [Lua](kmp.lua) | 17,304.0 µs | 18,712.0 µs | 15,680.0 µs | 0.925× | 1.104× |
| `k_nucleotide` | [Tapas](k_nucleotide.tap) · [Python](k_nucleotide.py) · [Lua](k_nucleotide.lua) | 90,551.0 µs | 123,312.0 µs | 69,107.0 µs | 0.734× | 1.310× |
| `binary_trees` | [Tapas](binary_trees.tap) · [Python](binary_trees.py) · [Lua](binary_trees.lua) | 540,982.0 µs | 546,828.0 µs | 558,296.0 µs | 0.989× | 0.969× |
| `lru_cache` | [Tapas](lru_cache.tap) · [Python](lru_cache.py) · [Lua](lru_cache.lua) | 43,484.0 µs | 70,803.0 µs | 36,086.0 µs | 0.614× | 1.205× |
| `huffman` | [Tapas](huffman.tap) · [Python](huffman.py) · [Lua](huffman.lua) | 80.0 µs | 82.0 µs | 1,576.0 µs | 0.976× | 0.051× |
| `fannkuch` | [Tapas](fannkuch.tap) · [Python](fannkuch.py) · [Lua](fannkuch.lua) | 39,954.0 µs | 38,693.0 µs | 18,136.0 µs | 1.033× | 2.203× |
| `n_body` | [Tapas](n_body.tap) · [Python](n_body.py) · [Lua](n_body.lua) | 24,717.0 µs | 22,094.0 µs | 10,687.0 µs | 1.119× | 2.313× |

The geometric mean ratio for this group: Tapas/Python **0.935×** (16 cases), Tapas/Lua **1.102×** (16 cases).

### VM Hot Paths

Diagnose bottlenecks by retaining the operations being measured and using reasonable implementations within that constraint. Task results expose capability gaps; matched-mechanism controls and runtime A/B experiments support attribution. For example, all heap_callback implementations execute language-level comparators; a native-search versus explicit-while ratio does not isolate loop cost. See [SEMANTICS.md](SEMANTICS.md#热点合同) for the diagnostic contract. Missing Lua implementations are shown as — and excluded from the Lua geometric mean.

| Benchmark | Source | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | [Tapas](hotspots/vm_hot_paths.tap) · [Python](hotspots/vm_hot_paths.py) · [Lua](hotspots/vm_hot_paths.lua) | 76,644.0 µs | 139,001.0 µs | 43,978.0 µs | 0.551× | 1.743× |
| `float_arithmetic` | [Tapas](hotspots/float_arithmetic.tap) · [Python](hotspots/float_arithmetic.py) · [Lua](hotspots/float_arithmetic.lua) | 22,124.0 µs | 41,795.0 µs | 10,485.0 µs | 0.529× | 2.110× |
| `function_call_baseline` | [Tapas](hotspots/function_call_baseline.tap) · [Python](hotspots/function_call_baseline.py) · [Lua](hotspots/function_call_baseline.lua) | 2,695.0 µs | 5,995.0 µs | 1,183.0 µs | 0.450× | 2.278× |
| `function_calls` | [Tapas](hotspots/function_calls.tap) · [Python](hotspots/function_calls.py) · [Lua](hotspots/function_calls.lua) | 6,728.0 µs | 9,164.0 µs | 3,528.0 µs | 0.734× | 1.907× |
| `function_callbacks` | [Tapas](hotspots/function_callbacks.tap) · [Python](hotspots/function_callbacks.py) · [Lua](hotspots/function_callbacks.lua) | 12,437.0 µs | 12,534.0 µs | 5,740.0 µs | 0.992× | 2.167× |
| `list_access` | [Tapas](hotspots/list_access.tap) · [Python](hotspots/list_access.py) · [Lua](hotspots/list_access.lua) | 6,157.0 µs | 7,085.0 µs | 1,809.0 µs | 0.869× | 3.404× |
| `list_append` | [Tapas](hotspots/list_append.tap) · [Python](hotspots/list_append.py) · [Lua](hotspots/list_append.lua) | 7,495.0 µs | 9,755.0 µs | 6,723.0 µs | 0.768× | 1.115× |
| `list_update` | [Tapas](hotspots/list_update.tap) · [Python](hotspots/list_update.py) · [Lua](hotspots/list_update.lua) | 5,102.0 µs | 8,708.0 µs | 2,599.0 µs | 0.586× | 1.963× |
| `list_copy` | [Tapas](hotspots/list_copy.tap) · [Python](hotspots/list_copy.py) · [Lua](hotspots/list_copy.lua) | 7,748.0 µs | 22,688.0 µs | 158,678.0 µs | 0.342× | 0.049× |
| `list_from_iterable` | [Tapas](hotspots/list_from_iterable.tap) · [Python](hotspots/list_from_iterable.py) · [Lua](hotspots/list_from_iterable.lua) | 10,152.0 µs | 220,112.0 µs | 179,016.0 µs | 0.046× | 0.057× |
| `list_replicate` | [Tapas](hotspots/list_replicate.tap) · [Python](hotspots/list_replicate.py) · [Lua](hotspots/list_replicate.lua) | 5,487.0 µs | 13,885.0 µs | 164,371.0 µs | 0.395× | 0.033× |
| `list_sort` | [Tapas](hotspots/list_sort.tap) · [Python](hotspots/list_sort.py) · [Lua](hotspots/list_sort.lua) | 3,825.0 µs | 8,361.0 µs | 18,803.0 µs | 0.457× | 0.203× |
| `dict_insert` | [Tapas](hotspots/dict_insert.tap) · [Python](hotspots/dict_insert.py) · [Lua](hotspots/dict_insert.lua) | 7,468.0 µs | 8,255.0 µs | 1,312.0 µs | 0.905× | 5.692× |
| `dict_lookup` | [Tapas](hotspots/dict_lookup.tap) · [Python](hotspots/dict_lookup.py) · [Lua](hotspots/dict_lookup.lua) | 5,737.0 µs | 15,349.0 µs | 1,887.0 µs | 0.374× | 3.040× |
| `dict_update` | [Tapas](hotspots/dict_update.tap) · [Python](hotspots/dict_update.py) · [Lua](hotspots/dict_update.lua) | 4,857.0 µs | 20,566.0 µs | 2,024.0 µs | 0.236× | 2.400× |
| `dict_delete` | [Tapas](hotspots/dict_delete.tap) · [Python](hotspots/dict_delete.py) · [Lua](hotspots/dict_delete.lua) | 3,622.0 µs | 8,380.0 µs | 774.0 µs | 0.432× | 4.680× |
| `dict_literal` | [Tapas](hotspots/dict_literal.tap) · [Python](hotspots/dict_literal.py) · [Lua](hotspots/dict_literal.lua) | 16,410.0 µs | 22,667.0 µs | 29,254.0 µs | 0.724× | 0.561× |
| `dict_counting` | [Tapas](hotspots/dict_counting.tap) · [Python](hotspots/dict_counting.py) · [Lua](hotspots/dict_counting.lua) | 19,579.0 µs | 31,392.0 µs | 5,849.0 µs | 0.624× | 3.347× |
| `tail_recursion` | [Tapas](hotspots/tail_recursion.tap) · [Python](hotspots/tail_recursion.py) · [Lua](hotspots/tail_recursion.lua) | 1,816.0 µs | 4,074.0 µs | 687.0 µs | 0.446× | 2.643× |
| `branch_logic` | [Tapas](hotspots/branch_logic.tap) · [Python](hotspots/branch_logic.py) · [Lua](hotspots/branch_logic.lua) | 21,933.0 µs | 27,433.0 µs | 10,736.0 µs | 0.800× | 2.043× |
| `constraint_probe` | [Tapas](hotspots/constraint_probe.tap) · [Python](hotspots/constraint_probe.py) · [Lua](hotspots/constraint_probe.lua) | 68,434.0 µs | 77,499.0 µs | 33,192.0 µs | 0.883× | 2.062× |
| `dp_row` | [Tapas](hotspots/dp_row.tap) · [Python](hotspots/dp_row.py) · [Lua](hotspots/dp_row.lua) | 34,390.0 µs | 28,591.0 µs | 29,510.0 µs | 1.203× | 1.165× |
| `heap_callback` | [Tapas](hotspots/heap_callback.tap) · [Python](hotspots/heap_callback.py) · [Lua](hotspots/heap_callback.lua) | 11,374.0 µs | 8,930.0 µs | 8,459.0 µs | 1.274× | 1.345× |
| `heap_order` | [Tapas](hotspots/heap_order.tap) · [Python](hotspots/heap_order.py) · [Lua](hotspots/heap_order.lua) | 2,882.0 µs | 4,298.0 µs | 9,191.0 µs | 0.671× | 0.314× |
| `record_update` | [Tapas](hotspots/record_update.tap) · [Python](hotspots/record_update.py) · [Lua](hotspots/record_update.lua) | 10,749.0 µs | 12,508.0 µs | 7,328.0 µs | 0.859× | 1.467× |
| `recursive_loops` | [Tapas](hotspots/recursive_loops.tap) · [Python](hotspots/recursive_loops.py) · [Lua](hotspots/recursive_loops.lua) | 15,943.0 µs | 11,859.0 µs | 8,416.0 µs | 1.344× | 1.894× |
| `string_count` | [Tapas](hotspots/string_count.tap) · [Python](hotspots/string_count.py) · [Lua](hotspots/string_count.lua) | 24,422.0 µs | 12,788.0 µs | 20,278.0 µs | 1.910× | 1.204× |
| `substring_count` | [Tapas](hotspots/substring_count.tap) · [Python](hotspots/substring_count.py) · [Lua](hotspots/substring_count.lua) | 18,591.0 µs | 21,822.0 µs | 16,103.0 µs | 0.852× | 1.155× |
| `tree_build` | [Tapas](hotspots/tree_build.tap) · [Python](hotspots/tree_build.py) · [Lua](hotspots/tree_build.lua) | 21,752.0 µs | 24,453.0 µs | 50,962.0 µs | 0.890× | 0.427× |
| `while_search` | [Tapas](hotspots/while_search.tap) · [Python](hotspots/while_search.py) · [Lua](hotspots/while_search.lua) | 52,252.0 µs | 27,820.0 µs | 28,116.0 µs | 1.878× | 1.858× |

The geometric mean ratio for this group: Tapas/Python **0.642×** (30 cases), Tapas/Lua **1.120×** (30 cases).

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
See [heap callback fairness](hotspots/HEAP_CALLBACK.md) for the contract, Python representation screening, and interpretation limits.
See [remaining algorithm gaps](hotspots/REMAINING_GAPS.md) for source A/B evidence separating syntax, native operations, and object representation in six complete algorithms.
See [idiomatic source optimization](hotspots/IDIOMATIC.md) for candidate implementations and source A/B tests across all three languages.
See the [Lua source audit](hotspots/LUA_AUDIT.md) for the 16-algorithm review, archived candidates, and independent confirmation; retained implementations are measured choices, not a claim of global optimality.
After retaining a runtime or benchmark-source optimization, remeasure the final build and update both README reports, raw samples, and validation records together.
Performance thresholds are not part of the regular test suite because CPU load, power settings, compiler version, and Python version affect the results; published comparisons should always include the runtime environment.

## Notes

These programs compare semantically equivalent tasks across the available implementations; they do not represent complete application performance.
Each language uses an idiomatic implementation with equivalent semantics; advantages from built-in bulk operations and data structures are part of the comparison.
The comparison checks fixed-workload results; verify_semantics.py additionally checks small kernel inputs and complete output sequences.
Results depend on system load, power settings, compiler version, Python version, and Lua version.
Regenerate the reports with the commands in the Usage section after changing the VM or runtime environment.
