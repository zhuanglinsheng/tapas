# Tapas Type System Design

[简体中文](TypeSystem_zh.md) | English

This document defines the next stage of the Tapas type system: type
annotations, runtime `Type` values, static checking, the `types` package, and
implementation constraints. Syntax and interfaces that have not yet entered
the current language specification are described here with their proposed
normative semantics.

The design follows four principles:

- Type annotations participate only in compile-time analysis. They do not
  change the runtime representation of a value or insert implicit checks or
  conversions.
- A `Type` is also an ordinary runtime value and may be stored, passed,
  returned, and exported.
- User-defined Types are immutable, and their complete definitions must be
  visible at compile time.
- A value whose type cannot be determined statically may still flow into an
  annotated position. Programs use `types::matches` for explicit validation at
  dynamic boundaries.

Conceptually, every Type is a recursive, immutable mapping:

```text
Dictionary[String, Type]
```

This is a uniform semantic model; it does not require the underlying C object
to contain an ordinary `tdict`.



## 1. Type Values and Static Types

### 1.1 Runtime representation

`Type` is a distinct composite value category, alongside String, List,
Dictionary, and Time. An ordinary Dictionary does not become a Type merely
because its contents have the logical shape of a Type definition.

A Type is immutable once constructed. Indexing, reflection, and iteration may
read its definition or return a copy in an ordinary container, but they must
not expose a writable location within the Type.

Type definitions recursively refer to other Types. The sole recursion
terminus is:

```text
types::AnyType -> {}
```

`types::AnyType` is the only Type with an empty definition and also denotes any
value. Users cannot construct an empty Type:

```text
types::make_type() // Compile error: a field Type must not be empty.
```

Every definition path must eventually reach `types::AnyType`. This design does
not support direct or indirect recursion. Self-reference, mutual references
within a module, and cyclic references across modules are all compile errors.

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

`types::List`, `types::Pair`, and `types::Dictionary` are called **raw
container Types**.

### 1.3 Static Type expressions

A runtime value being a Type does not by itself make the expression valid in a
type annotation. The compiler recognizes only the following as static Type
expressions:

1. a predefined Type from the `types` package;
2. an immutable static Type binding already established in the current scope;
3. a static Type binding exported by a module interface;
4. a direct call to `types::make_type`, `types::union`, `types::list`,
   `types::pair`, or `types::dictionary` that satisfies Section 2.

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
let choose = (flag){
    if(flag){
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

The runtime type of `current` is `types::Type`, but `current` cannot be used in
an annotation.

The five Type constructors use function-call syntax, but they are compile-time
built-in forms rather than first-class functions that can be stored, passed, or
called indirectly. They are valid only as calls recognized directly by the
compiler, and every argument must be a static Type expression. The compiler
emits a runtime Type constant or module-initialization code for the result. A
program cannot construct a new Type definition from a file, the network, or
other runtime data.



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

The Pairs are constructor inputs only. The result remains, conceptually, an
immutable `Dictionary[String, Type]`. The compiler checks that:

1. the call contains at least one argument;
2. every top-level argument is a Pair written directly in the call;
3. the left member of each Pair is a String literal;
4. field names are unique and do not begin with the reserved character `@`;
5. the right member of each Pair is a static Type expression.

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
sequences; no Unicode normalization is performed. Every field is required.
This design has no optional fields.

A value matches a field Type if and only if it is a Dictionary, contains every
required field, and the value of each required field recursively matches the
corresponding Type. Undeclared extra fields do not affect matching.

Field Types use structural equivalence. Names assigned to a Type, declaration
locations, and field declaration order do not affect equivalence. Two
definitions are equivalent when their field names and corresponding Types are
identical. A Dictionary may therefore match several field Types; there is no
global inheritance hierarchy.

### 2.2 Parameterized containers

Lists, Pairs, and Dictionaries with uniform key and value Types are described
by these forms:

```tapas
let IntList = types::list(types::Int)
let Entry = types::pair(types::String, types::Int)
let Scores = types::dictionary(types::String, types::Float)
let Matrix = types::list(types::list(types::Float))
```

Constructor arguments must be static Type expressions and may be nested to a
finite depth. An annotation accepts only a Type reference, so a constructed
Type should first be bound to a name. This design does not add syntax such as
`List[Int]`:

```tapas
let values: IntList = [1, 2, 3]
```

A raw container Type checks only the runtime category and is not equivalent to
a parameterized container using `types::AnyType`:

```tapas
let raw_list_is_distinct = types::List != types::list(types::AnyType)
```

A field Type and a uniform-key/value Dictionary Type are separate constraints.
The former requires a fixed set of fields; the latter checks every key and
value in the container. This design cannot combine both constraints in one
Type.

Iterators do not currently support an element Type because `types::matches`
cannot check every element without consuming the Iterator. Arrays continue to
use `types::RealArray` and `types::BoolArray`; integer parameters such as shape
and dimension are outside this design, whose constructors accept only Type
parameters.

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
3. sorts members by canonical representation;
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

### 2.4 Canonical representation and construction order

The canonical representation used for Type equivalence, ordering, hashing,
and module interfaces is generated as follows:

1. `types::AnyType` uses a fixed terminus marker;
2. every other predefined Type uses a fixed numeric identifier;
3. a field Type encodes, in UTF-8 byte order by field name, each name's length,
   the name itself, and the field Type;
4. a parameterized container encodes the raw container identifier, fixed
   parameter names, and parameter Types;
5. a union is flattened and deduplicated, then its members are encoded in byte
   order of their canonical representations;
6. every segment includes its category and length; displayed text must not
   simply be concatenated.

Type cycles are forbidden, so recursive computation of a canonical
representation always terminates. The canonical representation is not a
display format and is not returned by `types::definition`.

A field Type also has a **construction order**: the order in which its Pairs
appear in `make_type`. The compiler and module interface preserve this order
for positional structure literals. It is not part of the structural Type
definition and does not participate in equivalence, assignability, or hashing.
Type aliases and module imports preserve the construction order of the
original Type.

Two equivalent field Types may have different construction orders. Their
positional structure literals are interpreted according to their respective
orders even though both Types describe the same structure.



## 3. Type Annotations and Object Construction

### 3.1 Annotation syntax and name resolution

The annotation syntax in a declaration is:

```text
declarator     = IDENTIFIER, [ ":", type-reference ], "=", expression ;
type-reference = IDENTIFIER, { "::", IDENTIFIER } ;
```

An annotation may refer only to a static Type binding. It cannot invoke a
constructor, perform indexing, or evaluate another expression:

```text
let Identifier = types::union(types::Int, types::String)
let value: Identifier = read()

let other: types::union(types::Int, types::String) = read() // Compile error
```

The `types::` qualifier may be omitted for a predefined Type in an annotation:

```tapas
let count: Int = 1
let name: String = 'Tapas'
```

An unqualified name following `:` is resolved in this order:

1. look for a binding with the same name in the current lexical scope;
2. if the binding is a static Type, use that Type;
3. if it is an ordinary value or a non-static runtime Type, report an error and
   do not fall back;
4. if the current scope contains no such binding, look for a predefined Type in
   the `types` package;
5. if no Type is found, report an unknown Type.

Qualified names do not use fallback resolution. `model::Person` must resolve
to a static Type through the module interface for `model`. Ordinary
expressions do not use predefined-Type fallback either, so obtaining a runtime
Type value requires its full name:

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

### 3.2 Dictionaries and structure literals

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

Variable initialization, later assignment, and field writes all use the same
assignability relation. Future annotations for function parameters and return
values should use this relation as well.

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
8. both `A` and `B` are field Types, `A` contains every field of `B`, and the
   Types of fields with the same name are equivalent.

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
Writing an undeclared field through a field-Type reference is a compile error;
otherwise a narrower alias could corrupt an extra field required by the wider
Type. To add fields, first assign the value explicitly to `types::Dictionary`
and accept the loss of static field constraints.

Parameterized containers and field Types are assignable to their corresponding
raw container Types. Assignment to a raw container or `types::AnyType` loses
some static constraints. If the value is modified through the wider alias, the
compiler does not guarantee that its original annotation still holds and does
not insert a runtime check.

When the compiler cannot determine an expression's Type, it uses the internal
state `Unknown`. `Unknown` is not a Type, but it may enter any target position
without causing an implicit runtime check:

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
   and Value are their respective canonical unions;
5. infer the corresponding raw container Type for an empty container or when
   any required member is `Unknown`.

If an inferred canonical union has only one member, that member is used
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
| `types::pair(Type, Type) -> Type` | Construct a parameterized Pair Type |
| `types::dictionary(Type, Type) -> Type` | Construct a parameterized Dictionary Type |

The runtime-checking interfaces are:

| Signature | Purpose |
|---|---|
| `types::of(AnyType) -> Type` | Return the intrinsic runtime Type of a value |
| `types::matches(AnyType, Type) -> Bool` | Test whether a value matches a Type |

The read-only reflection interfaces are:

| Signature | Purpose |
|---|---|
| `types::fields(Type) -> Dictionary` | Return a copy of the user fields of a field Type |
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

It never returns a union, parameterized container, or user-defined field Type:

```tapas
let dictionary_runtime_type_is_generic = types::of({'name' : 'Ada'}) == types::Dictionary
let incomplete_person_does_not_match = types::matches({'name' : 'Ada'}, Person)
```

`types::matches(value, expected)` applies these rules in order:

1. if `expected` is `types::AnyType`, return `true`, including for Nil;
2. if `expected` is a union, return `true` when any member matches;
3. if `expected` is a parameterized List, the value must be a List and every
   element must match Item;
4. if `expected` is a parameterized Pair, the value must be a Pair and its two
   members must match First and Second respectively;
5. if `expected` is a parameterized Dictionary, the value must be a Dictionary
   and every key and value must match Key and Value respectively;
6. if `expected` is a field Type, the value must be a Dictionary containing
   every required field, and each field value must match;
7. otherwise, use the assignability rules in Section 4.1 to determine whether
   `types::of(value)` is assignable to `expected`.

An empty List or Dictionary matches the corresponding parameterized container
Type. A container check traverses its current contents and takes time linear in
the number of members. It neither modifies the value nor attaches the result to
the value.

### 5.3 Reflection, equality, and errors

`types::fields(T)` returns a copy of the user fields for a field Type and an
empty Dictionary for any other Type. `types::members(T)` returns union members
in canonical order, or a one-element List containing `T` for any non-union.
`types::base(T)` returns `types::List`, `types::Pair`, or
`types::Dictionary` for a parameterized container, and otherwise returns `T`
itself.

For a parameterized container, `types::parameters(T)` returns a copy in an
ordinary Dictionary with these fixed keys:

- List: `item`;
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

Type `==` compares canonical representations; `!=` is its negation. Between a
Type and a non-Type, `==` returns `false` and `!=` returns `true`.
`identical(A, B)` tests only whether both values refer to the same Type object.
Equivalent Types may or may not share an object, and programs must not depend
on either behavior.

When a Type is used as a Dictionary key, key equality follows `==` and its hash
is derived from the canonical representation. Equivalent Types must have the
same hash and address the same Dictionary entry.

`types::of` accepts any value. The second argument to `types::matches` must be
a Type. The other reflection functions report a runtime argument-type error
when given a non-Type. Invalid argument counts, shapes, or static properties in
a Type construction form are reported by the compiler.



## 6. Modules and Tooling

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

The module interface stores a Type's canonical representation. For a field
Type, it also stores the construction order required by positional structure
literals. The interface does not store object addresses, results from
`types::definition`, or displayed text. An importing module reconstructs or
reuses the runtime Type from its canonical representation; construction order
is used only to expand structure literals at compile time.

A module may also export a binding whose runtime value is a Type but whose
source is not a static Type expression. An importer can use that binding only
as an ordinary runtime value, not in an annotation. A module interface must
distinguish a “static Type binding” from an “ordinary binding whose runtime
type is Type.”

The Language Server reads the same module interface to provide Type-name
completion, go to definition, field completion, container-member inference,
find references, and static diagnostics. The compiler and Language Server
perform the prescribed static evaluation only for the five Type construction
forms; they do not execute ordinary Tapas functions.



## 7. Implementation Constraints

### 7.1 Internal object

Type uses the distinct composite type code `compo_ttypeval`. To avoid confusion
with the existing `ttypes`, its C structure is named `ttypeval`:

```c
typedef struct ttypeval {
    tcompo_v compo_base;
    thashtbl *definition;
} ttypeval;
```

`compo_base` is the object header shared by composite values and stores the
Type vtable pointer and reference count. `definition` is an immutable
`String -> Type` mapping that stores only one level of the current Type's
definition. Its Type values refer directly to existing `ttypeval` objects
rather than copying the entire definition tree. The implementation uses
`thashtbl` directly and does not wrap it in a mutable `tdict`.

Only the following internal shapes are valid for `definition`:

| Type | Logical contents |
|---|---|
| `types::AnyType` | `{}` |
| Any other predefined Type | `{'@builtin/name' : types::AnyType}` |
| Field Type | `{'field-name' : FieldType, ...}` |
| Parameterized List | `{'@base' : types::List, '@item' : Item}` |
| Parameterized Pair | `{'@base' : types::Pair, '@first' : First, '@second' : Second}` |
| Parameterized Dictionary | `{'@base' : types::Dictionary, '@key' : Key, '@value' : Value}` |
| Union | `{'@union/0' : Member0, '@union/1' : Member1, ...}` |

For example:

```tapas
let int_definition_example = types::Int
// {'@builtin/Int' : types::AnyType}

let list_definition_example = types::list(types::Int)
// {'@base' : types::List, '@item' : types::Int}

let point_definition_example = Point
// {'x' : types::Float, 'y' : types::Float}
```

Every key must be a String and every value must be a Type.
`types::AnyType` is the only empty table. An ordinary field name cannot begin
with `@`, and distinct internal shapes cannot be mixed.

These reserved keys are implementation details. Programs must not depend on
their exact names or order in the result of `types::definition` or in printed
output. Union members must be normalized and sorted before insertion. The
number in `@union/N` denotes canonical member order and must not be derived
from hash-table iteration order.

A raw container and a parameterized container using `types::AnyType` have
different definitions. For example, `types::List` uses `@builtin/List`, while
`types::list(types::AnyType)` uses `@base` and `@item`; the two are therefore
not equivalent.

### 7.2 Immutability and the object protocol

An implementation writes a `ttypeval` only during internal construction. Once
published, no `definition` entry may be added, removed, or replaced. The hash
table owns references to its keys and values. Releasing a Type must release the
table and, under the existing reference-counting rules, the Strings and Types
it contains. Runtime entry points should still defensively reject an empty
definition, a non-String key, a non-Type value, an invalid reserved key, or an
invalid internal shape.

No public operation may expose an internally writable location:

- ordinary indexing, `len`, `keys`, and iteration expose only user fields and
  hide every `@` key;
- `types::fields`, `types::members`, `types::parameters`, and
  `types::definition` return copies or new containers;
- `types::base` returns the raw container Type according to the internal
  category;
- `==` and hashing use the canonical representation, independently of object
  addresses and hash-table iteration order.

The `ttypeval` vtable provides the fixed type name `Type`, composite type code
`compo_ttypeval`, release, read-only access, string representation, `==`, `!=`,
and identity. When a Type is used as a Dictionary key, hashing and equivalence
must use the canonical representation rather than the address comparison used
by ordinary composite objects.

Field construction order is not stored in `ttypeval`; the compiler and module
interface maintain it separately. The current structure does not constrain a
future implementation. It may add category or hash caches, use sorted arrays
or small-object inlining, or intern objects, provided the public behavior and
the `Dictionary[String, Type]` logical model remain unchanged.



## 8. Current Scope

This design does not currently represent:

- recursive Types;
- optional fields;
- Iterator element Types;
- array shapes, dimensions, or other numeric parameters;
- function parameter or return Types;
- intersections, nominal inheritance, or method Types.

For now, a recursive position can use only `types::AnyType`, thereby giving up
the static constraint at that position. Any future addition of the capabilities
above requires a separate design and must not change the meaning of the Types
defined here.
