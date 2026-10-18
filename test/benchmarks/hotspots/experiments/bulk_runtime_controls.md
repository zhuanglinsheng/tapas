# Runtime A/B results

Measured at: 2026-10-17T10:40:26.106416+00:00

System: `macOS-27.0.1-arm64-arm-64bit-Mach-O`

A (baseline): `08a94a92f5dd975b1aed1273b7d71a0cab640523739f967fc73f0ababde9b59b`

B (candidate): `2c543424a5884d5a647a99e8cf7b3134d170920aa714813705e4ae0d1e00dc1a`

Each runtime gets one discarded warm-up and 7 measured runs per case in each of 3 rounds. Fresh processes run sequentially in alternating A/B order. Times are internal process CPU times; startup and compilation are excluded. Every checksum must agree across both runtimes and all runs.

Successive rounds reverse the first runtime and shuffle case order with a recorded seed. Round medians expose between-round variation. For an A/A noise control, pass the same binary twice.

B/A is the median of paired time ratios; below 1 means the candidate is faster. The IQR describes the middle half of paired ratios, not a confidence interval. A near-1 ratio should be treated as noise unless repeated measurements establish otherwise.

| Case | A median ms | B median ms | Paired B/A | Paired IQR | Round medians |
| --- | ---: | ---: | ---: | --- | --- |
| `recursive_fibonacci` | 4.194 | 4.182 | 0.985 | 0.968–1.028 | 1.023, 0.983, 0.971 |
| `dict_update` | 4.845 | 4.766 | 0.983 | 0.967–0.995 | 0.983, 0.982, 0.984 |
| `list_copy` | 4.832 | 4.832 | 0.982 | 0.968–1.039 | 0.976, 0.984, 0.982 |
| `function_calls` | 6.628 | 6.655 | 1.007 | 0.987–1.020 | 1.009, 1.003, 1.017 |
| `huffman` | 2.089 | 2.092 | 0.997 | 0.978–1.007 | 0.999, 0.997, 0.979 |

algorithm: geometric mean B/A **0.991** (2 cases).
hotspot: geometric mean B/A **0.990** (3 cases).

Sources are identical between A and B; no benchmark rewrite is used to obtain a speedup.
