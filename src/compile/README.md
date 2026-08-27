# Compiler architecture

The production compiler is split into a reusable front end, static analysis,
and bytecode emission. Source files and Markdown code blocks use the AST path
exclusively. The legacy token compiler remains only behind the old public
single-unit compatibility API.

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
  structures, slices, and compatibility kappas.
- `semantic.c` builds lexical scopes, declarations, and reference-to-symbol
  resolutions independently of bytecode emission.
- `type_info.c` records editor-facing expression and symbol Type facts without
  VM state. It currently covers annotations, literals, functions, arithmetic,
  pairs, lists, and dictionaries. Rich structural checking is being migrated
  here incrementally from the bytecode compiler.
- `frontend.c` owns the document, tokens, AST, diagnostics, and semantic model
  as one lifecycle. This is the entry point intended for the compiler and a
  future language server.

Parser recursion represents grammar nesting and is capped by an explicit
limit. It does not obstruct AST construction or later iterative analysis.

## Static Type analysis

- `ast_typecheck.c` evaluates the five recognized static Type constructors,
  resolves local and imported Type names, infers expression and literal Types,
  and applies contextual assignability checks.
- Compiler bindings keep value Type, static Type value, annotation state, and
  field construction order. Construction order is deliberately separate from
  runtime `ttypeval` so structural equality and hashing remain order-free.
- `binding.c` owns binding metadata, module-interface lifetime, and the shared
  Type assignability relation. Token-oriented compatibility inference remains
  isolated in `typecheck.c`.
- Imported source modules expose a compiler-only interface containing exported
  static Types and field construction order. Runtime imports still receive
  ordinary library values.

## Bytecode back end

- `ast_emit.c` emits expressions and function bodies.
- `statement_ast_emit.c` emits declarations, assignments, control flow,
  imports, modules, and structure literals. It also extracts static module
  interfaces before local compiler bindings leave scope.
- `context.c` owns register counters, runtime slots, compiler binding metadata,
  and compiler lifetime.
- `source.c` coordinates source files, Markdown extraction, imports, source
  context, and public compile entry points.

`lexer.c`, `expression.c`, `statement.c`, `dispatch.c`, and the token-oriented
part of `typecheck.c` are retained for ABI compatibility with `parse_unit` and
older embedders. New compiler behavior must be implemented in the reusable
front end or AST analysis and emission layers.
