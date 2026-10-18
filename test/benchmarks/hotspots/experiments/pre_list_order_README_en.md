# Tapas, Python, and Lua Performance Comparison

[简体中文](README.md) | English | [Project Home](../../README_en.md)

Test date: 2026-10-17

## Test Environment

- System: `macOS-27.0.1-arm64-arm-64bit-Mach-O`
- Architecture: `arm64`
- Python: `3.14.2`
- Tapas: `Tapas 0.1.0 Copyright (C) 2020-2026`
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
| `recursive_fibonacci` | [Tapas](../../recursive_fibonacci.tap) · [Python](../../recursive_fibonacci.py) · [Lua](../../recursive_fibonacci.lua) | 4,166.0 µs | 5,508.0 µs | 2,636.0 µs | 0.756× | 1.580× |
| `sieve` | [Tapas](../../sieve.tap) · [Python](../../sieve.py) · [Lua](../../sieve.lua) | 92.0 µs | 51.0 µs | 595.0 µs | 1.804× | 0.155× |
| `merge_sort` | [Tapas](../../merge_sort.tap) · [Python](../../merge_sort.py) · [Lua](../../merge_sort.lua) | 3,995.0 µs | 3,808.0 µs | 4,537.0 µs | 1.049× | 0.881× |
| `n_queens` | [Tapas](../../n_queens.tap) · [Python](../../n_queens.py) · [Lua](../../n_queens.lua) | 72,144.0 µs | 79,205.0 µs | 48,719.0 µs | 0.911× | 1.481× |
| `longest_common_subsequence` | [Tapas](../../longest_common_subsequence.tap) · [Python](../../longest_common_subsequence.py) · [Lua](../../longest_common_subsequence.lua) | 13,427.0 µs | 9,633.0 µs | 11,302.0 µs | 1.394× | 1.188× |
| `matrix_multiply` | [Tapas](../../matrix_multiply.tap) · [Python](../../matrix_multiply.py) · [Lua](../../matrix_multiply.lua) | 3,034.0 µs | 3,058.0 µs | 1,195.0 µs | 0.992× | 2.539× |
| `sudoku` | [Tapas](../../sudoku.tap) · [Python](../../sudoku.py) · [Lua](../../sudoku.lua) | 267,970.0 µs | 276,465.0 µs | 135,927.0 µs | 0.969× | 1.971× |
| `dijkstra` | [Tapas](../../dijkstra.tap) · [Python](../../dijkstra.py) · [Lua](../../dijkstra.lua) | 7,222.0 µs | 3,319.0 µs | 5,917.0 µs | 2.176× | 1.221× |
| `optimize` | [Tapas](../../optimize.tap) · [Python](../../optimize.py) · [Lua](../../optimize.lua) | 18,393.0 µs | 19,989.0 µs | 14,068.0 µs | 0.920× | 1.307× |
| `kmp` | [Tapas](../../kmp.tap) · [Python](../../kmp.py) · [Lua](../../kmp.lua) | 17,314.0 µs | 18,896.0 µs | 28,227.0 µs | 0.916× | 0.613× |
| `k_nucleotide` | [Tapas](../../k_nucleotide.tap) · [Python](../../k_nucleotide.py) · [Lua](../../k_nucleotide.lua) | 91,471.0 µs | 123,323.0 µs | 68,723.0 µs | 0.742× | 1.331× |
| `binary_trees` | [Tapas](../../binary_trees.tap) · [Python](../../binary_trees.py) · [Lua](../../binary_trees.lua) | 534,349.0 µs | 534,670.0 µs | 650,140.0 µs | 0.999× | 0.822× |
| `lru_cache` | [Tapas](../../lru_cache.tap) · [Python](../../lru_cache.py) · [Lua](../../lru_cache.lua) | 42,089.0 µs | 70,396.0 µs | 34,835.0 µs | 0.598× | 1.208× |
| `huffman` | [Tapas](../../huffman.tap) · [Python](../../huffman.py) · [Lua](../../huffman.lua) | 73.0 µs | 71.0 µs | 1,543.0 µs | 1.028× | 0.047× |
| `fannkuch` | [Tapas](../../fannkuch.tap) · [Python](../../fannkuch.py) · [Lua](../../fannkuch.lua) | 39,633.0 µs | 38,531.0 µs | 28,092.0 µs | 1.029× | 1.411× |
| `n_body` | [Tapas](../../n_body.tap) · [Python](../../n_body.py) · [Lua](../../n_body.lua) | 24,215.0 µs | 20,074.0 µs | 10,550.0 µs | 1.206× | 2.295× |

The geometric mean ratio for this group: Tapas/Python **1.038×** (16 cases), Tapas/Lua **0.941×** (16 cases).

### VM Hot Paths

This table automatically includes every Tapas/Python case in `hotspots/`: basic VM operations, algorithm-derived composite workloads, and negative controls. The Python column runs each file's default implementation; new diagnostic cases use idiomatic optimized references. Eight Python structural controls are reported separately below. Missing Lua implementations are shown as — and excluded from the Lua geometric mean; each mean states its actual sample count. `function_call_baseline` and `function_calls` share a loop body to estimate direct-call overhead; `function_callbacks` adds a callback layer.

| Benchmark | Measured workload | Source | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | kernel: integer loop, modulo and branches | [Tapas](../vm_hot_paths.tap) · [Python](../vm_hot_paths.py) · [Lua](../vm_hot_paths.lua) | 75,364.0 µs | 139,323.0 µs | 44,460.0 µs | 0.541× | 1.695× |
| `float_arithmetic` | kernel: dependent floating-point arithmetic | [Tapas](../float_arithmetic.tap) · [Python](../float_arithmetic.py) · [Lua](../float_arithmetic.lua) | 22,006.0 µs | 41,909.0 µs | 11,686.0 µs | 0.525× | 1.883× |
| `function_call_baseline` | kernel: call control: loop, modulo and addition | [Tapas](../function_call_baseline.tap) · [Python](../function_call_baseline.py) · [Lua](../function_call_baseline.lua) | 2,875.0 µs | 6,336.0 µs | 1,188.0 µs | 0.454× | 2.420× |
| `function_calls` | kernel: direct calls to a fixed scalar function | [Tapas](../function_calls.tap) · [Python](../function_calls.py) · [Lua](../function_calls.lua) | 6,785.0 µs | 9,154.0 µs | 3,615.0 µs | 0.741× | 1.877× |
| `function_callbacks` | kernel: a fixed callback through a wrapper | [Tapas](../function_callbacks.tap) · [Python](../function_callbacks.py) · [Lua](../function_callbacks.lua) | 12,390.0 µs | 12,693.0 µs | 5,838.0 µs | 0.976× | 2.122× |
| `list_access` | kernel: sequential list reads and loop overhead | [Tapas](../list_access.tap) · [Python](../list_access.py) · [Lua](../list_access.lua) | 6,167.0 µs | 7,262.0 µs | 1,875.0 µs | 0.849× | 3.289× |
| `list_append` | kernel: element-wise append and growth | [Tapas](../list_append.tap) · [Python](../list_append.py) · [Lua](../list_append.lua) | 7,740.0 µs | 9,772.0 µs | 6,579.0 µs | 0.792× | 1.176× |
| `list_update` | kernel: overwriting existing list slots | [Tapas](../list_update.tap) · [Python](../list_update.py) · [Lua](../list_update.lua) | 4,868.0 µs | 6,033.0 µs | 2,373.0 µs | 0.807× | 2.051× |
| `list_copy` | task: full shallow copy, allocation and reclamation | [Tapas](../list_copy.tap) · [Python](../list_copy.py) · [Lua](../list_copy.lua) | 4,825.0 µs | 21,528.0 µs | 152,792.0 µs | 0.224× | 0.032× |
| `list_from_iterable` | task: materializing an integer range | [Tapas](../list_from_iterable.tap) · [Python](../list_from_iterable.py) · [Lua](../list_from_iterable.lua) | 10,056.0 µs | 217,219.0 µs | 180,089.0 µs | 0.046× | 0.056× |
| `list_replicate` | task: constructing a repeated sequence | [Tapas](../list_replicate.tap) · [Python](../list_replicate.py) · [Lua](../list_replicate.lua) | 5,604.0 µs | 14,072.0 µs | 166,533.0 µs | 0.398× | 0.034× |
| `dict_insert` | kernel: sequential integer-key insertion and growth | [Tapas](../dict_insert.tap) · [Python](../dict_insert.py) · [Lua](../dict_insert.lua) | 7,344.0 µs | 8,573.0 µs | 1,267.0 µs | 0.857× | 5.796× |
| `dict_lookup` | kernel: sequential existing integer-key lookup | [Tapas](../dict_lookup.tap) · [Python](../dict_lookup.py) · [Lua](../dict_lookup.lua) | 6,052.0 µs | 15,757.0 µs | 1,991.0 µs | 0.384× | 3.040× |
| `dict_update` | kernel: scalar overwrite at existing integer keys | [Tapas](../dict_update.tap) · [Python](../dict_update.py) · [Lua](../dict_update.lua) | 4,814.0 µs | 20,179.0 µs | 1,927.0 µs | 0.239× | 2.498× |
| `dict_delete` | kernel: element-wise deletion of existing integer keys | [Tapas](../dict_delete.tap) · [Python](../dict_delete.py) · [Lua](../dict_delete.lua) | 3,557.0 µs | 8,026.0 µs | 783.0 µs | 0.443× | 4.543× |
| `dict_literal` | kernel: small dictionary construction, read and reclamation | [Tapas](../dict_literal.tap) · [Python](../dict_literal.py) · [Lua](../dict_literal.lua) | 16,860.0 µs | 23,094.0 µs | 29,131.0 µs | 0.730× | 0.579× |
| `dict_counting` | kernel: explicit loop: default lookup, increment and write | [Tapas](../dict_counting.tap) · [Python](../dict_counting.py) · [Lua](../dict_counting.lua) | 19,796.0 µs | 32,709.0 µs | 5,910.0 µs | 0.605× | 3.350× |
| `tail_recursion` | semantics: tail-call behavior and frame management | [Tapas](../tail_recursion.tap) · [Python](../tail_recursion.py) · [Lua](../tail_recursion.lua) | 1,770.0 µs | 3,958.0 µs | 711.0 µs | 0.447× | 2.489× |
| `branch_logic` | kernel: short-circuit branches over a fixed state sequence | [Tapas](../branch_logic.tap) · [Python](../branch_logic.py) · [Lua](../branch_logic.lua) | 21,815.0 µs | 27,365.0 µs | 10,839.0 µs | 0.797× | 2.013× |
| `constraint_probe` | kernel: Sudoku predicate indexing, comparison and early exit | [Tapas](../constraint_probe.tap) · [Python](../constraint_probe.py) · [Lua](../constraint_probe.lua) | 71,531.0 µs | 78,645.0 µs | 33,171.0 µs | 0.910× | 2.156× |
| `dp_row` | kernel: one LCS row update with cached left and diagonal cells | [Tapas](../dp_row.tap) · [Python](../dp_row.py) · [Lua](../dp_row.lua) | 38,981.0 µs | 28,983.0 µs | 29,285.0 µs | 1.345× | 1.331× |
| `heap_callback` | task: priority-record heap comparator callbacks, including copying, heapify and ordered draining | [Tapas](../heap_callback.tap) · [Python](../heap_callback.py) · [Lua](../heap_callback.lua) | 8,748.0 µs | 3,996.0 µs | 8,027.0 µs | 2.189× | 1.090× |
| `record_update` | representation: fixed-field updates and object representation | [Tapas](../record_update.tap) · [Python](../record_update.py) · [Lua](../record_update.lua) | 10,558.0 µs | 12,555.0 µs | 7,334.0 µs | 0.841× | 1.440× |
| `recursive_loops` | task: short-sequence sums in a recursion tree | [Tapas](../recursive_loops.tap) · [Python](../recursive_loops.py) · [Lua](../recursive_loops.lua) | 15,350.0 µs | 12,023.0 µs | 8,421.0 µs | 1.277× | 1.823× |
| `string_count` | task: complete character histogram | [Tapas](../string_count.tap) · [Python](../string_count.py) · [Lua](../string_count.lua) | 24,755.0 µs | 13,702.0 µs | 20,366.0 µs | 1.807× | 1.216× |
| `substring_count` | task: slicing, hashing and overlapping substring counts | [Tapas](../substring_count.tap) · [Python](../substring_count.py) · [Lua](../substring_count.lua) | 18,071.0 µs | 21,018.0 µs | 16,566.0 µs | 0.860× | 1.091× |
| `tree_build` | representation: read-only trees with immutable leaf markers and fresh internal nodes | [Tapas](../tree_build.tap) · [Python](../tree_build.py) · [Lua](../tree_build.lua) | 21,830.0 µs | 24,972.0 µs | 50,949.0 µs | 0.874× | 0.428× |
| `while_search` | task: linear search for the first matching position | [Tapas](../while_search.tap) · [Python](../while_search.py) · [Lua](../while_search.lua) | 53,223.0 µs | 27,906.0 µs | 28,099.0 µs | 1.907× | 1.894× |

The geometric mean ratio for this group: Tapas/Python **0.659×** (28 cases), Tapas/Lua **1.218×** (28 cases).

Kernels, tasks, representations and semantic cases measure different layers; this group mean is not a pure VM score. See the [case audit](../AUDIT.md).

### Python structural controls

The main table keeps idiomatic defaults. These additional explicit-loop or conventional-object controls expose bulk-operation, iteration and layout effects; they do not isolate a VM instruction cost. Both Python variants must agree. Controls are excluded from all geometric means.

| Case | Python default | Python structural control | Tapas/default | Tapas/control |
| --- | ---: | ---: | ---: | ---: |
| `dp_row` | 28,983.0 µs | 29,998.0 µs | 1.345× | 1.299× |
| `heap_callback` | 3,996.0 µs | 15,716.0 µs | 2.189× | 0.557× |
| `record_update` | 12,555.0 µs | 13,525.0 µs | 0.841× | 0.781× |
| `recursive_loops` | 12,023.0 µs | 14,251.0 µs | 1.277× | 1.077× |
| `string_count` | 13,702.0 µs | 23,558.0 µs | 1.807× | 1.051× |
| `substring_count` | 21,018.0 µs | 21,802.0 µs | 0.860× | 0.829× |
| `tree_build` | 24,972.0 µs | 46,034.0 µs | 0.874× | 0.474× |
| `while_search` | 27,906.0 µs | 59,009.0 µs | 1.907× | 0.902× |

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
Algorithm-derived deficits are covered in the [diagnostic analysis](../ANALYSIS.md), with optimized Python references, matched controls, and ablations.
Run `python3.14 test/benchmarks/diagnose_script.py build-release/bin/tapas --algorithms --ablations --controls`.
See the [implementation notes](../OPTIMIZATION.md) and [A/B results](../AB_RESULTS.md). Use `ab_script.py BASELINE CANDIDATE --rounds 3 --runs 8` for alternating rounds on identical sources; pass the same binary twice for an A/A noise control.
See the implementation notes for hotspot-only PGO training and build commands; `profile_script.py` records training inputs, binary hashes, and the profile hash.
See the [heap experiments](../HEAP.md) for Dijkstra and Huffman source A/B tests. `source_ab_script.py` compares two equivalent Tapas programs on the same runtime.
See the [bulk-operation experiments](../BULK.md) for sieve fill/count source A/B tests and the Python count reference choice.
See [generic heap optimization](../HEAP_OPTIMIZATION.md) for heap strategy and callback-entry A/B experiments.
See [remaining algorithm gaps](../REMAINING_GAPS.md) for source A/B evidence separating syntax, native operations, and object representation in six complete algorithms.
See [idiomatic source optimization](../IDIOMATIC.md) for candidate implementations and source A/B tests across all three languages.
After retaining a runtime or benchmark-source optimization, remeasure the final build and update both README reports, raw samples, and validation records together.
Performance thresholds are not part of the regular test suite because CPU load, power settings, compiler version, and Python version affect the results; published comparisons should always include the runtime environment.

## Notes

These programs compare semantically equivalent tasks across the available implementations; they do not represent complete application performance.
Each language uses an idiomatic implementation with equivalent semantics; advantages from built-in bulk operations and data structures are part of the comparison.
The comparison checks fixed-workload results; verify_semantics.py additionally checks small kernel inputs and complete output sequences.
Results depend on system load, power settings, compiler version, Python version, and Lua version.
Regenerate the reports with the commands in the Usage section after changing the VM or runtime environment.
