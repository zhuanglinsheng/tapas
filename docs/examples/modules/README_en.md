# Module Examples

[简体中文](README.md) | English | [Project Home](../../../README_en.md)

This directory demonstrates both Tapas module layouts:

- [`library.tap`](library.tap) is a single-file module;
- [`greeter/__init__.tap`](greeter/__init__.tap) is the entry point of a
  directory package;
- [`main.tap`](main.tap) imports and calls both using paths relative to its own
  location.

Run the example from the repository root:

```sh
build/bin/tapas docs/examples/modules/main.tap
```
