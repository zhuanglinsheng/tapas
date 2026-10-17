# Runtime A/B results

Measured at: 2026-10-17T05:10:40.349062+00:00

System: `macOS-27.0.1-arm64-arm-64bit-Mach-O`

A (baseline): `2f6ad6c89b390bd58c387aca7593b7ba61679803c57b2f1097174c2a6adaf967`

B (candidate): `2f6ad6c89b390bd58c387aca7593b7ba61679803c57b2f1097174c2a6adaf967`

Each runtime gets one discarded warm-up and 7 measured runs per case in each of 3 rounds. Fresh processes run sequentially in alternating A/B order. Times are internal process CPU times; startup and compilation are excluded. Every checksum must agree across both runtimes and all runs.

Successive rounds reverse the first runtime and shuffle case order with a recorded seed. Round medians expose between-round variation. For an A/A noise control, pass the same binary twice.

B/A is the median of paired time ratios; below 1 means the candidate is faster. The IQR describes the middle half of paired ratios, not a confidence interval. A near-1 ratio should be treated as noise unless repeated measurements establish otherwise.

| Case | A median ms | B median ms | Paired B/A | Paired IQR | Round medians |
| --- | ---: | ---: | ---: | --- | --- |
| `dijkstra` | 20.533 | 20.408 | 1.001 | 0.982–1.007 | 1.005, 0.994, 0.997 |
| `sudoku` | 403.976 | 401.594 | 1.000 | 0.988–1.012 | 1.004, 0.997, 1.014 |
| `float_arithmetic` | 23.354 | 23.208 | 0.996 | 0.970–1.008 | 1.013, 0.984, 0.965 |
| `function_call_baseline` | 2.874 | 2.833 | 0.998 | 0.943–1.024 | 0.997, 1.000, 0.989 |
| `dict_delete` | 3.781 | 3.765 | 0.996 | 0.985–1.024 | 1.013, 0.992, 1.000 |
| `branch_logic` | 23.769 | 23.701 | 1.002 | 0.984–1.015 | 1.002, 1.000, 1.003 |
| `record_update` | 16.332 | 16.190 | 0.993 | 0.974–1.013 | 0.987, 0.995, 0.990 |
| `recursive_loops` | 16.197 | 16.173 | 0.999 | 0.987–1.005 | 0.996, 1.003, 0.994 |
| `dp_row` | 43.426 | 43.468 | 1.002 | 0.994–1.013 | 0.996, 0.996, 1.015 |
| `n_body` | 24.058 | 24.084 | 1.000 | 0.992–1.018 | 0.996, 1.000, 1.018 |

algorithm: geometric mean B/A **1.000** (3 cases).
hotspot: geometric mean B/A **0.998** (7 cases).

Sources are identical between A and B; no benchmark rewrite is used to obtain a speedup.
