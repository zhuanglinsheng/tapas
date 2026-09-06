![Tapas](docs/Logo.png)

# Tapas

[简体中文](README.md) | English

Tapas is a DSL and solver for business-constraint testing. It expresses
constraints from orders, payments, permissions, AI agents, and simulation
environments as executable Rules that can check a real system's inputs,
outputs, and state transitions, or generate boundary data and inputs that
deliberately violate a selected condition.

Tapas centers both checking criteria and data-generation models on the same
Rule. Rules can be composed, restricted, or inverted so tests can construct
data for a specific objective without maintaining separate rules and examples;
when conditions cannot hold together, the solver also identifies the relevant
Rules and conflicting conditions.

## A Simple Example: Generate a Targeted Failure

**Scenario:** A warehouse needs an insufficient-stock sales request. Each order
may buy between 1 and 6 units, but the quantity sold cannot exceed current
stock.

1. **Describe the business rule.** `SaleRequest` defines every condition a
normal sales request must satisfy:

```tapas
let SaleRequest = rule (
        current_stock: Int,
        sale_quantity: Int
) {
    'each order may buy between 1 and 6 units':
        sale_quantity in rules::range(1, 6)

    'sale quantity cannot exceed current stock':
        sale_quantity <= current_stock
}
```

2. **Generate insufficient-stock data.** Use `rules::violate` to reverse “sale
quantity cannot exceed current stock,” then generate one value from the target
scenario:

```tapas
let InsufficientStockRequest = rules::violate(
    SaleRequest,
    'sale quantity cannot exceed current stock'
)

let generated = solve::sample(
        InsufficientStockRequest,
        1,
        {
            'current_stock': rules::range(0, 7),
            'sale_quantity': rules::range(1, 6)
        },
        rng = random::generator(random::pcg32_xsh_rr, 42)
)

let test_input = generated::samples[0]
pprint('test input = ', test_input)
```
<pre class='Tapas-Return'>
test input = {
    sale_quantity : 2,
    current_stock : 0
}
</pre>

3. **Confirm the generation objective.** Bind the sample to the original Rule.
The result should be `unsat`, and its conflict should point to the condition
that was reversed:

```tapas
let result = solve::hold(SaleRequest(test_input))
pprint('status    = ', result::status)
pprint('conflicts = ', result::conflicts)
```
<pre class='Tapas-Return'>
status    = unsat
conflicts = [{
    rule : Rule #1[Int, Int],
    condition : (sale_quantity <= current_stock)
}]
</pre>

This is not an accidental random edge case: Tapas preserves “buy between 1 and
6 units” and reverses only the selected stock condition. The fixed random
source makes the output reproducible. See the
[solve package documentation](src/stdlib/solve/README.md) for distributions,
random generators, and sampling limits; the complete program is available in
the [failure-data generation example](examples/solve/generate_violations.tap).

## Explore Further

| What you want to learn | Start here |
| --- | --- |
| How to check a complete business state transition | [Retail tutorial](examples/retail/README_en.md): bind prior state, request, actual output, and resulting state to a model, then check correct rejections and an intentional defect |
| How to generate failure test data | [Failure-data generation example](examples/solve/generate_violations.tap): derive a scenario from a named Rule condition, sample it, and check the results |
| How to solve rules and read failures | [Feasibility](examples/solve/feasibility.tap) and [conflict diagnostics](examples/solve/diagnostics.tap): two small standalone programs |
| How to read and write Tapas code | [A First Look at Tapas](docs/examples/Basics_en.md): variables, functions, and control flow; [modules and packages](docs/examples/modules/README_en.md): organizing code across files |
| What else you can run | [Example collection](examples/README_en.md): solving, business tests, and general algorithms |

## Installation and Build

Download a prebuilt Linux or macOS archive from
[GitHub Releases](https://github.com/zhuanglinsheng/tapas/releases), or build
Tapas from source.

Tapas depends on GNU Readline and Python 3, with the OR-Tools package installed
in the Python environment. Make sure the system can find GNU Readline and
Python through its default search paths, and that Python can import `ortools`.
With a C23 compiler and CMake 3.21 or newer, build and run the tests from the
repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The suite covers source and bytecode execution, valid and invalid language
rules, solving, the language server, formatting, and runnable documentation
examples.

Install Tapas in your user directory:

```sh
cmake --install build --prefix "$HOME/.tapas"
export PATH="$HOME/.tapas/bin:$PATH"
```

See [Usage](docs/Usage_en.md) for dependency installation, and the
[solve documentation](src/stdlib/solve/README.md) (Chinese) for supported
expressions.

## Visual Studio Code Extension

The Tapas extension for Visual Studio Code provides syntax highlighting, live
diagnostics, Type hovers and completion, plus cross-module navigation,
reference search, and rename. Its language service indexes `.tap` files across
the workspace and continues to provide recoverable analysis while source code
is temporarily incomplete.

After installing Tapas Core, install the extension from the
[Visual Studio Code Marketplace](https://marketplace.visualstudio.com/items?itemName=tapas-language.tapas-language).
See the [extension documentation](https://github.com/zhuanglinsheng/tapas/blob/main/editors/vscode/README.md)
for VSIX installation and runtime or language-server path configuration.

## Documentation

- [Solver API](src/stdlib/solve/README.md) (Chinese) — feasibility queries, conflict
  diagnostics, supported expressions, and runtime configuration.
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
