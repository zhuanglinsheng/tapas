![Tapas](docs/Logo.png)

# Tapas

[简体中文](README.md) | English | [Project Home](README_en.md)

Tapas is an expression-oriented programming language designed for readability
and equipped with a structural type system. It is evolving into a programmable
declarative language for testing complex systems. Its goal is to make domain
rules first-class values that programs can construct, compose, and interpret,
allowing validators, generators, and other interpreters to derive valid states,
behavioral checks, boundary scenarios, conflict explanations, and failure
reduction from the same rule definitions.

Tapas initially focuses on describing test rules for complex stateful systems
and AI agent business environments. It keeps code close to business concepts
and constraints, allowing readers to understand what a test verifies without
first learning how its rules are executed, generated, or solved.

## Language Features

- Business rules can be stored, passed around, and composed like ordinary data.
  The same rule can drive direct checks or be analyzed and interpreted by other
  tools.
- A flexible type system catches errors at compile time while letting programs
  inspect and compose types at runtime to describe complex domain states.
- A compact, expression-oriented core includes first-class functions, closures,
  recursion, common containers, and dense arrays.
- The Language Server and Visual Studio Code extension provide live diagnostics,
  completion, hover information, and cross-module navigation.

Tapas is implemented in C23. Source is compiled to bytecode and executed by a
stack-based virtual machine. Tapas programs can run as scripts, in the
interactive REPL, or as code blocks in Markdown. The public C API embeds the
Tapas runtime in other programs.

## Examples

[A First Look at Tapas](docs/examples/Basics_en.md) brings variables, lists,
functions, and control flow together in one short, runnable program. From
Fibonacci and classic sorting algorithms to graph search and dynamic
programming, the complete programs below show how Tapas expresses different
kinds of computation:

| Example | Topic and focus |
| --- | --- |
| [Fibonacci](examples/fibonacci.tap) | Recursive and iterative Fibonacci, demonstrating recursion, loops, and lists |
| [Sorting](examples/sorting.tap) | Bubble, selection, insertion, shell, merge, quick, and heap sort, demonstrating slices, higher-order functions, and mutation |
| [Binary search](examples/binary_search.tap) | Searching an ordered list, demonstrating loop boundaries and early returns |
| [Euclidean algorithm](examples/euclidean_algorithm.tap) | GCD, LCM, and Bézout coefficients, demonstrating integer arithmetic and multi-value results |
| [Sieve of Eratosthenes](examples/sieve_of_eratosthenes.tap) | Finding primes, demonstrating Boolean lists and nested loops |
| [Breadth-first search](examples/breadth_first_search.tap) | Traversing a dictionary-backed graph, demonstrating queues, dictionaries, and membership tests |
| [Longest common subsequence](examples/longest_common_subsequence.tap) | Dynamic programming over two strings, demonstrating arrays and result reconstruction |
| [Newton's method](examples/newton_method.tap) | Approximating square roots and nonlinear roots, demonstrating floating-point and math functions |

See the [syntax example collection](docs/examples/syntax) for more examples and
[modules and directory packages](docs/examples/modules/README_en.md) for module
organization.

## Build, Test, and Install

Tapas requires a C23 compiler, CMake 3.21 or newer, and GNU Readline.
From the project root, build and test a Release configuration, then install it
in the user directory:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build --prefix "$HOME/.tapas"
```

Add the installation directory to `PATH`, verify the version, and run an
example:

```sh
export PATH="$HOME/.tapas/bin:$PATH"
tapas --version
tapas examples/fibonacci.tap
```

See [Usage](docs/Usage_en.md) for dependency installation, installation, the
REPL, bytecode, modules, and more.

## Visual Studio Code Extension

Tapas provides a separate thin VS Code extension with syntax highlighting,
live diagnostics, Type hover information, completion, definition lookup,
references, rename, workspace module analysis, running, and formatting. The
extension does not contain Tapas Core.

First install Tapas Core under `$HOME/.tapas` as described above. After the
official release, install **Tapas Language Support** from the Visual Studio
Code Marketplace, or download the standalone VSIX from the GitHub Release:

```sh
code --install-extension tapas-language-0.1.0.vsix
```

The extension searches explicit settings, `$HOME/.tapas/bin`, and the system
`PATH` for `tapas` and `tapas-language-server`. It does not execute binaries
from the current workspace. See the
[VS Code extension documentation](editors/vscode/README.md) for source
debugging, development installation, tests, packaging, and detailed
configuration.

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
  functions, modules, and arrays.
- [Standard Library](docs/Stdlib_en.md) — root built-ins, native packages, and
  source packages shipped with Tapas.
- [Code Style](docs/Style_en.md) — indentation, function and control-flow
  layout, and the `format` package.
- [Type System Design](docs/TypeSystem_en.md) — compile-time annotations,
  Type values, structural Types, and the `types` package.
- [Rules](docs/Rules_en.md) — Rule literals, composition, standard checks,
  public Rule IR, and evaluators.
- [C Interaction](docs/Foreign_en.md) — embedding sessions, registering C
  functions, working with values, and extending composite types.
- [Runtime Mechanism](docs/Mechanism_en.md) — compiler, bytecode, virtual machine,
  environments, and reference counting.
- [Performance Benchmarks](test/benchmarks/Results_en.md) — per-workload Tapas
  and Python comparisons for algorithms and VM hot paths.

## License

Tapas is distributed under the MIT License. See [LICENSE](LICENSE).

## Contact

Issues and pull requests are welcome. Contact:
<linsheng.z@outlook.com>.
