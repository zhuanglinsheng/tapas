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
| `recursive_fibonacci` | [Tapas](../../recursive_fibonacci.tap) · [Python](../../recursive_fibonacci.py) · [Lua](../../recursive_fibonacci.lua) | 4,130.0 µs | 5,402.0 µs | 2,573.0 µs | 0.765× | 1.605× |
| `sieve` | [Tapas](../../sieve.tap) · [Python](../../sieve.py) · [Lua](../../sieve.lua) | 86.0 µs | 51.0 µs | 608.0 µs | 1.686× | 0.141× |
| `merge_sort` | [Tapas](../../merge_sort.tap) · [Python](../../merge_sort.py) · [Lua](../../merge_sort.lua) | 4,847.0 µs | 4,528.0 µs | 5,510.0 µs | 1.070× | 0.880× |
| `n_queens` | [Tapas](../../n_queens.tap) · [Python](../../n_queens.py) · [Lua](../../n_queens.lua) | 67,293.0 µs | 79,359.0 µs | 48,189.0 µs | 0.848× | 1.396× |
| `longest_common_subsequence` | [Tapas](../../longest_common_subsequence.tap) · [Python](../../longest_common_subsequence.py) · [Lua](../../longest_common_subsequence.lua) | 14,477.0 µs | 10,510.0 µs | 12,074.0 µs | 1.377× | 1.199× |
| `matrix_multiply` | [Tapas](../../matrix_multiply.tap) · [Python](../../matrix_multiply.py) · [Lua](../../matrix_multiply.lua) | 3,040.0 µs | 3,091.0 µs | 1,243.0 µs | 0.984× | 2.446× |
| `sudoku` | [Tapas](../../sudoku.tap) · [Python](../../sudoku.py) · [Lua](../../sudoku.lua) | 365,509.0 µs | 306,801.0 µs | 158,438.0 µs | 1.191× | 2.307× |
| `dijkstra` | [Tapas](../../dijkstra.tap) · [Python](../../dijkstra.py) · [Lua](../../dijkstra.lua) | 8,434.0 µs | 3,349.0 µs | 6,088.0 µs | 2.518× | 1.385× |
| `optimize` | [Tapas](../../optimize.tap) · [Python](../../optimize.py) · [Lua](../../optimize.lua) | 18,729.0 µs | 20,379.0 µs | 14,004.0 µs | 0.919× | 1.337× |
| `kmp` | [Tapas](../../kmp.tap) · [Python](../../kmp.py) · [Lua](../../kmp.lua) | 21,103.0 µs | 19,237.0 µs | 28,821.0 µs | 1.097× | 0.732× |
| `k_nucleotide` | [Tapas](../../k_nucleotide.tap) · [Python](../../k_nucleotide.py) · [Lua](../../k_nucleotide.lua) | 112,211.0 µs | 123,888.0 µs | 69,548.0 µs | 0.906× | 1.613× |
| `binary_trees` | [Tapas](../../binary_trees.tap) · [Python](../../binary_trees.py) · [Lua](../../binary_trees.lua) | 738,612.0 µs | 570,079.0 µs | 877,616.0 µs | 1.296× | 0.842× |
| `lru_cache` | [Tapas](../../lru_cache.tap) · [Python](../../lru_cache.py) · [Lua](../../lru_cache.lua) | 42,902.0 µs | 71,112.0 µs | 35,747.0 µs | 0.603× | 1.200× |
| `huffman` | [Tapas](../../huffman.tap) · [Python](../../huffman.py) · [Lua](../../huffman.lua) | 2,059.0 µs | 1,199.0 µs | 1,596.0 µs | 1.717× | 1.290× |
| `fannkuch` | [Tapas](../../fannkuch.tap) · [Python](../../fannkuch.py) · [Lua](../../fannkuch.lua) | 40,044.0 µs | 38,691.0 µs | 28,697.0 µs | 1.035× | 1.395× |
| `n_body` | [Tapas](../../n_body.tap) · [Python](../../n_body.py) · [Lua](../../n_body.lua) | 24,467.0 µs | 20,608.0 µs | 10,839.0 µs | 1.187× | 2.257× |

The geometric mean ratio for this group: Tapas/Python **1.131×** (16 cases), Tapas/Lua **1.194×** (16 cases).

### VM Hot Paths

This table automatically includes every Tapas/Python case in `hotspots/`: basic VM operations, algorithm-derived composite workloads, and negative controls. The Python column runs each file's default implementation; new diagnostic cases use idiomatic optimized references. Seven Python structural controls are reported separately below. Missing Lua implementations are shown as — and excluded from the Lua geometric mean; each mean states its actual sample count. `function_call_baseline` and `function_calls` share a loop body to estimate direct-call overhead; `function_callbacks` adds a callback layer.

| Benchmark | Measured workload | Source | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | kernel: integer loop, modulo and branches | [Tapas](../../hotspots/vm_hot_paths.tap) · [Python](../../hotspots/vm_hot_paths.py) · [Lua](../../hotspots/vm_hot_paths.lua) | 75,002.0 µs | 140,269.0 µs | 44,447.0 µs | 0.535× | 1.687× |
| `float_arithmetic` | kernel: dependent floating-point arithmetic | [Tapas](../../hotspots/float_arithmetic.tap) · [Python](../../hotspots/float_arithmetic.py) · [Lua](../../hotspots/float_arithmetic.lua) | 22,003.0 µs | 42,155.0 µs | 10,537.0 µs | 0.522× | 2.088× |
| `function_call_baseline` | kernel: call control: loop, modulo and addition | [Tapas](../../hotspots/function_call_baseline.tap) · [Python](../../hotspots/function_call_baseline.py) · [Lua](../../hotspots/function_call_baseline.lua) | 2,733.0 µs | 6,110.0 µs | 1,198.0 µs | 0.447× | 2.281× |
| `function_calls` | kernel: direct calls to a fixed scalar function | [Tapas](../../hotspots/function_calls.tap) · [Python](../../hotspots/function_calls.py) · [Lua](../../hotspots/function_calls.lua) | 6,605.0 µs | 9,197.0 µs | 3,679.0 µs | 0.718× | 1.795× |
| `function_callbacks` | kernel: a fixed callback through a wrapper | [Tapas](../../hotspots/function_callbacks.tap) · [Python](../../hotspots/function_callbacks.py) · [Lua](../../hotspots/function_callbacks.lua) | 12,357.0 µs | 12,861.0 µs | 5,947.0 µs | 0.961× | 2.078× |
| `list_access` | kernel: sequential list reads and loop overhead | [Tapas](../../hotspots/list_access.tap) · [Python](../../hotspots/list_access.py) · [Lua](../../hotspots/list_access.lua) | 6,347.0 µs | 7,102.0 µs | 1,863.0 µs | 0.894× | 3.407× |
| `list_append` | kernel: element-wise append and growth | [Tapas](../../hotspots/list_append.tap) · [Python](../../hotspots/list_append.py) · [Lua](../../hotspots/list_append.lua) | 7,774.0 µs | 9,475.0 µs | 6,488.0 µs | 0.820× | 1.198× |
| `list_update` | kernel: overwriting existing list slots | [Tapas](../../hotspots/list_update.tap) · [Python](../../hotspots/list_update.py) · [Lua](../../hotspots/list_update.lua) | 4,926.0 µs | 6,150.0 µs | 2,456.0 µs | 0.801× | 2.006× |
| `list_copy` | task: full shallow copy, allocation and reclamation | [Tapas](../../hotspots/list_copy.tap) · [Python](../../hotspots/list_copy.py) · [Lua](../../hotspots/list_copy.lua) | 4,971.0 µs | 21,715.0 µs | 153,989.0 µs | 0.229× | 0.032× |
| `list_from_iterable` | task: materializing an integer range | [Tapas](../../hotspots/list_from_iterable.tap) · [Python](../../hotspots/list_from_iterable.py) · [Lua](../../hotspots/list_from_iterable.lua) | 10,396.0 µs | 218,369.0 µs | 179,308.0 µs | 0.048× | 0.058× |
| `list_replicate` | task: constructing a repeated sequence | [Tapas](../../hotspots/list_replicate.tap) · [Python](../../hotspots/list_replicate.py) · [Lua](../../hotspots/list_replicate.lua) | 5,517.0 µs | 14,023.0 µs | 165,315.0 µs | 0.393× | 0.033× |
| `dict_insert` | kernel: sequential integer-key insertion and growth | [Tapas](../../hotspots/dict_insert.tap) · [Python](../../hotspots/dict_insert.py) · [Lua](../../hotspots/dict_insert.lua) | 7,753.0 µs | 8,511.0 µs | 1,270.0 µs | 0.911× | 6.105× |
| `dict_lookup` | kernel: sequential existing integer-key lookup | [Tapas](../../hotspots/dict_lookup.tap) · [Python](../../hotspots/dict_lookup.py) · [Lua](../../hotspots/dict_lookup.lua) | 5,896.0 µs | 15,913.0 µs | 1,906.0 µs | 0.371× | 3.093× |
| `dict_update` | kernel: scalar overwrite at existing integer keys | [Tapas](../../hotspots/dict_update.tap) · [Python](../../hotspots/dict_update.py) · [Lua](../../hotspots/dict_update.lua) | 4,869.0 µs | 20,581.0 µs | 1,984.0 µs | 0.237× | 2.454× |
| `dict_delete` | kernel: element-wise deletion of existing integer keys | [Tapas](../../hotspots/dict_delete.tap) · [Python](../../hotspots/dict_delete.py) · [Lua](../../hotspots/dict_delete.lua) | 3,668.0 µs | 8,173.0 µs | 773.0 µs | 0.449× | 4.745× |
| `dict_literal` | kernel: small dictionary construction, read and reclamation | [Tapas](../../hotspots/dict_literal.tap) · [Python](../../hotspots/dict_literal.py) · [Lua](../../hotspots/dict_literal.lua) | 17,115.0 µs | 22,698.0 µs | 29,587.0 µs | 0.754× | 0.578× |
| `dict_counting` | kernel: explicit loop: default lookup, increment and write | [Tapas](../../hotspots/dict_counting.tap) · [Python](../../hotspots/dict_counting.py) · [Lua](../../hotspots/dict_counting.lua) | 20,694.0 µs | 32,338.0 µs | 5,909.0 µs | 0.640× | 3.502× |
| `tail_recursion` | semantics: tail-call behavior and frame management | [Tapas](../../hotspots/tail_recursion.tap) · [Python](../../hotspots/tail_recursion.py) · [Lua](../../hotspots/tail_recursion.lua) | 1,821.0 µs | 4,747.0 µs | 714.0 µs | 0.384× | 2.550× |
| `branch_logic` | kernel: short-circuit branches over a fixed state sequence | [Tapas](../../hotspots/branch_logic.tap) · [Python](../../hotspots/branch_logic.py) · [Lua](../../hotspots/branch_logic.lua) | 22,453.0 µs | 28,374.0 µs | 11,525.0 µs | 0.791× | 1.948× |
| `constraint_probe` | kernel: Sudoku predicate indexing, comparison and early exit | [Tapas](../../hotspots/constraint_probe.tap) · [Python](../../hotspots/constraint_probe.py) · [Lua](../../hotspots/constraint_probe.lua) | 70,214.0 µs | 80,512.0 µs | 33,878.0 µs | 0.872× | 2.073× |
| `dp_row` | kernel: one LCS row update from a valid predecessor row | [Tapas](../../hotspots/dp_row.tap) · [Python](../../hotspots/dp_row.py) · [Lua](../../hotspots/dp_row.lua) | 41,837.0 µs | 31,009.0 µs | 31,429.0 µs | 1.349× | 1.331× |
| `record_update` | representation: fixed-field updates and object representation | [Tapas](../../hotspots/record_update.tap) · [Python](../../hotspots/record_update.py) · [Lua](../../hotspots/record_update.lua) | 10,912.0 µs | 12,909.0 µs | 7,490.0 µs | 0.845× | 1.457× |
| `recursive_loops` | task: short-sequence sums in a recursion tree | [Tapas](../../hotspots/recursive_loops.tap) · [Python](../../hotspots/recursive_loops.py) · [Lua](../../hotspots/recursive_loops.lua) | 15,980.0 µs | 12,069.0 µs | 8,476.0 µs | 1.324× | 1.885× |
| `string_count` | task: complete character histogram | [Tapas](../../hotspots/string_count.tap) · [Python](../../hotspots/string_count.py) · [Lua](../../hotspots/string_count.lua) | 25,033.0 µs | 14,276.0 µs | 21,008.0 µs | 1.754× | 1.192× |
| `substring_count` | task: slicing, hashing and overlapping substring counts | [Tapas](../../hotspots/substring_count.tap) · [Python](../../hotspots/substring_count.py) · [Lua](../../hotspots/substring_count.lua) | 23,459.0 µs | 23,851.0 µs | 18,396.0 µs | 0.984× | 1.275× |
| `tree_build` | representation: recursive read-only tree construction and lifetimes | [Tapas](../../hotspots/tree_build.tap) · [Python](../../hotspots/tree_build.py) · [Lua](../../hotspots/tree_build.lua) | 30,506.0 µs | 26,121.0 µs | 71,700.0 µs | 1.168× | 0.425× |
| `while_search` | task: linear search for the first matching position | [Tapas](../../hotspots/while_search.tap) · [Python](../../hotspots/while_search.py) · [Lua](../../hotspots/while_search.lua) | 54,064.0 µs | 28,710.0 µs | 28,199.0 µs | 1.883× | 1.917× |

The geometric mean ratio for this group: Tapas/Python **0.639×** (27 cases), Tapas/Lua **1.238×** (27 cases).

Kernels, tasks, representations and semantic cases measure different layers; this group mean is not a pure VM score. See the [case audit](../../hotspots/AUDIT.md).

### Python structural controls

The main table keeps idiomatic defaults. These additional explicit-loop or conventional-object controls expose bulk-operation, iteration and layout effects; they do not isolate a VM instruction cost. Both Python variants must agree. Controls are excluded from all geometric means.

| Case | Python default | Python structural control | Tapas/default | Tapas/control |
| --- | ---: | ---: | ---: | ---: |
| `dp_row` | 31,009.0 µs | 32,799.0 µs | 1.349× | 1.276× |
| `record_update` | 12,909.0 µs | 14,248.0 µs | 0.845× | 0.766× |
| `recursive_loops` | 12,069.0 µs | 14,378.0 µs | 1.324× | 1.111× |
| `string_count` | 14,276.0 µs | 23,997.0 µs | 1.754× | 1.043× |
| `substring_count` | 23,851.0 µs | 23,153.0 µs | 0.984× | 1.013× |
| `tree_build` | 26,121.0 µs | 47,333.0 µs | 1.168× | 0.644× |
| `while_search` | 28,710.0 µs | 60,851.0 µs | 1.883× | 0.888× |

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
Performance thresholds are not part of the regular test suite because CPU load, power settings, compiler version, and Python version affect the results; published comparisons should always include the runtime environment.

## Notes

These programs compare semantically equivalent tasks across the available implementations; they do not represent complete application performance.
Each language uses an idiomatic implementation with equivalent semantics; advantages from built-in bulk operations and data structures are part of the comparison.
The comparison checks fixed-workload results; verify_semantics.py additionally checks small kernel inputs and complete output sequences.
Results depend on system load, power settings, compiler version, Python version, and Lua version.
Regenerate the reports with the commands in the Usage section after changing the VM or runtime environment.
