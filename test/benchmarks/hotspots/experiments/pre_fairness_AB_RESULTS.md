# Runtime A/B results

Measured at: 2026-10-17T07:39:03.258648+00:00

System: `macOS-27.0.1-arm64-arm-64bit-Mach-O`

A (baseline): `2f6ad6c89b390bd58c387aca7593b7ba61679803c57b2f1097174c2a6adaf967`

B (candidate): `c76702ab58d12126884e8a56669a40cb1b184d861fff626ba5ae09a9d8e6bb90`

Each runtime gets one discarded warm-up and 8 measured runs per case in each of 3 rounds. Fresh processes run sequentially in alternating A/B order. Times are internal process CPU times; startup and compilation are excluded. Every checksum must agree across both runtimes and all runs.

Successive rounds reverse the first runtime and shuffle case order with a recorded seed. Round medians expose between-round variation. For an A/A noise control, pass the same binary twice.

B/A is the median of paired time ratios; below 1 means the candidate is faster. The IQR describes the middle half of paired ratios, not a confidence interval. A near-1 ratio should be treated as noise unless repeated measurements establish otherwise.

| Case | A median ms | B median ms | Paired B/A | Paired IQR | Round medians |
| --- | ---: | ---: | ---: | --- | --- |
| `recursive_fibonacci` | 4.315 | 4.172 | 0.972 | 0.952–0.986 | 0.982, 0.964, 0.963 |
| `sieve` | 1.060 | 1.028 | 0.969 | 0.958–1.003 | 0.962, 1.001, 0.968 |
| `merge_sort` | 5.313 | 4.830 | 0.906 | 0.875–0.933 | 0.907, 0.896, 0.913 |
| `n_queens` | 133.954 | 126.368 | 0.944 | 0.939–0.950 | 0.946, 0.944, 0.941 |
| `longest_common_subsequence` | 16.836 | 14.527 | 0.865 | 0.856–0.873 | 0.858, 0.867, 0.872 |
| `matrix_multiply` | 3.344 | 3.087 | 0.928 | 0.900–0.935 | 0.929, 0.916, 0.927 |
| `sudoku` | 397.437 | 359.589 | 0.903 | 0.897–0.908 | 0.898, 0.907, 0.903 |
| `dijkstra` | 20.428 | 19.198 | 0.946 | 0.932–0.955 | 0.942, 0.943, 0.953 |
| `optimize` | 19.723 | 18.345 | 0.926 | 0.921–0.934 | 0.931, 0.922, 0.932 |
| `kmp` | 23.970 | 20.657 | 0.859 | 0.852–0.870 | 0.854, 0.862, 0.864 |
| `k_nucleotide` | 183.209 | 138.220 | 0.752 | 0.749–0.761 | 0.752, 0.752, 0.759 |
| `binary_trees` | 748.076 | 723.152 | 0.968 | 0.964–0.970 | 0.966, 0.967, 0.968 |
| `lru_cache` | 45.784 | 42.010 | 0.918 | 0.914–0.926 | 0.921, 0.914, 0.919 |
| `huffman` | 2.233 | 2.062 | 0.928 | 0.909–0.944 | 0.921, 0.935, 0.913 |
| `fannkuch` | 41.587 | 39.434 | 0.948 | 0.943–0.950 | 0.948, 0.947, 0.949 |
| `n_body` | 23.906 | 24.123 | 1.009 | 1.004–1.016 | 1.008, 1.013, 1.003 |
| `vm_hot_paths` | 82.175 | 74.558 | 0.910 | 0.899–0.912 | 0.910, 0.904, 0.909 |
| `float_arithmetic` | 23.096 | 22.046 | 0.958 | 0.941–0.971 | 0.957, 0.947, 0.966 |
| `function_call_baseline` | 2.709 | 2.715 | 0.995 | 0.972–1.023 | 1.018, 0.997, 0.982 |
| `function_calls` | 7.033 | 6.688 | 0.957 | 0.940–0.967 | 0.958, 0.958, 0.953 |
| `function_callbacks` | 13.251 | 12.210 | 0.928 | 0.917–0.934 | 0.927, 0.928, 0.929 |
| `list_access` | 7.115 | 6.268 | 0.879 | 0.858–0.897 | 0.897, 0.869, 0.867 |
| `list_append` | 7.896 | 7.579 | 0.955 | 0.944–0.969 | 0.959, 0.945, 0.965 |
| `list_update` | 5.652 | 4.862 | 0.859 | 0.843–0.873 | 0.865, 0.848, 0.857 |
| `list_copy` | 4.809 | 4.769 | 0.989 | 0.968–1.025 | 0.990, 1.000, 0.980 |
| `list_from_iterable` | 10.285 | 10.137 | 0.980 | 0.921–1.003 | 0.986, 0.944, 0.987 |
| `list_replicate` | 5.564 | 5.500 | 0.978 | 0.956–1.000 | 0.997, 0.966, 0.975 |
| `dict_insert` | 7.295 | 7.143 | 0.978 | 0.952–1.003 | 0.963, 0.979, 0.986 |
| `dict_lookup` | 7.223 | 5.851 | 0.808 | 0.800–0.828 | 0.822, 0.812, 0.806 |
| `dict_update` | 6.045 | 4.792 | 0.789 | 0.779–0.805 | 0.788, 0.793, 0.789 |
| `dict_delete` | 3.731 | 3.541 | 0.950 | 0.933–0.969 | 0.940, 0.946, 0.959 |
| `dict_literal` | 21.736 | 16.904 | 0.777 | 0.771–0.783 | 0.777, 0.774, 0.778 |
| `dict_counting` | 22.823 | 19.270 | 0.857 | 0.831–0.876 | 0.870, 0.876, 0.833 |
| `tail_recursion` | 1.789 | 1.796 | 1.001 | 0.990–1.016 | 1.004, 1.000, 1.001 |
| `branch_logic` | 23.331 | 21.798 | 0.935 | 0.908–0.941 | 0.930, 0.924, 0.937 |
| `constraint_probe` | 73.594 | 69.026 | 0.938 | 0.934–0.942 | 0.941, 0.938, 0.934 |
| `dp_row` | 43.113 | 41.480 | 0.963 | 0.953–0.969 | 0.962, 0.965, 0.959 |
| `record_update` | 15.881 | 10.597 | 0.670 | 0.662–0.676 | 0.667, 0.670, 0.671 |
| `recursive_loops` | 16.232 | 15.324 | 0.947 | 0.935–0.959 | 0.953, 0.939, 0.951 |
| `string_count` | 26.431 | 24.421 | 0.927 | 0.912–0.930 | 0.912, 0.925, 0.930 |
| `substring_count` | 19.706 | 18.367 | 0.935 | 0.906–0.952 | 0.924, 0.945, 0.941 |
| `tree_build` | 30.953 | 29.268 | 0.944 | 0.937–0.954 | 0.948, 0.939, 0.951 |
| `while_search` | 58.818 | 52.724 | 0.897 | 0.888–0.908 | 0.908, 0.894, 0.899 |

algorithm: geometric mean B/A **0.919** (16 cases).
hotspot: geometric mean B/A **0.911** (27 cases).

Sources are identical between A and B; no benchmark rewrite is used to obtain a speedup.
