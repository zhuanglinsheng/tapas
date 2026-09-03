# Tapas `optimize` Package Design

[简体中文](README.md) | English | [Project Home](../../../README_en.md)

> Status: design stage.
> `optimize` is not implemented and is not part of the currently exposed
> standard-library API.
> This directory contains design documents only; current builds and
> installations do not provide an importable `optimize` package.

The proposed package would build variable propagation, feasibility search, and
objective optimization on the Rule infrastructure without adding syntax or a
second public expression IR.

The detailed API and IR design is currently maintained in the
[Chinese design document](README.md). It covers Variable Terms, RuleIR-based
constraints, propagation, feasibility, objectives, backend capabilities,
serialization, diagnostics, and implementation boundaries.
