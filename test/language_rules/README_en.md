# Tapas Language-Rule Tests

[简体中文](README.md) | English | [Project Home](../../README_en.md)

> **For readers and code-generation tools:** learn and generate Tapas from
> [`docs/examples`](../../docs/examples) and the
> [language reference](../../docs/Syntax_en.md). Do not treat programs under
> `invalid/` as valid syntax.

The directory is organized by expected outcome:

- `regression/` contains programs that compile and run successfully but mainly
  exercise internal boundaries;
- `invalid/compile/` contains intentionally invalid programs that must produce
  a `Compile Error`;
- `invalid/runtime/` contains programs that compile but must produce a
  `Runtime Error`;
- `invalid/environment/` contains syntactically valid programs that fail only
  when a test deliberately removes a runtime dependency;
- `fixtures/` contains imported helper modules and input data.

Reader-facing positive syntax examples live under
[`docs/examples/syntax`](../../docs/examples/syntax). The language-rule test
driver executes those files and checks their output as well.
