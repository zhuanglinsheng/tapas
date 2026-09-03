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

See the [syntax example collection](docs/examples/syntax) for more examples.

## Build and Run

Tapas requires a C23 compiler, CMake 3.21 or newer, and GNU Readline.
From the project root, build Tapas, run the tests, and execute an example:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
build/bin/tapas examples/fibonacci.tap
```

See [Usage](docs/Usage_en.md) for dependency installation, installation, the
REPL, bytecode, modules, and more.

## Visual Studio Code

The extension under [editors/vscode](editors/vscode) provides syntax
highlighting, live diagnostics, Type hover information, completion, definition
lookup, references, rename, workspace module analysis, and a command for
running the current Tapas file.

This is a thin extension with no bundled native programs. Install Tapas Core
first so `tapas` and `tapas-language-server` are on `PATH`, or configure their
paths in settings. Install the extension from a development checkout with:

```sh
editors/vscode/install.sh
```

Run **Developer: Reload Window** in VS Code after installation. See the
[VS Code extension documentation](editors/vscode/README_en.md) for capabilities
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
- Examples: [Basic Syntax](docs/examples/Basics_en.md),
  [extended syntax programs](docs/examples/syntax), and
  [modules and directory packages](docs/examples/modules/README_en.md).

## License

Tapas is distributed under the MIT License. See [LICENSE](LICENSE).

## Contact

Issues and pull requests are welcome. Contact:
<zhuanglinsheng@outlook.com>.
