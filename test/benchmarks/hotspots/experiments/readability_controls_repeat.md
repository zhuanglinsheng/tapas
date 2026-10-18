# Runtime A/B results

Measured at: 2026-10-17T14:52:05.756657+00:00

System: `macOS-27.0.1-arm64-arm-64bit-Mach-O`

A (baseline): `bb1ba3e66bb6bb36cf8b4be0a2ffe345fc1d167322a928ad649251d4e3417a7a`

B (candidate): `3e4cd12f3fdad4264a87719da4a041ea577684fdf749704f0d3939076602db2b`

Each runtime gets one discarded warm-up and 11 measured runs per case in each of 5 rounds. Fresh processes run sequentially in alternating A/B order. Times are internal process CPU times; startup and compilation are excluded. Every checksum must agree across both runtimes and all runs.

Successive rounds reverse the first runtime and shuffle case order with a recorded seed. Round medians expose between-round variation. For an A/A noise control, pass the same binary twice.

B/A is the median of paired time ratios; below 1 means the candidate is faster. The IQR describes the middle half of paired ratios, not a confidence interval. A near-1 ratio should be treated as noise unless repeated measurements establish otherwise.

| Case | A median ms | B median ms | Paired B/A | Paired IQR | Round medians |
| --- | ---: | ---: | ---: | --- | --- |
| `list_sort` | 3.786 | 3.895 | 1.034 | 1.006–1.062 | 1.047, 1.034, 1.018, 1.048, 1.032 |
| `heap_order` | 2.798 | 2.812 | 1.006 | 0.975–1.045 | 1.008, 0.975, 1.019, 0.980, 1.006 |
| `k_nucleotide` | 91.720 | 92.652 | 1.010 | 1.000–1.018 | 1.001, 1.010, 1.016, 1.008, 1.014 |

algorithm: geometric mean B/A **1.010** (1 cases).
hotspot: geometric mean B/A **1.020** (2 cases).

Sources are identical between A and B; no benchmark rewrite is used to obtain a speedup.
