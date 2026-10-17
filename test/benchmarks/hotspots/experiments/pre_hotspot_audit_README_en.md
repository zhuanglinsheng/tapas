> 审查前历史结果；dp_row 和 dict_counting 对应旧输入，不能代表当前测量。

# Tapas, Python, and Lua Performance Comparison

[简体中文](../../README.md) | English | [Project Home](../../../../README_en.md)

Test date: 2026-10-17

## Test Environment

- System: `macOS-27.0.1-arm64-arm-64bit-Mach-O`
- Architecture: `arm64`
- Python: `3.14.2`
- Tapas: `Tapas 0.1.0 Copyright (C) 2020-2026`
- Lua interpreter: `Lua 5.5.1  Copyright (C) 1994-2026 Lua.org, PUC-Rio`
- Tapas executable: `build-pgo/bin/tapas`
- Measured runs: 7 per benchmark, after 1 warm-up run
- Build options: `CMAKE_BUILD_TYPE:STRING=Release`
- Build options: `CMAKE_C_COMPILER:FILEPATH=/usr/bin/cc`
- Build options: `CMAKE_C_FLAGS:STRING=-flto=full -fprofile-instr-use=/Users/zhuanglinsheng/Documents/Github/tapas/build-pgo/profile/hotspots.profdata`
- Build options: `CMAKE_C_FLAGS_RELEASE:STRING=-O3 -DNDEBUG`
- Build options: `CMAKE_EXE_LINKER_FLAGS:STRING=`
- Build options: `TAPAS_ENABLE_IPO:BOOL=OFF`

## Results

Times are median process CPU times in microseconds and exclude process startup, source loading, and compilation.
A ratio below 1 means Tapas is faster (against Python and Lua respectively).

### Complete Algorithms

Keep the specified algorithm while allowing idiomatic containers and standard-library bulk operations. Inputs, repetitions, outputs, and timing boundaries are equivalent; line-by-line translations are not required. See SEMANTICS.md for the contracts.

| Benchmark | Source | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `recursive_fibonacci` | [Tapas](../../recursive_fibonacci.tap) · [Python](../../recursive_fibonacci.py) · [Lua](../../recursive_fibonacci.lua) | 3,534.0 µs | 5,452.0 µs | 2,608.0 µs | 0.648× | 1.355× |
| `sieve` | [Tapas](../../sieve.tap) · [Python](../../sieve.py) · [Lua](../../sieve.lua) | 1,010.0 µs | 110.0 µs | 609.0 µs | 9.182× | 1.658× |
| `merge_sort` | [Tapas](../../merge_sort.tap) · [Python](../../merge_sort.py) · [Lua](../../merge_sort.lua) | 4,481.0 µs | 4,498.0 µs | 5,453.0 µs | 0.996× | 0.822× |
| `n_queens` | [Tapas](../../n_queens.tap) · [Python](../../n_queens.py) · [Lua](../../n_queens.lua) | 67,838.0 µs | 79,489.0 µs | 48,832.0 µs | 0.853× | 1.389× |
| `longest_common_subsequence` | [Tapas](../../longest_common_subsequence.tap) · [Python](../../longest_common_subsequence.py) · [Lua](../../longest_common_subsequence.lua) | 13,994.0 µs | 10,510.0 µs | 12,228.0 µs | 1.331× | 1.144× |
| `matrix_multiply` | [Tapas](../../matrix_multiply.tap) · [Python](../../matrix_multiply.py) · [Lua](../../matrix_multiply.lua) | 3,005.0 µs | 3,061.0 µs | 1,215.0 µs | 0.982× | 2.473× |
| `sudoku` | [Tapas](../../sudoku.tap) · [Python](../../sudoku.py) · [Lua](../../sudoku.lua) | 351,994.0 µs | 301,510.0 µs | 155,680.0 µs | 1.167× | 2.261× |
| `dijkstra` | [Tapas](../../dijkstra.tap) · [Python](../../dijkstra.py) · [Lua](../../dijkstra.lua) | 19,159.0 µs | 3,390.0 µs | 5,894.0 µs | 5.652× | 3.251× |
| `optimize` | [Tapas](../../optimize.tap) · [Python](../../optimize.py) · [Lua](../../optimize.lua) | 17,647.0 µs | 20,193.0 µs | 14,122.0 µs | 0.874× | 1.250× |
| `kmp` | [Tapas](../../kmp.tap) · [Python](../../kmp.py) · [Lua](../../kmp.lua) | 20,647.0 µs | 19,126.0 µs | 28,311.0 µs | 1.080× | 0.729× |
| `k_nucleotide` | [Tapas](../../k_nucleotide.tap) · [Python](../../k_nucleotide.py) · [Lua](../../k_nucleotide.lua) | 107,076.0 µs | 128,233.0 µs | 69,610.0 µs | 0.835× | 1.538× |
| `binary_trees` | [Tapas](../../binary_trees.tap) · [Python](../../binary_trees.py) · [Lua](../../binary_trees.lua) | 702,433.0 µs | 565,875.0 µs | 870,355.0 µs | 1.241× | 0.807× |
| `lru_cache` | [Tapas](../../lru_cache.tap) · [Python](../../lru_cache.py) · [Lua](../../lru_cache.lua) | 41,543.0 µs | 70,667.0 µs | 35,110.0 µs | 0.588× | 1.183× |
| `huffman` | [Tapas](../../huffman.tap) · [Python](../../huffman.py) · [Lua](../../huffman.lua) | 1,982.0 µs | 1,262.0 µs | 1,538.0 µs | 1.571× | 1.289× |
| `fannkuch` | [Tapas](../../fannkuch.tap) · [Python](../../fannkuch.py) · [Lua](../../fannkuch.lua) | 37,779.0 µs | 37,990.0 µs | 28,451.0 µs | 0.994× | 1.328× |
| `n_body` | [Tapas](../../n_body.tap) · [Python](../../n_body.py) · [Lua](../../n_body.lua) | 22,765.0 µs | 20,548.0 µs | 10,823.0 µs | 1.108× | 2.103× |

The geometric mean ratio for this group: Tapas/Python **1.266×** (16 cases), Tapas/Lua **1.415×** (16 cases).

### VM Hot Paths

This table automatically includes every Tapas/Python case in `hotspots/`: basic VM operations, algorithm-derived composite workloads, and negative controls. The Python column runs each file's default implementation; new diagnostic cases use idiomatic optimized references. See the analysis and diagnose_script.py for matched Python controls. Missing Lua implementations are shown as — and excluded from the Lua geometric mean; each mean states its actual sample count. `function_call_baseline` and `function_calls` share a loop body to estimate direct-call overhead; `function_callbacks` adds a callback layer.

| Benchmark | Source | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | [Tapas](../../hotspots/vm_hot_paths.tap) · [Python](../../hotspots/vm_hot_paths.py) · [Lua](../../hotspots/vm_hot_paths.lua) | 73,881.0 µs | 139,473.0 µs | 44,327.0 µs | 0.530× | 1.667× |
| `float_arithmetic` | [Tapas](../../hotspots/float_arithmetic.tap) · [Python](../../hotspots/float_arithmetic.py) · [Lua](../../hotspots/float_arithmetic.lua) | 21,721.0 µs | 42,053.0 µs | 10,589.0 µs | 0.517× | 2.051× |
| `function_call_baseline` | [Tapas](../../hotspots/function_call_baseline.tap) · [Python](../../hotspots/function_call_baseline.py) · [Lua](../../hotspots/function_call_baseline.lua) | 2,696.0 µs | 6,165.0 µs | 1,205.0 µs | 0.437× | 2.237× |
| `function_calls` | [Tapas](../../hotspots/function_calls.tap) · [Python](../../hotspots/function_calls.py) · [Lua](../../hotspots/function_calls.lua) | 6,351.0 µs | 9,095.0 µs | 3,577.0 µs | 0.698× | 1.776× |
| `function_callbacks` | [Tapas](../../hotspots/function_callbacks.tap) · [Python](../../hotspots/function_callbacks.py) · [Lua](../../hotspots/function_callbacks.lua) | 11,829.0 µs | 12,551.0 µs | 5,877.0 µs | 0.942× | 2.013× |
| `list_access` | [Tapas](../../hotspots/list_access.tap) · [Python](../../hotspots/list_access.py) · [Lua](../../hotspots/list_access.lua) | 4,736.0 µs | 7,039.0 µs | 1,869.0 µs | 0.673× | 2.534× |
| `list_append` | [Tapas](../../hotspots/list_append.tap) · [Python](../../hotspots/list_append.py) · [Lua](../../hotspots/list_append.lua) | 6,970.0 µs | 9,793.0 µs | 6,544.0 µs | 0.712× | 1.065× |
| `list_update` | [Tapas](../../hotspots/list_update.tap) · [Python](../../hotspots/list_update.py) · [Lua](../../hotspots/list_update.lua) | 4,853.0 µs | 6,195.0 µs | 2,407.0 µs | 0.783× | 2.016× |
| `list_copy` | [Tapas](../../hotspots/list_copy.tap) · [Python](../../hotspots/list_copy.py) · [Lua](../../hotspots/list_copy.lua) | 4,830.0 µs | 21,593.0 µs | 155,118.0 µs | 0.224× | 0.031× |
| `list_from_iterable` | [Tapas](../../hotspots/list_from_iterable.tap) · [Python](../../hotspots/list_from_iterable.py) · [Lua](../../hotspots/list_from_iterable.lua) | 10,040.0 µs | 217,282.0 µs | 177,675.0 µs | 0.046× | 0.057× |
| `list_replicate` | [Tapas](../../hotspots/list_replicate.tap) · [Python](../../hotspots/list_replicate.py) · [Lua](../../hotspots/list_replicate.lua) | 5,383.0 µs | 13,938.0 µs | 163,780.0 µs | 0.386× | 0.033× |
| `dict_insert` | [Tapas](../../hotspots/dict_insert.tap) · [Python](../../hotspots/dict_insert.py) · [Lua](../../hotspots/dict_insert.lua) | 7,209.0 µs | 8,247.0 µs | 1,270.0 µs | 0.874× | 5.676× |
| `dict_lookup` | [Tapas](../../hotspots/dict_lookup.tap) · [Python](../../hotspots/dict_lookup.py) · [Lua](../../hotspots/dict_lookup.lua) | 5,648.0 µs | 15,827.0 µs | 1,847.0 µs | 0.357× | 3.058× |
| `dict_update` | [Tapas](../../hotspots/dict_update.tap) · [Python](../../hotspots/dict_update.py) · [Lua](../../hotspots/dict_update.lua) | 5,361.0 µs | 19,702.0 µs | 2,068.0 µs | 0.272× | 2.592× |
| `dict_delete` | [Tapas](../../hotspots/dict_delete.tap) · [Python](../../hotspots/dict_delete.py) · [Lua](../../hotspots/dict_delete.lua) | 3,523.0 µs | 8,411.0 µs | 787.0 µs | 0.419× | 4.476× |
| `dict_literal` | [Tapas](../../hotspots/dict_literal.tap) · [Python](../../hotspots/dict_literal.py) · [Lua](../../hotspots/dict_literal.lua) | 16,887.0 µs | 23,132.0 µs | 29,171.0 µs | 0.730× | 0.579× |
| `dict_counting` | [Tapas](../../hotspots/dict_counting.tap) · [Python](../../hotspots/dict_counting.py) · [Lua](../../hotspots/dict_counting.lua) | 16,931.0 µs | 28,951.0 µs | 5,358.0 µs | 0.585× | 3.160× |
| `tail_recursion` | [Tapas](../../hotspots/tail_recursion.tap) · [Python](../../hotspots/tail_recursion.py) · [Lua](../../hotspots/tail_recursion.lua) | 1,566.0 µs | 4,253.0 µs | 706.0 µs | 0.368× | 2.218× |
| `branch_logic` | [Tapas](../../hotspots/branch_logic.tap) · [Python](../../hotspots/branch_logic.py) · [Lua](../../hotspots/branch_logic.lua) | 22,558.0 µs | 27,973.0 µs | 11,015.0 µs | 0.806× | 2.048× |
| `constraint_probe` | [Tapas](../../hotspots/constraint_probe.tap) · [Python](../../hotspots/constraint_probe.py) · [Lua](../../hotspots/constraint_probe.lua) | 67,116.0 µs | 79,895.0 µs | 33,577.0 µs | 0.840× | 1.999× |
| `dp_row` | [Tapas](../../hotspots/dp_row.tap) · [Python](../../hotspots/dp_row.py) · [Lua](../../hotspots/dp_row.lua) | 39,176.0 µs | 32,241.0 µs | 31,802.0 µs | 1.215× | 1.232× |
| `record_update` | [Tapas](../../hotspots/record_update.tap) · [Python](../../hotspots/record_update.py) · [Lua](../../hotspots/record_update.lua) | 8,854.0 µs | 12,627.0 µs | 8,008.0 µs | 0.701× | 1.106× |
| `recursive_loops` | [Tapas](../../hotspots/recursive_loops.tap) · [Python](../../hotspots/recursive_loops.py) · [Lua](../../hotspots/recursive_loops.lua) | 14,137.0 µs | 11,851.0 µs | 8,376.0 µs | 1.193× | 1.688× |
| `string_count` | [Tapas](../../hotspots/string_count.tap) · [Python](../../hotspots/string_count.py) · [Lua](../../hotspots/string_count.lua) | 23,924.0 µs | 13,774.0 µs | 20,675.0 µs | 1.737× | 1.157× |
| `substring_count` | [Tapas](../../hotspots/substring_count.tap) · [Python](../../hotspots/substring_count.py) · [Lua](../../hotspots/substring_count.lua) | 19,264.0 µs | 23,380.0 µs | 20,163.0 µs | 0.824× | 0.955× |
| `tree_build` | [Tapas](../../hotspots/tree_build.tap) · [Python](../../hotspots/tree_build.py) · [Lua](../../hotspots/tree_build.lua) | 29,553.0 µs | 26,622.0 µs | 71,037.0 µs | 1.110× | 0.416× |
| `while_search` | [Tapas](../../hotspots/while_search.tap) · [Python](../../hotspots/while_search.py) · [Lua](../../hotspots/while_search.lua) | 53,787.0 µs | 28,158.0 µs | 28,448.0 µs | 1.910× | 1.891× |

The geometric mean ratio for this group: Tapas/Python **0.607×** (27 cases), Tapas/Lua **1.162×** (27 cases).

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
Performance thresholds are not part of the regular test suite because CPU load, power settings, compiler version, and Python version affect the results; published comparisons should always include the runtime environment.

## Notes

These programs compare semantically equivalent tasks across the available implementations; they do not represent complete application performance.
Each language uses an idiomatic implementation with equivalent semantics; advantages from built-in bulk operations and data structures are part of the comparison.
The comparison checks fixed-workload results; verify_semantics.py additionally checks small kernel inputs and complete output sequences.
Results depend on system load, power settings, compiler version, Python version, and Lua version.
Regenerate the reports with the commands in the Usage section after changing the VM or runtime environment.
