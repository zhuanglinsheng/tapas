# Tapas for Visual Studio Code

[简体中文](README_zh.md) |  English

This extension connects Visual Studio Code directly to the C implementation of
`tapas-language-server`. It recognizes `.tap` files and has no npm runtime
dependencies.

## Current capabilities

### Editing

- Tapas syntax highlighting for comments, strings, numbers, keywords, built-in
  Types, constants, and operators.
- Line comments with `//`.
- Bracket matching, automatic bracket and quote closing, surrounding pairs,
  and brace-based indentation.
- Correct LSP locations for Chinese identifiers, emoji, and other UTF-8 source
  through UTF-16 position conversion.

### Diagnostics and Type information

- Live syntax diagnostics after a document is opened or changed.
- Recoverable semantic diagnostics and symbol information while source is
  temporarily incomplete.
- Hover information for explicit annotations and locally inferred Types.
- Local inference currently covers literals, functions, arithmetic, pairs,
  lists, dictionaries, imports, and annotated bindings.

### Navigation and refactoring

- Go to definitions within a document and across imported user modules.
- Find references to exported user-module members across the workspace,
  optionally including the export declaration.
- Document symbols for bindings, functions, imports, parameters, and iteration
  variables.
- Workspace symbol search across top-level bindings, functions, and imports in
  every indexed module.
- Prefix completion for visible local symbols, root built-ins, and default
  packages, including available Type details and function signatures.
- Member completion after `package::` for the `math`, `types`, `dense`, and
  `time` default packages.
- Member completion after `package::` for user modules introduced by
  `import ... as package`; exports come from the dictionary returned by the
  module's final statement.
- Namespace aliases remain resolvable, for example
  `let numbers = math; numbers::sqrt(...)`.
- Rename validation and preview. Local rename edits the declaration and local
  references; exported-member rename edits the public export key and every
  resolvable workspace member reference.

### Workspace analysis

- Recursive indexing of `.tap` files under workspace roots, plus on-demand
  loading for directly imported modules outside those roots.
- Import lookup relative to the importing file and then each workspace root.
  Directory modules resolve through `__init__.tap`, matching the compiler.
- Unsaved open documents override disk snapshots; closing restores disk-backed
  analysis.
- File watching refreshes module interfaces after `.tap` files are created,
  changed, or deleted.
- Diagnostics for unresolved imports, unknown module members, and circular
  imports, with cycle-safe traversal.

### Running

- Run an open `.tap` file from the editor-title play button or the
  **Tapas: Run Current File** command.
- The extension saves the file first and shows output and errors in the VS Code
  integrated terminal.
- The file directory becomes the working directory, and its workspace root is
  added to the Tapas module search path.
- Running is independent from the Language Server, so programs can still run
  when analysis is unavailable.

The parser and semantic model are independent from the Tapas VM. Editing a
document never executes user code.

## Current boundaries

Public module interfaces come from the dictionary returned by the module's
final statement. Cross-module queries currently target public members accessed
with `::`; ordinary local symbols remain file- and lexical-scope based.
Signature help, semantic tokens, incremental text synchronization, and complete
structural Type hover information remain planned work.
The run command does not provide breakpoints, stepping, variable inspection, or
call stacks; those features require a separate Tapas Debug Adapter.

## Build and run from the repository

Tapas requires a C23 compiler, CMake, and GNU Readline. Build the runtime and
Language Server first:

```sh
cmake -S . -B build
cmake --build build -j4
```

Open the repository in VS Code, select **Run and Debug**, and launch
**Tapas Extension**. The checked-in launch configuration starts an Extension
Development Host and finds `build/bin/tapas-language-server` automatically.

## Install locally

Run:

```sh
editors/vscode/install.sh
```

The script installs the extension under the local VS Code extensions directory
and bundles the current `build/bin/tapas-language-server` and `build/bin/tapas`
binaries. Run
**Developer: Reload Window** in VS Code, then open a `.tap` file.

The extension searches for the Language Server in this order:

1. the `tapas.languageServer.path` setting;
2. the binary bundled by `install.sh`;
3. `build/bin/tapas-language-server` in the extension repository or current
   workspace;
4. `tapas-language-server` on `PATH`.

Set `tapas.languageServer.path` to an absolute path when automatic discovery is
not suitable.

The extension searches for the Tapas runtime in this order:

1. the `tapas.runtime.path` setting;
2. the executable bundled by `install.sh`;
3. `build/bin/tapas` in the extension repository or current workspace;
4. `tapas` on `PATH`.

Running the current file is equivalent to:

```sh
tapas -p WORKSPACE_ROOT CURRENT_FILE
```

## Tests

The protocol test starts the real C Language Server and exercises initialize,
document synchronization, diagnostics, default-package completion, user-module
member completion, cross-file hover, and definition lookup through the same Node
protocol client used by the extension:

```sh
node editors/vscode/test/protocol.test.js build/bin/tapas-language-server
```

It is also registered in CTest as `vscode_protocol` when Node.js is available.

## Implementation

- `extension.js` maps Language Server responses to VS Code providers and owns
  document and diagnostic lifecycles.
- `protocol.js` implements Content-Length framing and JSON-RPC request,
  response, and notification handling.
- `language-configuration.json` defines comments, brackets, closing pairs, and
  indentation.
- `syntaxes/tapas.tmLanguage.json` defines TextMate syntax highlighting.
- `../../src/lsp/` contains the C Language Server.
- `../../src/compile/module.c` extracts VM-independent public module interfaces
  and exposes the standard-environment catalog.
- `../../src/compile/workspace.c` owns document overlays, disk indexing, the
  import graph, and cross-module resolution.
