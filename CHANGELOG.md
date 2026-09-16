# Changelog

## 0.2.0 — 2026-09-17

This release develops Tapas from a rule checker into a rule-driven test-data
system. It is an early-development release with source and API changes from
0.1.0. The entry below describes implemented functionality, not design drafts.

### Added

- CP-SAT feasibility queries with `solve::hold`, checker-validated witnesses,
  bounded integer domains, presolve, and conflict explanations.
- `solve::sample` for constrained test-data generation, configurable proposals,
  seeded randomness, query budgets, trace information, and UNSAT-region caching.
- `random` generators and `finite` distributions for reproducible sampling.
- Enum Types, user-defined Type templates, `InstanceOf` identity constraints,
  Rule domains (`rules::points` / `rules::range`), and function/instance reflection
  through `parameters` and `arguments`.
- Rule implication (`implies`), logical negation and composition, structured
  expression IR, and `restrict`, `item`, `drop`, `violate`, `drop_if`, and `violate_if`
  transformations.
- Retail checking, generation, and sampling examples, plus expanded bilingual
  language and standard-library documentation.

### Changed

- Rule bodies accept bare RuleInstance items and Bool/RuleInstance operands for
  `not`, `and`, and `or`. Ordinary conditions still require Bool.
- Unannotated dictionary literals remain mutable raw `Dictionary` values;
  their initial keys do not imply a fixed structural Type.
- Language-server hover and completion share a consistent presentation layer.
  Type annotations participate in navigation, references, and renaming.
- Source formatting handles implication clauses and multiline parameters.
  Markdown fences execute statement sequences with shared document state.
- Public headers and internal source directories were reorganized. Core Types
  and package-owned Types are distinguished; package Types use qualified names.

### Upgrading from 0.1.0

- Replace `require R(x)` in a Rule body with `R(x)`. `require` is no longer a
  keyword; bare Rule values still need to be applied to arguments first.
- Use package-qualified annotations such as `rules::PointsOf[Int]`,
  `rules::RangeOf[Int]`, `evaluators::Evaluator`, and `solve::HoldResult`.
  Public core IR Type names are `RuleTerm` and `RuleItem`.
- Annotate dictionaries when fixed field or key/value constraints are needed.
- Recompile `.tapc` files with the new runtime. Cross-version bytecode and
  internal IR compatibility must not be assumed.
- Native integrations must rebuild against the current public headers under
  `include/tapas`; compiler and VM internals are not a stable public API.
- Solving and sampling require Python 3 with OR-Tools 9.14.6206. By default,
  Tapas runs `python3` from `PATH`; no virtual environment is required. Existing
  compatible installations can be used directly. `TAPAS_SOLVE_PYTHON` remains
  an optional interpreter override. Ordinary checking does not need OR-Tools.

### Limits to keep in mind

- A query may return `unsupported`, `unknown`, or `error`; callers must inspect
  its status before using a witness. Sampling can finish `incomplete`.
- `unsat` applies to the configured search domain, not every possible Int.
- Sampling is budgeted and does not promise uniform samples over all solutions.

## 0.1.0

Initial tagged release of the C23 runtime, compiler, Rule checker, command-line
interface, standard library, language server, and VS Code extension.
