# Tapas Examples

[简体中文](README.md) | English | [Project Home](../README_en.md)

This directory organizes examples by product purpose. `general` contains
standalone `.tap` programs that demonstrate only Tapas's general programming
foundation. Product examples for Rules, test generation, state exploration,
and agent benchmarks will live in separate subdirectories. Shorter examples
of basic syntax and modules are available under
[`docs/examples`](../docs/examples).

Run any example from the project root:

```sh
build/bin/tapas examples/general/fibonacci.tap
```

| File | Topic | Tapas features demonstrated |
| --- | --- | --- |
| [`fibonacci.tap`](general/fibonacci.tap) | Recursive and iterative Fibonacci | Recursion, loops, and lists |
| [`sorting.tap`](general/sorting.tap) | Seven classic sorting algorithms | Higher-order functions, slices, mutation, and recursion |
| [`binary_search.tap`](general/binary_search.tap) | Binary search | Loops, boundary handling, and early returns |
| [`euclidean_algorithm.tap`](general/euclidean_algorithm.tap) | GCD, LCM, and extended Euclid | Integer arithmetic and multi-value results |
| [`sieve_of_eratosthenes.tap`](general/sieve_of_eratosthenes.tap) | Sieve of Eratosthenes | Boolean lists and nested loops |
| [`breadth_first_search.tap`](general/breadth_first_search.tap) | Breadth-first graph traversal | Dictionaries, queues, and membership tests |
| [`longest_common_subsequence.tap`](general/longest_common_subsequence.tap) | Longest common subsequence | Dynamic programming, arrays, and strings |
| [`newton_method.tap`](general/newton_method.tap) | Newton's method | Floating-point arithmetic, math functions, and convergence |

Every `general` example is run by CTest. New general examples should remain
standalone, use deterministic input, and avoid network access, interactive
input, or pre-generated `.tapc` files.
