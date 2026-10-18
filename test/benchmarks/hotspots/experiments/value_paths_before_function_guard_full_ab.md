# Runtime A/B results

Measured at: 2026-10-17T16:21:45.923079+00:00

System: `macOS-27.0.1-arm64-arm-64bit-Mach-O`

A (baseline): `17cd65cd3faf59131d8f126ee339aec8c3e1e32aa78fcd141e53fb91fb30752d`

B (candidate): `dc1db3ddbca91b306cb688adc538cfcf9d83b24a2ea72fa56576541a96c5cb51`

Each runtime gets one discarded warm-up and 7 measured runs per case in each of 3 rounds. Fresh processes run sequentially in alternating A/B order. Times are internal process CPU times; startup and compilation are excluded. Each execution uses a fresh working directory to isolate automatic bytecode caches. Every checksum must agree across both runtimes and all runs.

Successive rounds reverse the first runtime and shuffle case order with a recorded seed. Round medians expose between-round variation. For an A/A noise control, pass the same binary twice.

B/A is the median of paired time ratios; below 1 means the candidate is faster. The IQR describes the middle half of paired ratios, not a confidence interval. A near-1 ratio should be treated as noise unless repeated measurements establish otherwise.

| Case | A median ms | B median ms | Paired B/A | Paired IQR | Round medians |
| --- | ---: | ---: | ---: | --- | --- |
| `recursive_fibonacci` | 4.180 | 4.162 | 0.987 | 0.978–1.009 | 0.981, 0.985, 0.993 |
| `sieve` | 0.040 | 0.041 | 1.000 | 0.929–1.120 | 1.000, 0.951, 1.077 |
| `merge_sort` | 3.989 | 4.011 | 1.002 | 0.971–1.021 | 0.982, 1.002, 1.012 |
| `n_queens` | 67.292 | 67.066 | 0.997 | 0.994–0.999 | 0.993, 0.999, 0.999 |
| `longest_common_subsequence` | 14.124 | 12.460 | 0.883 | 0.872–0.893 | 0.880, 0.885, 0.888 |
| `matrix_multiply` | 3.039 | 3.066 | 0.988 | 0.968–1.022 | 0.987, 0.993, 0.985 |
| `sudoku` | 255.341 | 254.983 | 0.998 | 0.994–1.003 | 0.995, 1.001, 0.998 |
| `dijkstra` | 3.925 | 3.940 | 1.005 | 0.986–1.014 | 1.012, 0.990, 1.006 |
| `optimize` | 18.235 | 18.210 | 1.000 | 0.993–1.006 | 0.996, 1.006, 0.999 |
| `kmp` | 17.308 | 17.246 | 0.994 | 0.985–1.005 | 0.998, 0.998, 0.984 |
| `k_nucleotide` | 91.091 | 90.412 | 0.992 | 0.990–0.999 | 0.999, 0.995, 0.990 |
| `binary_trees` | 537.464 | 535.408 | 0.996 | 0.989–1.001 | 0.992, 0.992, 0.999 |
| `lru_cache` | 41.849 | 41.702 | 0.998 | 0.993–1.008 | 0.995, 0.997, 1.004 |
| `huffman` | 0.072 | 0.074 | 1.027 | 1.000–1.049 | 1.042, 1.028, 1.014 |
| `fannkuch` | 39.394 | 39.128 | 0.994 | 0.988–1.001 | 0.996, 0.991, 0.993 |
| `n_body` | 24.069 | 24.007 | 0.997 | 0.990–1.010 | 1.013, 0.992, 0.997 |
| `vm_hot_paths` | 73.998 | 74.189 | 1.002 | 0.993–1.008 | 1.004, 1.002, 0.994 |
| `float_arithmetic` | 21.770 | 22.048 | 1.015 | 1.006–1.027 | 1.006, 1.015, 1.017 |
| `function_call_baseline` | 2.718 | 2.739 | 1.003 | 0.980–1.030 | 0.989, 1.024, 1.001 |
| `function_calls` | 6.658 | 6.568 | 0.986 | 0.975–1.002 | 0.978, 1.014, 0.978 |
| `function_callbacks` | 12.175 | 12.155 | 0.997 | 0.991–1.008 | 1.005, 0.997, 0.995 |
| `list_access` | 6.243 | 6.154 | 0.990 | 0.971–1.008 | 1.007, 0.991, 0.986 |
| `list_append` | 7.527 | 7.505 | 0.992 | 0.984–1.016 | 0.990, 0.992, 1.001 |
| `list_update` | 4.806 | 4.842 | 1.002 | 0.985–1.027 | 1.005, 0.999, 1.009 |
| `list_copy` | 4.843 | 4.766 | 0.984 | 0.953–1.024 | 1.001, 0.966, 0.990 |
| `list_from_iterable` | 10.092 | 10.061 | 0.995 | 0.989–1.011 | 0.994, 0.999, 0.995 |
| `list_replicate` | 5.403 | 5.391 | 0.992 | 0.969–1.016 | 0.992, 0.968, 1.015 |
| `list_sort` | 3.584 | 3.590 | 1.000 | 0.970–1.049 | 1.000, 1.005, 0.980 |
| `dict_insert` | 7.123 | 7.306 | 1.001 | 0.983–1.043 | 0.998, 1.011, 1.001 |
| `dict_lookup` | 5.857 | 5.806 | 0.996 | 0.982–1.018 | 0.996, 0.991, 0.998 |
| `dict_update` | 4.776 | 4.783 | 1.000 | 0.992–1.013 | 1.009, 0.996, 0.998 |
| `dict_delete` | 3.498 | 3.503 | 0.996 | 0.992–1.005 | 0.996, 1.000, 0.991 |
| `dict_literal` | 16.923 | 16.455 | 0.972 | 0.967–0.978 | 0.974, 0.971, 0.972 |
| `dict_counting` | 19.706 | 19.670 | 0.999 | 0.991–1.008 | 1.009, 0.999, 0.990 |
| `tail_recursion` | 1.778 | 1.788 | 1.001 | 0.981–1.020 | 1.008, 1.000, 1.001 |
| `branch_logic` | 21.766 | 21.804 | 1.003 | 0.996–1.006 | 1.005, 1.000, 1.004 |
| `constraint_probe` | 68.785 | 68.299 | 0.991 | 0.989–1.000 | 0.992, 0.991, 0.990 |
| `dp_row` | 38.665 | 34.394 | 0.892 | 0.883–0.898 | 0.892, 0.894, 0.890 |
| `heap_callback` | 8.728 | 8.696 | 0.997 | 0.988–1.007 | 0.994, 1.005, 1.000 |
| `heap_order` | 2.747 | 2.813 | 1.021 | 0.990–1.038 | 1.035, 1.008, 1.024 |
| `record_update` | 10.646 | 10.565 | 0.998 | 0.987–1.004 | 0.999, 0.995, 1.001 |
| `recursive_loops` | 15.211 | 15.311 | 1.007 | 0.993–1.014 | 1.012, 1.001, 1.009 |
| `string_count` | 24.240 | 24.264 | 1.002 | 0.994–1.007 | 0.993, 1.002, 1.004 |
| `substring_count` | 18.567 | 18.222 | 0.984 | 0.971–1.002 | 0.987, 0.983, 0.982 |
| `tree_build` | 21.883 | 21.782 | 0.999 | 0.982–1.010 | 0.985, 0.999, 0.999 |
| `while_search` | 52.445 | 52.287 | 0.999 | 0.995–1.003 | 0.999, 1.002, 0.998 |

algorithm: geometric mean B/A **0.991** (16 cases).
hotspot: geometric mean B/A **0.994** (30 cases).

Sources are identical between A and B; no benchmark rewrite is used to obtain a speedup.
