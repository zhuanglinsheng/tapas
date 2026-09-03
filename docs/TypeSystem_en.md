# The Tapas Type System

[简体中文](TypeSystem_zh.md) | English | [Project Home](../README_en.md)

This document describes the type annotations, Type values, static checking,
and `types` package currently supported by Tapas. It supplements the type-system
definition in the [Language Reference](Syntax_en.md). The rules here are
normative for those features; exact Rule Types and their constructors are
defined separately in the [Rule documentation](Rules_en.md).

Type annotations participate only in compile-time analysis. They do not change
a value's runtime representation or insert implicit checks or conversions. A
Type can also be stored, passed, returned, and exported as an ordinary value;
this document calls such a value a Type value. User-defined Types are immutable,
and their complete definitions must be visible at compile time.

When the compiler cannot determine a value's Type, the value may still flow
into an annotated position. Code that needs to verify its actual structure at
a dynamic boundary must call `types::matches` explicitly.



## 1. Core Concepts

### 1.1 Type values and runtime Types

A Type value belongs to a distinct value category alongside String, List,
Dictionary, and Time. An ordinary Dictionary does not become a Type value merely
because it resembles a Type definition. Once constructed, a Type value is
immutable. Indexing, reflection, and iteration may read its definition or
return an ordinary container copy, but cannot expose writable internal state.

A Type value and a runtime Type are different concepts. A Type value is a value
that a program can store and pass, such as `types::Int`. A runtime Type describes
the category to which a value belongs while the program executes; the runtime
Type of the integer `1`, for example, is `types::Int`. Because `types::of(value)`
returns a runtime Type, `types::of(types::Int)` is `types::Type`.

### 1.2 Predefined Types

The `types` package exports the following Type values:

| Name | Values represented |
|---|---|
| `types::AnyType` | Any value; also the recursion terminus of Type definitions |
| `types::Nil` | The internal nil value |
| `types::Bool` | Booleans |
| `types::Int` | Integers |
| `types::Float` | Floating-point numbers |
| `types::String` | Strings |
| `types::List` | Lists with no element constraint |
| `types::Pair` | Pairs with no constraint on either member |
| `types::Dictionary` | Dictionaries with no field, key, or value constraint |
| `types::Iterator` | Iterators |
| `types::Function` | Functions |
| `types::Library` | Libraries |
| `types::RealArray` | Real arrays |
| `types::BoolArray` | Boolean arrays |
| `types::Time` | Instants in time |
| `types::Type` | Type values |
| `types::Indexable` | Objects that support index reads |
| `types::IndexSettable` | Objects that support index writes |
| `types::Appendable` | Objects that support appending |
| `types::Deletable` | Objects that support deletion |
| `types::Contains` | Objects that support `in` membership tests |
| `types::Iterable` | Objects that support `for` iteration |
| `types::Rule` | Any Rule |
| `types::RuleInstance` | Any Rule instance |

Type names in documentation and source use the capitalization shown above;
without the `types::` qualifier they remain `Nil`, `Bool`, `Int`, and `Float`.
Lowercase `nil` denotes the internal absence value, while lowercase `int()`,
`float()`, and `bool()` name conversion functions. `Any`, `Array`, and
`number` are not standard Type names; use `AnyType`, `RealArray | BoolArray`,
and `Int | Float`, respectively.

`types::List`, `types::Pair`, `types::Dictionary`, and `types::Iterator` describe
only a value's basic category; this document calls them raw container Types.
Capability Types such as `Indexable` and `Appendable` require the corresponding
operation without constraining its parameter or result Types. See the
[Standard Library](Stdlib_en.md) for the operations supported by each value
category and the [Rule documentation](Rules_en.md) for exact Rule and
RuleInstance Types.

### 1.3 Static Type expressions

An expression that produces a Type value at runtime is not necessarily valid
in a type annotation. The compiler recognizes only the following as static
Type expressions:

1. a predefined Type from the `types` package;
2. an established static Type binding that has not subsequently been
   reassigned in the current scope;
3. a static Type binding exported by a module interface;
4. a Type construction form that satisfies Section 2, or an exact Rule Type
   construction form defined by the Rule documentation.

A type annotation may additionally use the parameterized Type applications
defined in Section 3.1. This syntax constructs a Type only in an annotation; it
is not an ordinary indexing expression and executes no runtime code.

Any number of aliases may be introduced for a static Type with `let`:

```tapas
let Integer = types::Int
let AlsoInteger = Integer
let value: AlsoInteger = 1
```

Ordinary function calls, parameters, indexing, conditional expressions, `var`
bindings, and other runtime operations are not static Type expressions. The
compiler does not execute or inline an ordinary function to obtain a Type:

```tapas
function choose(flag)
{
    if (flag) {
        return types::Int
    }
    return types::String
}

let Chosen = choose(true)
// Chosen contains a Type at runtime, but is not a static Type binding.
```

A Type may still be stored and selected at runtime:

```tapas
var current = types::Int
current = types::String
```

`current` stores a Type value, and its runtime Type is `types::Type`, but it
cannot be used in an annotation.

Type construction forms use function-call syntax, but they are compile-time
built-ins rather than first-class functions that can be stored, passed, or
called indirectly. The compiler must recognize the form directly, and each Type
argument must be a static Type expression. The compiler emits a Type-value
constant or module-initialization code for the result; a program cannot build a
new Type definition from a file, the network, or other runtime data.



## 2. Constructing Types

### 2.1 Field Types

`types::make_type` accepts one or more Pair expressions written directly in
the call. The left member of each Pair is a field name, and the right member is
the field's Type:

```tapas
let Person = types::make_type(
    'name' : types::String,
    'age' : types::Int,
)
```

Pairs describe fields only. The result is a distinct, immutable Type value, not
an ordinary Dictionary. The compiler checks that:

1. the call contains at least one argument;
2. every top-level argument is a Pair written directly in the call;
3. the left member of each Pair is a String literal;
4. field names are unique and do not begin with the reserved character `@`;
5. the right member of each Pair is a static Type expression;
6. an optional field wraps one directly written Pair in `types::optional`.

Fields are separated by commas. Because the Pair operator is right-associative,
a nested Pair cannot stand in for multiple arguments:

```text
types::make_type('x' : types::Int : types::String)
// Compile error: the right side of field x is not a Type.
```

Neither a Dictionary nor a variable holding a Pair is accepted as constructor
input:

```text
let fields = {'name' : types::String}
let name_field = 'name' : types::String

let Person = types::make_type(fields)          // Compile error
let OtherPerson = types::make_type(name_field) // Compile error
```

Field names are case-sensitive and are compared as their original UTF-8 byte
sequences; no Unicode normalization is performed. Fields are required by
default and may be marked optional with `types::optional`:

```tapas
let User = types::make_type(
    'name' : types::String,
    types::optional('age' : types::Int),
)
```

`types::optional` is valid only directly inside `types::make_type` and must wrap
one directly written Pair. An optional field may be absent; when present, its
value must still match the declared Type. `types::optional_fields(T)` returns
the optional field names of field Type `T`. Deleting an optional field is valid;
deleting a required field is a compile error.

A value matches a field Type if and only if it is a Dictionary, contains every
required field, and every declared field that is present recursively matches
its Type. Undeclared extra fields do not affect matching.

Field Types use structural equivalence. Binding names, declaration locations,
and field order do not affect equivalence. Two definitions are equivalent only
when their field names, field Types, and optional status are identical. A
Dictionary may therefore match several field Types; there is no global
inheritance hierarchy.

### 2.2 Parameterized containers

Parameterized containers constrain List elements, the two members of a Pair,
Dictionary keys and values, or the elements produced by an Iterator:

```tapas
let IntList = types::list(types::Int)
let Entry = types::pair(types::String, types::Int)
let Scores = types::dictionary(types::String, types::Float)
let IntIterator = types::iterator(types::Int)
let Matrix = types::list(types::list(types::Float))
```

Constructor arguments must be static Type expressions and may be nested to a
finite depth. A constructed Type may be bound to a name or written directly in
an annotation with a parameterized Type application:

```tapas
let values: IntList = [1, 2, 3]
let direct_values: List[Int] = [1, 2, 3]
let matrix: List[List[Float]] = []
let scores: Dictionary[String, Float] = {}
let indices: Iterator[Int] = 0 to 10
```

`List[T]`, `Pair[A, B]`, `Dictionary[K, V]`, and `Iterator[T]` produce Types
equivalent to `types::list(T)`, `types::pair(A, B)`,
`types::dictionary(K, V)`, and `types::iterator(T)`, respectively. The bracket
form makes annotations concise but does not add user-defined generic templates.

A raw container Type checks only the runtime category and is not equivalent to
a parameterized container using `types::AnyType`:

```tapas
let raw_list_is_distinct = types::List != types::list(types::AnyType)
```

A field Type and a uniform-key/value Dictionary Type are separate constraints.
The former requires a fixed set of fields; the latter checks every key and
value in the container. The current type system cannot combine both constraints
in one Type.

The current runtime Iterator produces only Int values, so an integer range
matches `Iterator[Int]`. `types::matches` can validate that constraint without
consuming the Iterator; other element Types do not match.

### 2.3 Unions

`types::union` accepts at least two static Type arguments:

```tapas
let Identifier = types::union(
    types::Int,
    types::String,
)
```

To construct a union, the compiler:

1. flattens existing unions;
2. removes equivalent duplicate members;
3. sorts members into a stable canonical order;
4. uses the member itself if deduplication leaves only one member;
5. uses `types::AnyType` if the union contains `types::AnyType`.

The following unions are therefore equivalent, and `types::members` returns
their members in the same order:

```tapas
let canonical_identifier_a = types::union(types::Int, types::String)
let canonical_identifier_b = types::union(types::String, types::Int)
```

`types::union(T, T)` is valid and produces `T`. A source-level call with fewer
than two arguments is a compile error.

### 2.4 Recursive Types

A recursive Type first declares an uninitialized `let` explicitly annotated
as `Type`, then completes it with one static Type assignment:

```text
let Tree: Type

Tree = types::make_type(
    'value' : types::Int,
    'children' : types::list(Tree),
)
```

Before completion, the name may occur only as a static Type reference needed
to complete a recursive definition; it cannot be read as an ordinary runtime
value. Several names may be declared first to form mutual recursion:

```text
let Left: Type
let Right: Type

Left = types::make_type('rights' : types::list(Right))
Right = types::make_type('lefts' : types::list(Left))
```

Every cycle must pass through a real Type constructor such as a field,
container, union, or function. Pure alias cycles such as `A = A` or
`A = B; B = A` are compile errors. A completed Type graph is immutable.
Reassigning the variable later does not mutate the sealed graph or redirect
existing back references.

Recursive Types retain structural equivalence. Equality, assignability, and
matching remember visited node pairs or value-Type pairs and do not expand a
cycle indefinitely. Display uses a finite recursion marker rather than trying
to print the whole graph. Module interfaces preserve the structured graph
rather than depending on expanded display text.

### 2.5 Type equivalence and field construction order

Binding names, declaration locations, and object addresses do not determine
Type equivalence. Predefined Types compare by category; unions compare their
flattened, deduplicated members; parameterized Types compare their base and
parameters; and field Types compare field names, field Types, and optional
status. Recursive Types compare as finite graphs with back references, so the
comparison never expands indefinitely.

Equivalent Types have the same hash and address the same Dictionary key,
although different Types may coincidentally share a hash. Printed output and
the diagnostic data returned by `types::definition` do not participate in
equivalence.

A field Type also preserves the order in which fields appear in
`types::make_type` for use by positional structure literals. This construction
order is not part of the structural definition and does not affect equivalence,
assignability, or hashing. Two equivalent field Types may therefore have
different construction orders and interpret positional structure literals in
their respective orders. Type aliases and module imports preserve the original
construction order.



## 3. Type Annotations and Value Construction

### 3.1 Annotation syntax and name resolution

Annotations follow declaration names and function parameters, and may also
describe function results:

```tapas
let annotation_count: Int = 1
let annotation_values: List[Int] = [1, 2, 3]
let annotation_identifier: Int | String = 'T-1'

function annotation_size(text: String) -> Int
{
    return len(text)
}
```

The complete grammar is:

```text
declarator       = IDENTIFIER, [ ":", type-expression ], [ "=", expression ] ;
type-expression  = union-type ;
union-type       = primary-type, { "|", primary-type } ;
primary-type     = function-type | type-application | qualified-type-name ;
function-type    = function-constructor, "[",
                   [ type-arguments, [ "," ] | "..." ], "]",
                   "->", type-expression ;
function-constructor = "Function" | "types::Function" ;
type-application = qualified-type-name,
                   "[", [ type-arguments, [ "," ] ], "]" ;
type-arguments   = type-expression, { ",", type-expression } ;
qualified-type-name = IDENTIFIER, { "::", IDENTIFIER } ;
```

A parameterized Type application accepts only these built-in constructors:

| Form | Arity | Equivalent construction form |
|---|---:|---|
| `List[T]` | 1 | `types::list(T)` |
| `Pair[A, B]` | 2 | `types::pair(A, B)` |
| `Dictionary[K, V]` | 2 | `types::dictionary(K, V)` |
| `Iterator[T]` | 1 | `types::iterator(T)` |
| `Union[A, B, ...]` | at least 2 | `types::union(A, B, ...)` |
| `A \| B \| ...` | at least 2 | `Union[A, B, ...]` |
| `Function[A, B, ...] -> R` | zero or more parameters and one result | no runtime construction form |
| `Rule[A, B, ...]` | zero or more parameters | `types::rule(A, B, ...)` |
| `RuleInstance[A, B, ...]` | zero or more parameters | `types::rule_instance(A, B, ...)` |

A constructor may also be qualified, as in `types::List[T]`. Each argument is
recursively a `type-expression`, so applications may be nested to a finite
depth. Names are case-sensitive. A wrong arity, an unsupported base Type, or
an argument that is not a valid Type expression is a compile error. A trailing
comma is allowed. `Function[] -> R` denotes a function with no parameters;
`Function[...] -> R` denotes a variadic function, whose variadic items cannot
currently carry a separate Type annotation. See the [Rule documentation](Rules_en.md)
for the details of Rule Types.

Source annotations use the more concise `|` form by default. It is Type syntax
sugar for `Union` and has lower precedence than Type applications and function
Types. `List[Int | Float]` is equivalent to
`List[Union[Int, Float]]`; in `Function[Int | Float] -> String | Nil`, the
parameter and result are both union Types. `A | B | C` directly creates one
three-member union rather than nested binary unions. In an ordinary expression,
`|` remains element-wise OR. Tools currently display the stable full form
`Union[A, B]`.

An unqualified constructor name follows the normal shadowing rule. If the
current scope contains a binding with that name, resolution does not fall back
to the built-in constructor. User-defined parameterized Type constructors are
not currently supported.

An annotation cannot call a constructor or evaluate another ordinary
expression:

```text
let Identifier = types::union(types::Int, types::String)
let value: Identifier = read()
let direct: Int | String = read()

let other: types::union(types::Int, types::String) = read() // Compile error
let invalid: Int[String] = 1                                // Compile error
```

The `types::` qualifier may be omitted for a predefined Type in an annotation:

```tapas
let count: Int = 1
let name: String = 'Tapas'
```

An unqualified name following `:` is resolved in this order:

1. look for a binding with the same name in the current lexical scope;
2. if the binding is a static Type, use that Type;
3. if it is an ordinary value, or a Type value that cannot be resolved as a
   static Type expression, report an error and do not fall back;
4. if the current scope contains no such binding, look for a predefined Type in
   the `types` package;
5. if no Type is found, report an unknown Type.

Qualified names do not use fallback resolution. `model::Person` must resolve
to a static Type through the module interface for `model`. Ordinary
expressions do not use predefined-Type fallback either, so obtaining a Type
value requires its full name:

```tapas
let schema = types::Int
```

If an ordinary value shadows a predefined name, the fully qualified name
remains available:

```text
let Int = 42
let value: Int = 1        // Compile error: Int is an ordinary value.
let other: types::Int = 1
```

A type annotation supplies a target Type to the compiler. It performs no
implicit conversion and is not stored in the runtime variable slot.

### 3.2 Function signature annotations

Function parameter and result annotations use the same `type-expression` as a
declaration:

```text
parameter        = IDENTIFIER, [ ":", type-expression ] ;
fixed-parameters = parameter, { ",", parameter }, [ "," ] ;
return-annotation = "->", type-expression ;
function-literal = parameter-list, [ return-annotation ], block ;
```

The named declaration
`function name(parameters) -> Type
{ ... }` uses the same signature syntax. It
stores the function value in the read-only environment binding `name`, which
other closures may capture.

Each fixed parameter may be annotated or left unannotated:

```text
function find(values: List[Int], target: Int, start) -> Int
{
    // ...
}
```

An exact function Type can annotate higher-order parameters, results, and
ordinary bindings:

```tapas
function apply(
        callback: Function[Int] -> String,
        value: Int,
) -> String {
    return callback(value)
}

let formatter: Function[Int] -> String = (value: Int) -> String
{
    return str(value)
}
```

`->` is right-associative, so `Function[] -> Function[Int] -> String` denotes
a zero-argument function that returns a formatting function. Exact function
Types require the same fixed arity. Parameter Types are contravariant and the
result Type is covariant. Fixed and variadic signatures are not assignable to
one another.

Parameter annotations are resolved in the definition environment of the
function literal, before parameter names can shadow Type names. The resolved
Types belong to the function signature and provide the target Types of the
parameter bindings while the body is analyzed. A statically known call target
also checks each argument with the assignability relation in Section 4.1;
`this` uses the same signature as the current function.

`-> Type` annotates the function result. The compiler checks every reachable
exit with the relation in Section 4.1: a `return` with an expression checks
that expression, while a bare `return` and reaching the end of the body both
produce `Nil`. If the compiler cannot prove that every path returns, the
result therefore includes `Nil`. An explicit result Type makes the result of a
recursive call known before the body is analyzed. Without a result annotation,
the function result is `Unknown`; this design does not require automatic
inference.

Parameter annotations provide compile-time constraints only. Compilation is
accepted when an argument or call target is `Unknown`, and the compiler does
not insert an implicit check at function entry. Use `types::matches` or an
inline `assert(rule { ... })` when a dynamic boundary must enforce a more specific
condition. The variadic `...` form cannot carry a parameter annotation, but a
result annotation may follow its parameter list.

The compiler uses a precise function signature to check the body, statically
known calls, and module interfaces. This signature is compile-time Type
metadata and does not change the runtime function object;
`types::of(function_value)` still returns raw `types::Function`. Exact function
Types currently have no runtime construction or reflection interface.

### 3.3 Field Types and structure literals

A field Type still describes an ordinary Dictionary at runtime. The complete
construction syntax is a Dictionary literal with a type annotation:

```tapas
let Point = types::make_type(
    'x' : types::Float,
    'y' : types::Float,
)

let origin: Point = {
    'x' : 0.0,
    'y' : 0.0,
}
```

`types::of(origin)` returns `types::Dictionary`, while
`types::matches(origin, Point)` returns `true`.

When the compiler knows that the target is a single field Type, a structure
literal provides a shorthand:

```tapas
let positional_origin: Point = {0.0, 0.0}
let same_origin: Point = {0.0, y=0.0}
let named_origin: Point = {x=0.0, y=0.0}
```

A structure literal follows these rules:

1. its target must be a single field Type; the shorthand is unavailable
   without an annotation or when the target is a union or
   `types::Dictionary`;
2. positional items precede named items and fill fields not yet specified in
   construction order;
3. a named item has the form `IDENTIFIER = expression` and may refer only to a
   declared field whose name is a valid Identifier;
4. no field may be specified twice, and every required field must receive a
   value;
5. the shorthand cannot contain undeclared fields; use an ordinary Dictionary
   literal for extra fields or field names that are not valid Identifiers;
6. expressions are evaluated from left to right in source order, and the
   result is an ordinary Dictionary whose keys are field-name Strings.

In `{0.0, y=0.0}`, the positional item fills `x` and the named item fills `y`.
The syntax depends on the target Type; it does not create a “Point object” with
a distinct runtime identity.



## 4. Static Checking

### 4.1 Assignability

Variable initialization, later assignment, field writes, and function
parameter and result annotations use the same assignability relation.

A known expression Type `A` is assignable to target Type `B` if and only if one
of these rules applies:

1. `A` and `B` are equivalent;
2. `B` is `types::AnyType`;
3. `A` is a union and every member of `A` is assignable to `B`;
4. `B` is a union and `A` is assignable to at least one member of `B`;
5. `A` is a parameterized container and `B` is the corresponding raw
   container Type;
6. `A` and `B` are parameterized containers of the same kind and every
   corresponding Type parameter is equivalent;
7. `A` is a field Type and `B` is `types::Dictionary`;
8. both `A` and `B` are field Types, and `A` satisfies the required fields,
   optional fields, and corresponding field Types expected by `B`;
9. `A` and `B` are exact function Types satisfying the parameter-contravariance
   and result-covariance rules in Section 3.2;
10. `A` and `B` are Rule-related Types satisfying the assignability rules in
    the Rule documentation.

A field Type and a uniform-key/value Dictionary Type are not assignable to one
another. Apart from the rules above, two known Types are not assignable. There
is no implicit conversion such as Int to Float.

Fields with the same name and parameterized-container Type arguments are
compared invariantly because the corresponding Dictionaries, Lists, and Pairs
are mutable. If `List[Int]` were assignable to `List[Int | Float]`, code using
the latter view could insert a Float and invalidate the original annotation:

```text
let Number = types::union(types::Int, types::Float)
let IntList = types::list(types::Int)
let NumberList = types::list(Number)

let ints: IntList = [1, 2]
let numbers: NumberList = ints // Compile error
```

Field Types support width assignability: an `A` with more fields is assignable
to a `B` with fewer, provided fields with the same name have equivalent Types.
Every required field of `B` must also be required in `A`; an optional field of
`B` may be absent from `A`, or may be required or optional there. Writing an
undeclared field through a field-Type reference is a compile error; otherwise a
narrower alias could corrupt an extra field required by the wider Type. To add
fields, first assign the value explicitly to `types::Dictionary` and accept the
loss of static field constraints.

Parameterized containers and field Types are assignable to their corresponding
raw container Types. Assignment to a raw container or `types::AnyType` loses
some static constraints. If the value is modified through the wider alias, the
compiler does not guarantee that its original annotation still holds and does
not insert a runtime check.

When the compiler cannot determine an expression's Type, static analysis uses
the internal state `Unknown`. `Unknown` is not a program-visible Type value and
cannot appear in an annotation. It may enter any target position without
causing an implicit runtime check:

```text
var amount: Int = 1
amount = 2
amount = 'three'          // Compile error
amount = foreign_value()  // Unknown; accepted by the compiler
```

Both `let` and `var` retain the assignment behavior of the current language.
When a binding is annotated, every subsequent assignment that can be analyzed
statically is checked against the original annotation. An unannotated static
Type binding ceases to be usable as a static Type name after reassignment, even
if its new runtime value is also a Type. Errors involving name resolution,
argument counts, duplicate fields, or deletion of required fields are governed
by their own rules and are not part of assignability.

A first assignment after a declaration also participates in definite-
initialization analysis: a later read is valid only if every continuing control-
flow path has assigned the binding. A loop body alone cannot establish a
post-loop initialization fact. Completion of a recursive Type definition must
occur on the unconditional path of its declaring block, not in a conditional
branch or loop.

### 4.2 Literal checking and inference

When a fresh literal has a target Type, it is checked contextually instead of
first being fixed to a narrower mutable-container Type. Each field, element,
key, or member expression must be assignable to its corresponding target Type.

An ordinary Dictionary literal must contain every required field and may
contain extra fields. The structure-literal shorthand accepts only fields
declared by the target Type:

```tapas
let Number = types::union(types::Int, types::Float)
let Measurement = types::make_type('value' : Number)

let sample: Measurement = {'value' : 1}
let concise_sample: Measurement = {value=1}

let NumberList = types::list(Number)
let number_values: NumberList = [1, 2.5]
```

A fresh literal may be assigned directly to a wider container target. A
mutable container already stored in a variable must follow the invariant rule.

Without a target Type, container literals are inferred as follows:

1. if every element Type of a non-empty List is known, Item is their canonical
   union;
2. if both member Types of a Pair are known, they become First and Second;
3. if every key of a non-empty Dictionary is a String literal and every value
   Type is known, infer a field Type;
4. for any other non-empty Dictionary whose key and value Types are known, Key
   and Value are the normalized unions of those Types;
5. infer the corresponding raw container Type for an empty container or when
   any required member is `Unknown`.

If an inferred union has only one member after normalization, that member is used
directly. This is not subject to the source-level rule that `types::union`
requires at least two arguments.

The compiler can infer the result of reading a declared field from a value with
a field Type. For a statically recognizable write through such a value, the
new value must be assignable to the field's declared Type. Deleting a required
field or writing an undeclared field is a compile error:

```text
var person: Person = load_person()
person['age'] = 37
person['age'] = 'old'       // Compile error
person['nickname'] = 'Ada'  // Compile error: Person has no nickname field.
```

Mutation through a raw-container alias, native extension, or other `Unknown`
path does not trigger an implicit check and may cause a value to cease matching
its original annotation. Call `types::matches` explicitly when a structure
must be guaranteed across a dynamic boundary.



## 5. The `types` Package and Runtime Behavior

### 5.1 Interface overview

The compile-time Type construction forms are:

| Signature | Purpose |
|---|---|
| `types::make_type(...Pair(String, Type)) -> Type` | Construct a non-empty field Type |
| `types::union(Type, Type, ...) -> Type` | Construct a union |
| `types::list(Type) -> Type` | Construct a parameterized List Type |
| `types::iterator(Type) -> Type` | Construct a parameterized Iterator Type |
| `types::pair(Type, Type) -> Type` | Construct a parameterized Pair Type |
| `types::dictionary(Type, Type) -> Type` | Construct a parameterized Dictionary Type |
| `types::optional(Pair(String, Type))` | Mark an optional field inside a field Type |

Annotations also provide `List[T]`, `Iterator[T]`, `Pair[A, B]`,
`Dictionary[K, V]`, `Union[A, B, ...]`, `A | B`, and exact function Types.
These forms are not ordinary runtime indexing and do not define new Type
templates. Rule-related construction forms are defined in the
[Rule documentation](Rules_en.md).

The runtime-checking interfaces are:

| Signature | Purpose |
|---|---|
| `types::of(AnyType) -> Type` | Return the intrinsic runtime Type of a value |
| `types::matches(AnyType, Type) -> Bool` | Test whether a value matches a Type |

The read-only reflection interfaces are:

| Signature | Purpose |
|---|---|
| `types::fields(Type) -> Dictionary` | Return a copy of the user fields of a field Type |
| `types::optional_fields(Type) -> List` | Return the optional field names of a field Type |
| `types::members(Type) -> List` | Return the members of a union |
| `types::base(Type) -> Type` | Return the raw Type of a parameterized container |
| `types::parameters(Type) -> Dictionary` | Return a copy of a parameterized container's parameters |
| `types::definition(Type) -> Dictionary` | Return a diagnostic copy of the definition |

### 5.2 `types::of` and `types::matches`

`types::of` returns only the intrinsic runtime category of a value:

| Value | Result |
|---|---|
| Nil | `types::Nil` |
| Bool | `types::Bool` |
| Int | `types::Int` |
| Float | `types::Float` |
| String | `types::String` |
| List | `types::List` |
| Pair | `types::Pair` |
| Dictionary | `types::Dictionary` |
| Iterator | `types::Iterator` |
| Function | `types::Function` |
| Library | `types::Library` |
| Real array | `types::RealArray` |
| Boolean array | `types::BoolArray` |
| Instant in time | `types::Time` |
| Type | `types::Type` |
| Rule | `types::Rule` |
| RuleInstance | `types::RuleInstance` |

The table lists the core value categories; see the [Rule documentation](Rules_en.md)
for Rule IR and evaluator value Types. `types::of` never returns a union,
parameterized container, exact function Type, or user-defined field Type:

```tapas
let dictionary_runtime_type_is_generic = types::of({'name' : 'Ada'}) == types::Dictionary
let incomplete_person_does_not_match = types::matches({'name' : 'Ada'}, Person)
```

`types::matches(value, expected)` applies these rules in order:

1. if `expected` is `types::AnyType`, return `true`, including for Nil;
2. if `expected` is a union, return `true` when any member matches;
3. if `expected` is a parameterized List, the value must be a List and every
   element must match Item;
4. if `expected` is a parameterized Iterator, the value must be an Iterator and
   its intrinsic element Type must match Item;
5. if `expected` is a parameterized Pair, the value must be a Pair and its two
   members must match First and Second respectively;
6. if `expected` is a parameterized Dictionary, the value must be a Dictionary
   and every key and value must match Key and Value respectively;
7. if `expected` is a field Type, the value must be a Dictionary containing
   every required field, and every declared field that is present must match;
8. if `expected` is an exact Rule, RuleInstance, or RuleTerm Type, matching
   follows the signature or result-Type rules in the Rule documentation;
9. otherwise, use the assignability rules in Section 4.1 to determine whether
   `types::of(value)` is assignable to `expected`.

An empty List or Dictionary matches the corresponding parameterized container
Type. A container check traverses its current contents and takes time linear in
the number of members. It neither modifies the value nor attaches the result to
the value.

### 5.3 Reflection, equality, and errors

`types::fields(T)` returns a copy of the user fields for a field Type and an
empty Dictionary for any other Type. `types::members(T)` returns union members
in canonical order, or a one-element List containing `T` for any non-union.
`types::base(T)` returns the corresponding `types::List`, `types::Iterator`,
`types::Pair`, or `types::Dictionary` for a parameterized container. It returns
the corresponding raw Type for an exact Rule or RuleInstance Type, and returns
`T` itself for other Types.

For a parameterized container, `types::parameters(T)` returns a copy in an
ordinary Dictionary with these fixed keys:

- List: `item`;
- Iterator: `item`;
- Pair: `first`, `second`;
- Dictionary: `key`, `value`.

It returns an empty Dictionary for any other Type. Modifying a reflection
result never affects the original Type. A program may use
`types::base(T) != T` to determine whether `T` is a parameterized container;
Type equivalence is tested directly with `==`.

Ordinary read-only operations on a Type expose only its user fields:

- `T['name']` returns the field Type and reports an indexing error when the
  field does not exist;
- `len(T)` returns the number of user fields;
- `keys(T)` and iteration return only user fields;
- predefined Types, parameterized containers, and unions all have zero user
  fields.

`types::definition(T)` returns a new ordinary Dictionary on every call. The
result may contain internal encodings, is not stable across versions, and
cannot be passed back to `types::make_type`. A Type's printed representation is
likewise intended only for diagnostics.

For non-recursive Types, `==` follows the equivalence rules in Section 2.5; for
recursive Types, it compares finite Type graphs. `!=` is its negation. Between a
Type and a non-Type, `==` returns `false` and `!=` returns `true`.
`identical(A, B)` tests only whether both values refer to the same Type object.
Equivalent Types may or may not share an object, and programs must not depend
on either behavior.

When a Type is used as a Dictionary key, key equality follows `==`. A
non-recursive Type's hash is derived from its structural definition; a
recursive Type uses a stable hash compatible with graph equivalence. Equivalent
Types must have the same hash and address the same Dictionary entry.

`types::of` accepts any value. The second argument to `types::matches` must be
a Type. The other reflection functions report a runtime argument-type error
when given a non-Type. Invalid argument counts, shapes, or static properties in
a Type construction form are reported by the compiler.



## 6. Types in Modules

A module may export a static Type:

```text
// geometry.tap
let Point = types::make_type(
    'x' : types::Float,
    'y' : types::Float,
)

return {'Point' : Point}
```

```text
// app.tap
import geometry.tap as geometry

let origin: geometry::Point = {
    'x' : 0.0,
    'y' : 0.0,
}
```

The module interface stores the complete structure of a static Type. For a
field Type, it also preserves the construction order required by positional
structure literals. An importing module does not reconstruct this information
from printed output or from `types::definition`.

A module may also export an ordinary binding that happens to hold a Type value.
If the binding did not originate from a static Type expression, an importer may
read and pass it but cannot use it in an annotation. Module interfaces therefore
distinguish static Type bindings from ordinary bindings whose runtime Type is
`types::Type`.



## 7. Current Limitations and Implementation Boundary

The current type system does not represent:

- array shapes, dimensions, or other numeric parameters;
- precise function-signature Types that can be constructed or reflected at
  runtime;
- intersections, nominal inheritance, or method Types.

Arrays continue to use `types::RealArray` and `types::BoolArray`; their shapes
and dimensions are not Type parameters. Future additions of the capabilities
listed above must not change the meaning of the Types defined here.

This document specifies public language behavior, not internal structures such
as `ttypeval`, the Type arena, canonical encodings, or caches. The contents of
`types::definition` and printed Type text are diagnostic and may change with the
implementation; programs must not depend on reserved keys or their order.
The compiler, Language Server, and runtime may use different representations as
long as they preserve the construction, equivalence, assignability, matching,
and reflection behavior specified here.
