# Tapas, Python, and Lua Performance Comparison

[简体中文](README.md) | English | [Project Home](../../README_en.md)

Test date: 2026-10-17

## Test Environment

- System: `macOS-27.0.1-arm64-arm-64bit-Mach-O`
- Architecture: `arm64`
- Python: `3.14.2`
- Tapas: `Tapas 0.1.0 Copyright (C) 2020-2026`
- Lua interpreter: `Lua 5.5.1  Copyright (C) 1994-2026 Lua.org, PUC-Rio`
- Tapas executable: `/Users/zhuanglinsheng/Documents/Github/tapas/build-pgo/bin/tapas`
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
| `recursive_fibonacci` | [Tapas](recursive_fibonacci.tap) · [Python](recursive_fibonacci.py) · [Lua](recursive_fibonacci.lua) | 3,580.0 µs | 5,578.0 µs | 2,749.0 µs | 0.642× | 1.302× |
| `sieve` | [Tapas](sieve.tap) · [Python](sieve.py) · [Lua](sieve.lua) | 1,031.0 µs | 115.0 µs | 604.0 µs | 8.965× | 1.707× |
| `merge_sort` | [Tapas](merge_sort.tap) · [Python](merge_sort.py) · [Lua](merge_sort.lua) | 4,533.0 µs | 4,586.0 µs | 5,462.0 µs | 0.988× | 0.830× |
| `n_queens` | [Tapas](n_queens.tap) · [Python](n_queens.py) · [Lua](n_queens.lua) | 68,438.0 µs | 79,744.0 µs | 48,275.0 µs | 0.858× | 1.418× |
| `longest_common_subsequence` | [Tapas](longest_common_subsequence.tap) · [Python](longest_common_subsequence.py) · [Lua](longest_common_subsequence.lua) | 14,152.0 µs | 10,747.0 µs | 12,282.0 µs | 1.317× | 1.152× |
| `matrix_multiply` | [Tapas](matrix_multiply.tap) · [Python](matrix_multiply.py) · [Lua](matrix_multiply.lua) | 2,980.0 µs | 3,136.0 µs | 1,217.0 µs | 0.950× | 2.449× |
| `sudoku` | [Tapas](sudoku.tap) · [Python](sudoku.py) · [Lua](sudoku.lua) | 348,821.0 µs | 297,860.0 µs | 156,866.0 µs | 1.171× | 2.224× |
| `dijkstra` | [Tapas](dijkstra.tap) · [Python](dijkstra.py) · [Lua](dijkstra.lua) | 7,918.0 µs | 3,485.0 µs | 5,998.0 µs | 2.272× | 1.320× |
| `optimize` | [Tapas](optimize.tap) · [Python](optimize.py) · [Lua](optimize.lua) | 17,778.0 µs | 20,179.0 µs | 14,106.0 µs | 0.881× | 1.260× |
| `kmp` | [Tapas](kmp.tap) · [Python](kmp.py) · [Lua](kmp.lua) | 20,167.0 µs | 18,772.0 µs | 28,348.0 µs | 1.074× | 0.711× |
| `k_nucleotide` | [Tapas](k_nucleotide.tap) · [Python](k_nucleotide.py) · [Lua](k_nucleotide.lua) | 104,759.0 µs | 123,390.0 µs | 71,206.0 µs | 0.849× | 1.471× |
| `binary_trees` | [Tapas](binary_trees.tap) · [Python](binary_trees.py) · [Lua](binary_trees.lua) | 697,364.0 µs | 564,594.0 µs | 864,780.0 µs | 1.235× | 0.806× |
| `lru_cache` | [Tapas](lru_cache.tap) · [Python](lru_cache.py) · [Lua](lru_cache.lua) | 41,464.0 µs | 70,524.0 µs | 34,928.0 µs | 0.588× | 1.187× |
| `huffman` | [Tapas](huffman.tap) · [Python](huffman.py) · [Lua](huffman.lua) | 1,978.0 µs | 1,157.0 µs | 1,513.0 µs | 1.710× | 1.307× |
| `fannkuch` | [Tapas](fannkuch.tap) · [Python](fannkuch.py) · [Lua](fannkuch.lua) | 37,724.0 µs | 38,003.0 µs | 28,324.0 µs | 0.993× | 1.332× |
| `n_body` | [Tapas](n_body.tap) · [Python](n_body.py) · [Lua](n_body.lua) | 22,643.0 µs | 20,428.0 µs | 10,512.0 µs | 1.108× | 2.154× |

The geometric mean ratio for this group: Tapas/Python **1.197×** (16 cases), Tapas/Lua **1.336×** (16 cases).

### VM Hot Paths

This table automatically includes every Tapas/Python case in `hotspots/`: basic VM operations, algorithm-derived composite workloads, and negative controls. The Python column runs each file's default implementation; new diagnostic cases use idiomatic optimized references. Seven Python structural controls are reported separately below. Missing Lua implementations are shown as — and excluded from the Lua geometric mean; each mean states its actual sample count. `function_call_baseline` and `function_calls` share a loop body to estimate direct-call overhead; `function_callbacks` adds a callback layer.

| Benchmark | Measured workload | Source | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | kernel: integer loop, modulo and branches | [Tapas](hotspots/vm_hot_paths.tap) · [Python](hotspots/vm_hot_paths.py) · [Lua](hotspots/vm_hot_paths.lua) | 73,364.0 µs | 139,106.0 µs | 45,347.0 µs | 0.527× | 1.618× |
| `float_arithmetic` | kernel: dependent floating-point arithmetic | [Tapas](hotspots/float_arithmetic.tap) · [Python](hotspots/float_arithmetic.py) · [Lua](hotspots/float_arithmetic.lua) | 21,781.0 µs | 42,124.0 µs | 10,722.0 µs | 0.517× | 2.031× |
| `function_call_baseline` | kernel: call control: loop, modulo and addition | [Tapas](hotspots/function_call_baseline.tap) · [Python](hotspots/function_call_baseline.py) · [Lua](hotspots/function_call_baseline.lua) | 2,732.0 µs | 6,200.0 µs | 1,205.0 µs | 0.441× | 2.267× |
| `function_calls` | kernel: direct calls to a fixed scalar function | [Tapas](hotspots/function_calls.tap) · [Python](hotspots/function_calls.py) · [Lua](hotspots/function_calls.lua) | 6,278.0 µs | 9,065.0 µs | 3,567.0 µs | 0.693× | 1.760× |
| `function_callbacks` | kernel: a fixed callback through a wrapper | [Tapas](hotspots/function_callbacks.tap) · [Python](hotspots/function_callbacks.py) · [Lua](hotspots/function_callbacks.lua) | 11,686.0 µs | 12,506.0 µs | 5,807.0 µs | 0.934× | 2.012× |
| `list_access` | kernel: sequential list reads and loop overhead | [Tapas](hotspots/list_access.tap) · [Python](hotspots/list_access.py) · [Lua](hotspots/list_access.lua) | 4,554.0 µs | 6,951.0 µs | 1,873.0 µs | 0.655× | 2.431× |
| `list_append` | kernel: element-wise append and growth | [Tapas](hotspots/list_append.tap) · [Python](hotspots/list_append.py) · [Lua](hotspots/list_append.lua) | 7,112.0 µs | 9,804.0 µs | 6,567.0 µs | 0.725× | 1.083× |
| `list_update` | kernel: overwriting existing list slots | [Tapas](hotspots/list_update.tap) · [Python](hotspots/list_update.py) · [Lua](hotspots/list_update.lua) | 4,892.0 µs | 6,194.0 µs | 2,420.0 µs | 0.790× | 2.021× |
| `list_copy` | task: full shallow copy, allocation and reclamation | [Tapas](hotspots/list_copy.tap) · [Python](hotspots/list_copy.py) · [Lua](hotspots/list_copy.lua) | 4,857.0 µs | 21,500.0 µs | 153,800.0 µs | 0.226× | 0.032× |
| `list_from_iterable` | task: materializing an integer range | [Tapas](hotspots/list_from_iterable.tap) · [Python](hotspots/list_from_iterable.py) · [Lua](hotspots/list_from_iterable.lua) | 10,149.0 µs | 214,444.0 µs | 175,904.0 µs | 0.047× | 0.058× |
| `list_replicate` | task: constructing a repeated sequence | [Tapas](hotspots/list_replicate.tap) · [Python](hotspots/list_replicate.py) · [Lua](hotspots/list_replicate.lua) | 5,488.0 µs | 13,804.0 µs | 164,196.0 µs | 0.398× | 0.033× |
| `dict_insert` | kernel: sequential integer-key insertion and growth | [Tapas](hotspots/dict_insert.tap) · [Python](hotspots/dict_insert.py) · [Lua](hotspots/dict_insert.lua) | 7,225.0 µs | 8,271.0 µs | 1,271.0 µs | 0.874× | 5.685× |
| `dict_lookup` | kernel: sequential existing integer-key lookup | [Tapas](hotspots/dict_lookup.tap) · [Python](hotspots/dict_lookup.py) · [Lua](hotspots/dict_lookup.lua) | 5,562.0 µs | 15,544.0 µs | 1,883.0 µs | 0.358× | 2.954× |
| `dict_update` | kernel: scalar overwrite at existing integer keys | [Tapas](hotspots/dict_update.tap) · [Python](hotspots/dict_update.py) · [Lua](hotspots/dict_update.lua) | 5,279.0 µs | 19,886.0 µs | 1,930.0 µs | 0.265× | 2.735× |
| `dict_delete` | kernel: element-wise deletion of existing integer keys | [Tapas](hotspots/dict_delete.tap) · [Python](hotspots/dict_delete.py) · [Lua](hotspots/dict_delete.lua) | 3,551.0 µs | 9,552.0 µs | 775.0 µs | 0.372× | 4.582× |
| `dict_literal` | kernel: small dictionary construction, read and reclamation | [Tapas](hotspots/dict_literal.tap) · [Python](hotspots/dict_literal.py) · [Lua](hotspots/dict_literal.lua) | 16,739.0 µs | 23,091.0 µs | 29,625.0 µs | 0.725× | 0.565× |
| `dict_counting` | kernel: explicit loop: default lookup, increment and write | [Tapas](hotspots/dict_counting.tap) · [Python](hotspots/dict_counting.py) · [Lua](hotspots/dict_counting.lua) | 19,184.0 µs | 31,671.0 µs | 5,803.0 µs | 0.606× | 3.306× |
| `tail_recursion` | semantics: tail-call behavior and frame management | [Tapas](hotspots/tail_recursion.tap) · [Python](hotspots/tail_recursion.py) · [Lua](hotspots/tail_recursion.lua) | 1,536.0 µs | 4,150.0 µs | 715.0 µs | 0.370× | 2.148× |
| `branch_logic` | kernel: short-circuit branches over a fixed state sequence | [Tapas](hotspots/branch_logic.tap) · [Python](hotspots/branch_logic.py) · [Lua](hotspots/branch_logic.lua) | 22,205.0 µs | 27,270.0 µs | 10,675.0 µs | 0.814× | 2.080× |
| `constraint_probe` | kernel: Sudoku predicate indexing, comparison and early exit | [Tapas](hotspots/constraint_probe.tap) · [Python](hotspots/constraint_probe.py) · [Lua](hotspots/constraint_probe.lua) | 67,028.0 µs | 78,647.0 µs | 34,112.0 µs | 0.852× | 1.965× |
| `dp_row` | kernel: one LCS row update from a valid predecessor row | [Tapas](hotspots/dp_row.tap) · [Python](hotspots/dp_row.py) · [Lua](hotspots/dp_row.lua) | 38,480.0 µs | 30,477.0 µs | 31,425.0 µs | 1.263× | 1.225× |
| `record_update` | representation: fixed-field updates and object representation | [Tapas](hotspots/record_update.tap) · [Python](hotspots/record_update.py) · [Lua](hotspots/record_update.lua) | 8,751.0 µs | 12,653.0 µs | 7,808.0 µs | 0.692× | 1.121× |
| `recursive_loops` | task: short-sequence sums in a recursion tree | [Tapas](hotspots/recursive_loops.tap) · [Python](hotspots/recursive_loops.py) · [Lua](hotspots/recursive_loops.lua) | 13,947.0 µs | 11,856.0 µs | 8,341.0 µs | 1.176× | 1.672× |
| `string_count` | task: complete character histogram | [Tapas](hotspots/string_count.tap) · [Python](hotspots/string_count.py) · [Lua](hotspots/string_count.lua) | 23,371.0 µs | 13,925.0 µs | 20,573.0 µs | 1.678× | 1.136× |
| `substring_count` | task: slicing, hashing and overlapping substring counts | [Tapas](hotspots/substring_count.tap) · [Python](hotspots/substring_count.py) · [Lua](hotspots/substring_count.lua) | 18,024.0 µs | 21,036.0 µs | 17,817.0 µs | 0.857× | 1.012× |
| `tree_build` | representation: recursive read-only tree construction and lifetimes | [Tapas](hotspots/tree_build.tap) · [Python](hotspots/tree_build.py) · [Lua](hotspots/tree_build.lua) | 27,767.0 µs | 24,693.0 µs | 80,137.0 µs | 1.124× | 0.346× |
| `while_search` | task: linear search for the first matching position | [Tapas](hotspots/while_search.tap) · [Python](hotspots/while_search.py) · [Lua](hotspots/while_search.lua) | 52,651.0 µs | 27,887.0 µs | 28,008.0 µs | 1.888× | 1.880× |

The geometric mean ratio for this group: Tapas/Python **0.606×** (27 cases), Tapas/Lua **1.157×** (27 cases).

Kernels, tasks, representations and semantic cases measure different layers; this group mean is not a pure VM score. See the [case audit](hotspots/AUDIT.md).

### Python structural controls

The main table keeps idiomatic defaults. These additional explicit-loop or conventional-object controls expose bulk-operation, iteration and layout effects; they do not isolate a VM instruction cost. Both Python variants must agree. Controls are excluded from all geometric means.

| Case | Python default | Python structural control | Tapas/default | Tapas/control |
| --- | ---: | ---: | ---: | ---: |
| `dp_row` | 30,477.0 µs | 32,791.0 µs | 1.263× | 1.173× |
| `record_update` | 12,653.0 µs | 13,413.0 µs | 0.692× | 0.652× |
| `recursive_loops` | 11,856.0 µs | 14,179.0 µs | 1.176× | 0.984× |
| `string_count` | 13,925.0 µs | 23,844.0 µs | 1.678× | 0.980× |
| `substring_count` | 21,036.0 µs | 22,672.0 µs | 0.857× | 0.795× |
| `tree_build` | 24,693.0 µs | 46,552.0 µs | 1.124× | 0.596× |
| `while_search` | 27,887.0 µs | 59,601.0 µs | 1.888× | 0.883× |

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
See the [heap experiments](hotspots/HEAP.md) for Dijkstra and Huffman source A/B tests. `source_ab_script.py` compares two equivalent Tapas programs on the same runtime.
Performance thresholds are not part of the regular test suite because CPU load, power settings, compiler version, and Python version affect the results; published comparisons should always include the runtime environment.

## Notes

These programs compare semantically equivalent tasks across the available implementations; they do not represent complete application performance.
Each language uses an idiomatic implementation with equivalent semantics; advantages from built-in bulk operations and data structures are part of the comparison.
The comparison checks fixed-workload results; verify_semantics.py additionally checks small kernel inputs and complete output sequences.
Results depend on system load, power settings, compiler version, Python version, and Lua version.
Regenerate the reports with the commands in the Usage section after changing the VM or runtime environment.
