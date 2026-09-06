# Tapas Standard Library

[简体中文](Stdlib_zh.md) | English | [Project Home](../README_en.md)

This document describes the root built-ins, native packages, and source
packages shipped with Tapas. See the [Language Reference](Syntax_en.md) for
syntax and operator semantics, the [Type System](TypeSystem_en.md) for Types
and the `types` package, and [Rules](Rules_en.md) for Rule and evaluator APIs.

Root functions are called by name; package members use `package::name`. The
first argument of a root function may act as the receiver of a tunnel call.
Standard-library APIs evolve with the language version and are not lexical or
syntactic definitions.

## 1. Reading the signatures

`A | B` means either runtime Type. `AnyType` means any Tapas value.
`...AnyType` means zero or more arguments. A `Nil` result Type denotes a procedure
whose result cannot be stored.

Every root function can be used as a tunnel call when its first parameter is
the receiver. For example, `append(items, value)` and
`items.append(value)` are equivalent.

## 2. Console, text files, inspection, and time

| Signature | Returns | Behavior |
|---|---|---|
| `print(...AnyType)` | `Nil` | Prints abbreviated representations, then LF. |
| `pprint(...AnyType)` | `Nil` | Prints full representations, then LF. |
| `input()` | `String \| Nil` | Reads one line from standard input; returns `nil` at EOF. |
| `input(prompt: String)` | `String \| Nil` | Writes and flushes a prompt without LF, then reads one line; returns `nil` at EOF. |
| `len(value: AnyType)` | `Int` | Composite length; 0 for `nil`; 1 for other scalars. |
| `type(value: AnyType)` | `String` | Runtime type name. |
| `parameters(value: Function \| Rule \| RuleInstance \| RuleIR)` | `List[Pair[String, Type]]` | Parameter names and actual Type values, in declaration order; does not execute the Rule. |
| `arguments(instance: RuleInstance)` | `List[AnyType]` | Bound argument values in declaration order; does not execute or check the Rule. |
| `copy(value: T)` | `T` | Scalar copy or shallow composite copy. |
| `identical(left: AnyType, right: AnyType)` | `Bool` | Runtime identity/value identity. |
| `assert(rule: RuleInstance \| Rule)` | `Nil` | Checks a Rule and raises a runtime error when a Condition fails. See [Rules](Rules_en.md#3-checking-apis) for complete semantics. |
| `clock()` | `Float` | Process CPU time in seconds. |
| `clock_ns()` | `Int` | Process CPU time in nanoseconds, suitable for measuring short code. |
| `now()` | `Time` | Absolute instant at the time of the call. Default display uses local time. |

`pprint` names structural full printing. Use `str` to produce a string; the
conventional string-formatting name `sprint` is no longer used for output.
The `io` package provides the minimal whole-text-file interface:

| Signature | Returns | Behavior |
|---|---|---|
| `io::read_text(path: String)` | `String` | Reads the complete file. |
| `io::write_text(path: String, text: String)` | `Nil` | Creates or replaces the file. |
| `io::append_text(path: String, text: String)` | `Nil` | Creates the file or appends at its end. |
| `io::replace_text(path: String, text: String)` | `Nil` | Writes a temporary file in the same directory and atomically replaces the target. |

These functions preserve the exact string byte length. File handles, mode
strings, and binary values are deliberately left to separate lifecycle and
type designs.

The `syntax` package exposes the compiler's lexical view to formatters and
other source tools:

| Name | Type | Behavior |
|---|---|---|
| `syntax::Token` | `Type` | Structural Type with `kind: String`, `start: Int`, and `end: Int` fields. |
| `syntax::tokens(source: String)` | `List` | Ordered token records including trivia and EOF. Positions are half-open byte ranges in the source. |
| `syntax::line_feed` | `String` | The line-feed character. |
| `syntax::horizontal_tab` | `String` | The horizontal-tab character. |

`format` is a source standard package whose entry remains
`format/__init__.tap`. It exports `source`, `is_formatted`, `file`,
`check_file`, and `main`, and runs through `tapas -m format`. See
[Code Style](Style_en.md) for the normative layout.

## 3. Conversion and construction

| Signature | Returns | Behavior |
|---|---|---|
| `int(value: Bool \| Int \| Float \| String)` | `Int` | Numeric conversion; string is complete base-10 input. |
| `float(value: Bool \| Int \| Float \| String)` | `Float` | Numeric conversion. |
| `bool(value: AnyType)` | `Bool` | Explicit truth conversion. |
| `str(value: AnyType)` | `String` | Full textual representation. |
| `list(...AnyType)` | `List` | New list containing the arguments. |
| `pair(first: AnyType, second: AnyType)` | `Pair` | New pair. |
| `iter(start: Int, end: Int)` | `Iterator` | Half-open range with inferred step. |
| `iter(start: Int, step: Int, end: Int)` | `Iterator` | Half-open range with explicit nonzero step. |
| `array(rows: Int, cols: Int, fill: Bool \| Int \| Float \| List)` | `RealArray \| BoolArray` | New dense array. |

Array dimensions are non-negative. An explicit iterator step of zero is an
error.

## 4. Collection operations

| Signature | Returns | Mutation and result |
|---|---|---|
| `push_front(list: List, value: AnyType)` | `Nil` | Inserts `value` at the front. |
| `push_back(list: List, value: AnyType)` | `Nil` | Inserts `value` at the back. |
| `append(target: Appendable, value: AnyType)` | `Nil` | Appends text, an item, or a pair. |
| `insert(list: List, value: AnyType, index: Int)` | `Nil` | Inserts before `index`. |
| `pop_front(list: List[T])` | `T` | Removes and returns the first item. |
| `pop_back(list: List[T])` | `T` | Removes and returns the last item. |
| `delete(target: Deletable, index_or_key: AnyType)` | `Nil` | Deletes an item. |
| `idx(target: Indexable, index: AnyType)` | `AnyType` | One-argument indexing. |
| `keys(dict: Dictionary)` | `List` | Keys in unspecified order. |
| `values(dict: Dictionary)` | `List` | Values in corresponding order. |
| `concat(left: List, right: List)` | `List` | New shallow concatenation. |
| `sort(list: List)` | `Nil` | Sorts in place using the runtime total order. |

`append(dictionary, value)` requires a `Pair`. `pop_front` and `pop_back`
transfer ownership of the removed item and reject empty lists. `delete` remains
a generic mutation and returns `nil`. Negative list indices count from the end
where an operation accepts them. Collections cannot own `nil`.

## 5. Session functions

| Signature | Returns | Context |
|---|---|---|
| `__ls__()` | `List` | Names in the current root library. |
| `__ls__(library: Library)` | `List` | Names in `library`. |
| `__path__()` | `List` | Current library search paths. |
| `__path__(library: Library)` | `List` | Search paths of `library`. |
| `__param__(index: Int)` | `AnyType` | Argument of the current variadic call. |
| `__nparam__()` | `Int` | Argument count of the current variadic call. |
| `__binary__()` | `Nil` | Prints current bytecode. |
| `__binary__(value: Library \| Function)` | `Nil` | Prints bytecode for `value`. |

Names beginning with `__` are implementation-reserved.


## 6. Time package `time`

| Signature | Returns | Behavior |
|---|---|---|
| `time::from_unix(seconds: Int)` | `Time` | Creates an instant from whole seconds since the Unix epoch. |
| `time::unix(value: Time)` | `Int` | Returns whole Unix seconds for an instant. |
| `time::format(value: Time, pattern: String)` | `String` | Formats in local time using the host C `strftime` pattern. |

Timestamps and offsets must fit both the host `time_t` range and the Tapas
`Int` range. Supported formatting conversions are defined by the host C
library. This version stores no time-zone information and provides neither UTC
formatting nor date parsing.

## 7. Scalar mathematics package `math`

Numeric parameters in this table have Type `Int | Float`; unless stated
otherwise, functions return `Float`. Domain, overflow, infinity, and NaN behavior
follows the host C math library.

| Group | Signatures |
|---|---|
| Absolute and roots | `abs(Int \| Float) -> Int \| Float`, `fabs(Int \| Float) -> Float`, `sqrt(Int \| Float) -> Float`, `rsqrt(Int \| Float) -> Float`, `cbrt(Int \| Float) -> Float` |
| Power and geometry | `pow(Int \| Float, Int \| Float) -> Float`, `hypot(Int \| Float, Int \| Float) -> Float` |
| Trigonometric | `sin`, `cos`, `tan`, `asin`, `acos`, `atan`: `(Int \| Float) -> Float`; `atan2(Int \| Float, Int \| Float) -> Float` |
| Hyperbolic | `sinh`, `cosh`, `tanh`, `asinh`, `acosh`, `atanh`: `(Int \| Float) -> Float` |
| Exponential | `exp`, `exp2`, `expm1`: `(Int \| Float) -> Float` |
| Logarithmic | `log`, `log2`, `log10`, `log1p`, `logb`: `(Int \| Float) -> Float`; `ilogb(Int \| Float) -> Int` |
| Decomposition | `frexp(Int \| Float) -> Pair[Float, Int]`, `modf(Int \| Float) -> Pair[Float, Float]` |
| Scaling | `ldexp(Int \| Float, Int) -> Float`, `scalbn(Int \| Float, Int) -> Float`, `scalbln(Int \| Float, Int) -> Float` |
| Error and gamma | `erf`, `erfc`, `lgamma`, `tgamma`: `(Int \| Float) -> Float` |
| Rounding to Float | `ceil`, `floor`, `nearbyint`, `rint`, `round`, `trunc`: `(Int \| Float) -> Float` |
| Rounding to Int | `lrint`, `llrint`, `lround`, `llround`: `(Int \| Float) -> Int` |
| Remainder | `fmod(Int \| Float, Int \| Float) -> Float`, `remainder(Int \| Float, Int \| Float) -> Float`, `remquo(Int \| Float, Int \| Float) -> Pair[Float, Int]` |
| Floating manipulation | `copysign`, `nextafter`, `fdim`, `fmax`, `fmin`: `(Int \| Float, Int \| Float) -> Float`; `fma(Int \| Float, Int \| Float, Int \| Float) -> Float` |
| Reciprocal and NaN | `eleinv(Int \| Float) -> Float`, `make_nan() -> Float` |
| Classification | `isfinite`, `isinf`, `isnan`, `isnormal`, `signbit`: `(Int \| Float) -> Bool`; `fpclassify(Int \| Float) -> Int` |
| Ordered predicates | `isgreater`, `isgreaterequal`, `isless`, `islessequal`, `islessgreater`, `isunordered`: `(Int \| Float, Int \| Float) -> Bool` |

Every name in this table is accessed through `math::`, for example
`math::sqrt(2)`. Scaling-function integer arguments convert floats toward
zero.

## 8. Dense-array package `dense`

| Signature | Returns | Behavior |
|---|---|---|
| `dense::rows(value: RealArray \| BoolArray)` | `Int` | Row count. |
| `dense::cols(value: RealArray \| BoolArray)` | `Int` | Column count. |
| `dense::transpose(value: RealArray \| BoolArray)` | `RealArray \| BoolArray` | New transposed array. |
| `dense::identity(size: Int)` | `RealArray` | Identity matrix of the requested order. |
| `dense::trace(value: RealArray)` | `Float` | Main-diagonal sum; rectangular arrays are accepted. |
| `dense::inner(left: RealArray, right: RealArray)` | `Float` | Frobenius inner product of equal-shaped arrays. |
| `dense::outer(left: RealArray, right: RealArray)` | `RealArray` | Outer product of two row or column vectors. |
| `dense::norm(value: RealArray)` | `Float` | Vector 2-norm or matrix Frobenius norm. |
| `dense::normalize(value: RealArray)` | `RealArray` | Equal-shaped unit-norm copy. |
| `dense::copy_into(source: RealArray, target: RealArray)` | `Nil` | Copies into an existing equal-shaped array without allocating a result. |
| `dense::scale_inplace(value: RealArray, factor: Int \| Float)` | `Nil` | Performs `value *= factor` in place. |
| `dense::add_scaled_inplace(target: RealArray, source: RealArray, factor: Int \| Float)` | `Nil` | Performs `target += factor * source` in place. |
| `dense::gemm(alpha: Int \| Float, left: RealArray, right: RealArray, beta: Int \| Float, output: RealArray)` | `Nil` | Performs `output = alpha * left @ right + beta * output` in place. |

```tapas
let builtin_dense_matrix = array(2, 3, [1, 2, 3, 4, 5, 6])
print(dense::rows(builtin_dense_matrix), ' x ', dense::cols(builtin_dense_matrix))
pprint(dense::transpose(builtin_dense_matrix))
print(dense::norm(array(1, 2, [3, 4])))
```

<pre class='Tapas-Return'>
2 x 3
[[1, 4],
 [2, 5],
 [3, 6]]
5
</pre>

`inner` requires equal row and column counts. `outer` accepts only a
two-dimensional array with one dimension equal to 1; row and column vector
orientation is ignored, and the result shape follows the two element counts.
`normalize` rejects an array with zero norm. `copy_into`, `scale_inplace`,
`add_scaled_inplace`, and `gemm` reuse caller-owned storage. A `gemm` output
must not be the same array as either input.

Operators provide convenient, non-mutating expressions and allocate their
results. Their element-wise paths use one fused pass instead of copying and
then computing; matrix multiplication `@` still maps directly to `dgemm`.
The performance-oriented `dense` paths reuse output buffers: `copy_into`,
`scale_inplace`, `add_scaled_inplace`, and `gemm` map directly to `dcopy`,
`dscal`, `daxpy`, and `dgemm`. `inner` and `norm` use `ddot` and `dnrm2`;
`outer` uses `dgemm` with `beta = 0`, avoiding a separate zero-fill pass.
`identity`, `trace`, and `transpose` do not require BLAS.

Linear solves, inverses, determinants, rank, and eigenvalue decompositions are
not BLAS primitives. Future APIs for them should keep high-level names in
`dense` while using a separate optional LAPACK backend, rather than embedding
slow or numerically unstable substitutes.
