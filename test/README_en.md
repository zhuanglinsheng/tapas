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

Some test and documentation commands create `.tapc` files beside their source.
These are ignored build artifacts and can be deleted safely. Ordinary language
tests compile Tapas source afresh on every run. Tests that require bytecode or
an imported compiled module remove stale `.tapc` files, rebuild them from
source, and remove the generated files after verification.
