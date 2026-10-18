# Runtime A/B results

Measured at: 2026-10-17T15:03:41.673975+00:00

System: `macOS-27.0.1-arm64-arm-64bit-Mach-O`

A (baseline): `e122256812628f8fb3001a34988ecc8947d1def236ecb201e514141611ee89c0`

B (candidate): `e144aa797300470d02d21e7aa573011ef8cbc2ab96eb3795cee46f5559ccb002`

Each runtime gets one discarded warm-up and 11 measured runs per case in each of 3 rounds. Fresh processes run sequentially in alternating A/B order. Times are internal process CPU times; startup and compilation are excluded. Every checksum must agree across both runtimes and all runs.

Successive rounds reverse the first runtime and shuffle case order with a recorded seed. Round medians expose between-round variation. For an A/A noise control, pass the same binary twice.

B/A is the median of paired time ratios; below 1 means the candidate is faster. The IQR describes the middle half of paired ratios, not a confidence interval. A near-1 ratio should be treated as noise unless repeated measurements establish otherwise.

| Case | A median ms | B median ms | Paired B/A | Paired IQR | Round medians |
| --- | ---: | ---: | ---: | --- | --- |
| `list_sort` | 3.720 | 3.858 | 1.026 | 1.002–1.055 | 1.015, 1.030, 1.045 |
| `heap_order` | 2.782 | 2.825 | 1.002 | 0.982–1.035 | 1.002, 1.001, 1.009 |
| `k_nucleotide` | 91.930 | 92.702 | 1.012 | 0.999–1.020 | 1.004, 1.019, 1.013 |

algorithm: geometric mean B/A **1.012** (1 cases).
hotspot: geometric mean B/A **1.014** (2 cases).

Sources are identical between A and B; no benchmark rewrite is used to obtain a speedup.
