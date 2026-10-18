# Runtime A/B results

Measured at: 2026-10-17T13:11:01.160028+00:00

System: `macOS-27.0.1-arm64-arm-64bit-Mach-O`

A (baseline): `7777c261957cc1780d2e9ad58b08e4f79fed29ae692ef55d68e9afea4ea7d41c`

B (candidate): `b026c1defa442d43c39564627a4bb2ada8d2ca1f420da0e1919b962adf0d5d47`

Each runtime gets one discarded warm-up and 11 measured runs per case in each of 3 rounds. Fresh processes run sequentially in alternating A/B order. Times are internal process CPU times; startup and compilation are excluded. Every checksum must agree across both runtimes and all runs.

Successive rounds reverse the first runtime and shuffle case order with a recorded seed. Round medians expose between-round variation. For an A/A noise control, pass the same binary twice.

B/A is the median of paired time ratios; below 1 means the candidate is faster. The IQR describes the middle half of paired ratios, not a confidence interval. A near-1 ratio should be treated as noise unless repeated measurements establish otherwise.

| Case | A median ms | B median ms | Paired B/A | Paired IQR | Round medians |
| --- | ---: | ---: | ---: | --- | --- |
| `list_sort` | 4.988 | 4.182 | 0.841 | 0.810–0.859 | 0.855, 0.829, 0.841 |

hotspot: geometric mean B/A **0.841** (1 cases).

Sources are identical between A and B; no benchmark rewrite is used to obtain a speedup.
