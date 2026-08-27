![Tapas](docs/Logo.png)

# Tapas

[简体中文](README.md) | English

Tapas is a compact, expression-oriented programming language and runtime for
economic modelling, numerical experiments, and embeddable scripting. The
current implementation is written in C23 and includes a bytecode compiler,
stack-based virtual machine, interactive REPL, module system, Markdown
execution, and a public C API.

## Features

- Interactive execution, scripts, modules, and reusable `.tapc` bytecode.
- First-class functions, closures, recursion, common containers, and dense arrays.
- Compile-time annotations, structural Types, union Types, and runtime reflection.
- Execution of Tapas source and fenced Tapas blocks in Markdown documents.
- A public C API, Language Server, and Visual Studio Code extension.

## Build

Tapas requires a C23 compiler, CMake 3.10 or newer, and GNU Readline.

From the project root, run:

```sh
cmake -S . -B build
cmake --build build
```

See [Usage](docs/Usage_en.md) for dependency installation, testing, and
installation instructions.

## Quick Start

Create a file named `hello.tap`:

```tapas
print('Hello, Tapas!')
```
<pre class='Tapas-Return'>
Hello, Tapas!
</pre>

Then execute it:

```sh
build/bin/tapas hello.tap
```

See [Usage](docs/Usage_en.md) for the REPL, command strings, bytecode, Markdown
execution, and module paths.

## Visual Studio Code

The extension under [editors/vscode](editors/vscode) provides syntax
highlighting, live diagnostics, Type hover information, completion, definition
lookup, references, rename, workspace module analysis, and a command for
running the current Tapas file.

After building the project, install the extension with:

```sh
editors/vscode/install.sh
```

Run **Developer: Reload Window** in VS Code after installation. See the
[VS Code extension documentation](editors/vscode/README.md) for capabilities
and configuration.

## Using Tapas From C

The public headers are under `include/tapas`. This example executes Tapas source
through a session:

```c
#include "tapas/tapas.h"

int main(void)
{
    tsession *session = tsession_new();

    tsession_execute_str(session, "print(6 * 7)", 1);
    tsession_free(session);
    return 0;
}
```

See [C Interaction](docs/Foreign_en.md) for API, linking, and extension details.

## Documentation

- [Usage](docs/Usage_en.md) — building, command-line options, scripts, bytecode,
  Markdown execution, and module paths.
- [Language Reference](docs/Syntax_en.md) — syntax, types, operators, statements,
  functions, modules, arrays, and built-ins.
- [Type System Design](docs/TypeSystem_en.md) — compile-time annotations,
  runtime Type values, structural Types, and the `types` package.
- [C Interaction](docs/Foreign_en.md) — embedding sessions, registering C
  functions, working with values, and extending composite types.
- [Runtime Mechanism](docs/Mechanism_en.md) — compiler, bytecode, virtual machine,
  environments, and reference counting.
- Examples: [Recursive Fibonacci](docs/examples/Fibonacci_en.md) and
  [Sorting Algorithms](docs/examples/Sort_en.md).

## License

Tapas is distributed under the MIT License. See [LICENSE](LICENSE).

## Contact

Issues and pull requests are welcome. Contact:
<linsheng.z@outlook.com>.
