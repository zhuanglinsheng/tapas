# Runtime A/B results

Measured at: 2026-10-17T15:24:12.990722+00:00

System: `macOS-27.0.1-arm64-arm-64bit-Mach-O`

A (baseline): `e122256812628f8fb3001a34988ecc8947d1def236ecb201e514141611ee89c0`

B (candidate): `17cd65cd3faf59131d8f126ee339aec8c3e1e32aa78fcd141e53fb91fb30752d`

Each runtime gets one discarded warm-up and 9 measured runs per case in each of 3 rounds. Fresh processes run sequentially in alternating A/B order. Times are internal process CPU times; startup and compilation are excluded. Every checksum must agree across both runtimes and all runs.

Successive rounds reverse the first runtime and shuffle case order with a recorded seed. Round medians expose between-round variation. For an A/A noise control, pass the same binary twice.

B/A is the median of paired time ratios; below 1 means the candidate is faster. The IQR describes the middle half of paired ratios, not a confidence interval. A near-1 ratio should be treated as noise unless repeated measurements establish otherwise.

| Case | A median ms | B median ms | Paired B/A | Paired IQR | Round medians |
| --- | ---: | ---: | ---: | --- | --- |
| `recursive_fibonacci` | 4.128 | 4.158 | 1.000 | 0.987–1.017 | 0.987, 1.011, 0.996 |
| `sieve` | 0.040 | 0.040 | 1.025 | 0.939–1.175 | 1.054, 1.000, 1.026 |
| `merge_sort` | 4.037 | 4.019 | 0.996 | 0.983–1.024 | 0.993, 1.015, 1.020 |
| `n_queens` | 67.669 | 67.603 | 0.996 | 0.991–1.003 | 0.995, 0.995, 0.998 |
| `longest_common_subsequence` | 13.283 | 13.318 | 0.999 | 0.985–1.022 | 0.993, 1.002, 1.002 |
| `matrix_multiply` | 3.051 | 3.077 | 1.006 | 0.980–1.025 | 1.011, 0.997, 1.018 |
| `sudoku` | 257.635 | 257.738 | 1.002 | 0.992–1.008 | 1.005, 1.004, 1.000 |
| `dijkstra` | 4.035 | 4.037 | 1.001 | 0.969–1.041 | 1.001, 1.001, 1.000 |
| `optimize` | 18.318 | 18.341 | 1.003 | 0.994–1.019 | 1.019, 1.003, 1.002 |
| `kmp` | 17.341 | 17.279 | 0.995 | 0.990–1.002 | 0.992, 0.997, 0.998 |
| `k_nucleotide` | 91.419 | 92.408 | 1.003 | 0.994–1.009 | 1.001, 1.000, 1.003 |
| `binary_trees` | 544.396 | 544.375 | 1.001 | 0.990–1.012 | 0.990, 1.001, 1.008 |
| `lru_cache` | 42.420 | 42.480 | 1.002 | 0.996–1.012 | 1.001, 0.998, 1.005 |
| `huffman` | 0.079 | 0.078 | 0.987 | 0.937–1.068 | 1.012, 0.940, 0.987 |
| `fannkuch` | 39.558 | 39.564 | 1.002 | 0.997–1.013 | 1.013, 1.002, 0.999 |
| `n_body` | 24.192 | 24.079 | 0.994 | 0.986–1.007 | 1.006, 0.991, 0.994 |
| `list_sort` | 3.808 | 3.719 | 0.976 | 0.939–1.019 | 0.968, 0.973, 0.991 |
| `heap_order` | 2.771 | 2.792 | 1.004 | 0.973–1.030 | 0.977, 0.987, 1.019 |
| `heap_callback` | 8.780 | 8.752 | 1.002 | 0.983–1.010 | 1.007, 0.998, 0.998 |

algorithm: geometric mean B/A **1.001** (16 cases).
hotspot: geometric mean B/A **0.994** (3 cases).

Sources are identical between A and B; no benchmark rewrite is used to obtain a speedup.
