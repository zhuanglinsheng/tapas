# Runtime A/B results

Measured at: 2026-10-17T09:39:05.576974+00:00

System: `macOS-27.0.1-arm64-arm-64bit-Mach-O`

A (baseline): `86f4ec5f0efb799a1b71b46f346d2b9f36963284c5c00fc2f6d04362481cc2a2`

B (candidate): `badc859b19a913f55f45666e2243db6119f66e5329064a181d41cc494debee1e`

Each runtime gets one discarded warm-up and 5 measured runs per case in each of 3 rounds. Fresh processes run sequentially in alternating A/B order. Times are internal process CPU times; startup and compilation are excluded. Every checksum must agree across both runtimes and all runs.

Successive rounds reverse the first runtime and shuffle case order with a recorded seed. Round medians expose between-round variation. For an A/A noise control, pass the same binary twice.

B/A is the median of paired time ratios; below 1 means the candidate is faster. The IQR describes the middle half of paired ratios, not a confidence interval. A near-1 ratio should be treated as noise unless repeated measurements establish otherwise.

| Case | A median ms | B median ms | Paired B/A | Paired IQR | Round medians |
| --- | ---: | ---: | ---: | --- | --- |
| `dict_counting` | 19.947 | 20.070 | 1.006 | 0.991–1.043 | 0.999, 1.043, 1.006 |
| `string_count` | 24.816 | 24.752 | 0.999 | 0.981–1.004 | 1.003, 0.983, 1.002 |
| `substring_count` | 21.010 | 20.297 | 0.970 | 0.947–1.009 | 0.956, 1.004, 0.994 |
| `huffman` | 2.106 | 2.091 | 1.007 | 0.993–1.019 | 1.019, 1.011, 0.995 |
| `k_nucleotide` | 109.156 | 110.159 | 1.008 | 0.997–1.026 | 1.026, 1.011, 0.997 |
| `list_from_iterable` | 10.972 | 10.667 | 1.003 | 0.983–1.044 | 1.016, 1.013, 0.997 |
| `fannkuch` | 40.461 | 40.944 | 1.007 | 1.000–1.031 | 1.012, 1.000, 1.032 |
| `float_arithmetic` | 22.642 | 22.615 | 0.998 | 0.981–1.026 | 1.026, 0.998, 0.995 |

algorithm: geometric mean B/A **1.007** (3 cases).
hotspot: geometric mean B/A **0.995** (5 cases).

Sources are identical between A and B; no benchmark rewrite is used to obtain a speedup.

## Scope and recovered regression

This experiment checks eight affected workloads/controls, not the full fixed
16-algorithm score. The unchanged-source comparisons use ordinary Release
builds. The baseline is a copy of the pre-audit Release binary; its hash and
build configuration match the archived heap experiment.

An initial generic join loop checked the storage kind on every element and
measured B/A 1.236 on the original List workload. Moving the branch outside the
loop alone still measured 1.074. Restoring direct List storage access and its
original element checks while dispatching other Iterable inputs separately
removed this regression. The common three-argument idx_or path is also
separated from variadic handling, preserving one Dictionary probe. Small
changes in the larger workloads vary across rounds and are not claimed as
stable improvements.

- `join_list`: paired B/A 1.001; rounds 0.999, 1.010, 1.000.
- `concat_lists`: paired B/A 0.997; rounds 0.982, 0.991, 1.005.
- `list_from_list`: paired B/A 0.652; rounds 0.658, 0.652, 0.651.

The list(List) improvement comes from directly copying retained values rather
than using per-element iteration callbacks. These kernels are excluded from
the main benchmark tables and the fixed algorithm score.

See [initial workload samples](builtin_capabilities_ab_initial.json),
[initial kernel samples](builtin_collection_kernels_initial.json),
[intermediate kernel samples](builtin_collection_kernels_split.json),
[final kernel samples](builtin_collection_kernels_final.json),
[initial sources](builtin_capabilities_initial_sources.json), and
[final sources](builtin_capabilities_final_sources.json).

Release CTest passed 53/53, including executable bilingual Stdlib examples.
ASAN core/data-structure/capability/language regressions passed 4/4. A custom
extension probe verifies multi-index forwarding, one-probe fallback, iterable
concatenation/joining, and retained composite lifetimes.
