# Runtime A/B results

Measured at: 2026-10-17T07:41:10.755927+00:00

System: `macOS-27.0.1-arm64-arm-64bit-Mach-O`

A (baseline): `c76702ab58d12126884e8a56669a40cb1b184d861fff626ba5ae09a9d8e6bb90`

B (candidate): `5d96c79c880b4eb498b1275825744ba58e08b8201f3f5e3f641995ed4133a084`

Each runtime gets one discarded warm-up and 8 measured runs per case in each of 3 rounds. Fresh processes run sequentially in alternating A/B order. Times are internal process CPU times; startup and compilation are excluded. Every checksum must agree across both runtimes and all runs.

Successive rounds reverse the first runtime and shuffle case order with a recorded seed. Round medians expose between-round variation. For an A/A noise control, pass the same binary twice.

B/A is the median of paired time ratios; below 1 means the candidate is faster. The IQR describes the middle half of paired ratios, not a confidence interval. A near-1 ratio should be treated as noise unless repeated measurements establish otherwise.

| Case | A median ms | B median ms | Paired B/A | Paired IQR | Round medians |
| --- | ---: | ---: | ---: | --- | --- |
| `recursive_fibonacci` | 4.206 | 3.525 | 0.839 | 0.817–0.856 | 0.826, 0.843, 0.839 |
| `sieve` | 1.045 | 1.009 | 0.973 | 0.947–1.002 | 0.984, 0.958, 0.999 |
| `merge_sort` | 4.811 | 4.508 | 0.928 | 0.911–0.945 | 0.932, 0.933, 0.917 |
| `n_queens` | 127.043 | 125.957 | 0.993 | 0.972–0.996 | 0.995, 0.989, 0.971 |
| `longest_common_subsequence` | 14.790 | 13.971 | 0.945 | 0.936–0.963 | 0.949, 0.951, 0.941 |
| `matrix_multiply` | 3.123 | 3.035 | 0.990 | 0.965–1.019 | 0.993, 0.992, 0.967 |
| `sudoku` | 363.758 | 351.320 | 0.966 | 0.948–0.971 | 0.968, 0.965, 0.968 |
| `dijkstra` | 19.837 | 19.232 | 0.973 | 0.959–0.989 | 0.981, 0.979, 0.961 |
| `optimize` | 18.491 | 18.107 | 0.975 | 0.967–0.986 | 0.967, 0.987, 0.977 |
| `kmp` | 20.858 | 20.349 | 0.976 | 0.964–0.985 | 0.975, 0.983, 0.963 |
| `k_nucleotide` | 140.076 | 144.580 | 1.036 | 1.026–1.047 | 1.037, 1.033, 1.034 |
| `binary_trees` | 735.387 | 697.164 | 0.943 | 0.936–0.957 | 0.947, 0.937, 0.946 |
| `lru_cache` | 42.216 | 41.566 | 0.984 | 0.978–0.988 | 0.983, 0.984, 0.985 |
| `huffman` | 2.072 | 2.003 | 0.970 | 0.957–0.976 | 0.965, 0.969, 0.973 |
| `fannkuch` | 40.326 | 38.142 | 0.945 | 0.939–0.955 | 0.944, 0.951, 0.944 |
| `n_body` | 24.172 | 22.845 | 0.942 | 0.934–0.951 | 0.941, 0.940, 0.944 |
| `vm_hot_paths` | 75.159 | 74.828 | 0.993 | 0.982–1.001 | 0.993, 0.995, 0.987 |
| `float_arithmetic` | 22.164 | 21.723 | 0.981 | 0.970–0.994 | 0.987, 0.978, 0.984 |
| `function_call_baseline` | 2.703 | 2.704 | 0.986 | 0.963–1.019 | 1.006, 0.984, 0.972 |
| `function_calls` | 6.730 | 6.402 | 0.955 | 0.945–0.964 | 0.955, 0.962, 0.954 |
| `function_callbacks` | 12.264 | 11.716 | 0.952 | 0.945–0.960 | 0.952, 0.957, 0.948 |
| `list_access` | 6.236 | 4.573 | 0.730 | 0.709–0.747 | 0.752, 0.714, 0.730 |
| `list_append` | 7.721 | 7.079 | 0.926 | 0.894–0.941 | 0.908, 0.937, 0.923 |
| `list_update` | 4.934 | 4.902 | 0.993 | 0.971–1.018 | 0.988, 0.996, 1.004 |
| `list_copy` | 4.952 | 4.960 | 0.999 | 0.972–1.038 | 0.988, 1.016, 0.991 |
| `list_from_iterable` | 10.252 | 10.264 | 1.000 | 0.992–1.013 | 1.005, 0.999, 0.999 |
| `list_replicate` | 5.512 | 5.415 | 0.978 | 0.955–1.000 | 0.967, 0.992, 0.968 |
| `dict_insert` | 7.409 | 7.556 | 1.009 | 0.993–1.031 | 1.004, 1.005, 1.017 |
| `dict_lookup` | 5.981 | 5.689 | 0.937 | 0.919–0.968 | 0.936, 0.945, 0.933 |
| `dict_update` | 4.796 | 5.349 | 1.108 | 1.089–1.131 | 1.107, 1.113, 1.114 |
| `dict_delete` | 3.623 | 3.534 | 0.979 | 0.966–0.997 | 0.987, 0.972, 0.974 |
| `dict_literal` | 17.003 | 16.594 | 0.971 | 0.968–0.978 | 0.970, 0.973, 0.974 |
| `dict_counting` | 19.424 | 20.431 | 1.055 | 1.036–1.072 | 1.071, 1.053, 1.052 |
| `tail_recursion` | 1.807 | 1.521 | 0.846 | 0.828–0.860 | 0.858, 0.845, 0.826 |
| `branch_logic` | 22.412 | 22.695 | 1.009 | 0.999–1.016 | 1.012, 1.013, 1.002 |
| `constraint_probe` | 69.654 | 67.107 | 0.961 | 0.954–0.969 | 0.960, 0.965, 0.959 |
| `dp_row` | 41.739 | 39.009 | 0.928 | 0.922–0.940 | 0.932, 0.929, 0.927 |
| `record_update` | 10.726 | 8.980 | 0.832 | 0.827–0.845 | 0.829, 0.832, 0.839 |
| `recursive_loops` | 15.517 | 14.045 | 0.907 | 0.903–0.914 | 0.906, 0.909, 0.905 |
| `string_count` | 24.634 | 23.488 | 0.956 | 0.947–0.962 | 0.956, 0.954, 0.957 |
| `substring_count` | 18.836 | 18.191 | 0.971 | 0.927–1.023 | 0.966, 0.959, 0.974 |
| `tree_build` | 29.514 | 27.665 | 0.942 | 0.936–0.953 | 0.941, 0.942, 0.948 |
| `while_search` | 53.435 | 48.570 | 0.907 | 0.899–0.913 | 0.910, 0.900, 0.906 |

algorithm: geometric mean B/A **0.960** (16 cases).
hotspot: geometric mean B/A **0.953** (27 cases).

Sources are identical between A and B; no benchmark rewrite is used to obtain a speedup.
