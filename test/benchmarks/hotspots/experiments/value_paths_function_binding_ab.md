# Runtime A/B results

Measured at: 2026-10-17T16:24:27.805415+00:00

System: `macOS-27.0.1-arm64-arm-64bit-Mach-O`

A (baseline): `17cd65cd3faf59131d8f126ee339aec8c3e1e32aa78fcd141e53fb91fb30752d`

B (candidate): `fa7b0ec89ed71d51d49d4ae9bd03746af7b646491026849b2f8266569ffd2ce1`

Each runtime gets one discarded warm-up and 11 measured runs per case in each of 5 rounds. Fresh processes run sequentially in alternating A/B order. Times are internal process CPU times; startup and compilation are excluded. Each execution uses a fresh working directory to isolate automatic bytecode caches. Every checksum must agree across both runtimes and all runs.

Successive rounds reverse the first runtime and shuffle case order with a recorded seed. Round medians expose between-round variation. For an A/A noise control, pass the same binary twice.

B/A is the median of paired time ratios; below 1 means the candidate is faster. The IQR describes the middle half of paired ratios, not a confidence interval. A near-1 ratio should be treated as noise unless repeated measurements establish otherwise.

| Case | A median ms | B median ms | Paired B/A | Paired IQR | Round medians |
| --- | ---: | ---: | ---: | --- | --- |
| `float_arithmetic` | 21.716 | 21.704 | 0.997 | 0.988–1.009 | 1.004, 0.998, 0.996, 0.990, 0.999 |
| `fannkuch` | 39.261 | 39.024 | 0.993 | 0.990–0.998 | 0.996, 0.993, 0.993, 0.992, 0.992 |
| `longest_common_subsequence` | 14.179 | 12.446 | 0.875 | 0.870–0.884 | 0.876, 0.877, 0.872, 0.882, 0.875 |

algorithm: geometric mean B/A **0.932** (2 cases).
hotspot: geometric mean B/A **0.997** (1 cases).

Sources are identical between A and B; no benchmark rewrite is used to obtain a speedup.
