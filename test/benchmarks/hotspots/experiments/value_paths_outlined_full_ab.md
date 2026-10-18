# Runtime A/B results

Measured at: 2026-10-17T16:14:54.205231+00:00

System: `macOS-27.0.1-arm64-arm-64bit-Mach-O`

A (baseline): `17cd65cd3faf59131d8f126ee339aec8c3e1e32aa78fcd141e53fb91fb30752d`

B (candidate): `a6592d49a27a271f281e926b4b498789271a48963044664b804bbbea0631e4da`

Each runtime gets one discarded warm-up and 7 measured runs per case in each of 3 rounds. Fresh processes run sequentially in alternating A/B order. Times are internal process CPU times; startup and compilation are excluded. Each execution uses a fresh working directory to isolate automatic bytecode caches. Every checksum must agree across both runtimes and all runs.

Successive rounds reverse the first runtime and shuffle case order with a recorded seed. Round medians expose between-round variation. For an A/A noise control, pass the same binary twice.

B/A is the median of paired time ratios; below 1 means the candidate is faster. The IQR describes the middle half of paired ratios, not a confidence interval. A near-1 ratio should be treated as noise unless repeated measurements establish otherwise.

| Case | A median ms | B median ms | Paired B/A | Paired IQR | Round medians |
| --- | ---: | ---: | ---: | --- | --- |
| `recursive_fibonacci` | 4.179 | 4.101 | 0.983 | 0.950–1.008 | 0.978, 0.991, 1.003 |
| `sieve` | 0.040 | 0.040 | 1.000 | 0.910–1.066 | 1.051, 0.976, 1.025 |
| `merge_sort` | 3.997 | 3.944 | 0.988 | 0.971–1.014 | 0.994, 0.995, 0.972 |
| `n_queens` | 67.668 | 68.168 | 1.005 | 1.001–1.013 | 1.005, 1.005, 1.012 |
| `longest_common_subsequence` | 14.219 | 12.625 | 0.890 | 0.875–0.894 | 0.879, 0.895, 0.893 |
| `matrix_multiply` | 3.044 | 2.985 | 0.975 | 0.964–0.996 | 0.978, 0.988, 0.970 |
| `sudoku` | 255.672 | 256.267 | 1.003 | 0.998–1.008 | 1.004, 1.001, 1.003 |
| `dijkstra` | 3.977 | 3.950 | 0.993 | 0.970–1.039 | 0.977, 1.009, 0.993 |
| `optimize` | 18.280 | 18.416 | 1.003 | 0.994–1.016 | 0.994, 1.006, 1.003 |
| `kmp` | 17.347 | 17.382 | 1.002 | 0.995–1.020 | 1.004, 1.018, 0.994 |
| `k_nucleotide` | 90.849 | 89.928 | 0.989 | 0.983–0.992 | 0.990, 0.988, 0.990 |
| `binary_trees` | 536.263 | 533.264 | 0.995 | 0.992–0.998 | 0.991, 0.994, 0.998 |
| `lru_cache` | 41.756 | 42.126 | 1.010 | 1.004–1.015 | 1.004, 1.012, 1.011 |
| `huffman` | 0.076 | 0.074 | 0.974 | 0.901–1.060 | 0.961, 0.974, 1.097 |
| `fannkuch` | 39.382 | 40.278 | 1.023 | 1.017–1.033 | 1.017, 1.029, 1.031 |
| `n_body` | 24.034 | 24.147 | 1.002 | 0.994–1.006 | 1.002, 1.000, 1.005 |
| `vm_hot_paths` | 74.313 | 74.079 | 1.000 | 0.986–1.014 | 0.987, 0.999, 1.014 |
| `float_arithmetic` | 21.777 | 21.743 | 0.996 | 0.987–1.006 | 1.006, 0.995, 0.987 |
| `function_call_baseline` | 2.742 | 2.743 | 0.992 | 0.980–1.017 | 1.000, 0.992, 0.987 |
| `function_calls` | 6.608 | 6.618 | 0.996 | 0.986–1.017 | 0.995, 1.006, 1.015 |
| `function_callbacks` | 12.115 | 12.127 | 1.000 | 0.985–1.008 | 1.000, 1.001, 0.994 |
| `list_access` | 6.207 | 6.211 | 1.003 | 0.971–1.022 | 0.977, 1.013, 1.020 |
| `list_append` | 7.576 | 7.559 | 0.999 | 0.990–1.020 | 0.999, 1.004, 0.996 |
| `list_update` | 4.831 | 4.831 | 1.004 | 0.980–1.014 | 1.009, 1.004, 0.991 |
| `list_copy` | 4.778 | 4.793 | 1.001 | 0.964–1.015 | 1.001, 0.986, 1.001 |
| `list_from_iterable` | 10.098 | 10.120 | 1.006 | 0.994–1.017 | 0.995, 0.998, 1.013 |
| `list_replicate` | 5.408 | 5.390 | 0.989 | 0.982–1.014 | 0.989, 0.994, 0.982 |
| `list_sort` | 3.593 | 3.615 | 1.007 | 0.971–1.017 | 1.012, 0.987, 1.003 |
| `dict_insert` | 7.333 | 7.288 | 0.996 | 0.970–1.022 | 1.001, 0.967, 0.996 |
| `dict_lookup` | 5.855 | 5.782 | 0.989 | 0.969–1.006 | 0.969, 1.000, 1.000 |
| `dict_update` | 4.803 | 4.802 | 1.002 | 0.983–1.018 | 1.004, 1.016, 0.991 |
| `dict_delete` | 3.542 | 3.526 | 0.990 | 0.979–1.007 | 0.997, 0.979, 1.005 |
| `dict_literal` | 16.924 | 16.769 | 0.994 | 0.980–1.000 | 0.989, 0.994, 0.995 |
| `dict_counting` | 19.821 | 20.044 | 1.012 | 1.006–1.021 | 1.008, 1.021, 1.010 |
| `tail_recursion` | 1.767 | 1.813 | 1.018 | 0.993–1.043 | 1.036, 1.006, 1.018 |
| `branch_logic` | 21.784 | 21.741 | 1.001 | 0.996–1.008 | 1.009, 1.001, 0.997 |
| `constraint_probe` | 68.890 | 68.354 | 0.997 | 0.990–1.006 | 0.999, 0.993, 0.997 |
| `dp_row` | 38.661 | 34.693 | 0.898 | 0.892–0.904 | 0.903, 0.895, 0.896 |
| `heap_callback` | 8.789 | 8.712 | 0.992 | 0.987–1.003 | 0.997, 0.989, 0.992 |
| `heap_order` | 2.794 | 2.815 | 1.004 | 0.972–1.029 | 1.004, 0.965, 1.021 |
| `record_update` | 10.649 | 10.587 | 0.992 | 0.980–1.001 | 0.993, 0.999, 0.989 |
| `recursive_loops` | 15.313 | 15.313 | 0.998 | 0.991–1.011 | 0.995, 0.997, 1.002 |
| `string_count` | 24.467 | 24.303 | 0.992 | 0.969–0.997 | 0.995, 0.994, 0.990 |
| `substring_count` | 18.520 | 18.417 | 0.991 | 0.977–1.024 | 0.991, 0.990, 1.003 |
| `tree_build` | 21.896 | 21.671 | 0.993 | 0.985–1.001 | 0.991, 0.993, 0.996 |
| `while_search` | 52.474 | 51.589 | 0.983 | 0.980–0.989 | 0.986, 0.984, 0.980 |

algorithm: geometric mean B/A **0.989** (16 cases).
hotspot: geometric mean B/A **0.994** (30 cases).

Sources are identical between A and B; no benchmark rewrite is used to obtain a speedup.
