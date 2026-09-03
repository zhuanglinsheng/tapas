# Tapas Language Server

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
