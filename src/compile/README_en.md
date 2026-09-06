# Compiler architecture

[简体中文](README.md) | English | [Project Home](../../README_en.md)

The compiler is divided into a reusable front end, static Type analysis, and a
bytecode back end. Tapas files, Markdown blocks, single-statement compilation,
and the REPL all use the same AST pipeline.

```text
source -> frontend/{document, syntax, ast, parser, semantic, control_flow}
       -> types/{static_type, type_info, type_check}
       -> backend/{context, emit, type_bridge, source}
       -> bytecode
```

`frontend/` and `types/` include no VM, bytecode, or runtime-object headers.
The standard-enabled front-end target links `tapas_stdlib` so its Type metadata
comes from the same descriptors as the implementations. `backend/` may depend
on both the reusable front end and the runtime. CLI input state lives under
`src/cli/`. This dependency direction is the main directory boundary; file
count or line count alone is not a reason to add more top-level directories.

The standard library lives in `src/stdlib/`; root built-ins are grouped under
`builtins/`, while language packages use directories with matching names. Each C source keeps its
implementations, argument ranges, and Tapas Types together and exposes them
through one `textension_descriptor` consumed by the runtime, compiler, and LSP.
The `types` package constructor identities come from the same descriptor.

## Reusable front end

Compiler interfaces are implementation-private and live beside their
implementations under `src/compile/`. The external embedding API is provided
by `include/tapas/tsession.h`; AST, semantic-model, and workspace layouts are
not public ABI.

- `frontend/document.c` owns immutable UTF-8 source, line indexes, and
  half-open source spans.
- `frontend/syntax.c` produces lossless tokens including whitespace, newlines,
  and comments.
- `frontend/ast.c` owns compact arena nodes referenced by stable `tast_id`
  values.
- `frontend/parser.c` parses modules, statements, and Pratt expressions through
  one boundary scanner; each `if`/`elif`/`else` chain is one AST node with an
  explicit branch list.
- `frontend/semantic.c` builds lexical scopes, declarations, symbols, and name
  resolution.
- `frontend/control_flow.c` builds parent relations, child roles, enclosing
  functions, and definite-return facts in one pass.
- `frontend/definite_assignment.c` computes definite assignment at each name
  read and at compilation-unit exit.
- `frontend/module.c` extracts public module interfaces and generates standard
  environment symbols.
- `frontend/workspace.c` owns URI handling, file indexing, import graphs,
  module members, and the LSP workspace.
- `frontend/workspace_reference.c` maintains the sorted cross-module export
  reference index.
- `frontend/frontend.c` coordinates document, token, AST, semantic, Type, and
  diagnostic lifetimes.

Parser recursion represents syntax nesting only and has an explicit depth
limit. The CLI and LSP consume the same `tfrontend` result and no longer carry
separate syntax or Type rules.

## Static Type analysis

Current language Type semantics are maintained in
[TypeSystem_en.md](../../docs/TypeSystem_en.md); this section describes only
their compiler boundaries.

- `types/static_type.c` defines the Type arena, parser, formatter, structural
  equality, and assignability.
- `types/type_info.c` is the sole inference source for expression Types,
  symbol Types, and static Type values.
- `types/type_check.c` produces shared assignment, call, return, index, and
  container-mutation diagnostics.
- `types/type_constructor.c` exposes manifest-generated Type-constructor
  identities and argument ranges to static evaluation and checking.
- An external resolver supplies TypeInfo with preloaded C-function and imported
  module signatures. The CLI reads descriptors from its actual `tlib`; the
  LSP workspace reads the same standard-environment manifest.
- `Unknown` is an analysis state, not `AnyType`. A canonical signature may
  use explicit `Unknown` for a dependent result that the current pattern
  system cannot express, but this must not manufacture a concrete Type.
- Field construction order comes from static field Types and is retained in
  module interfaces when it crosses a module boundary.

## Bytecode back end

- `backend/context.c` owns registers, slots, compiler context, and lifecycle.
- `backend/binding.c` owns binding Types, static Type values, initialization
  state, and module interfaces.
- `backend/ast_emit.c` emits expressions and function literals.
- `backend/statement_ast_emit.c` emits declarations, assignments, control
  flow, imports, and modules.
- `backend/type_bridge.c` converts between static Type IR and runtime
  `ttypeval`.
- `backend/type_emit.c` materializes static Type values and field construction
  order.
- `backend/source.c` coordinates files, Markdown, imports, and public
  compilation entry points.

`tast_emitter` owns instructions, constants, and import paths. Statement and
control-flow emitters no longer forward this context redundantly. Public
`compiler.h` exposes only opaque `tcp` and compilation entry points; context
layout and slot operations remain in private `backend/` headers.

## Changes in this optimization pass

`tcontrol_flow` now stores parent, first-child, sibling, child-role,
enclosing-function, definite-return, and definite-assignment facts. As a result:

- `this`, return statements, and contextual function queries use the enclosing
  function index;
- index writes and optional field wrappers use parent and child-role facts;
- true-branch Type narrowing walks ancestors instead of scanning every control
  statement for every name;
- LSP cursor lookup descends through syntax children, then member resolution
  walks upward through parents;
- Semantic stores a declaration-to-symbol reverse index shared by TypeInfo and
  TypeCheck;
- the front end intersects initialization across every continuing path,
  excludes definitely returning paths, and treats loops conservatively;
- conditional semantics, narrowing, definite returns, and definite assignment
  traverse explicit branches instead of scanning adjacent statements;
- bindings from earlier compilation units enter Semantic as external symbols;
- Workspace builds a reference index sorted by target module and export name.

The front end now owns Type-constructor arity, static-argument, `make_type`
field, and indirect-call diagnostics. The back end no longer interprets or
validates constructor AST; it only materializes TypeInfo's `TypeId` values as
`ttypeval`. Field order is read from static Types or module interfaces.
Consequently, `backend/type_emit.c` fell from about 586 lines to about 220, and
the CLI and LSP receive identical constructor diagnostics.

## Current structural audit

Nested whole-tree query scans, duplicated Type-constructor semantics, and
back-end initialization snapshots have been removed. Cross-module references
use binary search to locate an indexed range, and rename emits edits directly
from references grouped by source document. Remaining whole-tree walks are
linear passes that build Semantic, relationship, TypeInfo, diagnostics, and
document references.

Remaining structural issues are:

- `parser.c` is about one thousand lines and combines expression, function,
  collection, and statement grammar;
- `statement_ast_emit.c` still combines control-flow jumps, binding writes, and
  module emission;
- `type_info.c` still combines static Type evaluation, expression inference,
  contextual propagation, and narrowing;
- `workspace.c` combines filesystem indexing, URI handling, document lifecycle,
  and namespace resolution;
- Semantic scope lookup still performs a linear symbol search;
- Type strings are eagerly formatted for all nodes and symbols, and annotations
  are reparsed.

The three current directory boundaries remain sound; another top-level compiler
directory is not justified.

## Next optimization order

### 1. Establish an operator signature table

Replace operator branches scattered across inference and the VM with one
read-only signature table. Each entry contains an operator, left and right Type
patterns, a result rule, and runtime constraints. Patterns cover concrete
Types, Numeric/Ordered/Array families, shared type variables, and parameterized
containers; result rules cover fixed Types, type variables, numeric promotion,
and array broadcasting.

The first entries should cover numeric operations and promotion, String/List
concatenation and repetition, scalar Bool versus BoolArray logic, comparisons,
membership, ranges, matrix operations, and Time offsets. Array shapes remain a
runtime check. Every VM branch must map to one unambiguous entry, with tests
generated from entries for accepted operands, rejected operands, and result
Types.

### 2. Split the `Unknown` analysis state

Keep the language Type `AnyType` unchanged. Internally split `Unknown` into
`Unresolved` for pending fixed-point analysis, `Dynamic` for explicit dynamic
boundaries, `ErrorType` for suppressing cascading diagnostics, and
`InferredHole` for constraints awaiting context. First record the origin of
existing Unknown values and solve contextual holes; then emit guards for
`Dynamic -> T` at assignment, call, return, and container-write boundaries;
finally prevent `Unresolved` from leaving the front end and let strict mode
reject implicit dynamic conversions.

The migration must preserve gradual guarantees: removing annotations cannot
change successful runtime behavior, while adding annotations may only report
errors earlier or make guards more precise.

### 3. Extend control-flow dataflow

The shared model now covers structural relations, definite returns, and
definite assignment. Next, model true and false condition facts, false-branch
Types formed by union subtraction, `and`/`or`/`not` short-circuit composition,
loop back edges, field-presence narrowing, user type guards, exhaustiveness,
and `Never`. Facts attach to symbols and CFG edges; assignment or a call that
may mutate an alias must invalidate affected narrowing.

### 4. Split large files at internal seams

After the models above stabilize:

- `parser.c` -> `parser_expression.c`, `parser_statement.c`, and private
  parser state;
- `type_info.c` -> static Type evaluation, expression inference, and flow
  analysis;
- `statement_ast_emit.c` -> binding, control-flow, and module emission;
- `workspace.c` -> paths/indexing, document storage, and resolution queries;
- `source.c` -> source loading, import compilation, and public entry points.

These files should remain under the existing `frontend/`, `types/`, and
`backend/` directories. No new top-level compiler directory is needed.

### 5. Reduce public header state

`compiler.h` is already small enough. Further header work should hide state,
not mechanically move declarations:

- replace the public `tparser` layout with document-level parsing entry points,
  then move parser state to a private header;
- add Frontend, Workspace, and Semantic accessors for the LSP before hiding
  their mutable arrays;
- keep AST nodes syntax-only and expose all relations and flow facts through
  `control_flow.h`;
- keep `type_constructor.h`, emitter state, and compiler context private;
- do not merge AST, Semantic, and TypeInfo: they have distinct lifetimes and
  consumers.

### 6. Reduce editor allocation and query cost

Format Type strings lazily for hover, completion, or diagnostic requests. Cache
canonical C-function signatures and annotations per Type arena. Add indexed
scope lookup and a compact reverse index for local symbol references.
