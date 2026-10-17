# Runtime A/B results

Measured at: 2026-10-17T09:34:35.163782+00:00

System: `macOS-27.0.1-arm64-arm-64bit-Mach-O`

A (baseline): `86f4ec5f0efb799a1b71b46f346d2b9f36963284c5c00fc2f6d04362481cc2a2`

B (candidate): `4892370519678ca55e46bb5ea717a83bac0470fdcb8e76f5bb49ee50de888b75`

Each runtime gets one discarded warm-up and 5 measured runs per case in each of 3 rounds. Fresh processes run sequentially in alternating A/B order. Times are internal process CPU times; startup and compilation are excluded. Every checksum must agree across both runtimes and all runs.

Successive rounds reverse the first runtime and shuffle case order with a recorded seed. Round medians expose between-round variation. For an A/A noise control, pass the same binary twice.

B/A is the median of paired time ratios; below 1 means the candidate is faster. The IQR describes the middle half of paired ratios, not a confidence interval. A near-1 ratio should be treated as noise unless repeated measurements establish otherwise.

| Case | A median ms | B median ms | Paired B/A | Paired IQR | Round medians |
| --- | ---: | ---: | ---: | --- | --- |
| `dict_counting` | 19.798 | 19.972 | 1.014 | 1.001–1.021 | 1.044, 1.001, 1.005 |
| `string_count` | 24.577 | 24.501 | 1.002 | 0.991–1.036 | 1.050, 0.994, 0.999 |
| `substring_count` | 20.175 | 20.165 | 1.010 | 0.975–1.037 | 1.037, 1.018, 0.984 |
| `huffman` | 2.075 | 2.086 | 0.998 | 0.974–1.023 | 0.988, 0.998, 1.005 |
| `k_nucleotide` | 107.637 | 109.078 | 1.013 | 1.008–1.016 | 1.015, 1.013, 1.008 |
| `list_from_iterable` | 10.113 | 10.121 | 1.002 | 0.987–1.067 | 1.002, 1.017, 1.001 |
| `fannkuch` | 39.653 | 39.653 | 0.999 | 0.994–1.006 | 0.997, 0.997, 1.003 |
| `float_arithmetic` | 21.907 | 21.972 | 1.005 | 0.996–1.016 | 1.002, 1.006, 1.005 |

algorithm: geometric mean B/A **1.003** (3 cases).
hotspot: geometric mean B/A **1.007** (5 cases).

Sources are identical between A and B; no benchmark rewrite is used to obtain a speedup.
