# Runtime A/B results

Measured at: 2026-10-17T14:51:20.438645+00:00

System: `macOS-27.0.1-arm64-arm-64bit-Mach-O`

A (baseline): `bb1ba3e66bb6bb36cf8b4be0a2ffe345fc1d167322a928ad649251d4e3417a7a`

B (candidate): `3e4cd12f3fdad4264a87719da4a041ea577684fdf749704f0d3939076602db2b`

Each runtime gets one discarded warm-up and 7 measured runs per case in each of 3 rounds. Fresh processes run sequentially in alternating A/B order. Times are internal process CPU times; startup and compilation are excluded. Every checksum must agree across both runtimes and all runs.

Successive rounds reverse the first runtime and shuffle case order with a recorded seed. Round medians expose between-round variation. For an A/A noise control, pass the same binary twice.

B/A is the median of paired time ratios; below 1 means the candidate is faster. The IQR describes the middle half of paired ratios, not a confidence interval. A near-1 ratio should be treated as noise unless repeated measurements establish otherwise.

| Case | A median ms | B median ms | Paired B/A | Paired IQR | Round medians |
| --- | ---: | ---: | ---: | --- | --- |
| `recursive_fibonacci` | 4.285 | 4.295 | 1.004 | 0.982–1.024 | 1.004, 1.004, 1.005 |
| `sieve` | 0.040 | 0.040 | 0.976 | 0.906–1.079 | 1.023, 0.975, 0.905 |
| `merge_sort` | 4.006 | 4.036 | 1.003 | 0.988–1.022 | 1.003, 1.003, 0.997 |
| `n_queens` | 68.544 | 68.243 | 0.989 | 0.983–1.005 | 0.989, 0.995, 0.983 |
| `longest_common_subsequence` | 13.566 | 13.622 | 1.001 | 0.979–1.018 | 0.988, 0.985, 1.009 |
| `matrix_multiply` | 3.150 | 3.183 | 1.005 | 0.978–1.024 | 1.005, 1.009, 1.013 |
| `sudoku` | 258.313 | 256.716 | 0.997 | 0.986–1.001 | 0.997, 0.998, 0.987 |
| `dijkstra` | 4.036 | 3.974 | 0.988 | 0.966–1.024 | 0.980, 0.993, 0.993 |
| `optimize` | 18.509 | 18.577 | 0.998 | 0.978–1.005 | 0.991, 0.998, 1.001 |
| `kmp` | 17.470 | 17.464 | 0.999 | 0.990–1.008 | 0.998, 1.007, 0.996 |
| `k_nucleotide` | 91.852 | 93.349 | 1.018 | 1.003–1.024 | 1.013, 1.018, 1.020 |
| `binary_trees` | 540.705 | 542.372 | 1.000 | 0.992–1.015 | 0.986, 1.003, 0.999 |
| `lru_cache` | 43.005 | 43.054 | 1.004 | 0.993–1.017 | 0.993, 1.009, 1.004 |
| `huffman` | 0.080 | 0.079 | 0.975 | 0.945–1.033 | 0.962, 1.000, 0.966 |
| `fannkuch` | 39.931 | 40.010 | 1.002 | 0.985–1.014 | 1.003, 0.994, 1.002 |
| `n_body` | 25.211 | 24.972 | 0.989 | 0.977–1.005 | 0.984, 0.988, 0.997 |
| `list_sort` | 3.900 | 4.045 | 1.047 | 0.989–1.080 | 1.067, 1.008, 1.010 |
| `heap_order` | 2.818 | 2.823 | 1.011 | 0.973–1.047 | 1.032, 0.975, 1.054 |
| `heap_callback` | 8.756 | 8.841 | 1.000 | 0.985–1.025 | 0.999, 0.990, 1.024 |

algorithm: geometric mean B/A **0.997** (16 cases).
hotspot: geometric mean B/A **1.019** (3 cases).

Sources are identical between A and B; no benchmark rewrite is used to obtain a speedup.
