# Runtime A/B results

Measured at: 2026-10-17T15:00:29.604838+00:00

System: `macOS-27.0.1-arm64-arm-64bit-Mach-O`

A (baseline): `e122256812628f8fb3001a34988ecc8947d1def236ecb201e514141611ee89c0`

B (candidate): `876296986ddc36f92d70a33d6585f788bc2fac2db0ece53cf422daed82bb21a0`

Each runtime gets one discarded warm-up and 9 measured runs per case in each of 3 rounds. Fresh processes run sequentially in alternating A/B order. Times are internal process CPU times; startup and compilation are excluded. Every checksum must agree across both runtimes and all runs.

Successive rounds reverse the first runtime and shuffle case order with a recorded seed. Round medians expose between-round variation. For an A/A noise control, pass the same binary twice.

B/A is the median of paired time ratios; below 1 means the candidate is faster. The IQR describes the middle half of paired ratios, not a confidence interval. A near-1 ratio should be treated as noise unless repeated measurements establish otherwise.

| Case | A median ms | B median ms | Paired B/A | Paired IQR | Round medians |
| --- | ---: | ---: | ---: | --- | --- |
| `list_sort` | 3.791 | 3.762 | 1.010 | 0.976–1.025 | 1.010, 1.008, 1.011 |
| `heap_order` | 2.751 | 2.774 | 0.995 | 0.979–1.026 | 0.993, 0.995, 1.020 |

hotspot: geometric mean B/A **1.003** (2 cases).

Sources are identical between A and B; no benchmark rewrite is used to obtain a speedup.
