# Runtime A/B results

Measured at: 2026-10-17T16:10:21.383958+00:00

System: `macOS-27.0.1-arm64-arm-64bit-Mach-O`

A (baseline): `17cd65cd3faf59131d8f126ee339aec8c3e1e32aa78fcd141e53fb91fb30752d`

B (candidate): `3556326be12aa43fdba5fd2321a89a750493f1e8c382ff0e49cd1afe64929592`

Each runtime gets one discarded warm-up and 7 measured runs per case in each of 3 rounds. Fresh processes run sequentially in alternating A/B order. Times are internal process CPU times; startup and compilation are excluded. Every checksum must agree across both runtimes and all runs.

Successive rounds reverse the first runtime and shuffle case order with a recorded seed. Round medians expose between-round variation. For an A/A noise control, pass the same binary twice.

B/A is the median of paired time ratios; below 1 means the candidate is faster. The IQR describes the middle half of paired ratios, not a confidence interval. A near-1 ratio should be treated as noise unless repeated measurements establish otherwise.

| Case | A median ms | B median ms | Paired B/A | Paired IQR | Round medians |
| --- | ---: | ---: | ---: | --- | --- |
| `recursive_fibonacci` | 4.225 | 4.202 | 1.007 | 0.977–1.013 | 0.988, 1.009, 0.971 |
| `sieve` | 0.041 | 0.040 | 0.953 | 0.919–1.025 | 0.953, 0.974, 0.951 |
| `merge_sort` | 4.005 | 3.931 | 0.992 | 0.966–1.009 | 0.982, 1.006, 0.992 |
| `n_queens` | 67.529 | 67.419 | 0.996 | 0.992–1.003 | 1.001, 0.996, 0.994 |
| `longest_common_subsequence` | 14.178 | 12.481 | 0.879 | 0.872–0.885 | 0.878, 0.884, 0.880 |
| `matrix_multiply` | 3.034 | 3.009 | 0.987 | 0.967–1.012 | 0.985, 1.010, 1.007 |
| `sudoku` | 255.273 | 255.130 | 0.999 | 0.996–1.006 | 0.999, 1.003, 1.003 |
| `dijkstra` | 3.912 | 3.931 | 1.001 | 0.987–1.026 | 1.001, 1.011, 0.993 |
| `optimize` | 18.135 | 18.321 | 1.009 | 1.005–1.012 | 1.010, 1.008, 1.011 |
| `kmp` | 17.271 | 17.276 | 1.000 | 0.995–1.005 | 1.001, 1.003, 0.997 |
| `k_nucleotide` | 91.019 | 90.901 | 0.995 | 0.992–1.002 | 0.994, 1.001, 0.994 |
| `binary_trees` | 535.844 | 534.420 | 0.998 | 0.992–1.001 | 0.998, 0.994, 1.000 |
| `lru_cache` | 41.993 | 41.972 | 0.998 | 0.994–1.002 | 0.997, 0.993, 0.998 |
| `huffman` | 0.074 | 0.074 | 1.014 | 0.948–1.048 | 0.987, 0.972, 1.014 |
| `fannkuch` | 39.457 | 39.156 | 0.989 | 0.977–0.994 | 0.987, 0.977, 0.993 |
| `n_body` | 23.982 | 23.991 | 0.999 | 0.991–1.006 | 0.995, 0.998, 1.003 |
| `vm_hot_paths` | 74.405 | 73.936 | 0.995 | 0.987–1.002 | 0.995, 0.994, 0.996 |
| `float_arithmetic` | 21.664 | 22.023 | 1.012 | 1.004–1.021 | 1.008, 1.011, 1.017 |
| `function_call_baseline` | 2.758 | 2.776 | 0.998 | 0.967–1.046 | 1.032, 0.971, 1.013 |
| `function_calls` | 6.610 | 6.595 | 0.996 | 0.988–1.010 | 0.996, 1.009, 0.995 |
| `function_callbacks` | 12.119 | 12.104 | 0.998 | 0.983–1.010 | 1.001, 0.996, 0.987 |
| `list_access` | 6.291 | 6.293 | 0.997 | 0.990–1.014 | 0.991, 1.013, 0.996 |
| `list_append` | 7.554 | 7.531 | 1.004 | 0.991–1.011 | 0.994, 1.005, 1.007 |
| `list_update` | 4.822 | 4.832 | 1.003 | 0.982–1.026 | 1.019, 0.985, 0.991 |
| `list_copy` | 4.835 | 4.670 | 0.974 | 0.952–1.008 | 0.950, 1.003, 0.972 |
| `list_from_iterable` | 10.105 | 10.030 | 0.995 | 0.986–1.002 | 0.997, 0.998, 0.994 |
| `list_replicate` | 5.337 | 5.400 | 1.012 | 0.989–1.029 | 1.023, 1.016, 0.991 |
| `list_sort` | 3.583 | 3.617 | 1.003 | 0.978–1.036 | 1.033, 1.003, 0.992 |
| `dict_insert` | 7.395 | 7.326 | 0.986 | 0.961–1.030 | 0.980, 0.986, 0.987 |
| `dict_lookup` | 5.805 | 5.803 | 1.003 | 0.986–1.012 | 1.003, 1.005, 1.000 |
| `dict_update` | 4.763 | 4.799 | 1.002 | 0.996–1.010 | 1.001, 1.005, 1.000 |
| `dict_delete` | 3.516 | 3.540 | 1.002 | 0.985–1.010 | 1.002, 1.008, 0.978 |
| `dict_literal` | 16.836 | 16.518 | 0.981 | 0.972–0.991 | 0.981, 0.977, 0.984 |
| `dict_counting` | 19.879 | 19.743 | 0.999 | 0.989–1.011 | 1.006, 0.997, 1.001 |
| `tail_recursion` | 1.841 | 1.813 | 0.994 | 0.961–1.014 | 0.987, 0.994, 1.001 |
| `branch_logic` | 21.948 | 21.860 | 1.000 | 0.987–1.006 | 0.996, 1.006, 0.996 |
| `constraint_probe` | 68.711 | 68.333 | 0.996 | 0.985–1.002 | 0.992, 0.991, 0.998 |
| `dp_row` | 38.712 | 34.663 | 0.895 | 0.888–0.898 | 0.887, 0.897, 0.894 |
| `heap_callback` | 8.773 | 8.721 | 0.999 | 0.984–1.009 | 1.002, 0.991, 0.994 |
| `heap_order` | 2.772 | 2.757 | 1.002 | 0.970–1.019 | 1.012, 0.984, 1.017 |
| `record_update` | 10.634 | 10.479 | 0.983 | 0.977–0.998 | 0.988, 0.980, 0.992 |
| `recursive_loops` | 15.278 | 15.349 | 1.003 | 1.000–1.016 | 1.003, 1.000, 1.007 |
| `string_count` | 24.254 | 24.334 | 1.007 | 0.994–1.012 | 1.005, 1.003, 1.008 |
| `substring_count` | 18.337 | 18.072 | 0.996 | 0.981–1.008 | 0.997, 0.996, 0.991 |
| `tree_build` | 21.766 | 21.761 | 0.998 | 0.987–1.013 | 0.995, 1.008, 0.994 |
| `while_search` | 52.314 | 52.365 | 1.000 | 0.997–1.005 | 1.000, 0.999, 1.002 |

algorithm: geometric mean B/A **0.988** (16 cases).
hotspot: geometric mean B/A **0.994** (30 cases).

Sources are identical between A and B; no benchmark rewrite is used to obtain a speedup.
