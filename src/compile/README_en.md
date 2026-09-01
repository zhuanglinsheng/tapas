# Compiler architecture

The production compiler is split into a reusable front end, static analysis,
and bytecode emission. Source files, Markdown blocks, single statements, and
the REPL all use the AST path exclusively.

## Reusable front end

The public interfaces live in `include/tapas/compile/` and do not depend on VM
registers or bytecode state.

- `document.c` owns immutable UTF-8 source text and line starts. Locations are
  half-open byte spans.
- `syntax.c` produces lossless tokens, including whitespace, newlines, and
  comments.
- `diagnostic.c` collects recoverable, span-based diagnostics.
- `ast.c` owns compact arena nodes referenced by stable integer IDs.
- `parser.c` parses complete modules, statements, and Pratt expressions. It
  builds AST nodes for functions, control flow, imports, collections,
  structures, slices, and other AST forms.
- `semantic.c` builds lexical scopes, declarations, and reference-to-symbol
  resolutions independently of bytecode emission.
- `type_info.c` records shared expression and symbol Type facts without VM
  state. It covers annotations, literals, functions, arithmetic, containers,
  fields, calls, forward Type declarations, and recursive Type graphs.
- `frontend.c` owns the document, tokens, AST, diagnostics, and semantic model
  as one lifecycle. This is the entry point intended for the compiler and a
  future language server.

Parser recursion represents grammar nesting and is capped by an explicit
limit. It does not obstruct AST construction or later iterative analysis.

## Static Type analysis

- `type_info.c` is the single inference source for expression, symbol, and
  static Type-value facts.
- The standalone frontend resolves the current document only. An embedding
  compiler supplies Types for preloaded bindings and imported modules through
  the TypeInfo external resolver, so both paths use the same inference rules.
- Frontend initialization selects that resolver up front, so each source is
  analyzed by TypeInfo only once.
- `type_emit.c` consumes TypeInfo and performs the contextual checks and
  runtime-Type materialization needed by bytecode emission. `type_bridge.c`
  owns conversion between static Type IR and runtime Types.
- Compiler bindings keep value Type, static Type value, annotation state, and
  field construction order. Construction order is deliberately separate from
  runtime `ttypeval` so structural equality and hashing remain order-free.
- `binding.c` owns binding metadata and module-interface lifetime, and delegates
  compile-time assignability to the shared Type IR rules in `static_type.c`.
- A binding records initialization separately from its Type. Branches intersect
  initialization over continuing paths, while loops preserve their entry state.
  `let` occupies temporary storage and cannot be captured; `var` occupies the
  environment and may be captured. Both are writable after initialization.
- An uninitialized `let Name: Type` owns a recursive Type placeholder. Its
  completing assignment seals the runtime Type graph through dedicated
  bytecode operations; ordinary reads before initialization are rejected.
- Imported source modules expose a compiler-only interface containing exported
  static Types and field construction order. Runtime imports still receive
  ordinary library values.

## Bytecode back end

- `ast_emit.c` emits expressions and function bodies.
- `statement_ast_emit.c` emits declarations, assignments, control flow,
  imports, modules, and structure literals. It also extracts static module
  interfaces before local compiler bindings leave scope.
- The back end traverses the AST directly. There is no separate whole-tree
  support prepass; the responsible emitter reports an unsupported node when
  it reaches one.
- `context.c` owns register counters, runtime slots, compiler binding metadata,
  and compiler lifetime.
- `source.c` coordinates source files, Markdown extraction, imports, source
  context, and public compile entry points.
  Top-level files and imported modules share the same Tapas/Markdown reader.

`parse_unit`, file compilation, Markdown, and the REPL all use the same AST
front end. The old Token lexer, immediate expression/statement compiler, and
Token-oriented Type inference have been removed.
