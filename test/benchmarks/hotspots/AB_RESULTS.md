> 本报告对应热路径语义审查前的输入；dp_row 与 dict_counting 已修订。历史源码见 experiments/pre_hotspot_audit_sources.json，当前跨语言结果及测量范围见 [AUDIT.md](AUDIT.md)。

# Runtime A/B results

Measured at: 2026-10-17T08:10:04.322839+00:00

System: `macOS-27.0.1-arm64-arm-64bit-Mach-O`

A (baseline): `2f6ad6c89b390bd58c387aca7593b7ba61679803c57b2f1097174c2a6adaf967`

B (candidate): `c76702ab58d12126884e8a56669a40cb1b184d861fff626ba5ae09a9d8e6bb90`

Each runtime gets one discarded warm-up and 5 measured runs per case in each of 3 rounds. Fresh processes run sequentially in alternating A/B order. Times are internal process CPU times; startup and compilation are excluded. Every checksum must agree across both runtimes and all runs.

Successive rounds reverse the first runtime and shuffle case order with a recorded seed. Round medians expose between-round variation. For an A/A noise control, pass the same binary twice.

B/A is the median of paired time ratios; below 1 means the candidate is faster. The IQR describes the middle half of paired ratios, not a confidence interval. A near-1 ratio should be treated as noise unless repeated measurements establish otherwise.

| Case | A median ms | B median ms | Paired B/A | Paired IQR | Round medians |
| --- | ---: | ---: | ---: | --- | --- |
| `recursive_fibonacci` | 4.368 | 4.220 | 0.968 | 0.924–1.012 | 0.978, 0.986, 0.947 |
| `sieve` | 1.090 | 1.036 | 0.953 | 0.936–0.964 | 0.944, 0.957, 0.956 |
| `merge_sort` | 5.376 | 4.809 | 0.902 | 0.864–0.911 | 0.903, 0.906, 0.862 |
| `n_queens` | 74.667 | 68.816 | 0.920 | 0.915–0.927 | 0.918, 0.915, 0.929 |
| `longest_common_subsequence` | 17.009 | 14.696 | 0.863 | 0.855–0.870 | 0.857, 0.863, 0.870 |
| `matrix_multiply` | 3.393 | 3.111 | 0.922 | 0.886–0.989 | 0.886, 0.902, 0.989 |
| `sudoku` | 406.081 | 367.633 | 0.905 | 0.895–0.915 | 0.907, 0.903, 0.912 |
| `dijkstra` | 20.645 | 19.554 | 0.943 | 0.938–0.959 | 0.938, 0.946, 0.959 |
| `optimize` | 19.945 | 18.412 | 0.922 | 0.916–0.925 | 0.925, 0.917, 0.923 |
| `kmp` | 24.006 | 20.761 | 0.865 | 0.864–0.871 | 0.872, 0.864, 0.843 |
| `k_nucleotide` | 145.123 | 108.571 | 0.754 | 0.748–0.756 | 0.756, 0.754, 0.752 |
| `binary_trees` | 758.051 | 731.628 | 0.971 | 0.966–0.974 | 0.966, 0.971, 0.972 |
| `lru_cache` | 46.260 | 42.785 | 0.922 | 0.917–0.925 | 0.922, 0.922, 0.919 |
| `huffman` | 2.263 | 2.106 | 0.920 | 0.867–0.943 | 0.866, 0.913, 0.959 |
| `fannkuch` | 42.385 | 40.564 | 0.952 | 0.945–0.959 | 0.957, 0.952, 0.950 |
| `n_body` | 24.356 | 24.377 | 1.006 | 0.996–1.012 | 1.006, 1.008, 1.002 |
| `vm_hot_paths` | 83.693 | 75.815 | 0.901 | 0.898–0.916 | 0.903, 0.902, 0.898 |
| `float_arithmetic` | 23.411 | 22.168 | 0.945 | 0.944–0.951 | 0.945, 0.948, 0.944 |
| `function_call_baseline` | 2.780 | 2.737 | 0.990 | 0.968–1.005 | 0.990, 0.996, 0.973 |
| `function_calls` | 7.159 | 6.877 | 0.955 | 0.936–0.975 | 0.955, 0.960, 0.947 |
| `function_callbacks` | 13.382 | 12.429 | 0.925 | 0.909–0.932 | 0.913, 0.930, 0.927 |
| `list_access` | 7.284 | 6.422 | 0.880 | 0.853–0.909 | 0.900, 0.853, 0.880 |
| `list_append` | 7.965 | 7.664 | 0.963 | 0.958–0.981 | 0.961, 0.959, 0.981 |
| `list_update` | 5.796 | 4.940 | 0.854 | 0.845–0.863 | 0.848, 0.856, 0.852 |
| `list_copy` | 4.955 | 5.007 | 0.987 | 0.947–1.031 | 1.031, 0.965, 0.988 |
| `list_from_iterable` | 10.624 | 10.375 | 0.975 | 0.911–0.995 | 0.913, 0.967, 0.995 |
| `list_replicate` | 5.652 | 5.584 | 0.986 | 0.970–1.019 | 0.986, 1.001, 0.977 |
| `dict_insert` | 7.380 | 7.513 | 1.015 | 0.985–1.029 | 1.003, 1.015, 1.022 |
| `dict_lookup` | 7.474 | 5.997 | 0.807 | 0.785–0.852 | 0.792, 0.825, 0.807 |
| `dict_update` | 6.201 | 4.830 | 0.783 | 0.769–0.792 | 0.773, 0.792, 0.783 |
| `dict_delete` | 3.780 | 3.599 | 0.956 | 0.941–0.970 | 0.964, 0.944, 0.944 |
| `dict_literal` | 22.300 | 17.305 | 0.774 | 0.768–0.780 | 0.780, 0.775, 0.771 |
| `dict_counting` | 18.991 | 16.827 | 0.890 | 0.881–0.901 | 0.890, 0.889, 0.904 |
| `tail_recursion` | 1.853 | 1.824 | 0.993 | 0.962–1.001 | 0.962, 0.996, 0.997 |
| `branch_logic` | 23.988 | 22.234 | 0.925 | 0.910–0.939 | 0.910, 0.930, 0.915 |
| `constraint_probe` | 74.872 | 70.239 | 0.937 | 0.934–0.944 | 0.934, 0.934, 0.942 |
| `dp_row` | 44.070 | 41.911 | 0.957 | 0.955–0.961 | 0.957, 0.957, 0.961 |
| `record_update` | 16.066 | 10.652 | 0.664 | 0.660–0.671 | 0.663, 0.668, 0.664 |
| `recursive_loops` | 16.482 | 15.768 | 0.947 | 0.932–0.963 | 0.946, 0.947, 0.945 |
| `string_count` | 26.909 | 24.998 | 0.926 | 0.921–0.930 | 0.930, 0.927, 0.923 |
| `substring_count` | 21.396 | 20.603 | 0.959 | 0.903–1.000 | 0.909, 0.931, 1.005 |
| `tree_build` | 31.205 | 29.666 | 0.950 | 0.937–0.955 | 0.952, 0.947, 0.950 |
| `while_search` | 59.448 | 53.085 | 0.892 | 0.881–0.903 | 0.901, 0.899, 0.881 |

algorithm: geometric mean B/A **0.916** (16 cases).
hotspot: geometric mean B/A **0.912** (27 cases).

Sources are identical between A and B; no benchmark rewrite is used to obtain a speedup.
