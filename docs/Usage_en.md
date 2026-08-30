# How to Use Tapas

[简体中文](Usage_zh.md) | English | [Project Home](../README_en.md)

This document explains how to build and run the Tapas command-line program.

For language syntax and built-in functions, see `Syntax_en.md`.



## Build

Tapas requires a C23 compiler, CMake 3.10 or newer, and GNU Readline.

On macOS, install Readline with Homebrew:

```sh
brew install readline
```

On Debian or Ubuntu, install the required build tools and Readline development
package:

```sh
sudo apt install build-essential cmake libreadline-dev
```

Configure and build from the project root:

```sh
cmake -S . -B build
cmake --build build
```

Build and run the tests with:

```sh
ctest --test-dir build --output-on-failure
```

Install the executable into the configured CMake prefix with:

```sh
cmake --install build
```

The executable is written to:

```text
build/bin/tapas
```

If Tapas has been installed into your `PATH`, the command may also be available
as `tap`.



## Command Summary

```text
tap [OPTION] [FILE/CMD]
```

If no option or file is given, Tapas starts an interactive REPL.

Supported options:

```text
-h                print command-line help
-v                print version information
-c FILE           compile .tap or .md source to .tapc bytecode
-e FILE           execute .tapc bytecode
-r FILE           display .tapc bytecode
-ce FILE          compile source, then execute the produced bytecode
-cr FILE          compile source, then display the produced bytecode
-p PATH           add a module search path for later commands in this session
-i CMD            execute one Tapas command string
--stdout          for Markdown input, write execution output to stdout
```

Options are processed from left to right. For example, `-p examples -ce main.tap`
adds `examples` to the module search path before compiling and executing
`main.tap`.



## Interactive Mode

Start the REPL by running Tapas with no arguments:

```sh
build/bin/tapas
```

The prompt is `>>` for a complete input unit and `..` while Tapas is waiting for
more lines, such as inside a function or block.

Example:

```text
>> let x = 1 + 2
>> print(x)
3
```

Use:

```text
exit()
```

to leave the REPL.

Use `__binary__()` inside Tapas code to inspect bytecode.



## Execute a Script

Tapas source files normally use the `.tap` suffix.

Example `hello.tap`:

```tapas
print('hello, Tapas')
```
<pre class='Tapas-Return'>
hello, Tapas
</pre>

Run it directly:

```sh
build/bin/tapas hello.tap
```

This compiles and executes the file in memory. It does not keep a `.tapc`
bytecode file.



## Execute a Command String

Use `-i` to run one Tapas command string:

```sh
build/bin/tapas -i "print(1 + 2)"
```

The command is executed as an interactive input block, so the command-line tool
prints `Result:` before the Tapas output.



## Compile and Run Bytecode

Use `-c` to compile a source file to bytecode:

```sh
build/bin/tapas -c hello.tap
```

This writes:

```text
hello.tapc
```

Use `-e` to execute the compiled bytecode:

```sh
build/bin/tapas -e hello.tapc
```

The `-e` option also accepts the source filename stem. These commands load the
same bytecode file:

```sh
build/bin/tapas -e hello.tap
build/bin/tapas -e hello.tapc
```

Use `-ce` to compile and execute in one command:

```sh
build/bin/tapas -ce hello.tap
```



## Inspect Bytecode

Use `-r` to display a `.tapc` file:

```sh
build/bin/tapas -r hello.tapc
```

Use `-cr` to compile and display bytecode in one command:

```sh
build/bin/tapas -cr hello.tap
```

Inside Tapas code, the debugging function `__binary__()` can also print the
current bytecode wrapper. See `Mechanism_en.md` for details.



## Markdown Files

Tapas can read Markdown files and execute fenced Tapas code blocks.

A Tapas code block uses either `tapas` or `tap` as the fence language:

````markdown
```tapas
print(1 + 2)
```
<pre class='Tapas-Return'>
3
</pre>
````

Repository documentation uses `tapas` for executable Tapas source and `text`
for syntax fragments, placeholders, deliberately invalid examples, and Tapas
source that requires a host extension not available in the standard binary.
Other languages use their normal fence names, such as `c`, `sh`, and `ebnf`.

When a document contains several `tapas` blocks, Tapas reads them in document
order as one program. A later block may therefore use declarations from an
earlier block. Each Markdown document in this repository, except the archived
`cpp/` implementation, must execute successfully with:

```sh
build/bin/tapas --stdout path/to/document.md
```

Prefer executable examples. Use `text` only when making the example executable
would hide the rule or behavior being explained.

When a Markdown file is passed as a normal file argument, Tapas executes each
Tapas code block and updates the Markdown file in place by inserting or
replacing return blocks:

```sh
build/bin/tapas docs/example.md
```

Generated output is written as:

```html
<pre class='Tapas-Return'>
3
</pre>
```

Use `--stdout` to execute the Markdown file without updating it in place:

```sh
build/bin/tapas --stdout docs/example.md
```

Markdown files can also be compiled with `-c`:

```sh
build/bin/tapas -c docs/example.md
```

This writes `docs/example.tapc`.



## Module Search Paths

Tapas modules are imported with the `import ... as ...` syntax:

```tapas
import examples/modules/demo-lib-1.tap as demo
```

Use `-p` to add a search path before compiling or executing a file:

```sh
build/bin/tapas -p docs -ce examples/main.tap
```

Inside Tapas, use `__path__().sprint()` to inspect the session search paths:

```tapas
__path__().sprint()
```
<pre class='Tapas-Return'>
[docs]
</pre>

Use `__ls__().sprint()` to inspect objects registered in the current root
library:

```tapas
__ls__().sprint()
```
<pre class='Tapas-Return'>
[print, sprint, len, type, copy, identical, clock, now, array, int, float, bool, str, list, push, append, insert, pop, delete, idx, keys, dkeys, dvalues, union, pair, iter, sort, dense, math, __ls__, __path__, __param__, __nparam__, __binary__]
</pre>



## Common Examples

Run a Markdown example and write output to the terminal:

```sh
build/bin/tapas --stdout docs/examples/Sort_en.md
```

Compile and execute a source file:

```sh
build/bin/tapas -ce docs/examples/Sort_en.md
```

Execute an existing bytecode file:

```sh
build/bin/tapas -e docs/examples/Sort.tapc
```

Print version information:

```sh
build/bin/tapas -v
```

Print help:

```sh
build/bin/tapas -h
```



## Notes and Limitations

- The command-line program supports `-h`, not `--help`.
- The matrix multiplication operator `@` requires an LP64 CBLAS dynamic library
  at runtime. Tapas loads it when a BLAS-backed array operation is first used.
  If automatic discovery fails, set `TAPAS_BLAS_LIBRARY` to the library path.
- The default Markdown mode updates the input file in place. Use `--stdout` for
  terminal output only.
- `-p PATH` affects later arguments in the same command invocation. Put it
  before the file that needs the path.
- `-e` and `-r` load `.tapc` bytecode. If a source filename is passed, Tapas
  replaces its suffix with `.tapc`.
- `.tapc` files depend on the default objects registered by the runtime. If the
  built-in object list changes, recompile old bytecode with `-c` or `-ce`.
- Tapas exits with an error when compilation or runtime errors are raised.
