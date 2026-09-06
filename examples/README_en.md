# Tapas Examples

[简体中文](README.md) | English | [Project Home](../README_en.md)

Start with a small program to learn Tapas, or follow the retail exchange example to use the same Rules for checking execution results and generating test inputs.

If the syntax is new to you, read the [language introduction](../docs/examples/Basics_en.md) first. For rule solving, start with the [solve examples](solve), then follow the [retail tutorial](retail/README_en.md).

## Running an example

Build Tapas using the [project instructions](../README_en.md). Run the commands below from the repository root; they use the executable from that build:

```sh
build/bin/tapas examples/general/fibonacci.tap
```

The `general` examples only need Tapas. The `solve` and `retail` examples also require Python and OR-Tools. Follow the [solve dependency instructions](../src/stdlib/solve/README.md#依赖) (Chinese) to prepare the environment.

## Solving a Rule

The [solve examples](solve) contain two standalone programs covering feasibility and conflict diagnostics.

The first allocates seven stock units between a new order and an existing reservation. It uses `rules::restrict` to fix the reservation at four, then calls `solve::hold` to find values for the remaining parameters:

```sh
build/bin/tapas examples/solve/feasibility.tap
```

A `status` of `sat` means a solution was found. The `witness` field holds the Rule instance with those parameter values. The example also checks that instance against the original rule.

The second program introduces contradictory conditions and shows how to read conflicts:

```sh
build/bin/tapas examples/solve/diagnostics.tap
```

For example, a quantity cannot be both at least eight and at most four. The query returns `unsat`, with the relevant Rules and conditions in `conflicts`. The program also demonstrates `unsupported`: the solver cannot currently handle the expression, which does not establish that the rule has no solution.

## Testing a retail exchange

The [retail tutorial](retail/README_en.md) follows Yusuf, who has received a keyboard and wants another variant of the same product, paying the price difference. The retail system must check identity, confirmation, stock, and payment capacity before accepting or rejecting the request.

Rules describe these requirements, while a simulator executes the exchange. Each execution check fixes the previous state, request, actual output, and next state in the top-level `ExchangeModel`, then calls `solve::hold` to check whether the execution satisfies the requirements.

Start with a valid exchange:

```sh
build/bin/tapas examples/retail/test_valid_exchange.tap
```

Then try the experiments below. Each file runs independently; none requires another test to run first.

| Experiment | Question being tested |
|---|---|
| [Valid exchange](retail/test_valid_exchange.tap) | Are the charge and order changes correct after acceptance? |
| [Rejected exchanges](retail/test_rejected_exchange.tap) | Does an ineligible request get rejected without a charge or order changes? |
| [Incorrect state transition](retail/test_invalid_transition.tap) | Can the model detect a simulator that skips the same-product check and exchanges a keyboard for a thermostat? |
| [Generating eligible inputs](retail/test_generate_valid_exchange.tap) | Can the solver find sufficient payment capacity within a range? Does tightening the range make the query infeasible? |
| [Generating ineligible inputs](retail/test_generate_rejected_exchange.tap) | Can it find insufficient payment capacity, then check that the simulator rejects correctly? |
| [Restricting several parameters](retail/test_generate_joint_inputs.tap) | Can it generate specified scenarios while payment capacity, authenticated user, and confirmation vary together? |

For example, run the final group with:

```sh
build/bin/tapas examples/retail/test_generate_joint_inputs.tap
```

These experiments first query for an input, execute it in the simulator, then run a second query to check the actual transition. They reuse the business rules and change only the restrictions for each query.

Distinguish business outcomes from query results: both a valid exchange and a correct rejection should produce `sat` when their execution records are checked. The deliberately incorrect transition should produce `unsat`, showing that the test detected the defect. During input generation, `unsat` means no input satisfies the current restrictions. The tutorial explains the code and results in detail.

## General programming examples

The programs in `general` use fixed inputs to demonstrate familiar algorithms and the syntax for functions, loops, and collections. Each can be run directly, just like the Fibonacci example above.

| Example | Topic | Syntax and features |
|---|---|---|
| [Fibonacci](general/fibonacci.tap) | Recursion and iteration | Function calls, loops, lists |
| [Sorting](general/sorting.tap) | Seven sorting algorithms | Higher-order functions, slices, mutation, recursion |
| [Binary search](general/binary_search.tap) | Searching a sorted list | Loop boundaries, early returns |
| [Euclidean algorithm](general/euclidean_algorithm.tap) | GCD, LCM, extended Euclid | Integer arithmetic, returning multiple results |
| [Sieve of Eratosthenes](general/sieve_of_eratosthenes.tap) | Finding primes | Boolean lists, nested loops |
| [Breadth-first search](general/breadth_first_search.tap) | Graph traversal | Dictionaries, queues, membership tests |
| [Longest common subsequence](general/longest_common_subsequence.tap) | Dynamic programming and reconstruction | Two-dimensional arrays, strings |
| [Newton’s method](general/newton_method.tap) | Iterative root finding | Floating-point arithmetic, math functions |

For shorter snippets, see the [syntax examples](../docs/examples/syntax). For imports across files, see [modules and directory packages](../docs/examples/modules/README_en.md).
