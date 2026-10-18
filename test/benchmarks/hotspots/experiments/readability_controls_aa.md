# Runtime A/B results

Measured at: 2026-10-17T14:52:32.473405+00:00

System: `macOS-27.0.1-arm64-arm-64bit-Mach-O`

A (baseline): `bb1ba3e66bb6bb36cf8b4be0a2ffe345fc1d167322a928ad649251d4e3417a7a`

B (candidate): `bb1ba3e66bb6bb36cf8b4be0a2ffe345fc1d167322a928ad649251d4e3417a7a`

Each runtime gets one discarded warm-up and 11 measured runs per case in each of 3 rounds. Fresh processes run sequentially in alternating A/B order. Times are internal process CPU times; startup and compilation are excluded. Every checksum must agree across both runtimes and all runs.

Successive rounds reverse the first runtime and shuffle case order with a recorded seed. Round medians expose between-round variation. For an A/A noise control, pass the same binary twice.

B/A is the median of paired time ratios; below 1 means the candidate is faster. The IQR describes the middle half of paired ratios, not a confidence interval. A near-1 ratio should be treated as noise unless repeated measurements establish otherwise.

| Case | A median ms | B median ms | Paired B/A | Paired IQR | Round medians |
| --- | ---: | ---: | ---: | --- | --- |
| `list_sort` | 3.721 | 3.689 | 0.981 | 0.969–1.025 | 0.975, 0.981, 0.997 |
| `heap_order` | 2.802 | 2.797 | 0.998 | 0.967–1.027 | 0.990, 1.001, 0.996 |
| `k_nucleotide` | 92.047 | 91.915 | 0.999 | 0.991–1.004 | 0.998, 1.000, 0.999 |

algorithm: geometric mean B/A **0.999** (1 cases).
hotspot: geometric mean B/A **0.989** (2 cases).

Sources are identical between A and B; no benchmark rewrite is used to obtain a speedup.
