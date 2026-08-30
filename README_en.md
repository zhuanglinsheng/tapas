![Tapas](docs/Logo.png)

# Tapas

[简体中文](README.md) | English

Tapas is an expression-oriented programming language designed for readability
and equipped with a structural type system. It is evolving into a programmable
declarative language for testing complex systems.

## Direction

Tapas aims to make domain rules first-class declarations that programs can
construct, compose, and interpret. Validators, generators, and other evaluators
can derive valid test states, behavioral checks, boundary scenarios, conflict
explanations, and failure reduction from the same declarations. The initial
focus is testing complex stateful systems and AI agent business environments.
Declarations should express domain intent directly, allowing readers to
understand a program without first learning its underlying execution,
generation, or solving mechanisms.

The proposed `declare` construct, typed declaration IR, evaluator interfaces,
and test-generation facilities are still being designed and implemented; they
are not part of the current language specification. See the
[verifiable agent environment design](docs/paper_or/AgentEnvironment_zh.md)
(in Chinese) for the current direction.

## Current Language Implementation

The current release provides the language, type-system, and runtime foundations
for that direction:

- Concise, expression-oriented syntax with first-class functions, closures, and recursion.
- Compile-time annotations, structural Types, union Types, first-class Type values, and runtime reflection.
- Common containers, dense arrays, modules, and reusable `.tapc` bytecode.
- A Language Server and Visual Studio Code extension.

Tapas is implemented in C23. Source is compiled to bytecode and executed by a
stack-based virtual machine. Tapas programs can run as scripts, in the
interactive REPL, or as code blocks in Markdown. The public C API embeds the
Tapas runtime in other programs.

## Algorithm Examples

The top-level [`examples`](examples) directory contains directly executable
`.tap` programs. Together they demonstrate common algorithms and the main Tapas
language features:

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

See [`examples/README_en.md`](examples/README_en.md) for the complete index and
maintenance conventions. Executable tutorials with explanations, complexity
analysis, and recorded output remain under [`docs/examples`](docs/examples).

## Build and Run

Tapas requires a C23 compiler, CMake 3.10 or newer, and GNU Readline.

From the project root, run:

```sh
cmake -S . -B build
cmake --build build
```

After building, create a file named `hello.tap`:

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

You can also run an algorithm example from the repository root:

```sh
build/bin/tapas examples/fibonacci.tap
```

See [Usage](docs/Usage_en.md) for dependency installation, testing,
installation, the REPL, command strings, bytecode, and module paths.

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
