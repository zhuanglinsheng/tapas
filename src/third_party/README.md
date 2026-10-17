# Vendored dependencies

These upstream sources are checked into the repository so builds need no network
access. Keep Tapas-specific wrappers in `src/stdlib`, not in these directories.

| Directory | Upstream revision | License | Consumer |
| --- | --- | --- | --- |
| `pcg-c` | `83252d9c23df9c82ecb42210afed61a7b42402d7` | MIT OR Apache-2.0 | `random` |
| `yyjson` | release `0.13.0` (archive `ibireme-yyjson-138b4a7`) | MIT | `json` |

Each dependency has a `VERSION` file recording its origin. yyjson contains only
upstream `src/yyjson.c`, `src/yyjson.h`, and `LICENSE`, without local source changes.
PCG was moved unchanged from `src/dsa/third_party/pcg-c`.

CMake builds each dependency as a separate static target with private linkage to
`tapas_stdlib`. Upstream include directories are system includes; Tapas warning
options apply only to first-party targets. User-supplied compiler flags still apply.
The yyjson target limits reader and writer nesting to 256 containers.

When updating, select an explicit release/commit, replace the upstream files and
record the revision and any local patches. Run JSON and random regression tests,
the full test suite, and installation checks. Keep licenses installed under
`share/licenses/tapas/<dependency>/`.
