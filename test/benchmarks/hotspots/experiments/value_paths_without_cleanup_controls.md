# Runtime A/B results

Measured at: 2026-10-17T16:17:07.395121+00:00

System: `macOS-27.0.1-arm64-arm-64bit-Mach-O`

A (baseline): `17cd65cd3faf59131d8f126ee339aec8c3e1e32aa78fcd141e53fb91fb30752d`

B (candidate): `dc1db3ddbca91b306cb688adc538cfcf9d83b24a2ea72fa56576541a96c5cb51`

Each runtime gets one discarded warm-up and 11 measured runs per case in each of 5 rounds. Fresh processes run sequentially in alternating A/B order. Times are internal process CPU times; startup and compilation are excluded. Each execution uses a fresh working directory to isolate automatic bytecode caches. Every checksum must agree across both runtimes and all runs.

Successive rounds reverse the first runtime and shuffle case order with a recorded seed. Round medians expose between-round variation. For an A/A noise control, pass the same binary twice.

B/A is the median of paired time ratios; below 1 means the candidate is faster. The IQR describes the middle half of paired ratios, not a confidence interval. A near-1 ratio should be treated as noise unless repeated measurements establish otherwise.

| Case | A median ms | B median ms | Paired B/A | Paired IQR | Round medians |
| --- | ---: | ---: | ---: | --- | --- |
| `fannkuch` | 39.796 | 39.657 | 0.995 | 0.987–1.006 | 0.990, 0.995, 0.998, 1.009, 0.996 |
| `tail_recursion` | 1.810 | 1.826 | 1.003 | 0.984–1.032 | 1.003, 0.993, 0.998, 1.008, 1.010 |
| `float_arithmetic` | 22.168 | 22.182 | 1.005 | 0.987–1.015 | 1.006, 0.998, 1.000, 1.009, 1.005 |

algorithm: geometric mean B/A **0.995** (1 cases).
hotspot: geometric mean B/A **1.004** (2 cases).

Sources are identical between A and B; no benchmark rewrite is used to obtain a speedup.
