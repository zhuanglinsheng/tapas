![Tapas](docs/Logo.png)

# Tapas

Tapas is a compact, expression-oriented programming language and runtime for
economic modelling, numerical experiments, and embeddable scripting. The
current implementation is written in C23 and includes a bytecode compiler,
stack-based virtual machine, interactive REPL, module system, Markdown
execution, and a public C API.

## Features

- Small C runtime with CMake-based builds.
- Interactive execution, source files, and reusable `.tapc` bytecode.
- Integers, floats, booleans, strings, lists, pairs, dictionaries, iterators,
  functions, libraries, dense arrays, and time values.
- First-class functions, closures, recursion, and module imports.
- Dense numeric and boolean arrays with slicing, element-wise operations,
  comparisons, transposition, and matrix multiplication.
- Scalar mathematics through the built-in `math` package.
- Execution of fenced Tapas blocks inside Markdown documents.
- Session and runtime APIs for embedding Tapas in C programs.

## Build

Tapas requires a C23 compiler, CMake 3.10 or newer, and GNU Readline.

On macOS, Readline can be installed with Homebrew:

```sh
brew install readline
```

On Debian or Ubuntu:

```sh
sudo apt install build-essential cmake libreadline-dev
```

Configure and build the project:

```sh
cmake -S . -B build
cmake --build build
```

The executable is generated at `build/bin/tapas`.

Run the test suite with:

```sh
ctest --test-dir build --output-on-failure
```

To install the command into the selected CMake prefix:

```sh
cmake --install build
```

## Quick Start

Start the interactive REPL:

```sh
build/bin/tapas
```

Run a command directly:

```sh
build/bin/tapas -i "print(1 + 2)"
```

Create a file named `hello.tap`:

```tapas
let values = [1, 2, 3, 4]
let matrix = array(2, 2, values)

print('values = ', values)
sprint(matrix ** eig::transpose(matrix))
```

Then execute it:

```sh
build/bin/tapas hello.tap
```

Tapas can also compile source into bytecode and execute it later:

```sh
build/bin/tapas -c hello.tap
build/bin/tapas -e hello.tapc
```

## Using Tapas From C

The public headers are under `include/tapas`. A session can execute source from
a file or a string:

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

The runtime also exposes constructors and operations for strings, lists,
dictionaries, iterators, dense arrays, and time values through
`tapas/tval.h` and the headers under `tapas/ds`.

## Documentation

- [Usage](docs/Usage.md) — building, command-line options, scripts, bytecode,
  Markdown execution, and module paths.
- [Language Reference](docs/Syntax.md) — syntax, types, operators, statements,
  functions, modules, arrays, and built-ins.
- [C Interaction](docs/Foreign.md) — embedding sessions, registering C
  functions, working with values, and extending composite types.
- [Runtime Mechanism](docs/Mechanism.md) — compiler, bytecode, virtual machine,
  environments, and reference counting.
- [Examples](docs/examples) — Fibonacci, sorting algorithms, bytecode, and
  module layouts.

## License

Tapas is distributed under the MIT License. See [LICENSE](LICENSE).

## Contact

Issues and pull requests are welcome. Contact:
<zhuanglinsheng@outlook.com>.
