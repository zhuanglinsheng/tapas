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
| `recursive_fibonacci` | [Tapas](recursive_fibonacci.tap) · [Python](recursive_fibonacci.py) · [Lua](recursive_fibonacci.lua) | 4,179.0 µs | 5,354.0 µs | 2,612.0 µs | 0.781× | 1.600× |
| `sieve` | [Tapas](sieve.tap) · [Python](sieve.py) · [Lua](sieve.lua) | 35.0 µs | 51.0 µs | 616.0 µs | 0.686× | 0.057× |
| `merge_sort` | [Tapas](merge_sort.tap) · [Python](merge_sort.py) · [Lua](merge_sort.lua) | 3,917.0 µs | 3,703.0 µs | 4,562.0 µs | 1.058× | 0.859× |
| `n_queens` | [Tapas](n_queens.tap) · [Python](n_queens.py) · [Lua](n_queens.lua) | 67,033.0 µs | 78,345.0 µs | 47,220.0 µs | 0.856× | 1.420× |
| `longest_common_subsequence` | [Tapas](longest_common_subsequence.tap) · [Python](longest_common_subsequence.py) · [Lua](longest_common_subsequence.lua) | 12,350.0 µs | 9,478.0 µs | 11,196.0 µs | 1.303× | 1.103× |
| `matrix_multiply` | [Tapas](matrix_multiply.tap) · [Python](matrix_multiply.py) · [Lua](matrix_multiply.lua) | 3,009.0 µs | 3,005.0 µs | 1,219.0 µs | 1.001× | 2.468× |
| `sudoku` | [Tapas](sudoku.tap) · [Python](sudoku.py) · [Lua](sudoku.lua) | 255,206.0 µs | 269,455.0 µs | 127,679.0 µs | 0.947× | 1.999× |
| `dijkstra` | [Tapas](dijkstra.tap) · [Python](dijkstra.py) · [Lua](dijkstra.lua) | 3,891.0 µs | 3,345.0 µs | 6,015.0 µs | 1.163× | 0.647× |
| `optimize` | [Tapas](optimize.tap) · [Python](optimize.py) · [Lua](optimize.lua) | 18,055.0 µs | 19,961.0 µs | 13,948.0 µs | 0.905× | 1.294× |
| `kmp` | [Tapas](kmp.tap) · [Python](kmp.py) · [Lua](kmp.lua) | 17,192.0 µs | 18,693.0 µs | 27,904.0 µs | 0.920× | 0.616× |
| `k_nucleotide` | [Tapas](k_nucleotide.tap) · [Python](k_nucleotide.py) · [Lua](k_nucleotide.lua) | 90,377.0 µs | 121,868.0 µs | 69,029.0 µs | 0.742× | 1.309× |
| `binary_trees` | [Tapas](binary_trees.tap) · [Python](binary_trees.py) · [Lua](binary_trees.lua) | 533,941.0 µs | 529,231.0 µs | 648,215.0 µs | 1.009× | 0.824× |
| `lru_cache` | [Tapas](lru_cache.tap) · [Python](lru_cache.py) · [Lua](lru_cache.lua) | 41,817.0 µs | 70,327.0 µs | 34,715.0 µs | 0.595× | 1.205× |
| `huffman` | [Tapas](huffman.tap) · [Python](huffman.py) · [Lua](huffman.lua) | 73.0 µs | 75.0 µs | 1,526.0 µs | 0.973× | 0.048× |
| `fannkuch` | [Tapas](fannkuch.tap) · [Python](fannkuch.py) · [Lua](fannkuch.lua) | 38,812.0 µs | 37,548.0 µs | 27,939.0 µs | 1.034× | 1.389× |
| `n_body` | [Tapas](n_body.tap) · [Python](n_body.py) · [Lua](n_body.lua) | 24,021.0 µs | 20,192.0 µs | 10,656.0 µs | 1.190× | 2.254× |

The geometric mean ratio for this group: Tapas/Python **0.929×** (16 cases), Tapas/Lua **0.839×** (16 cases).

### VM Hot Paths

Diagnose bottlenecks by retaining the operations being measured and using reasonable implementations within that constraint. Task results expose capability gaps; matched-mechanism controls and runtime A/B experiments support attribution. For example, all heap_callback implementations execute language-level comparators; a native-search versus explicit-while ratio does not isolate loop cost. See [SEMANTICS.md](SEMANTICS.md#热点合同) for the diagnostic contract. Missing Lua implementations are shown as — and excluded from the Lua geometric mean.

| Benchmark | Source | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | [Tapas](hotspots/vm_hot_paths.tap) · [Python](hotspots/vm_hot_paths.py) · [Lua](hotspots/vm_hot_paths.lua) | 74,201.0 µs | 137,815.0 µs | 43,784.0 µs | 0.538× | 1.695× |
| `float_arithmetic` | [Tapas](hotspots/float_arithmetic.tap) · [Python](hotspots/float_arithmetic.py) · [Lua](hotspots/float_arithmetic.lua) | 22,188.0 µs | 41,621.0 µs | 10,467.0 µs | 0.533× | 2.120× |
| `function_call_baseline` | [Tapas](hotspots/function_call_baseline.tap) · [Python](hotspots/function_call_baseline.py) · [Lua](hotspots/function_call_baseline.lua) | 2,666.0 µs | 5,999.0 µs | 1,183.0 µs | 0.444× | 2.254× |
| `function_calls` | [Tapas](hotspots/function_calls.tap) · [Python](hotspots/function_calls.py) · [Lua](hotspots/function_calls.lua) | 6,764.0 µs | 8,959.0 µs | 3,572.0 µs | 0.755× | 1.894× |
| `function_callbacks` | [Tapas](hotspots/function_callbacks.tap) · [Python](hotspots/function_callbacks.py) · [Lua](hotspots/function_callbacks.lua) | 12,277.0 µs | 12,450.0 µs | 5,758.0 µs | 0.986× | 2.132× |
| `list_access` | [Tapas](hotspots/list_access.tap) · [Python](hotspots/list_access.py) · [Lua](hotspots/list_access.lua) | 6,108.0 µs | 6,831.0 µs | 1,840.0 µs | 0.894× | 3.320× |
| `list_append` | [Tapas](hotspots/list_append.tap) · [Python](hotspots/list_append.py) · [Lua](hotspots/list_append.lua) | 7,510.0 µs | 9,532.0 µs | 6,484.0 µs | 0.788× | 1.158× |
| `list_update` | [Tapas](hotspots/list_update.tap) · [Python](hotspots/list_update.py) · [Lua](hotspots/list_update.lua) | 4,815.0 µs | 6,090.0 µs | 2,361.0 µs | 0.791× | 2.039× |
| `list_copy` | [Tapas](hotspots/list_copy.tap) · [Python](hotspots/list_copy.py) · [Lua](hotspots/list_copy.lua) | 4,822.0 µs | 21,354.0 µs | 151,352.0 µs | 0.226× | 0.032× |
| `list_from_iterable` | [Tapas](hotspots/list_from_iterable.tap) · [Python](hotspots/list_from_iterable.py) · [Lua](hotspots/list_from_iterable.lua) | 10,059.0 µs | 215,479.0 µs | 178,140.0 µs | 0.047× | 0.056× |
| `list_replicate` | [Tapas](hotspots/list_replicate.tap) · [Python](hotspots/list_replicate.py) · [Lua](hotspots/list_replicate.lua) | 5,560.0 µs | 14,094.0 µs | 161,689.0 µs | 0.394× | 0.034× |
| `list_sort` | [Tapas](hotspots/list_sort.tap) · [Python](hotspots/list_sort.py) · [Lua](hotspots/list_sort.lua) | 3,488.0 µs | 7,912.0 µs | 18,605.0 µs | 0.441× | 0.187× |
| `dict_insert` | [Tapas](hotspots/dict_insert.tap) · [Python](hotspots/dict_insert.py) · [Lua](hotspots/dict_insert.lua) | 7,294.0 µs | 8,057.0 µs | 1,292.0 µs | 0.905× | 5.646× |
| `dict_lookup` | [Tapas](hotspots/dict_lookup.tap) · [Python](hotspots/dict_lookup.py) · [Lua](hotspots/dict_lookup.lua) | 5,783.0 µs | 15,576.0 µs | 1,902.0 µs | 0.371× | 3.040× |
| `dict_update` | [Tapas](hotspots/dict_update.tap) · [Python](hotspots/dict_update.py) · [Lua](hotspots/dict_update.lua) | 4,768.0 µs | 19,637.0 µs | 1,917.0 µs | 0.243× | 2.487× |
| `dict_delete` | [Tapas](hotspots/dict_delete.tap) · [Python](hotspots/dict_delete.py) · [Lua](hotspots/dict_delete.lua) | 3,508.0 µs | 7,921.0 µs | 767.0 µs | 0.443× | 4.574× |
| `dict_literal` | [Tapas](hotspots/dict_literal.tap) · [Python](hotspots/dict_literal.py) · [Lua](hotspots/dict_literal.lua) | 16,341.0 µs | 22,570.0 µs | 28,870.0 µs | 0.724× | 0.566× |
| `dict_counting` | [Tapas](hotspots/dict_counting.tap) · [Python](hotspots/dict_counting.py) · [Lua](hotspots/dict_counting.lua) | 19,646.0 µs | 30,717.0 µs | 5,852.0 µs | 0.640× | 3.357× |
| `tail_recursion` | [Tapas](hotspots/tail_recursion.tap) · [Python](hotspots/tail_recursion.py) · [Lua](hotspots/tail_recursion.lua) | 1,799.0 µs | 4,046.0 µs | 707.0 µs | 0.445× | 2.545× |
| `branch_logic` | [Tapas](hotspots/branch_logic.tap) · [Python](hotspots/branch_logic.py) · [Lua](hotspots/branch_logic.lua) | 21,839.0 µs | 27,292.0 µs | 10,781.0 µs | 0.800× | 2.026× |
| `constraint_probe` | [Tapas](hotspots/constraint_probe.tap) · [Python](hotspots/constraint_probe.py) · [Lua](hotspots/constraint_probe.lua) | 68,613.0 µs | 77,739.0 µs | 33,121.0 µs | 0.883× | 2.072× |
| `dp_row` | [Tapas](hotspots/dp_row.tap) · [Python](hotspots/dp_row.py) · [Lua](hotspots/dp_row.lua) | 34,447.0 µs | 28,330.0 µs | 28,825.0 µs | 1.216× | 1.195× |
| `heap_callback` | [Tapas](hotspots/heap_callback.tap) · [Python](hotspots/heap_callback.py) · [Lua](hotspots/heap_callback.lua) | 8,623.0 µs | 8,344.0 µs | 8,046.0 µs | 1.033× | 1.072× |
| `heap_order` | [Tapas](hotspots/heap_order.tap) · [Python](hotspots/heap_order.py) · [Lua](hotspots/heap_order.lua) | 2,738.0 µs | 4,184.0 µs | 9,020.0 µs | 0.654× | 0.304× |
| `record_update` | [Tapas](hotspots/record_update.tap) · [Python](hotspots/record_update.py) · [Lua](hotspots/record_update.lua) | 10,440.0 µs | 12,493.0 µs | 7,772.0 µs | 0.836× | 1.343× |
| `recursive_loops` | [Tapas](hotspots/recursive_loops.tap) · [Python](hotspots/recursive_loops.py) · [Lua](hotspots/recursive_loops.lua) | 15,299.0 µs | 11,817.0 µs | 8,721.0 µs | 1.295× | 1.754× |
| `string_count` | [Tapas](hotspots/string_count.tap) · [Python](hotspots/string_count.py) · [Lua](hotspots/string_count.lua) | 24,713.0 µs | 13,953.0 µs | 20,959.0 µs | 1.771× | 1.179× |
| `substring_count` | [Tapas](hotspots/substring_count.tap) · [Python](hotspots/substring_count.py) · [Lua](hotspots/substring_count.lua) | 21,377.0 µs | 21,408.0 µs | 16,445.0 µs | 0.999× | 1.300× |
| `tree_build` | [Tapas](hotspots/tree_build.tap) · [Python](hotspots/tree_build.py) · [Lua](hotspots/tree_build.lua) | 21,674.0 µs | 24,311.0 µs | 50,437.0 µs | 0.892× | 0.430× |
| `while_search` | [Tapas](hotspots/while_search.tap) · [Python](hotspots/while_search.py) · [Lua](hotspots/while_search.lua) | 52,342.0 µs | 27,727.0 µs | 27,644.0 µs | 1.888× | 1.893× |

The geometric mean ratio for this group: Tapas/Python **0.637×** (30 cases), Tapas/Lua **1.091×** (30 cases).

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
After retaining a runtime or benchmark-source optimization, remeasure the final build and update both README reports, raw samples, and validation records together.
Performance thresholds are not part of the regular test suite because CPU load, power settings, compiler version, and Python version affect the results; published comparisons should always include the runtime environment.

## Notes

These programs compare semantically equivalent tasks across the available implementations; they do not represent complete application performance.
Each language uses an idiomatic implementation with equivalent semantics; advantages from built-in bulk operations and data structures are part of the comparison.
The comparison checks fixed-workload results; verify_semantics.py additionally checks small kernel inputs and complete output sequences.
Results depend on system load, power settings, compiler version, Python version, and Lua version.
Regenerate the reports with the commands in the Usage section after changing the VM or runtime environment.
