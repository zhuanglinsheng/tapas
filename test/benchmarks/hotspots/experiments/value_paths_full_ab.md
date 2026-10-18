# Runtime A/B results

Measured at: 2026-10-17T16:28:54.356025+00:00

System: `macOS-27.0.1-arm64-arm-64bit-Mach-O`

A (baseline): `17cd65cd3faf59131d8f126ee339aec8c3e1e32aa78fcd141e53fb91fb30752d`

B (candidate): `fa7b0ec89ed71d51d49d4ae9bd03746af7b646491026849b2f8266569ffd2ce1`

Each runtime gets one discarded warm-up and 7 measured runs per case in each of 3 rounds. Fresh processes run sequentially in alternating A/B order. Times are internal process CPU times; startup and compilation are excluded. Each execution uses a fresh working directory to isolate automatic bytecode caches. Every checksum must agree across both runtimes and all runs.

Successive rounds reverse the first runtime and shuffle case order with a recorded seed. Round medians expose between-round variation. For an A/A noise control, pass the same binary twice.

B/A is the median of paired time ratios; below 1 means the candidate is faster. The IQR describes the middle half of paired ratios, not a confidence interval. A near-1 ratio should be treated as noise unless repeated measurements establish otherwise.

| Case | A median ms | B median ms | Paired B/A | Paired IQR | Round medians |
| --- | ---: | ---: | ---: | --- | --- |
| `recursive_fibonacci` | 4.137 | 4.115 | 1.001 | 0.982–1.013 | 1.005, 1.001, 1.006 |
| `sieve` | 0.040 | 0.038 | 0.949 | 0.902–1.051 | 0.930, 1.000, 0.925 |
| `merge_sort` | 4.004 | 3.938 | 0.986 | 0.969–1.009 | 0.985, 0.991, 0.988 |
| `n_queens` | 67.413 | 67.156 | 0.996 | 0.995–1.001 | 0.996, 0.997, 0.997 |
| `longest_common_subsequence` | 14.086 | 12.363 | 0.879 | 0.871–0.883 | 0.879, 0.883, 0.874 |
| `matrix_multiply` | 3.010 | 3.015 | 1.009 | 0.997–1.021 | 1.009, 1.016, 1.002 |
| `sudoku` | 254.253 | 256.236 | 1.006 | 0.997–1.015 | 1.004, 1.013, 1.005 |
| `dijkstra` | 3.934 | 3.957 | 1.006 | 0.981–1.013 | 1.006, 0.965, 1.008 |
| `optimize` | 18.273 | 18.287 | 0.998 | 0.992–1.011 | 1.010, 0.994, 0.997 |
| `kmp` | 17.295 | 17.326 | 1.005 | 0.994–1.016 | 1.014, 1.006, 0.994 |
| `k_nucleotide` | 90.883 | 90.425 | 0.996 | 0.989–1.002 | 0.999, 0.993, 0.996 |
| `binary_trees` | 535.339 | 533.583 | 0.998 | 0.989–1.002 | 0.998, 0.990, 1.001 |
| `lru_cache` | 41.848 | 41.852 | 0.998 | 0.996–1.007 | 0.997, 1.004, 0.998 |
| `huffman` | 0.073 | 0.073 | 1.014 | 0.972–1.048 | 1.014, 1.043, 1.000 |
| `fannkuch` | 39.265 | 39.126 | 0.996 | 0.988–0.999 | 0.996, 0.995, 0.996 |
| `n_body` | 23.960 | 23.878 | 0.994 | 0.986–1.005 | 1.003, 0.997, 0.984 |
| `vm_hot_paths` | 73.772 | 73.919 | 1.004 | 0.994–1.010 | 0.997, 1.002, 1.008 |
| `float_arithmetic` | 21.710 | 21.907 | 1.004 | 0.994–1.018 | 1.004, 0.995, 1.005 |
| `function_call_baseline` | 2.724 | 2.739 | 1.009 | 0.974–1.031 | 1.011, 1.021, 0.994 |
| `function_calls` | 6.575 | 6.621 | 1.008 | 0.996–1.013 | 0.998, 1.010, 1.009 |
| `function_callbacks` | 12.073 | 12.076 | 1.000 | 0.993–1.008 | 1.001, 1.001, 0.997 |
| `list_access` | 6.190 | 6.143 | 0.991 | 0.979–1.006 | 0.990, 0.979, 1.000 |
| `list_append` | 7.494 | 7.521 | 1.005 | 0.978–1.017 | 1.005, 1.013, 0.986 |
| `list_update` | 4.848 | 4.850 | 1.004 | 0.969–1.015 | 1.006, 1.011, 0.969 |
| `list_copy` | 4.756 | 4.717 | 0.987 | 0.968–1.014 | 0.991, 1.018, 0.971 |
| `list_from_iterable` | 10.066 | 10.009 | 0.993 | 0.981–1.011 | 1.004, 0.993, 0.993 |
| `list_replicate` | 5.398 | 5.353 | 0.990 | 0.963–1.035 | 0.979, 0.994, 1.016 |
| `list_sort` | 3.584 | 3.607 | 1.007 | 0.991–1.017 | 1.012, 1.004, 1.003 |
| `dict_insert` | 7.170 | 7.224 | 0.996 | 0.961–1.025 | 1.015, 0.979, 0.996 |
| `dict_lookup` | 5.834 | 5.775 | 0.990 | 0.976–0.999 | 0.989, 1.009, 0.991 |
| `dict_update` | 4.781 | 4.811 | 0.995 | 0.986–1.034 | 0.995, 0.987, 1.015 |
| `dict_delete` | 3.542 | 3.570 | 1.001 | 0.988–1.015 | 1.005, 0.996, 1.001 |
| `dict_literal` | 16.884 | 16.336 | 0.969 | 0.961–0.981 | 0.965, 0.969, 0.981 |
| `dict_counting` | 19.806 | 19.770 | 0.998 | 0.991–1.010 | 0.996, 1.000, 1.002 |
| `tail_recursion` | 1.799 | 1.779 | 0.992 | 0.970–1.001 | 0.992, 0.992, 0.995 |
| `branch_logic` | 21.759 | 21.746 | 1.004 | 0.994–1.011 | 1.003, 1.005, 1.005 |
| `constraint_probe` | 69.021 | 68.504 | 0.991 | 0.987–0.996 | 0.994, 0.987, 0.991 |
| `dp_row` | 38.556 | 34.372 | 0.892 | 0.888–0.896 | 0.893, 0.891, 0.892 |
| `heap_callback` | 8.673 | 8.693 | 1.000 | 0.979–1.010 | 1.000, 1.002, 0.997 |
| `heap_order` | 2.747 | 2.729 | 0.993 | 0.970–1.006 | 0.999, 0.994, 0.969 |
| `record_update` | 10.572 | 10.495 | 0.996 | 0.978–1.006 | 0.996, 0.998, 0.990 |
| `recursive_loops` | 15.362 | 15.311 | 1.004 | 0.996–1.018 | 0.996, 1.029, 1.008 |
| `string_count` | 24.382 | 24.252 | 0.993 | 0.971–1.005 | 0.978, 0.993, 0.994 |
| `substring_count` | 18.012 | 17.864 | 0.985 | 0.972–1.008 | 0.979, 1.002, 0.984 |
| `tree_build` | 21.725 | 21.608 | 0.996 | 0.987–1.010 | 0.987, 0.997, 1.002 |
| `while_search` | 52.335 | 52.389 | 0.999 | 0.995–1.003 | 1.001, 0.996, 0.999 |

algorithm: geometric mean B/A **0.989** (16 cases).
hotspot: geometric mean B/A **0.993** (30 cases).

Sources are identical between A and B; no benchmark rewrite is used to obtain a speedup.
