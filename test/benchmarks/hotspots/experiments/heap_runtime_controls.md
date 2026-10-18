# Runtime A/B results

Measured at: 2026-10-17T11:04:11.619991+00:00

System: `macOS-27.0.1-arm64-arm-64bit-Mach-O`

A (baseline): `2c543424a5884d5a647a99e8cf7b3134d170920aa714813705e4ae0d1e00dc1a`

B (candidate): `80d8465438372b58270f8201cdc5cd9cfc244cee0b5d50ba5ff1da75470072c4`

Each runtime gets one discarded warm-up and 7 measured runs per case in each of 3 rounds. Fresh processes run sequentially in alternating A/B order. Times are internal process CPU times; startup and compilation are excluded. Every checksum must agree across both runtimes and all runs.

Successive rounds reverse the first runtime and shuffle case order with a recorded seed. Round medians expose between-round variation. For an A/A noise control, pass the same binary twice.

B/A is the median of paired time ratios; below 1 means the candidate is faster. The IQR describes the middle half of paired ratios, not a confidence interval. A near-1 ratio should be treated as noise unless repeated measurements establish otherwise.

| Case | A median ms | B median ms | Paired B/A | Paired IQR | Round medians |
| --- | ---: | ---: | ---: | --- | --- |
| `recursive_fibonacci` | 4.192 | 4.177 | 0.992 | 0.972–1.015 | 0.983, 1.010, 0.992 |
| `sieve` | 0.089 | 0.088 | 0.976 | 0.922–1.024 | 0.989, 0.963, 0.989 |
| `merge_sort` | 4.778 | 4.784 | 0.994 | 0.978–1.028 | 0.990, 0.994, 1.001 |
| `n_queens` | 67.827 | 67.712 | 0.998 | 0.989–1.005 | 0.997, 1.001, 0.998 |
| `longest_common_subsequence` | 14.529 | 14.482 | 1.001 | 0.991–1.008 | 1.004, 0.991, 1.000 |
| `matrix_multiply` | 3.052 | 3.063 | 1.002 | 0.988–1.016 | 0.999, 0.996, 1.008 |
| `sudoku` | 360.543 | 360.773 | 1.000 | 0.997–1.003 | 1.001, 1.000, 0.999 |
| `dijkstra` | 8.100 | 6.942 | 0.860 | 0.848–0.871 | 0.844, 0.857, 0.871 |
| `optimize` | 18.344 | 18.338 | 1.001 | 0.992–1.010 | 1.010, 0.997, 1.001 |
| `kmp` | 20.781 | 20.887 | 1.004 | 1.000–1.015 | 1.005, 1.005, 1.001 |
| `k_nucleotide` | 109.885 | 108.230 | 0.989 | 0.977–0.995 | 0.992, 0.978, 0.991 |
| `binary_trees` | 723.810 | 722.239 | 0.997 | 0.995–1.000 | 0.996, 0.999, 0.997 |
| `lru_cache` | 42.341 | 42.123 | 0.994 | 0.989–1.013 | 0.981, 0.999, 0.994 |
| `huffman` | 2.057 | 2.065 | 0.995 | 0.976–1.034 | 0.982, 1.000, 1.035 |
| `fannkuch` | 39.461 | 39.521 | 1.003 | 0.997–1.009 | 1.000, 0.997, 1.005 |
| `n_body` | 24.157 | 24.058 | 1.000 | 0.992–1.005 | 0.995, 1.001, 0.997 |
| `vm_hot_paths` | 74.661 | 74.743 | 1.000 | 0.991–1.007 | 1.001, 0.996, 1.001 |
| `float_arithmetic` | 21.986 | 22.135 | 1.007 | 0.999–1.014 | 1.013, 1.007, 1.002 |
| `function_call_baseline` | 2.739 | 2.754 | 1.002 | 0.968–1.022 | 1.007, 1.014, 0.994 |
| `function_calls` | 6.679 | 6.675 | 1.001 | 0.993–1.017 | 0.999, 0.995, 1.002 |
| `function_callbacks` | 12.263 | 12.206 | 0.996 | 0.984–1.005 | 0.991, 0.998, 1.000 |
| `list_access` | 6.281 | 6.194 | 0.986 | 0.970–1.014 | 0.975, 1.004, 0.986 |
| `list_append` | 7.559 | 7.605 | 1.009 | 0.996–1.021 | 1.009, 1.010, 1.005 |
| `list_update` | 4.828 | 4.827 | 1.002 | 0.980–1.015 | 1.014, 1.003, 0.991 |
| `list_copy` | 4.866 | 4.827 | 1.011 | 0.948–1.034 | 1.006, 1.030, 1.011 |
| `list_from_iterable` | 10.205 | 10.207 | 1.000 | 0.973–1.006 | 1.001, 1.000, 0.986 |
| `list_replicate` | 5.546 | 5.478 | 1.000 | 0.979–1.023 | 1.033, 0.995, 0.983 |
| `dict_insert` | 7.279 | 7.156 | 0.991 | 0.976–1.014 | 0.984, 1.019, 0.976 |
| `dict_lookup` | 5.887 | 5.863 | 0.994 | 0.978–1.007 | 0.982, 0.996, 1.004 |
| `dict_update` | 4.807 | 4.775 | 0.997 | 0.987–1.007 | 0.987, 0.997, 1.000 |
| `dict_delete` | 3.618 | 3.592 | 0.990 | 0.974–0.999 | 0.990, 0.986, 0.994 |
| `dict_literal` | 16.854 | 16.859 | 1.000 | 0.991–1.010 | 1.001, 0.998, 1.000 |
| `dict_counting` | 19.881 | 19.901 | 0.999 | 0.995–1.012 | 1.009, 0.998, 0.996 |
| `tail_recursion` | 1.789 | 1.790 | 1.000 | 0.966–1.033 | 0.988, 0.998, 1.031 |
| `branch_logic` | 21.926 | 22.001 | 0.998 | 0.987–1.010 | 0.997, 1.001, 0.990 |
| `constraint_probe` | 69.342 | 69.337 | 1.000 | 0.991–1.007 | 1.005, 0.998, 1.007 |
| `dp_row` | 40.636 | 40.903 | 1.003 | 0.999–1.016 | 0.999, 1.005, 1.005 |
| `heap_callback` | 11.253 | 8.766 | 0.779 | 0.776–0.788 | 0.779, 0.782, 0.777 |
| `record_update` | 10.571 | 10.551 | 1.000 | 0.994–1.005 | 0.997, 1.001, 1.001 |
| `recursive_loops` | 15.429 | 15.352 | 0.993 | 0.983–1.005 | 0.993, 1.002, 0.986 |
| `string_count` | 24.474 | 24.281 | 0.997 | 0.990–1.003 | 0.998, 0.996, 0.999 |
| `substring_count` | 18.935 | 18.908 | 0.992 | 0.980–1.042 | 1.023, 0.992, 0.988 |
| `tree_build` | 29.293 | 29.139 | 0.992 | 0.987–0.999 | 0.999, 0.988, 0.992 |
| `while_search` | 52.644 | 52.570 | 0.999 | 0.995–1.006 | 1.001, 0.999, 0.998 |

algorithm: geometric mean B/A **0.987** (16 cases).
hotspot: geometric mean B/A **0.990** (28 cases).

Sources are identical between A and B; no benchmark rewrite is used to obtain a speedup.
