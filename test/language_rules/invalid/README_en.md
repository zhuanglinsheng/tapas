# Invalid Tapas Programs

[简体中文](README.md) | English | [Project Home](../../../README_en.md)

> **DO NOT COPY THESE FILES AS VALID TAPAS SYNTAX.**

These programs intentionally fail so that tests can verify compile diagnostics,
runtime errors, and behavior when dependencies are unavailable. The first line
of every `.tap` file identifies its expected failure category:

- programs under `compile/` must produce a `Compile Error`;
- programs under `runtime/` must compile successfully and then produce a
  `Runtime Error`;
- programs under `environment/` use valid syntax and fail only when the test
  removes a required dependency.

For valid Tapas examples, see [`docs/examples`](../../../docs/examples) and the
[language reference](../../../docs/Syntax_en.md).
