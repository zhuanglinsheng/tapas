# Runtime A/B results

Measured at: 2026-10-17T14:54:47.014796+00:00

System: `macOS-27.0.1-arm64-arm-64bit-Mach-O`

A (baseline): `bb1ba3e66bb6bb36cf8b4be0a2ffe345fc1d167322a928ad649251d4e3417a7a`

B (candidate): `e122256812628f8fb3001a34988ecc8947d1def236ecb201e514141611ee89c0`

Each runtime gets one discarded warm-up and 7 measured runs per case in each of 3 rounds. Fresh processes run sequentially in alternating A/B order. Times are internal process CPU times; startup and compilation are excluded. Every checksum must agree across both runtimes and all runs.

Successive rounds reverse the first runtime and shuffle case order with a recorded seed. Round medians expose between-round variation. For an A/A noise control, pass the same binary twice.

B/A is the median of paired time ratios; below 1 means the candidate is faster. The IQR describes the middle half of paired ratios, not a confidence interval. A near-1 ratio should be treated as noise unless repeated measurements establish otherwise.

| Case | A median ms | B median ms | Paired B/A | Paired IQR | Round medians |
| --- | ---: | ---: | ---: | --- | --- |
| `recursive_fibonacci` | 4.217 | 4.188 | 1.005 | 0.981–1.033 | 0.989, 1.013, 0.993 |
| `sieve` | 0.040 | 0.040 | 1.024 | 0.962–1.108 | 1.024, 1.000, 1.111 |
| `merge_sort` | 4.088 | 4.029 | 0.976 | 0.966–1.019 | 0.975, 1.001, 0.991 |
| `n_queens` | 68.562 | 68.153 | 0.998 | 0.986–1.007 | 1.002, 0.986, 0.999 |
| `longest_common_subsequence` | 13.762 | 13.782 | 1.000 | 0.985–1.035 | 1.085, 1.000, 0.988 |
| `matrix_multiply` | 3.175 | 3.143 | 1.003 | 0.962–1.030 | 1.002, 0.980, 1.018 |
| `sudoku` | 260.857 | 259.742 | 0.997 | 0.986–1.006 | 0.997, 1.000, 0.997 |
| `dijkstra` | 4.010 | 4.033 | 1.006 | 0.982–1.016 | 0.986, 1.006, 1.010 |
| `optimize` | 18.355 | 18.477 | 1.006 | 0.995–1.026 | 1.006, 1.003, 1.026 |
| `kmp` | 17.528 | 17.454 | 0.999 | 0.987–1.010 | 1.004, 0.999, 0.992 |
| `k_nucleotide` | 90.858 | 90.778 | 1.001 | 0.996–1.005 | 1.001, 1.004, 1.001 |
| `binary_trees` | 543.810 | 543.379 | 1.001 | 0.983–1.011 | 1.011, 0.982, 1.001 |
| `lru_cache` | 42.309 | 42.426 | 1.001 | 0.986–1.020 | 1.001, 0.985, 1.009 |
| `huffman` | 0.080 | 0.077 | 1.000 | 0.925–1.073 | 0.949, 0.976, 1.013 |
| `fannkuch` | 40.022 | 40.074 | 0.996 | 0.986–1.007 | 0.991, 1.002, 0.989 |
| `n_body` | 24.585 | 24.673 | 1.006 | 0.988–1.018 | 1.007, 0.981, 1.011 |
| `list_sort` | 3.798 | 3.777 | 1.006 | 0.961–1.050 | 1.028, 0.985, 1.024 |
| `heap_order` | 2.848 | 2.876 | 0.991 | 0.954–1.038 | 1.007, 0.974, 1.010 |
| `heap_callback` | 8.823 | 8.759 | 0.993 | 0.976–1.016 | 0.984, 1.010, 0.995 |

algorithm: geometric mean B/A **1.001** (16 cases).
hotspot: geometric mean B/A **0.997** (3 cases).

Sources are identical between A and B; no benchmark rewrite is used to obtain a speedup.
