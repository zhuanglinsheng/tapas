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

Tapas ships 18 native packages and one source package, `format`, grouped by their relationship to the language core and their purpose:

| Category | Packages |
|---|---|
| Language-support packages | [`types`](#package-types), [`syntax`](#package-syntax), [`rules`](#package-rules), [`evaluators`](#package-evaluators) |
| Basic utility packages | [`bytes`](#package-bytes), [`strings`](#package-strings), [`json`](#package-json), [`heap`](#package-heap), [`os`](#package-os), [`path`](#package-path), [`fs`](#package-fs), [`io`](#package-io), [`time`](#package-time) |
| Mathematics and probability packages | [`math`](#package-math), [`dense`](#package-dense), [`random`](#package-random), [`finite`](#package-finite) |
| Application packages | [`solve`](#package-solve), [`format`](#package-format) |

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
| `try_int(value: AnyType)` | `Dictionary` | Attempts the int conversion and returns an error record on failure. |
| `try_float(value: AnyType)` | `Dictionary` | Attempts the float conversion and returns an error record on failure. |
| `bool(value: AnyType)` | `Bool` | nil, zero numbers, and zero-length composites are false; the strings true and false are recognized specially. |
| `str(value: AnyType)` | `String` | Full textual representation. |
| `list()` / `list(iterable: Iterable)` | `List` | Create an empty list, or consume any iterable into a new list. |
| `replicate(values: List, count: Int)` | `List` | Shallow-copies the whole list count times; count is a nonnegative Int, and zero returns an empty list. |
| `pair(first: AnyType, second: AnyType)` | `Pair` | New pair. |
| `iter(start: Int, end: Int)` | `Iterator` | Half-open range with inferred step. |
| `iter(start: Int, step: Int, end: Int)` | `Iterator` | Half-open range with explicit step. |
| `array(rows: Int, cols: Int, value: AnyType)` | `RealArray \| BoolArray` | Bool fills BoolArray; numbers fill RealArray; List input fills in row-major order. |

The exported int and float signatures accept AnyType, but their implementations only handle Bool, Int, Float, and String; other values fail.
String conversion retains host C strtol (base 10) and strtod syntax and locale behavior.
Leading whitespace is accepted, but at least one numeric token must be consumed and the entire
byte string must be validated. Empty/whitespace-only strings, embedded NUL, trailing unconsumed
characters (including whitespace), non-finite values, and nonzero errno are errors, including underflow.
Float-to-Int truncates toward zero and checks bounds before casting, rejecting NaN, infinity and
out-of-range values. Int-to-Float still rounds to floating-point precision. str preserves embedded NUL.

try_int and try_float share the strict implementation and return ok, value, code, error, and errno.
On success code/error are empty. On failure code is invalid_type, invalid_number, out_of_range, or
non_finite; value is Int 0 or Float 0.0 respectively. Always check ok before consuming value.
errno is always 0; diagnostic text is not a stable error code. Resource exhaustion still raises errors.
These records do not depend on json: they follow the ok-checking pattern of json::try_parse,
with operation-specific fallback values and additional fields.
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
| `append(target: Appendable, value: AnyType)` | `Nil` | Appends an item or a pair to a container such as a List or Dictionary. Strings are immutable; use `strings::join` for composition. |
| `insert(list: List, value: AnyType, index: Int)` | `Nil` | Inserts before index; negative indices count from the end, and the final index may equal the length. |
| `pop_front(list: List)` | `AnyType` | Removes and returns the first item. |
| `pop_back(list: List)` | `AnyType` | Removes and returns the last item. |
| `delete(target: Deletable, key: AnyType)` | `Nil` | Deletes an item. |
| `idx(target: Indexable, ...indices: AnyType)` | `AnyType` | Forwards one or more indices according to the target's indexing rules. |
| `idx_or(target: Indexable, ...indices: AnyType, default: AnyType)` | `AnyType` | The last argument is the default for an absent index; invalid types still fail. |
| `keys(value: Dictionary \| Type)` | `List` | Dictionary keys or enumerable field names of a Type. |
| `values(dictionary: Dictionary)` | `List` | Values in corresponding order. |
| `concat(left: Iterable, right: Iterable)` | `List` | Consumes both inputs in order into a new shallow List. |
| `fill(target: IndexSettable, value: AnyType, indices: Iterable)` | `Nil` | Writes the same value at each supplied index; each item is one complete index or key. |
| `count(values: Iterable, value: AnyType)` | `Int` | Counts elements matching by identical rules. |
| `sort(values: List)` | `Nil` | Sorts in place by numeric, bytewise String, or recursive lexicographic List order; not stable. |

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
| Iterable inputs | `list`, `concat`, and `count` accept Iterable; `concat` always returns a List. String composition uses `strings::join`, whose elements must be String. |
| Indexing and mutation | `idx` and `idx_or` forward index arguments; `append` and `delete` require Appendable and Deletable. `fill` requires IndexSettable and explicit indices. Capability Types do not specify key Types. |
| List operations | `replicate`, `insert`, front/back push/pop, and `sort` require an ordered List; indexing capabilities alone do not define insertion, removal, or sorting. |
| Key/value enumeration | Dictionary does not implicitly choose key or value iteration. Use `keys` or `values`; Indexable alone cannot enumerate valid keys. |
| Specialized construction and reflection | `array`, `iter`, `pair`, Rule reflection, and assertion retain their domain-specific contracts. |

`strings::join` and `concat` follow input iteration order without modifying inputs.
Shallow concatenation retains composite element references. String iteration
uses bytes: `strings::join('abc', '-')` returns `a-b-c`, without implying Unicode
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
other objects can match structurally; `identical(1, 1.0)` is false. `sort` and default heap operations share ordering for numbers, Strings and nested Lists.
Mixed Int/Float comparisons preserve integer precision.
Large numeric partitions use jittered stratified sampling. Homogeneous Int/Float uses a two-way partition when samples have no duplicates and a three-way partition otherwise; mixed Number retains the three-way partition. Strings compare their complete
byte contents, including embedded NULs. Lists compare successive elements until the
first difference; if the common prefix is equal, the shorter List comes first.
There is no ordering across numeric, String and List categories. NaN, unsupported
values, and excessive or cyclic nesting cause errors. Sort and default `heapify` validate all input values; default `push` validates
the new value. List operators visit only positions needed to decide the result.
`sort` is not stable and takes no comparator; heap accepts custom priority comparators.
Numeric Lists use inline three-way introsort, insertion sort for small partitions,
and heapsort when the depth budget expires, giving worst-case O(n log n) time and
O(log n) stack space. Monotone or bounded-shift inputs can finish in linear time.
String and nested-List comparisons cost more: large inputs first probe existing
order, while other inputs retain the platform qsort. Selection depends on value
types, size and existing order without changing the sorting contract.

Errors may leave a different permutation but retain every element and its references.

#### Bulk filling and counting

fill takes explicit indices; use iter(start, step, end) for a half-open strided range.
Any Iterable of keys is accepted, such as fill(dictionary, 0, ['a', 'b']).
It performs target[index] = value in order, including duplicate indices, without
snapshotting the index sequence. Composite replacements share the same object;
they are not deep-copied per position. List indices may be negative; filling never grows a List.
An index or write error stops the call without rolling back earlier writes.
Even an empty index sequence requires an IndexSettable target.

Each index item is passed unchanged as one write argument. Pair and List keys are
not expanded into coordinates. Array currently requires separate row and column
indices and cannot directly be filled through this interface; String is immutable.
IndexSettable does not imply an enumerable sequence of indices, and composite
Dictionary keys must not be mistaken for coordinate tuples.

count uses the same matching rules as identical and List membership. It does not
invoke overloaded == or convert between Int and Float. Empty inputs return 0.
String iteration uses bytes: count('ababa', 'a') is 3, while count('ababa', 'aba')
is 0; this is not substring counting. Pass keys or values explicitly for a
Dictionary. Array currently does not implement Iterable; use `dense::flat(array)` for an explicit iterable one-dimensional view.

```tapas
let stdlib_bulk_mask = replicate([true], 10)
fill(stdlib_bulk_mask, false, iter(2, 2, 10))
print(count(stdlib_bulk_mask, false))
let stdlib_bulk_dictionary = {'a': 1, 'b': 2}
fill(stdlib_bulk_dictionary, 7, ['a', 'b'])
print(count(values(stdlib_bulk_dictionary), 7))
```

<pre class='Tapas-Return'>
4
2
</pre>

```tapas
let stdlib_access_matrix = array(2, 3, [1, 2, 3, 4, 5, 6])
print(idx(stdlib_access_matrix, 1, 2))
print(idx_or(stdlib_access_matrix, 9, 0, -1))
print(strings::join('abc', '-'))
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

<a id="package-types"></a>

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

<a id="package-syntax"></a>

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

<a id="package-rules"></a>

### `rules`: Rule checking, transformations, and IR package

#### Types

| Name | Meaning |
|---|---|
| `PointsOf` | Finite point-domain template. |
| `RangeOf` | Closed integer interval-domain template. |
| `Table` | Immutable finite scalar relation table. |
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
| `table(columns: List[String], data: Dictionary[String, List])` | `Table` | Build a relation with explicit column order; List rows use in for membership. |
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

<a id="package-evaluators"></a>

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

Ordered by data processing, data structures, and system access, matching the overview table:

- Data processing: `bytes`, `strings`, `json`.
- Data structures: `heap`.
- System access: `os` (environment/programs), `path` (lexical paths), `fs` (filesystem), `io` (contents/streams), `time`.

<a id="package-bytes"></a>

### `bytes`: Byte-buffer package

#### Types

| Name | Meaning |
|---|---|
| `Buffer` | Fixed-length mutable byte sequence; one byte per item, read as Int. |

#### Functions

| Signature | Returns | Behavior |
|---|---|---|
| `buffer(length: Int, value: Int)` | `Buffer` | Allocate length items filled with value; length is nonnegative and value is in 0..255. |
| `from_string(value: String)` | `Buffer` | Copies all String bytes, including NUL and invalid UTF-8. |
| `to_string(value: Buffer)` | `String` | Copies all Buffer bytes without encoding conversion. |

Buffer supports Int and negative indices, `idx_or`, Iterable, MemberTestable, `len`, `copy`, `fill`, and `count`.
Writes require Int values in 0..255, without truncation or Bool/Float conversion. Append, deletion, and resizing are unsupported.
`copy` creates independent storage. Counting uses identical semantics on yielded Int values: 1, 1.0, and true are distinct.
`fill` accepts arbitrary Iterable indices; valid Iterator ranges use native bulk writes, with memset for contiguous bytes.
String/Buffer conversion uses direct memory copying without per-byte boxing and returns independent storage.
Mutating a Buffer cannot alter existing Strings. No encoding conversion or UTF-8 validation occurs;
use strings::is_utf8 explicitly when text is required. File operations remain the responsibility of io.

```tapas
let stdlib_byte_buffer = bytes::buffer(6, 1)
fill(stdlib_byte_buffer, 255, iter(1, 2, 6))
print(count(stdlib_byte_buffer, 255))
let stdlib_view_array = array(1, 3, true)
let stdlib_flat_view = dense::flat(stdlib_view_array)
fill(stdlib_flat_view, false, iter(1, 3))
print(count(stdlib_flat_view, true))
```

<pre class='Tapas-Return'>
3
1
</pre>

<a id="package-strings"></a>

### `strings`: Basic string operations

String composition uses `strings::join`. The root `join` remains a legacy compatibility
alias sharing the same implementation and retaining existing bytecode slots. New code should use the package API.

| Signature | Returns | Behavior |
|---|---|---|
| `join(values: Iterable, separator: String)` | `String` | Joins String elements in iteration order; the separator may be empty, and empty input returns empty String. |
| `split(text: String, separator: String)` | `List` | Splits on a nonempty separator, retaining empty fields; empty input returns a one-item List containing an empty String. |
| `lines(text: String)` | `List` | Splits LF, CRLF, and CR and removes line endings; empty input returns an empty List, and a terminal line ending adds no extra item. |
| `trim(text: String)` | `String` | Trims ASCII space, HT, LF, VT, FF, and CR from both ends. |
| `find(text: String, needle: String, [start: Int])` | `Int` | Finds the first absolute byte offset, or -1; start defaults to 0. |
| `replace(text: String, old: String, replacement: String)` | `String` | Replaces all non-overlapping matches left to right; old must be nonempty. |
| `starts_with(text: String, prefix: String)` | `Bool` | Tests a prefix; empty prefixes match. |
| `ends_with(text: String, suffix: String)` | `Bool` | Tests a suffix; empty suffixes match. |
| `is_utf8(text: String)` | `Bool` | Validates strict UTF-8; empty input and embedded NUL are valid. |

Operations preserve complete byte lengths without modifying inputs or normalizing Unicode.
join accepts any Iterable but requires every element to be String; String iteration yields bytes, not Unicode code points.
Negative find start offsets count from the end, clamping below zero; offsets beyond length return -1.
Empty needles match at valid start positions. split/replace reject empty patterns. lines retains actual empty lines.
See the [`strings` README](../src/stdlib/strings/README.md) for complete contracts.

```tapas
print(strings::join(strings::split('a::b::', '::'), '|'))
print(strings::find('ababa', 'aba', 1))
print(strings::replace('你好你好', '你好', '好'))
print(try_int('')::ok)
```

<pre class='Tapas-Return'>
a|b|
2
好好
false
</pre>

<a id="package-json"></a>

### `json`: JSON encoding and decoding

| Signature | Returns | Behavior |
|---|---|---|
| `parse(text: String)` | `AnyType` | Parses one complete strict JSON document; raises a runtime error on failure. |
| `try_parse(text: String)` | `Dictionary` | Returns `ok`, `value`, `error`, and `position` for handling invalid data. |
| `stringify(value: AnyType, [pretty: Bool])` | `String` | Writes compact JSON, or indented JSON when pretty is true. |

The exported `Null` Type and storable `json::null` value are distinct from `Nil`.
Objects become Dictionaries and arrays become Lists. Integer tokens must fit Int;
large integers never silently become Float. Strings require valid UTF-8 and preserve embedded NUL.
Duplicate keys, nesting beyond 256 containers, non-finite numbers, non-String dictionary keys,
and cyclic containers are rejected. Object output order is unspecified.
On successful `try_parse`, error is empty and position is -1. On failure, value is json::null;
position is a zero-based byte offset for syntax errors, or -1 for mapping errors.
Argument type and resource errors still raise runtime errors.
See the [`json` README](../src/stdlib/json/README.md) for mappings and limits.

<a id="package-heap"></a>

### `heap`: Binary heap package

`heap` maintains binary heaps in ordinary Lists, modifying them in place.

| Function | Behavior | Time complexity |
|---|---|---|
| `heapify(items[, less])` | Build a heap in place; return Nil | O(n) |
| `push(items, value[, less])` | Insert an element; return Nil | Amortized O(log n) |
| `pop(items[, less])` | Remove and return the root | O(log n) |
| `peek(items)` | Read the root | O(1) |

Default min-heaps accept Int, non-NaN Float, String, and Lists recursively
containing these values. Lists compare lexicographically, with shorter prefixes first;
`[1, 3]` precedes `[1, 9]`. Mixed Int/Float comparisons preserve integer precision.
Large numeric partitions use jittered stratified sampling. Homogeneous Int/Float uses a two-way partition when samples have no duplicates and a three-way partition otherwise; mixed Number retains the three-way partition.
Numeric, String and List categories cannot be ordered against each other. NaN,
unsupported payloads, and excessive or cyclic nesting cause errors during input
validation in default `heapify`/`push`; such records need an explicit comparator.
Input validation takes O(v), where v is
the number of recursively contained input values; comparison cost depends on the
common prefix length. An optional `less(a, b)`
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

<a id="package-os"></a>

### `os`: Environment and program execution

| Signature | Returns | Behavior |
|---|---|---|
| `getenv(name: String)` | `Dictionary` | found/value distinguishes an unset variable from an empty value. |
| `cwd()` | `String` | Current working directory. |
| `home()` | `String` | Nonempty HOME, otherwise the current user's account directory. |
| `temp_dir()` | `String` | Nonempty TMPDIR, otherwise `/tmp`; queries without creating anything. |
| `platform()` | `String` | Currently linux or macos. |
| `run(arguments: List[String], *, ...)` | `Dictionary` | Runs a program synchronously with captured streams, without an implicit shell. |

Named options: stdin (String or bytes::Buffer, empty by default), cwd (inherited by default),
env (String keys/values, empty by default), inherit_env (true), timeout (30 seconds; finite, positive, at most 1e9),
and output_limit (1048576 bytes per stream; nonnegative Int). Programs without `/` use the child PATH;
an unset PATH defaults to `/usr/bin:/bin`. Arguments/environment cannot contain NUL; stdin can.
Invalid types and unknown/duplicate options raise before launch.

Result status is exited, signaled, timeout, cancelled, spawn_error, or io_error. exit_code is valid only
for exited and is otherwise -1. signal reports an observed terminating signal; cancel_signal reports
the cancellation request; both use 0 when absent. stdout/stderr preserve raw bytes, elapsed is monotonic
duration, errno/error contain system diagnostics. output_truncated, stdout_truncated, stderr_truncated
indicate incomplete capture. Nonzero exit codes remain ordinary program results.
Output beyond the limit is drained and discarded. Timeout/cancellation terminates the process group
and reaps the direct child. SIGINT/SIGTERM are handled in the waiting thread; its mask is restored on return.
Global cwd, environment and signal handlers are unchanged; multithreaded hosts must coordinate signal routing.
See the [`os` README](../src/stdlib/os/README.md) for lifecycle details and limits.

```tapas
let stdlib_os_run = os::run(['/bin/sh', '-c', 'printf hello; printf log >&2; exit 7'])
print(stdlib_os_run::status)
print(stdlib_os_run::exit_code)
print(stdlib_os_run::stdout)
print(stdlib_os_run::stderr)
```

<pre class='Tapas-Return'>
exited
7
hello
log
</pre>

<a id="package-path"></a>

### `path`: Lexical paths

| Signature | Returns | Behavior |
|---|---|---|
| `join(...parts: String)` | `String` | Accepts at least one part, skips empty parts, inserts `/` as needed; later absolute parts discard preceding parts. |
| `normalize(value: String)` | `String` | Collapses `/`, removes `.`, and lexically reduces `..`; empty input becomes `.`. |
| `basename(value: String)` | `String` | Last component after ignoring trailing `/`; root returns `/`, empty input returns empty String. |
| `dirname(value: String)` | `String` | Parent after ignoring trailing `/`; returns `.` if none, and `/` for root. |
| `extension(value: String)` | `String` | Last extension including its dot; leading-only dots do not form an extension. |
| `is_absolute(value: String)` | `Bool` | Tests for a leading `/`. |

Uses Linux/macOS POSIX path text conventions and rejects embedded NUL. No filesystem access,
symlink resolution, tilde/variable/glob expansion, or Windows drive handling occurs.
join retains `.` and `..`; normalize explicitly reduces them. Lexical reduction may change the actual
file target when symlinks are involved. See the [`path` README](../src/stdlib/path/README.md).

```tapas
print(path::join('fixtures', 'orders.json'))
print(path::normalize('/a/../../b//'))
print(path::extension('/a/.env.local'))
```

<pre class='Tapas-Return'>
fixtures/orders.json
/b
.local
</pre>

<a id="package-fs"></a>

### `fs`: Filesystem operations

| Signature | Returns | Behavior |
|---|---|---|
| `stat(path: String)` / `lstat(path: String)` | `Dictionary` | Follows links / inspects the final link itself; fields kind, size, permissions, modified. |
| `exists(path: String)` | `Bool` | Missing paths return false; permission and other query errors still raise. |
| `list_dir(path: String)` | `List` | Byte-sorted entry names excluding `.` and `..`. |
| `mkdir(path: String, [parents: Bool])` | `Nil` | Creates a directory; parents defaults to false, or creates ancestors and accepts existing directories when true. |
| `copy(source: String, target: String)` | `Nil` | Copies regular-file data; rejects source links, special files and existing targets. |
| `rename(source: String, target: String)` | `Nil` | POSIX rename, including target replacement; cross-device moves fail. |
| `remove(path: String)` | `Nil` | Removes a file, final link or empty directory, without recursion. |
| `temp_dir([parent: String])` / `temp_file([parent: String])` | `String` | Creates a unique temporary item under TMPDIR or `/tmp` by default; caller owns cleanup. |

kind is file, directory, symlink or other; size is bytes, modified is a second-resolution Time.
Every function has a try_ counterpart with the same argument positions, returning ok/value/code/error/errno.
Failure values are empty Dictionary for metadata, empty List for enumeration, empty String for temporary paths,
false for exists and mutations; successful mutations return true. Check ok first. Resource exhaustion can still raise.
See the [`fs` README](../src/stdlib/fs/README.md) for stable error codes and side effects.

```tapas
let stdlib_fs_root = fs::temp_dir()
let stdlib_fs_path = path::join(stdlib_fs_root, 'fixture.json')
io::write_text(stdlib_fs_path, '{}')
print(fs::stat(stdlib_fs_path)::kind)
print(len(fs::list_dir(stdlib_fs_root)))
fs::remove(stdlib_fs_path)
fs::remove(stdlib_fs_root)
```

<pre class='Tapas-Return'>
file
1
</pre>

<a id="package-io"></a>

### `io`: File contents and standard streams

| Signature | Returns | Behavior |
|---|---|---|
| `read_text(path: String)` | `String` | Reads the complete file. |
| `write_text(path: String, text: String)` | `Nil` | Creates or replaces the file. |
| `append_text(path: String, text: String)` | `Nil` | Creates the file or appends at its end. |
| `replace_text(path: String, text: String)` | `Nil` | Writes a temporary file in the same directory and atomically replaces the target. |

These functions preserve the exact string byte length. File APIs use String or
bytes::Buffer. They do not expose file handles or mode strings requiring callers
to manage an open-file lifecycle.


Recoverable counterparts try_read_text(path), try_write_text(path, text), try_append_text(path, text),
and try_replace_text(path, text) return Dictionary records. Their parameters accept AnyType to
report argument errors. Fields are ok, value, code, error, errno: successful reads return String,
failed reads return an empty String; writes return true on success or false on failure. Check ok first.
On success code/error are empty and errno is 0. Failure codes are invalid_type, invalid_path,
not_found, permission_denied, is_directory, not_directory, no_space, already_exists, not_empty,
symlink_loop, cross_device, or io_error. io/fs share the system-error mapping.
errno is platform-specific and 0 for argument errors. Both interfaces reject empty/NUL-containing paths
and validate content types before opening files. Ordinary writes can partially succeed before failure;
failed atomic replacement cleans up its temporary file. Resource exhaustion may still raise errors.
See the [`io` README](../src/stdlib/io/README.md) for complete constraints.

#### Binary files and standard streams

| Signature | Returns | Behavior |
|---|---|---|
| `read_bytes(path: String)` | `bytes::Buffer` | Reads directly into byte storage, without a String intermediate. |
| `write_bytes(path: String, buffer: bytes::Buffer)` | `Nil` | Creates or overwrites a binary file. |
| `append_bytes(path: String, buffer: bytes::Buffer)` | `Nil` | Appends bytes. |
| `replace_bytes(path: String, buffer: bytes::Buffer)` | `Nil` | Writes a temporary file and atomically replaces the target. |
| `write(text: String)` / `write_error(text: String)` | `Nil` | Writes complete bytes to stdout/stderr without an automatic newline. |
| `flush([stream: String])` | `Nil` | Flushes stdout (default) or stderr. |
| `is_terminal(stream: String)` | `Bool` | Tests stdin/stdout/stderr for an interactive terminal. |

All four binary-file functions have try_ variants with the same result record; failed reads return an empty Buffer.
Standard-stream writes preserve NUL and do not transcode; streams cannot be closed through these APIs, and errors raise.

<a id="package-time"></a>

### `time`: Time package

| Signature | Returns | Behavior |
|---|---|---|
| `from_unix(seconds: Int)` | `Time` | Creates an instant from whole seconds since the Unix epoch. |
| `unix(value: Time)` | `Int` | Returns whole Unix seconds for an instant. |
| `format(value: Time, pattern: String)` | `String` | Formats in local time using the host C `strftime` pattern. |

Timestamps and offsets must fit both the host `time_t` range and the Tapas
`Int` range. Supported formatting conversions are defined by the host C
library. Time stores no time-zone information; UTC formatting and strict parsing are described below.

| Added signature | Returns | Behavior |
|---|---|---|
| `monotonic()` | `Float` | Monotonic seconds for elapsed differences, not Unix time. |
| `monotonic_ns()` | `Int` | Monotonic nanoseconds, with range checks. |
| `unix_ns()` | `Int` | Current Unix nanosecond timestamp, with range checks. |
| `format_utc(value: Time, pattern: String)` | `String` | Uses UTC and the host strftime pattern. |
| `parse_utc(text: String)` | `Time` | Strict YYYY-MM-DDTHH:MM:SSZ, years 0001..9999; rejects invalid dates, leap seconds, offsets and fractions. |
| `try_parse_utc(text: AnyType)` | `Dictionary` | ok/value/code/error/errno; failure value is epoch Time, with invalid_type, invalid_time or out_of_range code. |

Time retains second resolution. Nanosecond queries expose the current instant or monotonic clock without expanding Time.
Both formatting APIs reject NUL in patterns; format directives remain platform C-library behavior.
See the [`time` README](../src/stdlib/time/README.md) for complete contracts.

## Mathematics and probability packages

These packages provide scalar mathematics, dense-array computation, reproducible random numbers, and finite probability distributions.

<a id="package-math"></a>

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

`isqrt(value: Int) -> Int` returns the exact floor square root of a nonnegative Int across the full Int range; negative values raise an error.

<a id="package-dense"></a>

### `dense`: Dense-array package

#### Types

| Name | Meaning |
|---|---|
| `FlatView` | Mutable one-dimensional row-major view of a RealArray or BoolArray. |

#### Functions

| Signature | Returns | Behavior |
|---|---|---|
| `flat(value: RealArray \| BoolArray)` | `FlatView` | Create a shared-storage one-dimensional view without copying elements. |
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


`flat` views use a single Int index, support negative indices, iteration, `fill`, and `count`, and cannot resize.
The view retains its source array; writes through either object are immediately visible through the other.
BoolArray views read and write Bool. RealArray views read Float and accept Int or Float writes.
Counting matches yielded types, so Float 2.0 in a RealArray view does not match Int 2.
`copy(view)` retains the FlatView Type but creates independent storage. The source array's two-index and iteration rules remain unchanged.

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

<a id="package-random"></a>

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

<a id="package-finite"></a>

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

## Application packages

These packages serve specific applications such as Rule solving and sampling, and source formatting.

<a id="package-solve"></a>

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
| `cache_limit` | `Int` | `0`; maximum active rejection regions, with 0 meaning unlimited. |
| `cache_policy` | `String` | `'mass'`; evict by least unique prior mass, or use `'fifo'` / `'binding_count'`. |
| `exploration` | `String` | `'budget'`; existing budget scheduling, experimental `'gain'` gating, or `'off'`. |
| `core_call_limit` | `Int \| Nil` | `nil`; optional absolute cap on extra solver queries; 0 disables extra queries. |
| `discovery_horizon` | `Int` | `0`; estimate remaining proposals automatically; a positive value specifies an estimated total proposal horizon for gating. |
| `events` | `Bool` | `false`; record queries, cache changes, and exploration pause/resume events. |

SampleResult.status is complete, incomplete, unknown, unsupported, or error.
Other fields are reason, samples, candidates, solver_calls, core_calls,
cache_hits, cache_regions, trace, elapsed, events, and timings. Timings distinguish solver,
mass calculation, and cache selection; selection includes mass calculation, so these
components are not additive. Exhausting exploration budgets continues sampling; candidate
or time limits can produce a partial sample prefix. Samples may repeat; inspect status rather than treating a nonempty
samples list as completion.

The backend requires Python and OR-Tools; versions are in
[requirements.txt](../src/stdlib/solve/requirements.txt). It uses python3 by default;
TAPAS_SOLVE_PYTHON selects another interpreter path. See the
[solve package documentation](../src/stdlib/solve/README.md) for full result
fields, supported inputs, and strategy options.

<a id="package-format"></a>

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
