# Runtime A/B results

Measured at: 2026-10-17T11:00:15.593253+00:00

System: `macOS-27.0.1-arm64-arm-64bit-Mach-O`

A (baseline): `80d8465438372b58270f8201cdc5cd9cfc244cee0b5d50ba5ff1da75470072c4`

B (candidate): `ed8a71a5c3a6249af6ac7988476ee9f919e2214db55d98a6f9650cf3ba1afaf4`

Each runtime gets one discarded warm-up and 7 measured runs per case in each of 3 rounds. Fresh processes run sequentially in alternating A/B order. Times are internal process CPU times; startup and compilation are excluded. Every checksum must agree across both runtimes and all runs.

Successive rounds reverse the first runtime and shuffle case order with a recorded seed. Round medians expose between-round variation. For an A/A noise control, pass the same binary twice.

B/A is the median of paired time ratios; below 1 means the candidate is faster. The IQR describes the middle half of paired ratios, not a confidence interval. A near-1 ratio should be treated as noise unless repeated measurements establish otherwise.

| Case | A median ms | B median ms | Paired B/A | Paired IQR | Round medians |
| --- | ---: | ---: | ---: | --- | --- |
| `recursive_loops` | 15.278 | 15.306 | 0.996 | 0.990–1.015 | 1.009, 0.999, 0.995 |
| `function_callbacks` | 12.167 | 12.395 | 1.020 | 1.004–1.027 | 1.021, 1.016, 1.024 |
| `function_calls` | 6.695 | 6.670 | 0.994 | 0.986–1.001 | 1.000, 0.986, 0.998 |
| `dijkstra` | 6.949 | 6.858 | 0.984 | 0.971–1.003 | 0.984, 0.975, 0.984 |
| `huffman` | 2.086 | 2.097 | 0.999 | 0.982–1.016 | 0.996, 1.007, 0.991 |
| `tail_recursion` | 1.772 | 1.733 | 0.977 | 0.965–1.006 | 0.973, 0.972, 0.979 |

algorithm: geometric mean B/A **0.991** (2 cases).
hotspot: geometric mean B/A **0.997** (4 cases).

Sources are identical between A and B; no benchmark rewrite is used to obtain a speedup.
