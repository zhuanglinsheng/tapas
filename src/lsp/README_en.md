# Tapas Language Server

Type annotations have a separate `tannotation_index` containing source spans,
definition scopes, local symbol IDs, and member chains. It does not add expression
AST nodes or change parameter spans or control flow. Each analysis rebuilds the
index; module exports are resolved on demand rather than retained as pointers.
Hover, definition lookup, references, and rename share this index without
evaluating Type expressions.

[简体中文](README.md) | English | [Project Home](../../README_en.md)

`tapas-language-server` is a standalone stdio Language Server Protocol process.
It links only the reusable compiler front end; it does not initialize the VM or
execute user code.

Implemented protocol features:

- full document synchronization (`didOpen`, `didChange`, `didClose`);
- recoverable parser and semantic diagnostics;
- UTF-16 LSP position conversion for UTF-8 Tapas source;
- hover information for local Types, root built-in signatures, default
  packages, and package members;
- local and cross-module definition and reference lookup;
- document and workspace symbols;
- prefix-aware local and standard-environment completion;
- default-package and imported-module member completion after `::`;
- structural-field hover and `::` completion for annotated parameters, nested
  members, and imported Types;
- full-document semantic tokens from existing front-end tokens, AST, and symbol
  resolution;
- prepare-rename, local rename, and workspace edits for public module members;
- recursive workspace indexing, document overlays, import diagnostics, and
  watched-file refresh.

The server is started by an editor with:

```text
tapas-language-server
```

The repository includes a dependency-free VS Code client under
`editors/vscode/`. It can locate the server in the repository build directory,
through `PATH`, or through the `tapas.languageServer.path` setting.

## Layering

Type presentation is separate from expansion: `tstatic_type_display` preserves
names attached to individual annotation occurrences, while `tstatic_type_format`
retains the expanded structure. Named occurrences keep the underlying structural
Type; their names do not participate in equality or assignability. Aliases are
not chosen by searching for structurally equal definitions. Builtin Types keep
canonical names. Cross-module propagation qualifies proven public names and
falls back to structure for inaccessible names. Hover, completion, and module
details reuse the frontend presentation instead of reconstructing Rule declarations.

`json.c` owns the small protocol JSON parser. `server.c` owns framing, requests,
notifications, and LSP serialization. `src/compile/frontend/module.c` defines public
module interfaces and the standard environment; `src/compile/frontend/workspace.c` owns
document snapshots, disk indexing, imports, and cross-file identity;
`workspace_reference.c` maintains module-member references as documents change.
Protocol handlers translate those results into LSP positions and edits.

Standard names for semantic tokens come from a read-only signature index built
once at server startup, rather than a linear standard-library scan for every
identifier. `tapas/syntaxCatalog` exposes keywords, built-in Types, and default
packages to the protocol consistency test so it can verify the static TextMate
fallback grammar; editors do not call it at runtime.

Remaining editor work includes signature help, semantic-token delta responses,
and incremental text synchronization.

## Symbol presentation

Hover and completion details share the semantic formatter in `presentation.c`. See [the presentation rules](Presentation.md) for categories, aliases, record expansion, and the boundary with runtime printing. Handwritten package `detail` strings no longer define signatures.
