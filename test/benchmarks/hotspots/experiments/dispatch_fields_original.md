# Runtime A/B results

Measured at: 2026-10-17T05:29:55.745493+00:00

System: `macOS-27.0.1-arm64-arm-64bit-Mach-O`

A (baseline): `2f6ad6c89b390bd58c387aca7593b7ba61679803c57b2f1097174c2a6adaf967`

B (candidate): `4eb45fc24208ee395859c7d2a6ca05da6d9beaf26a29d6eb75c199aa43601ad7`

Each runtime gets one discarded warm-up and 8 measured runs per case in each of 3 rounds. Fresh processes run sequentially in alternating A/B order. Times are internal process CPU times; startup and compilation are excluded. Every checksum must agree across both runtimes and all runs.

Successive rounds reverse the first runtime and shuffle case order with a recorded seed. Round medians expose between-round variation. For an A/A noise control, pass the same binary twice.

B/A is the median of paired time ratios; below 1 means the candidate is faster. The IQR describes the middle half of paired ratios, not a confidence interval. A near-1 ratio should be treated as noise unless repeated measurements establish otherwise.

| Case | A median ms | B median ms | Paired B/A | Paired IQR | Round medians |
| --- | ---: | ---: | ---: | --- | --- |
| `recursive_fibonacci` | 4.307 | 4.221 | 0.990 | 0.972–0.998 | 0.992, 0.988, 0.982 |
| `sieve` | 1.063 | 1.044 | 0.984 | 0.947–1.005 | 0.974, 0.979, 0.997 |
| `merge_sort` | 5.263 | 5.066 | 0.957 | 0.944–0.978 | 0.958, 0.948, 0.977 |
| `n_queens` | 133.171 | 132.067 | 0.993 | 0.988–0.997 | 0.995, 0.993, 0.993 |
| `longest_common_subsequence` | 16.674 | 15.614 | 0.937 | 0.927–0.942 | 0.938, 0.936, 0.940 |
| `matrix_multiply` | 3.304 | 3.118 | 0.942 | 0.923–0.974 | 0.932, 0.947, 0.948 |
| `sudoku` | 395.639 | 371.628 | 0.937 | 0.936–0.941 | 0.937, 0.937, 0.939 |
| `dijkstra` | 20.357 | 20.674 | 1.014 | 1.008–1.029 | 1.013, 1.013, 1.020 |
| `optimize` | 19.642 | 18.657 | 0.950 | 0.946–0.954 | 0.952, 0.949, 0.949 |
| `kmp` | 23.731 | 23.453 | 0.986 | 0.978–0.991 | 0.986, 0.987, 0.977 |
| `k_nucleotide` | 182.800 | 170.871 | 0.935 | 0.931–0.940 | 0.935, 0.936, 0.933 |
| `binary_trees` | 744.371 | 735.928 | 0.990 | 0.987–0.992 | 0.988, 0.990, 0.990 |
| `lru_cache` | 45.468 | 44.038 | 0.970 | 0.961–0.974 | 0.970, 0.965, 0.972 |
| `huffman` | 2.215 | 2.184 | 0.988 | 0.973–0.999 | 0.988, 0.986, 0.990 |
| `fannkuch` | 41.483 | 39.860 | 0.962 | 0.957–0.966 | 0.964, 0.960, 0.960 |
| `n_body` | 23.796 | 23.904 | 1.006 | 0.994–1.009 | 1.010, 1.006, 0.996 |
| `vm_hot_paths` | 81.801 | 77.507 | 0.945 | 0.937–0.956 | 0.944, 0.943, 0.954 |
| `float_arithmetic` | 22.913 | 22.356 | 0.979 | 0.964–0.992 | 0.986, 0.974, 0.970 |
| `function_call_baseline` | 2.712 | 2.776 | 1.022 | 1.004–1.042 | 1.021, 1.023, 1.022 |
| `function_calls` | 6.908 | 6.939 | 1.005 | 0.985–1.012 | 1.002, 1.011, 0.990 |
| `function_callbacks` | 13.208 | 12.380 | 0.939 | 0.930–0.954 | 0.944, 0.942, 0.934 |
| `list_access` | 7.125 | 6.276 | 0.890 | 0.866–0.907 | 0.900, 0.903, 0.874 |
| `list_append` | 7.750 | 7.763 | 0.999 | 0.984–1.017 | 1.017, 0.997, 0.996 |
| `list_update` | 5.682 | 5.104 | 0.894 | 0.879–0.905 | 0.886, 0.887, 0.903 |
| `list_copy` | 4.843 | 4.764 | 0.976 | 0.952–1.021 | 0.975, 0.988, 0.976 |
| `list_from_iterable` | 10.175 | 10.085 | 0.992 | 0.968–1.004 | 0.979, 0.997, 0.993 |
| `list_replicate` | 5.488 | 5.375 | 0.983 | 0.954–0.998 | 0.951, 0.989, 0.989 |
| `dict_insert` | 6.612 | 6.516 | 0.989 | 0.975–1.013 | 0.986, 1.005, 0.987 |
| `dict_lookup` | 7.204 | 6.436 | 0.896 | 0.876–0.916 | 0.880, 0.908, 0.905 |
| `dict_update` | 6.053 | 5.862 | 0.978 | 0.947–0.988 | 0.948, 0.980, 0.977 |
| `dict_delete` | 3.729 | 3.643 | 0.981 | 0.962–0.998 | 0.987, 0.972, 0.976 |
| `dict_literal` | 21.643 | 17.448 | 0.804 | 0.795–0.810 | 0.801, 0.805, 0.807 |
| `dict_counting` | 22.983 | 20.151 | 0.886 | 0.866–0.915 | 0.914, 0.871, 0.872 |
| `tail_recursion` | 1.814 | 1.749 | 0.968 | 0.950–0.991 | 0.972, 0.962, 0.969 |
| `branch_logic` | 23.430 | 22.592 | 0.957 | 0.944–0.971 | 0.950, 0.957, 0.960 |
| `constraint_probe` | 73.028 | 70.940 | 0.971 | 0.965–0.978 | 0.971, 0.971, 0.973 |
| `dp_row` | 42.940 | 41.402 | 0.964 | 0.962–0.968 | 0.964, 0.965, 0.966 |
| `record_update` | 15.859 | 10.595 | 0.666 | 0.660–0.672 | 0.667, 0.667, 0.663 |
| `recursive_loops` | 16.093 | 15.482 | 0.965 | 0.955–0.973 | 0.969, 0.961, 0.964 |
| `string_count` | 26.250 | 25.850 | 0.986 | 0.978–0.989 | 0.988, 0.983, 0.985 |
| `substring_count` | 19.639 | 18.977 | 0.977 | 0.956–1.011 | 0.970, 0.977, 0.985 |
| `tree_build` | 30.811 | 29.921 | 0.973 | 0.969–0.979 | 0.975, 0.973, 0.973 |
| `while_search` | 58.449 | 52.625 | 0.898 | 0.890–0.904 | 0.900, 0.896, 0.898 |

algorithm: geometric mean B/A **0.971** (16 cases).
hotspot: geometric mean B/A **0.941** (27 cases).

Sources are identical between A and B; no benchmark rewrite is used to obtain a speedup.
