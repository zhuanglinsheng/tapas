# Tapas Language Server

[简体中文](README_zh.md) | English

`tapas-language-server` is a standalone stdio Language Server Protocol process.
It links only the reusable compiler front end; it does not initialize the VM or
execute user code.

Implemented protocol features:

- full document synchronization (`didOpen`, `didChange`, `didClose`);
- recoverable parser and semantic diagnostics;
- UTF-16 LSP position conversion for UTF-8 Tapas source;
- hover information with inferred or annotated local Types;
- local and cross-module definition and reference lookup;
- document and workspace symbols;
- prefix-aware local and standard-environment completion;
- default-package and imported-module member completion after `::`;
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
notifications, and LSP serialization. `src/compile/module.c` defines public
module interfaces and the standard environment; `src/compile/workspace.c` owns
document snapshots, disk indexing, imports, and cross-file identity. Protocol
handlers translate those results into LSP positions and edits.

Remaining editor work includes signature help, semantic tokens, incremental
text synchronization, and migration of the complete structural Type checker
from compiler bindings into the reusable type-information model.
