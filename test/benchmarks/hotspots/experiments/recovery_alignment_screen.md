# Runtime A/B results

Measured at: 2026-10-17T14:59:26.095084+00:00

System: `macOS-27.0.1-arm64-arm-64bit-Mach-O`

A (baseline): `e122256812628f8fb3001a34988ecc8947d1def236ecb201e514141611ee89c0`

B (candidate): `27924a52605303db1021e96b64e9c886d2a4660b129eb162ff2e03959d32b4a9`

Each runtime gets one discarded warm-up and 9 measured runs per case in each of 3 rounds. Fresh processes run sequentially in alternating A/B order. Times are internal process CPU times; startup and compilation are excluded. Every checksum must agree across both runtimes and all runs.

Successive rounds reverse the first runtime and shuffle case order with a recorded seed. Round medians expose between-round variation. For an A/A noise control, pass the same binary twice.

B/A is the median of paired time ratios; below 1 means the candidate is faster. The IQR describes the middle half of paired ratios, not a confidence interval. A near-1 ratio should be treated as noise unless repeated measurements establish otherwise.

| Case | A median ms | B median ms | Paired B/A | Paired IQR | Round medians |
| --- | ---: | ---: | ---: | --- | --- |
| `list_sort` | 3.796 | 3.932 | 1.023 | 0.991–1.066 | 1.023, 1.020, 1.025 |
| `heap_order` | 2.783 | 2.799 | 1.002 | 0.961–1.023 | 1.004, 0.990, 1.014 |
| `k_nucleotide` | 91.727 | 93.385 | 1.012 | 1.001–1.031 | 1.012, 1.019, 1.011 |

algorithm: geometric mean B/A **1.012** (1 cases).
hotspot: geometric mean B/A **1.012** (2 cases).

Sources are identical between A and B; no benchmark rewrite is used to obtain a speedup.
