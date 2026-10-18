# Runtime A/B results

Measured at: 2026-10-17T16:16:33.263296+00:00

System: `macOS-27.0.1-arm64-arm-64bit-Mach-O`

A (baseline): `17cd65cd3faf59131d8f126ee339aec8c3e1e32aa78fcd141e53fb91fb30752d`

B (candidate): `0938c6d6fe2ec4ef4f7a08e0a785a3926fc4ac649dad308e6c989c6255f163a1`

Each runtime gets one discarded warm-up and 11 measured runs per case in each of 5 rounds. Fresh processes run sequentially in alternating A/B order. Times are internal process CPU times; startup and compilation are excluded. Each execution uses a fresh working directory to isolate automatic bytecode caches. Every checksum must agree across both runtimes and all runs.

Successive rounds reverse the first runtime and shuffle case order with a recorded seed. Round medians expose between-round variation. For an A/A noise control, pass the same binary twice.

B/A is the median of paired time ratios; below 1 means the candidate is faster. The IQR describes the middle half of paired ratios, not a confidence interval. A near-1 ratio should be treated as noise unless repeated measurements establish otherwise.

| Case | A median ms | B median ms | Paired B/A | Paired IQR | Round medians |
| --- | ---: | ---: | ---: | --- | --- |
| `fannkuch` | 39.639 | 40.240 | 1.017 | 1.009–1.024 | 1.023, 1.013, 1.021, 1.018, 1.016 |
| `tail_recursion` | 1.819 | 1.777 | 0.979 | 0.956–1.006 | 0.975, 0.947, 0.993, 0.977, 0.992 |
| `float_arithmetic` | 22.163 | 22.127 | 1.001 | 0.988–1.014 | 0.998, 0.995, 1.002, 1.007, 0.999 |
| `longest_common_subsequence` | 14.386 | 12.679 | 0.875 | 0.868–0.888 | 0.874, 0.886, 0.873, 0.875, 0.878 |
| `lru_cache` | 42.756 | 42.829 | 1.000 | 0.992–1.009 | 1.011, 0.997, 0.998, 1.006, 0.998 |
| `dict_counting` | 20.201 | 20.049 | 0.993 | 0.982–1.005 | 0.991, 0.992, 0.995, 0.992, 0.993 |

algorithm: geometric mean B/A **0.962** (3 cases).
hotspot: geometric mean B/A **0.991** (3 cases).

Sources are identical between A and B; no benchmark rewrite is used to obtain a speedup.
