# Tapas for Visual Studio Code

[简体中文](https://github.com/zhuanglinsheng/tapas/blob/main/editors/vscode/README_zh.md) | English | [Project Home](https://github.com/zhuanglinsheng/tapas)

This extension connects Visual Studio Code directly to the C implementation of
`tapas-language-server`. It recognizes `.tap` files and has no npm runtime
dependencies.

Rule expressions support `not`, `and`, and `or` with `Bool` and `RuleInstance`
operands; `and` and `or` short-circuit. Write Rule dependencies as bare instances
such as `Valid(x)`; `require` is no longer a keyword. Outside Rule expressions,
these logical operators accept only `Bool` operands.

Use `tapas-language-server`, `tapas`, and the `format` package from the same Core
build. Updating the extension alone updates static highlighting, but does not
update diagnostics, execution, or formatting: the extension uses external Core
tools and does not bundle them.

## Current capabilities

### Editing

- Tapas syntax highlighting for comments, strings, numbers, keywords, built-in
  Types, constants, and operators.
- Semantic distinction for namespaces, Types, functions, parameters, and
  variables using compiler-front-end symbol resolution, with immediate static
  TextMate highlighting while the server is unavailable or starting.
- Line comments with `//`.
- Bracket matching, automatic bracket and quote closing, surrounding pairs,
  and brace-based indentation.
- Correct LSP locations for Chinese identifiers, emoji, and other UTF-8 source
  through UTF-16 position conversion.

### Diagnostics and Type information

- Live syntax diagnostics after a document is opened or changed.
- Recoverable semantic diagnostics and symbol information while source is
  temporarily incomplete.
- Hover information for explicit annotations, locally inferred Types, root
  built-in signatures, default packages, and package members.
- Local inference currently covers literals, functions, arithmetic, pairs,
  lists, dictionaries, imports, and annotated bindings.
- Fixed member reads such as `state::status` display the field Type on hover.
  Completion after `state::` uses structural Types, including function/Rule
  parameters, nested members, and imported Types such as `model::State`.
  These features reuse AST Type facts without executing user code.
- Type presentation preserves named structural references, for example `Rule[State]`
  and `Function[State] -> List[State]`, including aliases and nested annotations.
  Hover and completion share this view; hovering the Type value itself shows its
  definition. Explicit anonymous structural Types remain expanded; dictionary literals
  infer `Dictionary` and do not declare field constraints. Builtin Types use canonical names.
- Names and `::` members inside parameter, return, and variable Type annotations
  support hover, definition lookup, references, and rename through a separate
  annotation reference index, including Rule references in `InstanceOf[model::Exchange]`.
  Parameter annotations use the definition scope,
  even when a parameter has the same name as a Type or module.

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

### Formatting

- Supports VS Code **Format Document**, format on save, and the
  **Tapas: Format Document** command.
- The provider sends the current editor text through the `format` source
  package shipped with Tapas and returns one VS Code text edit. Unsaved changes
  are not written to the original file first.
- The extension maintains no second set of layout rules. Single-line function
  signatures, multiline parameter lists, and compact control flow behave
  exactly like `tapas -m format`.
- Formatting depends only on the Tapas runtime and `format` package, so it still
  works when the Language Server is unavailable.

The parser and semantic model are independent from the Tapas VM. Editing a
document never executes user code.

## Current boundaries

Public module interfaces come from the dictionary returned by the module's
final statement. Cross-module queries currently target public members accessed
with `::`; ordinary local symbols remain file- and lexical-scope based.
Signature help, semantic-token delta responses, and incremental text
synchronization remain planned work. Structural-field navigation/rename and
union/recursive structural completion are not yet supported; dynamically unknown
members may have no Type information.
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

## Install the extension

The extension is a thin client and contains no native binaries. Install Tapas
Core separately so that `tapas`, `tapas-language-server`, and the standard
library are available, or configure both executable paths explicitly in VS
Code settings.

Install **Tapas** from the Visual Studio Code Marketplace:

```sh
code --install-extension tapas-language.tapas-language
```

Alternatively, download a VSIX from the GitHub Release and install that file:

```sh
code --install-extension /path/to/tapas-language-VERSION.vsix
```

To install the client directly from a development checkout, run:

```sh
editors/vscode/install.sh
```

The script installs only the extension client under the local VS Code extensions directory. It does not copy the runtime, Language Server, or standard library. Run **Developer: Reload Window** in VS Code, then open a `.tap` file.

The extension searches for the Language Server in this order:

1. the `tapas.languageServer.path` setting;
2. `build/bin/tapas-language-server` in the extension repository while
   debugging the extension;
3. `$HOME/.tapas/bin/tapas-language-server`;
4. `tapas-language-server` on `PATH`.

The extension does not automatically execute a Language Server from the
current workspace.

Set `tapas.languageServer.path` to an absolute path when automatic discovery is
not suitable.

If `rule` or `types::enum` works in the CLI but produces editor syntax errors,
check for a stale server bundled in an older extension. When developing Tapas,
set workspace `tapas.languageServer.path` and `tapas.runtime.path` to absolute
paths for `build/bin/tapas-language-server` and `build/bin/tapas` from the same
build. Rebuild and run VS Code's `Developer: Reload Window`; updating source
alone does not replace an already running server.

The extension searches for the Tapas runtime in this order:

1. the `tapas.runtime.path` setting;
2. `build/bin/tapas` in the extension repository while debugging the
   extension;
3. `$HOME/.tapas/bin/tapas`;
4. `tapas` on `PATH`.

The extension does not automatically execute a Tapas runtime from the current
workspace.

Running the current file is equivalent to:

```sh
tapas -p WORKSPACE_ROOT CURRENT_FILE
```

For formatting, the extension runs an equivalent command in an isolated
temporary directory and returns the result as an editor change:

```sh
tapas -m format TEMPORARY_FILE
```

## Package a VSIX

Run the official `vsce` packaging tool from the extension directory:

```sh
cd editors/vscode
npx --yes @vscode/vsce package --no-dependencies
```

The manifest check runs automatically before packaging. The resulting VSIX
contains only the extension client, grammar, documentation, and license.

## Tests

The protocol test first exercises formatting through the real Tapas runtime,
then starts the real C Language Server and exercises initialize, document
synchronization, diagnostics, semantic tokens, default-package completion,
user-module member completion, cross-file hover, and definition lookup through
the same Node protocol client used by the extension. It also
compares the compiler's keyword, built-in Type, and default-package catalogs
against the TextMate fallback grammar to prevent version drift:

```sh
node editors/vscode/test/protocol.test.js \
  build/bin/tapas-language-server build/bin/tapas
```

It is also registered in CTest as `vscode_protocol` when Node.js is available.
