![Tapas](docs/Logo.png)

# Tapas

[简体中文](README.md) | English | [Project Home](README_en.md)

Tapas is a rule-driven testing system for complex business workflows.
Define business rules once, then use them to generate test inputs and check
execution results.

## A Simple Example

**Scenario:** A warehouse has 7 units in stock, but each order may buy at most
6. You want to test an order that empties the stock. Can these requirements
hold together?

Describe the order limit and stock change, then ask Tapas whether this test
scenario is feasible:

```tapas
let StockChange = rule (
        before: Int,   // Stock before the sale
        sold: Int,     // Quantity sold in this order
        after: Int     // Stock after the sale
) {
    "Each order may buy between 1 and 6 units":
        sold in rules::range(1, 6)

    "Quantity sold cannot exceed current stock":
        sold <= before

    "Remaining stock must equal initial stock minus quantity sold":
        after == before - sold
}
```

This rule describes the requirements that every sale must satisfy. Now fix the
stock before and after the sale to construct the “empty the stock with one
order” scenario:

```tapas
// Require one order to reduce stock from 7 to 0.
let trial_1 = rules::restrict(StockChange, 'before': 7, 'after': 0)

// Run the query.
let test_result_1 = solve::hold(trial_1)

// Show the conclusion and the conditions involved in the conflict.
pprint('status    = ', test_result_1::status)
pprint('conflicts = ', test_result_1::conflicts)
```
<pre class='Tapas-Return'>
status    = unsat
conflicts = [{
    rule : Rule #2[Int, Int, Int],
    condition : (sold in rules::range(1, 6))
}, {
    rule : Rule #2[Int, Int, Int],
    condition : (after == (before - sold))
}, {
    rule : Rule #3[Int, Int, Int],
    condition : (before == 7)
}, {
    rule : Rule #3[Int, Int, Int],
    condition : (after == 0)
}]
</pre>

The result is **`unsat` (no solution)**. Emptying the stock requires selling
7 units, but an order may buy at most 6. These requirements conflict.
`conflicts` lists the relevant rules and conditions to help identify the cause.

Change the target to leave 1 unit, and Tapas finds an input that sells **6 units**.
The business rules stay the same; only the test goal changes. Execute that input
in the inventory system and use the same rule to check the actual result.

```tapas
// Require 1 unit to remain in stock.
let trial_2 = rules::restrict(StockChange, 'before': 7, 'after': 1)

// Run the query.
let test_result_2 = solve::hold(trial_2)

// Show the conclusion and the conditions involved in the conflict.
pprint('status    = ', test_result_2::status)
pprint('conflicts = ', test_result_2::conflicts)
```
<pre class='Tapas-Return'>
status    = sat
conflicts = []
</pre>

## System Features

- Reuse the same rules for generation and checking. Input conditions and state
  changes for success and rejection can form one business model shared by tests.
- Find inputs for a test goal. Require an operation to empty stock, or fix
  identity and confirmation while seeking a request rejected solely for
  insufficient payment capacity.
- Adjust scenarios through restrictions. Fixed values, integer intervals, and
  discrete choices can be combined. Incompatible conditions produce an
  unsatisfiable result, with relevant Rules and conflict conditions when available.

Tapas targets stateful systems such as orders, payments, permissions, and
workflows, as well as AI agent and simulation environments. Current examples
connect generation, execution, and checking through test scripts. Multi-step
state exploration and failure reduction are not yet implemented.

## Explore Further

| What you want to learn | Start here |
| --- | --- |
| How to write a complete business test | [Retail tutorial](examples/retail/README_en.md): order modeling, correct rejections, defect detection, and multi-variable input generation |
| How to solve rules and read failures | [Feasibility](examples/solve/feasibility.tap) and [conflict diagnostics](examples/solve/diagnostics.tap): two small standalone programs |
| How to read and write Tapas code | [A First Look at Tapas](docs/examples/Basics_en.md): variables, functions, and control flow; [modules and packages](docs/examples/modules/README_en.md): organizing code across files |
| What else you can run | [Example collection](examples/README_en.md): solving, business tests, and general algorithms |

## Build, Test, and Install

Prepare a C23 compiler, CMake 3.21 or newer, GNU Readline, and Python 3. From
the repository root, build and run the tests:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Install Tapas in your user directory:

```sh
cmake --install build --prefix "$HOME/.tapas"
export PATH="$HOME/.tapas/bin:$PATH"
```

Before using the solver, make sure OR-Tools is installed for the system's
default `python3`. The following command runs a stock allocation example and
prints the solution:

```sh
tapas examples/solve/feasibility.tap
```

See [Usage](docs/Usage_en.md) for build dependency installation, and the
[solve documentation](src/stdlib/solve/README.md) (Chinese) for solver setup and
supported expressions. Ordinary rule checking does not require Python or
OR-Tools.

## Editing and Integration

The [Visual Studio Code extension](https://marketplace.visualstudio.com/items?itemName=tapas-language.tapas-language)
provides highlighting, diagnostics, completion, type hovers, navigation,
running, and formatting. It requires a separate Tapas Core installation; see
the [extension documentation](editors/vscode/README.md) for setup.

Tapas runs as scripts or can be embedded through its public C API. See
[C Interaction](docs/Foreign_en.md) for sessions, registering C functions, and
passing business data into Tapas.

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
