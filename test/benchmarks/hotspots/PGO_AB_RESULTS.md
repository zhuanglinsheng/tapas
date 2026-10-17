> 本报告对应热路径语义审查前的输入；dp_row 与 dict_counting 已修订。历史源码见 experiments/pre_hotspot_audit_sources.json，当前跨语言结果及测量范围见 [AUDIT.md](AUDIT.md)。

# Runtime A/B results

Measured at: 2026-10-17T08:11:25.237911+00:00

System: `macOS-27.0.1-arm64-arm-64bit-Mach-O`

A (baseline): `c76702ab58d12126884e8a56669a40cb1b184d861fff626ba5ae09a9d8e6bb90`

B (candidate): `6906b386baa5a91b88f69c86f207f66e17d64641e0945866a1531d02623bc21b`

Each runtime gets one discarded warm-up and 5 measured runs per case in each of 3 rounds. Fresh processes run sequentially in alternating A/B order. Times are internal process CPU times; startup and compilation are excluded. Every checksum must agree across both runtimes and all runs.

Successive rounds reverse the first runtime and shuffle case order with a recorded seed. Round medians expose between-round variation. For an A/A noise control, pass the same binary twice.

B/A is the median of paired time ratios; below 1 means the candidate is faster. The IQR describes the middle half of paired ratios, not a confidence interval. A near-1 ratio should be treated as noise unless repeated measurements establish otherwise.

| Case | A median ms | B median ms | Paired B/A | Paired IQR | Round medians |
| --- | ---: | ---: | ---: | --- | --- |
| `recursive_fibonacci` | 4.235 | 3.611 | 0.855 | 0.833–0.870 | 0.848, 0.891, 0.829 |
| `sieve` | 1.036 | 1.018 | 0.983 | 0.938–1.002 | 0.908, 0.983, 0.987 |
| `merge_sort` | 4.816 | 4.507 | 0.933 | 0.912–0.949 | 0.940, 0.912, 0.949 |
| `n_queens` | 68.624 | 67.815 | 0.981 | 0.976–0.988 | 0.976, 0.978, 0.988 |
| `longest_common_subsequence` | 14.649 | 14.062 | 0.960 | 0.959–0.967 | 0.965, 0.960, 0.964 |
| `matrix_multiply` | 3.056 | 3.001 | 0.977 | 0.968–0.985 | 0.987, 0.977, 0.974 |
| `sudoku` | 362.296 | 349.217 | 0.966 | 0.960–0.971 | 0.972, 0.963, 0.966 |
| `dijkstra` | 19.623 | 19.122 | 0.975 | 0.947–0.985 | 0.984, 0.977, 0.945 |
| `optimize` | 18.304 | 17.711 | 0.966 | 0.965–0.972 | 0.966, 0.967, 0.966 |
| `kmp` | 20.828 | 20.415 | 0.977 | 0.971–0.983 | 0.983, 0.977, 0.976 |
| `k_nucleotide` | 109.212 | 106.792 | 0.976 | 0.970–0.988 | 0.985, 0.970, 0.973 |
| `binary_trees` | 738.081 | 700.149 | 0.946 | 0.940–0.949 | 0.940, 0.949, 0.945 |
| `lru_cache` | 42.251 | 41.697 | 0.989 | 0.981–0.992 | 0.986, 0.992, 0.989 |
| `huffman` | 2.084 | 2.000 | 0.959 | 0.950–0.963 | 0.963, 0.955, 0.958 |
| `fannkuch` | 40.206 | 37.940 | 0.948 | 0.944–0.953 | 0.950, 0.953, 0.946 |
| `n_body` | 24.342 | 22.917 | 0.939 | 0.929–0.947 | 0.922, 0.936, 0.944 |
| `vm_hot_paths` | 75.856 | 74.354 | 0.992 | 0.972–0.994 | 0.993, 0.992, 0.985 |
| `float_arithmetic` | 22.311 | 21.827 | 0.971 | 0.964–0.977 | 0.966, 0.971, 0.975 |
| `function_call_baseline` | 2.710 | 2.686 | 0.985 | 0.970–1.002 | 0.970, 0.990, 1.002 |
| `function_calls` | 6.695 | 6.353 | 0.946 | 0.933–0.963 | 0.963, 0.935, 0.941 |
| `function_callbacks` | 12.290 | 11.780 | 0.960 | 0.950–0.966 | 0.960, 0.955, 0.961 |
| `list_access` | 6.355 | 4.556 | 0.717 | 0.704–0.738 | 0.719, 0.738, 0.704 |
| `list_append` | 7.670 | 6.993 | 0.912 | 0.903–0.926 | 0.907, 0.921, 0.906 |
| `list_update` | 4.851 | 4.884 | 1.008 | 0.983–1.017 | 1.000, 1.009, 1.006 |
| `list_copy` | 4.994 | 4.898 | 0.977 | 0.945–1.021 | 0.959, 0.977, 0.977 |
| `list_from_iterable` | 10.192 | 10.085 | 0.987 | 0.972–0.991 | 0.972, 0.991, 0.987 |
| `list_replicate` | 5.532 | 5.500 | 0.990 | 0.965–1.021 | 1.021, 0.973, 0.973 |
| `dict_insert` | 7.134 | 7.251 | 1.002 | 0.990–1.031 | 1.012, 1.018, 0.997 |
| `dict_lookup` | 5.937 | 5.525 | 0.939 | 0.923–0.954 | 0.953, 0.939, 0.927 |
| `dict_update` | 4.806 | 5.281 | 1.097 | 1.075–1.106 | 1.097, 1.089, 1.108 |
| `dict_delete` | 3.622 | 3.550 | 0.988 | 0.965–1.005 | 0.965, 0.985, 0.989 |
| `dict_literal` | 17.176 | 16.783 | 0.985 | 0.975–0.992 | 0.988, 0.985, 0.977 |
| `dict_counting` | 16.832 | 16.869 | 1.004 | 0.999–1.018 | 1.010, 1.004, 1.000 |
| `tail_recursion` | 1.811 | 1.530 | 0.850 | 0.843–0.856 | 0.855, 0.850, 0.846 |
| `branch_logic` | 22.335 | 22.601 | 1.013 | 1.006–1.021 | 1.021, 1.014, 1.006 |
| `constraint_probe` | 70.104 | 67.160 | 0.957 | 0.947–0.974 | 0.969, 0.947, 0.954 |
| `dp_row` | 41.690 | 38.968 | 0.936 | 0.930–0.945 | 0.939, 0.930, 0.937 |
| `record_update` | 10.842 | 9.115 | 0.835 | 0.827–0.856 | 0.835, 0.829, 0.856 |
| `recursive_loops` | 15.789 | 14.251 | 0.906 | 0.900–0.912 | 0.913, 0.900, 0.907 |
| `string_count` | 24.846 | 23.777 | 0.957 | 0.954–0.965 | 0.958, 0.954, 0.957 |
| `substring_count` | 19.602 | 18.674 | 0.947 | 0.872–0.980 | 0.911, 0.956, 0.947 |
| `tree_build` | 29.763 | 28.166 | 0.939 | 0.932–0.949 | 0.949, 0.935, 0.940 |
| `while_search` | 52.897 | 53.155 | 1.004 | 1.000–1.024 | 1.013, 1.001, 1.004 |

algorithm: geometric mean B/A **0.958** (16 cases).
hotspot: geometric mean B/A **0.953** (27 cases).

Sources are identical between A and B; no benchmark rewrite is used to obtain a speedup.
