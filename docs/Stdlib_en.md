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

> **Signature notation:** `A | B` lists accepted Types, `AnyType` means any value, and `T` preserves the input Type.
> `...` denotes variadic arguments; `?` or brackets around a parameter mark it as optional. Arguments after `*` must be named.
> A `Nil` procedure result cannot be stored. Package signatures omit the current package prefix; calls still use `dense::rows(...)`.
> Cross-package Type references retain their prefix. The tunnel call `items.append(value)` equals `append(items, value)`.

Tapas ships 12 native packages and one source package, `format`, grouped by their relationship to the language core and their purpose:

| Category | Packages |
|---|---|
| Language-support packages | [`types`](../src/stdlib/types/README.md), [`syntax`](../src/stdlib/syntax/README.md), [`rules`](../src/stdlib/rules/README.md), [`evaluators`](../src/stdlib/evaluators/README.md) |
| Basic utility packages | [`io`](../src/stdlib/io/README.md), [`time`](../src/stdlib/time/README.md), [`random`](../src/stdlib/random/README.md), [`heap`](../src/stdlib/heap/README.md), [`math`](../src/stdlib/math/README.md), [`dense`](../src/stdlib/dense/README.md) |
| Application packages | [`finite`](../src/stdlib/finite/README.md), [`solve`](../src/stdlib/solve/README.md), [`format`](../src/stdlib/format/README.md) |

`optimize` currently has only a [design draft](../src/stdlib/optimize/README.md) and is not shipped as an available package.

## Predefined functions

These functions are available directly in the program root scope without a package prefix.

### Console, inspection, and time

| Signature | Returns | Behavior |
|---|---|---|
| `print(...values: AnyType)` | `Nil` | Prints abbreviated representations consecutively without an automatic separator, then LF. |
| `pprint(...values: AnyType)` | `Nil` | Prints full representations consecutively without an automatic separator, then LF. |
| `input()` | `String \| Nil` | Reads one line from standard input; returns `nil` only at EOF with no content. |
| `input(prompt: String)` | `String \| Nil` | Writes and flushes a prompt without LF, then reads one line; returns `nil` only at EOF with no content. |
| `len(value: AnyType)` | `Int` | Composite length; 0 for `nil`; 1 for other scalars. |
| `type(value: AnyType)` | `String` | Runtime type name. |
| `parameters(value: Rule \| RuleInstance \| RuleIR)` | `List[Pair[String, Type]]` | Rule IR parameter names and descriptive Types in declaration order; does not execute the Rule. |
| `arguments(instance: RuleInstance)` | `List[AnyType]` | Bound argument values in declaration order; does not execute or check the Rule. |
| `copy(value: T)` | `T` | Copies according to the object's own copy protocol. |
| `identical(left: AnyType, right: AnyType)` | `Bool` | Matches runtime Types and object-defined contents. |
| `assert(rule: RuleInstance \| Rule)` | `Nil` | Checks a Rule and raises a runtime error when a Condition fails. See [Rules](Rules_en.md#3-checking-apis) for complete semantics. |
| `clock()` | `Float` | Process CPU time in seconds. |
| `clock_ns()` | `Int` | Process CPU time in nanoseconds, suitable for measuring short code. |
| `now()` | `Time` | Absolute instant at the time of the call. Default display uses local time. |

`input` removes the final LF and preceding CR. Content read before EOF is still returned as a String.

`pprint` names structural full printing. Use `str` to produce a string; the
conventional string-formatting name `sprint` is no longer used for output.

### Conversion and construction

| Signature | Returns | Behavior |
|---|---|---|
| `int(value: AnyType)` | `Int` | Accepts Bool, Int, Float, or String; truncates Float toward zero and parses decimal String input through the host C library. |
| `float(value: AnyType)` | `Float` | Accepts Bool, Int, Float, or String; parses String input through the host C library. |
| `bool(value: AnyType)` | `Bool` | nil, zero numbers, and zero-length composites are false; the strings true and false are recognized specially. |
| `str(value: AnyType)` | `String` | Full textual representation. |
| `list()` / `list(iterable: Iterable)` | `List` | Create an empty list, or consume any iterable into a new list. |
| `replicate(values: List, count: Int)` | `List` | Shallow-copies the whole list count times; count is a nonnegative Int, and zero returns an empty list. |
| `pair(first: AnyType, second: AnyType)` | `Pair` | New pair. |
| `iter(start: Int, end: Int)` | `Iterator` | Half-open range with inferred step. |
| `iter(start: Int, step: Int, end: Int)` | `Iterator` | Half-open range with explicit step. |
| `array(rows: Int, cols: Int, value: AnyType)` | `RealArray \| BoolArray` | Bool fills BoolArray; numbers fill RealArray; List input fills in row-major order. |

The exported int and float signatures accept AnyType, but their implementations only handle Bool, Int, Float, and String; other values fail.
String conversion uses the host C strtol (base 10) and strtod. Leading whitespace is accepted;
unconsumed trailing characters, including whitespace, or nonzero errno cause an error.
Parsing stops at the first NUL rather than validating the String's full byte length; currently float('') returns 0.
Float-to-Int conversion uses the host C cast without a separate non-finite or range check.
Bool converts to 0 or 1. For bool, the strings true and false are special; other Strings and
composites are tested by length, so bool('0') is true.

For array, a List must contain exactly rows * cols elements, either all Bool or all numeric
(Int and Float may mix). An empty List with a zero-element shape creates RealArray;
use a Bool scalar to create an empty BoolArray.

Array dimensions are non-negative. A zero iterator step, or a step pointing
away from the end, produces an empty range. Omitted steps infer `1` or `-1`
from the endpoints.

```tapas
print(int('  +12'), ', ', int(-3.75))
print(bool('false'), ', ', bool('0'))
pprint(replicate([1, 2], 2))
```

<pre class='Tapas-Return'>
12, -3
false, true
[1, 2, 1, 2]
</pre>

### Collection operations

| Signature | Returns | Mutation and result |
|---|---|---|
| `push_front(list: List, value: AnyType)` | `Nil` | Inserts `value` at the front. |
| `push_back(list: List, value: AnyType)` | `Nil` | Inserts `value` at the back. |
| `join(values: Iterable, separator: String)` | `String` | Joins String elements yielded by an iterable; the separator may be empty. |
| `append(target: Appendable, value: AnyType)` | `Nil` | Appends an item or a pair to a container such as a List or Dictionary. Strings are immutable; use `join` for composition. |
| `insert(list: List, value: AnyType, index: Int)` | `Nil` | Inserts before index; negative indices count from the end, and the final index may equal the length. |
| `pop_front(list: List)` | `AnyType` | Removes and returns the first item. |
| `pop_back(list: List)` | `AnyType` | Removes and returns the last item. |
| `delete(target: Deletable, key: AnyType)` | `Nil` | Deletes an item. |
| `idx(target: Indexable, ...indices: AnyType)` | `AnyType` | Forwards one or more indices according to the target's indexing rules. |
| `idx_or(target: Indexable, ...indices: AnyType, default: AnyType)` | `AnyType` | The last argument is the default for an absent index; invalid types still fail. |
| `keys(value: Dictionary \| Type)` | `List` | Dictionary keys or enumerable field names of a Type. |
| `values(dictionary: Dictionary)` | `List` | Values in corresponding order. |
| `concat(left: Iterable, right: Iterable)` | `List` | Consumes both inputs in order into a new shallow List. |
| `sort(values: List)` | `Nil` | Sorts in place; numeric values compare by magnitude, other values only by runtime storage category. |

`append(dictionary, value)` requires a `Pair`. `pop_front` and `pop_back`
transfer ownership of the removed item and reject empty lists. `delete` remains
a generic mutation and returns `nil`; deleting a missing Dictionary key fails.
`keys` and `values` return new shallow Lists in current hash-table traversal order,
with no insertion-order or sorting guarantee. Negative list indices count from the end
where an operation accepts them. `idx_or` performs one non-throwing lookup; it
does not issue a separate membership test. Collections cannot own `nil`.

#### Capability Types and specialized operations

Generalization follows the capabilities an operation needs. Indexable only
guarantees indexed reads; it does not imply mutation, iteration, an enumerable
index domain, or array shape.

| Category | Current contract |
|---|---|
| General value operations | `len`, `type`, `copy`, `identical`, and `str` use value or object protocols; numeric conversions still validate input Types. |
| Iterable inputs | `list`, `join`, and `concat` accept Iterable. `join` requires String elements and returns an empty string for empty input; `concat` always returns a List. |
| Indexing and mutation | `idx` and `idx_or` forward index arguments; `append` and `delete` require Appendable and Deletable. Capability Types do not specify key Types. |
| List operations | `replicate`, `insert`, front/back push/pop, and `sort` require an ordered List; indexing capabilities alone do not define insertion, removal, or sorting. |
| Key/value enumeration | Dictionary does not implicitly choose key or value iteration. Use `keys` or `values`; Indexable alone cannot enumerate valid keys. |
| Specialized construction and reflection | `array`, `iter`, `pair`, Rule reflection, and assertion retain their domain-specific contracts. |

`join` and `concat` follow input iteration order without modifying inputs.
Shallow concatenation retains composite element references. String iteration
uses bytes: `join('abc', '-')` returns `a-b-c`, without implying Unicode
code-point iteration. Array currently is not Iterable; `idx` and `idx_or`
require both row and column indices for it.

`idx_or` requires at least one index and takes the default as its last argument.
List, String, Dictionary, and Array implement non-throwing lookup. Third-party
objects must implement `try_indexable` to guarantee defaults for absent indices;
the compatibility fallback uses ordinary indexing and can raise an error.
Single-key Dictionary lookup retains its one-probe implementation.

`copy` follows the object's copy protocol: List and Dictionary shallow-copy
elements, Array copies independent data storage, and extensions define their
own behavior. `identical` is not merely address identity: same-Type Lists and
other objects can match structurally; `identical(1, 1.0)` is false. `sort` is not
a generic comparator interface and guarantees neither lexicographic string
order, content ordering for composites, NaN ordering, nor stability. Mixed Int/Float
comparisons convert to double and may lose large-integer precision. Use heap
comparators when priorities need a defined ordering.

```tapas
let stdlib_access_matrix = array(2, 3, [1, 2, 3, 4, 5, 6])
print(idx(stdlib_access_matrix, 1, 2))
print(idx_or(stdlib_access_matrix, 9, 0, -1))
print(join('abc', '-'))
print(len(concat(iter(1, 4), 'xy')))
```

<pre class='Tapas-Return'>
6
-1
a-b-c
5
</pre>

### Session functions

| Signature | Returns | Context |
|---|---|---|
| `__ls__()` | `List[String]` | Names in the current root library. |
| `__ls__(library: Library)` | `List[String]` | Names in `library`. |
| `__path__()` | `List[String]` | Current library search paths. |
| `__path__(library: Library)` | `List[String]` | Search paths of `library`. |
| `__param__(index: Int)` | `AnyType` | Argument of the current variadic call. |
| `__nparam__()` | `Int` | Argument count of the current variadic call. |
| `__binary__()` | `Nil` | Prints current bytecode. |
| `__binary__(value: Library \| Function)` | `Nil` | Prints bytecode for `value`. |

Names beginning with `__` are implementation-reserved.

The no-argument forms of `__ls__` and `__path__` require Library scope;
functions must pass a Library explicitly. `__param__` uses a zero-based index
within the current call. `__param__` and `__nparam__` require a function-call
environment. A Function passed to `__binary__` must be a Tapas bytecode function.

## Language-support packages

These packages expose or extend the type system, compiler lexical facilities, and Rule representation and evaluation protocols.

### `types`: Type construction and reflection package

#### Types

| Predefined Type group | Names |
|---|---|
| Basic values | `AnyType`, `Nil`, `Bool`, `Int`, `Float`, `String`, `Time`, `Type` |
| Containers and callable values | `List`, `Pair`, `Dictionary`, `Iterator`, `Function`, `Library`, `RealArray`, `BoolArray` |
| Rules | `Rule`, `RuleInstance`, `RuleIR`, `RuleTerm`, `RuleItem` |
| General capabilities | `Indexable`, `IndexSettable`, `Appendable`, `Deletable`, `MemberTestable`, `Iterable` |

The six capabilities describe index reads, index writes, appending, deletion, membership tests, and iteration, respectively.

#### Functions

| Signature | Returns | Behavior |
|---|---|---|
| `make_type(...fields: Pair[String, Type])` | `Type` | Structural Type from at least one named field. |
| `union(...members: Type)` | `Type` | Union of at least two Types. |
| `enum(...members: String)` | `Type` | Enum Type with at least one String member. |
| `list(item: Type)` | `Type` | List Type with an element Type. |
| `iterator(item: Type)` | `Type` | Iterator Type with an element Type. |
| `optional(field: Pair[String, Type])` | `Type` | Marks a field as optional for make_type. |
| `pair(first: Type, second: Type)` | `Type` | Pair Type with Types for both sides. |
| `dictionary(key: Type, value: Type)` | `Type` | Dictionary Type with key and value Types. |
| `rule(...parameters: Type)` | `Type` | Rule Type with parameter Types in declaration order. |
| `rule_instance(...parameters: Type)` | `Type` | RuleInstance Type with parameter Types in declaration order. |
| `instance_of(rule: Rule)` | `Type` | Instance Type matching one Rule identity, meaningful only within the current process. |
| `of(value: AnyType)` | `Type` | Runtime Type; ordinary parameterized containers return the base Type, Rules retain their signature. |
| `matches(value: AnyType, expected: Type)` | `Bool` | Checks structural, union, Enum, parameterized, or capability matching. |
| `fields(value: Type)` | `Dictionary` | Fields of a structural Type; empty dictionary for other Types. |
| `optional_fields(value: Type)` | `List[String]` | Names of optional fields. |
| `members(value: Type)` | `List` | Enum strings or union Types; other Types return a singleton list containing themselves. |
| `base(value: Type)` | `Type` | Returns the base of a parameterized Type. |
| `parameters(value: Type)` | `Dictionary` | Returns Type parameters; distinct from the root parameters function for Rule reflection. |
| `definition(value: Type)` | `Dictionary` | Returns a dictionary describing the Type definition. |
| `parameter(name: String)` | `Type` | Declares a Type-parameter placeholder. |
| `value_parameter(name: String)` | `Type` | Declares a value-parameter placeholder. |
| `template(type_parameters: List, value_parameters: List, definition: Type)` | `Type` | Constructs a Type template from parameter lists and a definition. |

These construction forms participate in compiler-recognized Type definitions and template declarations; they do not imply arbitrary dynamic user-Type creation from runtime data. Templates use Type[Types; Values], with compile-time literal and constraint checks for value arguments. See the [Type System](TypeSystem_en.md). Reflection returns fresh containers whose Type values may be shared.

See the full API constraints in [`types` README](../src/stdlib/types/README.md).

### `syntax`: Lexical package

Exposes the compiler's lexical view for formatters and other source tools.

#### Types

| Name | Meaning |
|---|---|
| `Token` | Structural Type with `kind: String`, `start: Int`, and `end: Int` fields. |

#### Constants

| Name | Type | Meaning |
|---|---|---|
| `line_feed` | `String` | The line-feed character. |
| `horizontal_tab` | `String` | The horizontal-tab character. |

#### Functions

| Signature | Returns | Behavior |
|---|---|---|
| `tokens(source: String)` | `List[Token]` | Ordered token records including trivia and EOF. Positions are half-open byte ranges in the source. |

### `rules`: Rule checking, transformations, and IR package

#### Types

| Name | Meaning |
|---|---|
| `PointsOf` | Finite point-domain template. |
| `RangeOf` | Closed integer interval-domain template. |
| `Parameter` | Rule parameter record. |
| `Capture` | Closure capture record. |
| `Condition` | Boolean condition Item. |
| `Requirement` | Nested Rule-call Item. |
| `Origin` | Source position and origin information. |
| `CheckResult` | Rule checking status, violations, and diagnostics. |
| `Violation` | Record of an unsatisfied condition. |
| `Diagnostic` | Checking diagnostic record. |

#### Functions

| Signature | Returns | Behavior |
|---|---|---|
| `item(rule: Rule, target: Int \| String \| RuleItem)` | `RuleItem` | Selects a direct Item of a Rule. |
| `drop(rule: Rule, target: Int \| String \| RuleItem)` | `Rule` | Derived Rule with the target Item removed. |
| `violate(rule: Rule, target: Int \| String \| RuleItem)` | `Rule` | Derived Rule requiring the target Item to be false. |
| `drop_if(rule: Rule, target: Int \| String \| RuleItem, premise: Rule)` | `Rule` | Allows ignoring the target when premise holds; otherwise requires the target. |
| `violate_if(rule: Rule, target: Int \| String \| RuleItem, premise: Rule)` | `Rule` | Requires exactly one of premise and target to hold. |
| `restrict(rule: Rule, ...restrictions: Pair)` | `Rule` | Adds fixed-value or domain restrictions by parameter name. |
| `membership(value: RuleTerm, domain: RuleTerm)` | `RuleTerm` | Constructs a membership Term. |
| `points(element_type: Type, ...values)` | `PointsOf` | Finite point domain with the specified element Type. |
| `range(start: Int, end: Int)` | `RangeOf[Int]` | Closed integer interval, unlike the half-open root iter range. |
| `term(type: Type)` | `Type` | RuleTerm Type for a specified value Type. |
| `check(rule: RuleInstance \| Rule)` | `CheckResult` | Checks Conditions and returns a check result. |
| `inspect(rule: RuleInstance \| Rule)` | `RuleIR` | Returns an immutable Rule IR view. |
| `parameters(ir: RuleIR \| Rule \| RuleInstance)` | `List[Parameter]` | Returns IR parameter records in declaration order. |
| `items(ir: RuleIR \| Rule \| RuleInstance)` | `List[RuleItem]` | Returns direct Item records. |
| `terms(ir: RuleIR \| Rule \| RuleInstance)` | `List[RuleTerm]` | Returns Term records. |
| `origin(value: AnyType)` | `Origin` | Reads origin information. |
| `semantic_hash(value: RuleIR \| Rule)` | `Int` | Hashes semantically relevant contents. |
| `content_hash(value: RuleIR \| Rule)` | `Int` | Hashes contents including representation and origin information. |
| `serialize(value: RuleIR \| Rule)` | `String` | Serializes supported Rule IR. |
| `deserialize(data: String)` | `Rule` | Restores a Rule from serialized text. |
| `parameter(name: String, type: Type)` | `Parameter` | Constructs a named parameter record. |
| `constant(value: AnyType)` | `RuleTerm` | Constructs a constant Term. |
| `call(function: AnyType, arguments: List[RuleTerm])` | `RuleTerm` | Constructs a function-call Term. |
| `negation(operand: RuleTerm)` | `RuleTerm[Bool]` | Constructs a logical NOT Term. |
| `conjunction(left: RuleTerm, right: RuleTerm)` | `RuleTerm[Bool]` | Constructs a logical AND Term. |
| `disjunction(left: RuleTerm, right: RuleTerm)` | `RuleTerm[Bool]` | Constructs a logical OR Term. |
| `condition(term: RuleTerm, description: String?)` | `Condition` | Constructs a Condition with an optional description. |
| `requirement(rule: Rule \| RuleTerm, arguments: List[RuleTerm])` | `Requirement` | Constructs a nested Rule Requirement. |
| `implication(antecedent: RuleTerm, consequents: List[RuleTerm], description: String?)` | `RuleItem` | Constructs an implication Item with an optional description. |
| `extension(provider: String, kind: String, arguments: List[RuleTerm], payload: AnyType)` | `RuleTerm` | Constructs an extension Term with provider, kind, and payload. |
| `make(display_name: String, parameters: List[Parameter], items: List[RuleItem])` | `Rule` | Constructs a Rule from parameter and Item lists. |

A target is a zero-based direct Item index, a unique description string, or a direct RuleItem belonging to that Rule. Transformations leave the original Rule unchanged; conditional transformations require a premise with the same parameter signature. Hashes are not collision-free Rule identities. Serialization supports only the specified versions, constants, and extension providers. See [Rules](Rules_en.md) for the checking and IR contracts.

See the full API constraints in [`rules` README](../src/stdlib/rules/README.md).

### `evaluators`: Custom Rule evaluators package

#### Types

| Name | Meaning |
|---|---|
| `Evaluator` | Evaluator Type holding a name, version, and callbacks. |
| `Context` | Binding and capture environment for one evaluator call. |
| `Result` | Result Type with status, value, violations, and diagnostics fields. |
| `Diagnostic` | Type of execution diagnostics. |

#### Functions

| Signature | Returns | Behavior |
|---|---|---|
| `make(name: String, version: Int, evaluate: Function, compile: Function?)` | `Evaluator` | Creates an evaluator; the compile callback is optional. |
| `eval(instance: RuleInstance, evaluator: Evaluator)` | `Result` | Evaluates a bound instance with an evaluator. |
| `compile(rule: Rule, evaluator: Evaluator)` | `Result` | Compiles a Rule; returns Unsupported when no compile callback is supplied. |
| `binding(context: Context, parameter: rules::Parameter \| Int)` | `AnyType` | Reads a binding by parameter record or position. |
| `capture(context: Context, capture: rules::Capture \| Int)` | `AnyType` | Reads a capture by record or position. |
| `value(context: Context, term: RuleTerm)` | `AnyType` | Obtains a directly interpretable Term value in the current Context. |
| `requirement(context: Context, requirement: rules::Requirement)` | `RuleInstance` | Resolves a target Rule and its arguments into a RuleInstance. |

Result.status is Success, Unsupported, or Failed. Callbacks must return a complete Result in every state and explicitly report Unsupported for valid structures they cannot interpret. A Context is valid only during its owning call and must not be saved or reused across calls. See [Rules](Rules_en.md) for the callback protocol.

See the full API constraints in [`evaluators` README](../src/stdlib/evaluators/README.md).

## Basic utility packages

These packages provide general operations, system access, data structures, and array computation.

### `io`: Text-file package

| Signature | Returns | Behavior |
|---|---|---|
| `read_text(path: String)` | `String` | Reads the complete file. |
| `write_text(path: String, text: String)` | `Nil` | Creates or replaces the file. |
| `append_text(path: String, text: String)` | `Nil` | Creates the file or appends at its end. |
| `replace_text(path: String, text: String)` | `Nil` | Writes a temporary file in the same directory and atomically replaces the target. |

These functions preserve the exact string byte length. File handles, mode
strings, and binary values are deliberately left to separate lifecycle and
type designs.

### `time`: Time package

| Signature | Returns | Behavior |
|---|---|---|
| `from_unix(seconds: Int)` | `Time` | Creates an instant from whole seconds since the Unix epoch. |
| `unix(value: Time)` | `Int` | Returns whole Unix seconds for an instant. |
| `format(value: Time, pattern: String)` | `String` | Formats in local time using the host C `strftime` pattern. |

Timestamps and offsets must fit both the host `time_t` range and the Tapas
`Int` range. Supported formatting conversions are defined by the host C
library. This version stores no time-zone information and provides neither UTC
formatting nor date parsing.

### `random`: Pseudorandom numbers and state package

Source and Generator are exported **Type values, not functions**. Source describes which
pseudorandom algorithm is used; Generator describes an object holding mutable random state.
Use the lowercase generator function to create a generator.

#### Types

| Name | Meaning |
|---|---|
| `Source` | Type of a random source. A source identifies the algorithm and initialization rules without holding sampling state. |
| `Generator` | Type of a random generator. A generator holds mutable state that advances when drawing values. |

#### Random-source constants

| Name | Type | Meaning |
|---|---|---|
| `pcg32_xsh_rr` | `Source` | Predefined source value using PCG-XSH-RR 64/32 with fixed stream 54; it is not called. |

#### Functions

| Signature | Returns | Behavior |
|---|---|---|
| `generator(source: Source, seed: Int)` | `Generator` | Creates independent state from a source and seed. |
| `next_int(rng: Generator, bound: Int)` | `Int` | Uniform integer in [0, bound); bound must be positive. |
| `next_float(rng: Generator)` | `Float` | Uniform float in [0, 1), using 53 bits of precision. |
| `next_bool(rng: Generator)` | `Bool` | Produces true or false with equal probability. |
| `advance(rng: Generator, steps: Int)` | `Nil` | Advances by 32-bit output positions; negative steps rewind. |

Pass the source constant to generator, then draw from the returned generator:

```tapas
let stdlib_random_rng: random::Generator = random::generator(random::pcg32_xsh_rr, 42)
let stdlib_random_value = random::next_int(stdlib_random_rng, 10)
```

The same source, seed, and call sequence produce the same results. Drawing and advance mutate Generator state. This package does not provide a cryptographic random source.

See the full API constraints in [`random` README](../src/stdlib/random/README.md).

### `heap`: Binary heap package

`heap` maintains binary heaps in ordinary Lists, modifying them in place.

| Function | Behavior | Time complexity |
|---|---|---|
| `heapify(items[, less])` | Build a heap in place; return Nil | O(n) |
| `push(items, value[, less])` | Insert an element; return Nil | Amortized O(log n) |
| `pop(items[, less])` | Remove and return the root | O(log n) |
| `peek(items)` | Read the root | O(1) |

The default is a numeric min-heap accepting Int and non-NaN Float values.
Mixed comparisons preserve integer precision. An optional `less(a, b)`
comparator must return Bool and define a strict weak ordering. All modifying
operations on a heap must use the same ordering. Comparators must not modify
the heap or the ordering keys. Equal priorities have no guaranteed order.
Both `pop` and `peek` raise an error on an empty heap.

```tapas
let stdlib_heap_items = [4, 1, 3]
heap::heapify(stdlib_heap_items)
heap::push(stdlib_heap_items, 2)
print(heap::pop(stdlib_heap_items))
print(heap::peek(stdlib_heap_items))
```

<pre class='Tapas-Return'>
1
2
</pre>

Use `(a, b) { return a[0] < b[0] }` for priority records and
`(a, b) { return a > b }` for a max-heap. Create an empty heap with `[]`
and query its size with `len`. Rebuild after directly modifying the list
or priorities; operations do not perform O(n) validity checks.
See the [heap package reference](../src/stdlib/heap/README.md) for details.

### `math`: Scalar mathematics package

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

### `dense`: Dense-array package

| Signature | Returns | Behavior |
|---|---|---|
| `rows(value: RealArray \| BoolArray)` | `Int` | Row count. |
| `cols(value: RealArray \| BoolArray)` | `Int` | Column count. |
| `transpose(value: RealArray \| BoolArray)` | `RealArray \| BoolArray` | New transposed array. |
| `identity(size: Int)` | `RealArray` | Identity matrix of the requested order. |
| `trace(value: RealArray)` | `Float` | Main-diagonal sum; rectangular arrays are accepted. |
| `inner(left: RealArray, right: RealArray)` | `Float` | Frobenius inner product of equal-shaped arrays. |
| `outer(left: RealArray, right: RealArray)` | `RealArray` | Outer product of two row or column vectors. |
| `norm(value: RealArray)` | `Float` | Vector 2-norm or matrix Frobenius norm. |
| `normalize(value: RealArray)` | `RealArray` | Equal-shaped unit-norm copy. |
| `copy_into(source: RealArray, target: RealArray)` | `Nil` | Copies into an existing equal-shaped array without allocating a result. |
| `scale_inplace(value: RealArray, factor: Int \| Float)` | `Nil` | Performs `value *= factor` in place. |
| `add_scaled_inplace(target: RealArray, source: RealArray, factor: Int \| Float)` | `Nil` | Performs `target += factor * source` in place. |
| `gemm(alpha: Int \| Float, left: RealArray, right: RealArray, beta: Int \| Float, output: RealArray)` | `Nil` | Performs `output = alpha * left @ right + beta * output` in place. |

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

## Application packages

These packages support finite probability modeling, Rule solving and sampling, and source formatting.

### `finite`: Finite probability distributions package

#### Types

| Name | Meaning |
|---|---|
| `Index` | Immutable, nonempty nonnegative integer coordinates; supports Indexable. |
| `Distribution` | Type of a probability distribution over a finite index space. |

#### Functions

| Signature | Returns | Behavior |
|---|---|---|
| `index(coords: List[Int])` | `Index` | Creates an Index from coordinates. |
| `shape(dist: Distribution)` | `Index` | Returns the dimension lengths. |
| `prob(dist: Distribution, index: Index)` | `Float` | Returns the probability of one index. |
| `mass(dist: Distribution, indices: List[Index])` | `Float` | Returns event probability; duplicate indices count once. |
| `samples(dist: Distribution, count: Int, rng: random::Generator)` | `List[Index]` | Draws independent samples and advances rng; duplicates are allowed. |
| `uniform(n: Int)` | `Distribution` | Uniform distribution over n indices. |
| `categorical(weights: List[Float])` | `Distribution` | One-dimensional distribution from nonnegative weights. |
| `piecewise_constant(n: Int, steps: List[Pair[Int, Float]])` | `Distribution` | Piecewise-constant weights; steps start at index 0. |
| `piecewise_linear(knots: List[Pair[Int, Float]])` | `Distribution` | Piecewise-linear weights; first knot is at 0, last coordinate plus one sets the size. |
| `product(dists: List[Distribution])` | `Distribution` | Independent joint distribution with concatenated shapes. |
| `mixture(dists: List[Distribution], weights: List[Float])` | `Distribution` | Weighted mixture of distributions with equal shapes. |
| `conditional(dist: Distribution, indices: List[Index])` | `Distribution` | Renormalizes over a deduplicated positive-mass event, preserving shape. |
| `map(dist: Distribution, shape: Index, mapping: Function[Index] -> Index)` | `Distribution` | Maps indices to a target shape and sums colliding probabilities. |
| `bernoulli(p: Float)` | `Distribution` | Bernoulli distribution with success probability p. |
| `binomial(trials: Int, p: Float)` | `Distribution` | Success count in trials independent Bernoulli trials. |
| `multinomial(trials: Int, weights: List[Float])` | `Distribution` | Category counts from trials draws with the supplied weights. |
| `hypergeometric(population: Int, successes: Int, draws: Int)` | `Distribution` | Success count when sampling without replacement from a finite population. |
| `zipf(n: Int, exponent: Float)` | `Distribution` | Finite Zipf distribution of size n and the specified exponent. |

Dimension lengths must be positive and coordinates must fit the shape. Weights must be finite, nonnegative, and have a positive total. Probability and distribution parameters are validated by each constructor. Distribution values are created through these constructors; infinite distributions are not implicitly truncated.

See the full API constraints in [`finite` README](../src/stdlib/finite/README.md).

### `solve`: Rule solving and sampling package

#### Types

| Name | Meaning |
|---|---|
| `HoldResult` | Feasibility status, query scope, witness, and diagnostics. |
| `SampleResult` | Sampling status, samples, execution statistics, and diagnostics. |

#### Functions

| Signature | Returns | Behavior |
|---|---|---|
| `hold(rule: Rule \| RuleInstance)` | `HoldResult` | Searches for a satisfying instance within the configured integer parameter domain. |
| `sample(rule: Rule, count: Int, space: Dictionary[String, Indexable], *, ...)` | `SampleResult` | Samples satisfying assignments over finite domains while preserving the conditional distribution. |

Check hold's status first: sat, unsat, unknown, unsupported, or error.
Other fields are scope, reason, witness, integer_min, integer_max, conflicts,
and bindings. The default integer parameter domain is [-1000000000000, 1000000000000],
configurable through TAPAS_SOLVE_INT_MIN and TAPAS_SOLVE_INT_MAX. Conclusions apply
only to this configured domain, not globally to the entire Int Type.

For sample, count is a nonnegative sample count. The space dictionary maps Rule
parameter names to finite, indexable domains. These parameters are sampling
variables; the solver completes other parameters without promising their
distribution. Optional arguments must be passed by name:

| Parameter | Type | Default and meaning |
|---|---|---|
| `distributions` | `Dictionary[String, finite::Distribution] \| finite::Distribution` | `{}`; per-parameter one-dimensional distributions or a joint distribution, with shapes matching the domains. Unspecified variables are uniform. |
| `rng` | `random::Generator` | A fresh PCG-XSH-RR Generator seeded with 0 for each call; supplied state advances. |
| `candidate_limit` | `Int` | `count * 1000`; maximum raw candidate count. |
| `time_limit` | `Float \| Nil` | `nil`; optional total wall-clock limit in seconds. |
| `core_budget_factor` | `Float` | `1.0`; positive extra-solving budget factor. |
| `fairness_interval` | `Int` | `4`; at least 2, controlling fair scheduling of generalization searches. |
| `generalization` | `String` | `'online_mass_core'`; generalization strategy for infeasible candidates. |
| `scheduler` | `String` | `'mass_fair'`; scheduling strategy for extra solver queries. |
| `trace` | `Bool` | `false`; whether to record per-candidate statistics. |

SampleResult.status is complete, incomplete, unknown, unsupported, or error.
Other fields are reason, samples, candidates, solver_calls, core_calls,
cache_hits, cache_regions, trace, and elapsed. Samples may repeat. Limits can
produce a partial sample prefix; inspect status rather than treating a nonempty
samples list as completion.

The backend requires Python and OR-Tools; versions are in
[requirements.txt](../src/stdlib/solve/requirements.txt). It uses python3 by default;
TAPAS_SOLVE_PYTHON selects another interpreter path. See the
[solve package documentation](../src/stdlib/solve/README.md) for full result
fields, supported inputs, and strategy options.

### `format`: Source-formatting package

This source package has its entry in format/__init__.tap and runs through
`tapas -m format`.

#### Types

| Name | Meaning |
|---|---|
| `Edit` | Text edit with start: Int, end: Int, and text: String fields. |

#### Functions

| Signature | Returns | Behavior |
|---|---|---|
| `edits(source_text: String)` | `List[Edit]` | Sorted, disjoint edits relative to the original source; empty when already formatted. |
| `source(source_text: String)` | `String` | Returns formatted text without changing the input. |
| `is_formatted(source_text: String)` | `Bool` | Checks whether text already has the standard format. |
| `file(path: String)` | `Bool` | Formats and atomically replaces a file; returns true when content changes. |
| `check_file(path: String)` | `Bool` | Checks file formatting without writing. |
| `main(arguments: List[String])` | `Int` | Command-line entry; returns 0 with no check failures, otherwise 1. |

Edit.start and Edit.end are half-open byte offsets in the original source.
Consuming all edits against the original text, or applying them in descending
start order, produces the same output as source. Offsets are not character
positions. Inserted newlines use the input's first newline sequence, or LF if
there are no input newlines. Command-line paths before --check are formatted;
paths after it are checked only. See [Code Style](Style_en.md) for normative
layout and the [format package documentation](../src/stdlib/format/README.md)
for the full editing contract.
