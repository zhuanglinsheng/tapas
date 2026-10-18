# Runtime A/B results

Measured at: 2026-10-17T11:04:35.429290+00:00

System: `macOS-27.0.1-arm64-arm-64bit-Mach-O`

A (baseline): `2c543424a5884d5a647a99e8cf7b3134d170920aa714813705e4ae0d1e00dc1a`

B (candidate): `80d8465438372b58270f8201cdc5cd9cfc244cee0b5d50ba5ff1da75470072c4`

Each runtime gets one discarded warm-up and 11 measured runs per case in each of 3 rounds. Fresh processes run sequentially in alternating A/B order. Times are internal process CPU times; startup and compilation are excluded. Every checksum must agree across both runtimes and all runs.

Successive rounds reverse the first runtime and shuffle case order with a recorded seed. Round medians expose between-round variation. For an A/A noise control, pass the same binary twice.

B/A is the median of paired time ratios; below 1 means the candidate is faster. The IQR describes the middle half of paired ratios, not a confidence interval. A near-1 ratio should be treated as noise unless repeated measurements establish otherwise.

| Case | A median ms | B median ms | Paired B/A | Paired IQR | Round medians |
| --- | ---: | ---: | ---: | --- | --- |
| `huffman` | 2.049 | 2.060 | 1.009 | 0.989–1.030 | 1.018, 1.009, 0.997 |
| `heap_callback` | 11.301 | 8.793 | 0.779 | 0.772–0.785 | 0.776, 0.779, 0.777 |
| `list_copy` | 4.835 | 4.885 | 1.013 | 0.975–1.041 | 1.020, 1.003, 1.015 |
| `function_callbacks` | 12.225 | 12.208 | 0.998 | 0.991–1.008 | 0.999, 0.998, 0.997 |

algorithm: geometric mean B/A **1.009** (1 cases).
hotspot: geometric mean B/A **0.923** (3 cases).

Sources are identical between A and B; no benchmark rewrite is used to obtain a speedup.
