# Tapas Examples

[简体中文](README.md) | English | [Project Home](../README_en.md)

This directory contains standalone `.tap` programs that can be executed
directly. They focus on complete implementations; tutorials with background,
complexity analysis, and recorded output remain under
[`docs/examples`](../docs/examples).

Run any example from the project root:

```sh
build/bin/tapas examples/fibonacci.tap
```

| File | Topic | Tapas features demonstrated |
| --- | --- | --- |
| [`fibonacci.tap`](fibonacci.tap) | Recursive and iterative Fibonacci | Recursion, loops, and lists |
| [`sorting.tap`](sorting.tap) | Seven classic sorting algorithms | Higher-order functions, slices, mutation, and recursion |
| [`binary_search.tap`](binary_search.tap) | Binary search | Loops, boundary handling, and early returns |
| [`euclidean_algorithm.tap`](euclidean_algorithm.tap) | GCD, LCM, and extended Euclid | Integer arithmetic and multi-value results |
| [`sieve_of_eratosthenes.tap`](sieve_of_eratosthenes.tap) | Sieve of Eratosthenes | Boolean lists and nested loops |
| [`breadth_first_search.tap`](breadth_first_search.tap) | Breadth-first graph traversal | Dictionaries, queues, and membership tests |
| [`longest_common_subsequence.tap`](longest_common_subsequence.tap) | Longest common subsequence | Dynamic programming, arrays, and strings |
| [`newton_method.tap`](newton_method.tap) | Newton's method | Floating-point arithmetic, math functions, and convergence |

Every top-level example is run by CTest. New examples should remain standalone,
use deterministic input, and avoid network access, interactive input, or
pre-generated `.tapc` files.
