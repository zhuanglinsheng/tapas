# Tests

[简体中文](README.md) | English | [Project Home](../README_en.md)

Test files are organized by purpose:

- `unit/` contains C unit tests for runtime data structures, the compiler front
  end, and the language server;
- `support/` contains local libraries used only by tests;
- [`language_rules/`](language_rules/README_en.md) separates valid regressions,
  expected compile errors, expected runtime errors, and supporting fixtures;
- `../docs/examples/syntax/` contains reader-facing syntax examples that are
  also executed by the language tests;
- `cmake/` contains integration-test drivers;
- [`benchmarks/`](benchmarks/README_en.md) contains matching Tapas and Python
  performance programs and bilingual results.

Explicit bytecode tests may create `.tapc` files beside their source, while
direct source execution and imported modules use `__tapas_build__` below the
test working directory. These ignored build artifacts can be deleted safely;
tests update timestamps or clear the relevant artifacts when they need to
verify recompilation.
