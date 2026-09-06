# The Tapas Programming Language

[简体中文](Syntax_zh.md) | English | [Project Home](../README_en.md)

This document is the normative language specification for Tapas. It defines
the source language independently of the current compiler implementation. A
conforming compiler, formatter, and language server must follow these rules.

The document has four parts:

1. a tutorial introduction with the language semantics;
2. lexical rules;
3. a complete syntactic grammar in EBNF;
4. default operator semantics for built-in types.

See the [Standard Library](Stdlib_en.md) for root built-ins, native packages,
and source-package APIs.

The words **must**, **must not**, **should**, and **may** are normative. Every
code block marked `tapas` is executable. The Tapas binary reads those blocks in
document order as one program, so a block may use declarations from an earlier
`tapas` block in the same document. Code marked `text` is only a notation
example and may intentionally be incomplete or invalid Tapas.

## Part I — Language Semantics Tutorial

### 1. A first program

Tapas is an expression-oriented scripting language with optional Type
annotations and compile-time checking. A source file contains statements
separated by a newline or semicolon. Newlines inside parentheses, brackets,
braces, or strings do not separate statements.

```tapas
var tutorial_limit = 5

function tutorial_square(x)
{
    return x * x
}

for (let tutorial_i in 0 to tutorial_limit) {
    print(tutorial_i, ' -> ', tutorial_square(tutorial_i))
}
```
<pre class='Tapas-Return'>
0 -> 0
1 -> 1
2 -> 4
3 -> 9
4 -> 16
</pre>

`var` and `let` both declare variables, but they have different lifetime and
capture rules. A named function begins with `function`, followed by its name,
parameter list, and body. An anonymous function literal has no name and consists
of a parameter list and body. `0 to 5` creates the half-open iterator
`0, 1, 2, 3, 4`.

For a runnable introductory program, see
[A First Look at Tapas](examples/Basics_en.md).

### 2. Statements and separators

A newline or `;` terminates a simple statement at delimiter depth zero. Empty
statements are ignored. These are equivalent:

```text
let a = 1
let b = 2

let a = 1; let b = 2
```

A compound statement owns a braced block and does not need a semicolon after
its closing brace. An `elif` or `else` may follow the previous `}` directly or
after newlines, blank lines, and comments. A semicolon ends the entire
conditional and is not allowed between its arms.

Comments begin with `//` outside a string and continue through the end of the
physical line. A comment ends the statement text on that line; an expression
cannot continue after a line comment unless it is already inside delimiters.

### 3. Value Types, Reference Types, and Type Annotations

Tapas distinguishes value Types from reference Types by their assignment and
argument-passing behavior. `Bool`, `Int`, and `Float` are value Types:
assignment and argument passing copy the value itself. All other built-in
objects are reference Types. Assigning or passing one copies its reference, so
several variables may refer to the same object. Section 5 covers copying and
identity in more detail.

Type annotations are optional. Without one, a variable may hold values of
different Types over time. When an annotation is present, the compiler checks
the initializer and subsequent assignments against it.

```tapas
let tutorial_number: Int | Float = 1
tutorial_number = 1.5
```

Here the annotation allows `tutorial_number` to hold either `Int` or `Float`,
so both assignments pass compile-time checking.

Tapas source provides these literals directly:

| Type | Literal examples | Notes |
|---|---|---|
| `Bool` | `true`, `false` | Conditions accept only `Bool`; there is no implicit truthiness conversion. |
| `Int` | `0`, `12`, `42` | Strict decimal integers. |
| `Float` | `0.5`, `.5`, `5.`, `1e3`, `1.5e-3` | Decimal or scientific notation. |
| `String` | `'text'`, `"text"` | Single- and double-quoted forms have the same meaning. |

Integers do not accept leading zeroes or radix prefixes, so `00`, `010`,
`0x10`, `0b10`, and `0o10` are compile errors. A floating-point exponent begins
with `e` or `E`, may have a sign, and must contain digits; examples include
`1E+6`, `.5e2`, and `5.e-1`. A leading `+` or `-` is a unary operator rather
than part of the literal.

The built-in reference Types are `String`, `List`, `Pair`, `Dictionary`,
`Iterator`, `Function`, `Library`, `RealArray`, `BoolArray`, `Time`, and `Type`.
The Rule system adds reference Types such as `Rule`, `RuleInstance`, `RuleIR`,
`RuleTerm`, `RuleItem`, and `Evaluator`. Native extensions may define further
reference Types through the Tapas object interface.

`Nil` is the internal result of a function with no useful result and is neither
a storable value Type nor a reference Type. It has no source literal and cannot
be stored in a variable or collection; a bare `return` returns `Nil` to the
caller.

See the [Type System](TypeSystem_en.md) for Type construction and static
checking, [Rules](Rules_en.md) for Rule values, the
[Standard Library](Stdlib_en.md) for collections, arrays, and time, and
[C Interaction](Foreign_en.md) for extension values. The
[Type example](examples/syntax/types.tap) combines union, enum, structural, and
parameterized container Types in one program.

A finite String set is expressed with the static `types::enum` Type
constructor; `enum` is not a declaration keyword:

```tapas
let TutorialOrderStatus = types::enum(
    'Pending',
    'Shipped',
    'Delivered',
)
let tutorial_status: TutorialOrderStatus = TutorialOrderStatus['Delivered']
```

The constructor requires one or more direct, unique String literals. Enum
members remain Strings at runtime, while String indexing on an enum Type
returns a member-checked value. A member literal may be used directly when the
target Type is known; a non-member literal is a compile error. An ordinary
String expression must first pass `types::matches` to be narrowed to the enum.
Enums with the same member set are structurally equivalent; a smaller member
set is assignable to an enum containing it, and every enum is assignable to
`String`. See [Enum Types](TypeSystem_en.md#24-enum-types) for the complete
equivalence, assignability, dynamic-indexing, reflection, and runtime-erasure
rules.

### 4. Variables, scope, and assignment

A declaration may omit its initializer. Multiple declarations may share one
statement:

```tapas
var tutorial_origin = 0, tutorial_step = 1
let tutorial_message = 'hello'
tutorial_message = 42

let tutorial_count: Int
tutorial_count = 3
```

An identifier must be declared and initialized before it is read. Its first
assignment may complete initialization. Two declarations
must not introduce the same name in the same scope, including one `var` and one
`let` declaration. A nested function may shadow a name from an outer function.
Built-ins in the root environment cannot be reassigned.

Initialization follows control flow. A variable is definitely initialized only
when it has been assigned on every continuing path reaching a read. A complete
`if`/`elif`/`else` chain may establish initialization; without `else`, the path
on which every condition is false remains. A `while` or `for` body may execute
zero times, so assignments in it do not establish initialization after the
loop. When an existing variable is a `for` target, it is initialized at each
body entry, but its post-loop definite-initialization state remains the state
from before the loop.

`var` creates an **environment variable**. Its scope begins after its
declarator and extends to the end of the current module or function body.
Nested functions capture it by reference. A `var` declaration is not permitted
inside an `if`, `elif`, `else`, `for`, or `while` block; declare it in
the surrounding function or module instead.

`let` creates a **temporary variable**. Its scope begins after its declarator
and extends to the end of the innermost current block. It is removed when that
block exits and cannot be captured by a nested function. Lexical scope and
closure capture are distinct here: a nested function definition may occur in
the lexical scope of a `let`, but the resulting closure may outlive that block.
A `let` uses temporary storage owned by the block and does not enter the closure
environment. Prohibiting capture lets that storage be released deterministically
when the block exits, without implicitly extending its lifetime. State shared
with a closure should be declared with `var`.

Function parameters behave like environment variables belonging to the
function call and may be captured by a nested function.

A named `function` declaration is a read-only environment binding. Later named
functions and anonymous closures may capture it, but no scope may assign a new
value to its name. It therefore supports stable references between module-level
functions without changing the temporary-storage rules of ordinary `let`.

Assignment targets are either a variable or one direct index into a variable:

```tapas
var tutorial_value = 1
tutorial_value = 2

let tutorial_items = [10, 20]
tutorial_items[0] = 11
```

Read-only member access (`object::name` or a bare `object.name`) is not an
assignment target. Assignment through a computed or chained target such as
`make_list()[0] = 1` or `matrix[0][1] = 1` is not part of the language.

### 5. Copy and identity

Assigning or passing a value Type copies the value itself. Assigning, passing,
or inserting a reference value copies its reference, so mutations through one
alias are visible through the others.

```tapas
let tutorial_inner = [1, 2]
let tutorial_outer = [tutorial_inner]
tutorial_inner[0] = 9
pprint(tutorial_outer)
```
<pre class='Tapas-Return'>
[[9, 2]]
</pre>

For a reference Type, `copy(value)` creates an independent outer object, but
elements held inside a copied list, pair, or dictionary are still copied
shallowly. Use `identical(a, b)` to test runtime identity. The `==` operator
uses the Type's equality operation and is not a substitute for identity testing.

### 6. Strings, lists, pairs, and dictionaries

Strings use either quote style and may span physical lines. Tapas strings are
byte strings: indexing counts bytes, not Unicode code points. There are no
escape sequences; a backslash is ordinary, and the delimiter quote cannot
occur inside the string.

Strings and lists accept one integer index. Negative indices count backward
from the end. They also accept a half-open slice `start:end`; either endpoint
may be omitted. Slice bounds may be negative and are normalized relative to
the length.

```tapas
let tutorial_text = 'Tapas'
let tutorial_numbers = [0, 1, 2, 3, 4]
print(tutorial_text[1:4])
pprint(tutorial_numbers[:3])
pprint(tutorial_numbers[-2:])
```
<pre class='Tapas-Return'>
apa
[0, 1, 2]
[3, 4]
</pre>

A string index returns a one-byte string. Assigning a string to a string index
or slice replaces that range. List element assignment accepts any non-`nil`
value. List slice assignment is not supported.

The colon operator constructs a pair. It is right-associative, so `a : b : c`
means `a : (b : c)`. A pair has length two and accepts indices `0`, `1`,
`-2`, and `-1`.

A dictionary literal contains comma-separated pair entries. Keys may be any
hashable Tapas value supported by the runtime; strings are recommended for
module APIs and named members. A duplicate key replaces the earlier value.
Dictionary iteration order is unspecified.

```tapas
let tutorial_person = {
    'name' : 'Tony',
    'age' : 20,
}
print(tutorial_person['name'])
print(tutorial_person::age)
```
<pre class='Tapas-Return'>
Tony
20
</pre>

`dictionary::name` is exactly `dictionary['name']`. A bare member expression
`dictionary.name` has the same read-only lookup meaning when it is not followed
by an argument list.

For slices, list mutation, pairs, dictionaries, and shallow copying in one
program, see the
[values and collections example](examples/syntax/values_and_collections.tap).

### 7. Dense arrays

Tapas arrays are row-major and two-dimensional. `array(rows, columns, fill)`
creates a `RealArray` when `fill` is an `Int` or `Float`, and a `BoolArray`
when `fill` is a `Bool`. A `List` fill must contain exactly
`rows * columns` values of one appropriate scalar category.

Arrays require two indices. Each index is an integer or slice. Two integer
indices return a scalar; if either index is a slice, the result is a new array.

```tapas
let tutorial_matrix = array(2, 3, [1, 2, 3, 4, 5, 6])
print(tutorial_matrix[1, 2])
pprint(tutorial_matrix[0:2, 1:3])
```
<pre class='Tapas-Return'>
6
[[2, 3],
 [5, 6]]
</pre>

Scalar assignment requires a matching scalar category. Slice assignment
requires an array with the same category and shape. Real arrays support
element-wise `+`, `-`, `*`, `/`, `%`, `^`, and comparisons with a number or a
real array of equal shape. `@` performs matrix multiplication between
compatible real arrays. Boolean arrays support element-wise `&` and `|` with
a boolean or equal-shaped boolean array; these operators do not short-circuit.
Matrix multiplication, equal-shaped array addition
and subtraction, scalar multiplication, and array negation require CBLAS at
runtime; Tapas implements the other element-wise operations directly. BLAS is
not required when Tapas is installed. `and` and `or` operate on scalar
booleans only.

For matrix norms, outer products, in-place updates, and matrix multiplication,
see the [dense-array example](examples/syntax/dense_arrays.tap).

### 8. Calls, indexing, members, and tunnel calls

A call evaluates its arguments from left to right, then calls the function.
An ordinary call is written `f(a, b)`. Indexing and calls may be chained.

Tapas also has **tunnel-call syntax**:

```text
receiver.function_name(arg1, arg2)
```

It is defined as:

```text
function_name(receiver, arg1, arg2)
```

For example, `values.append(3)` means `append(values, 3)`. The name after the
dot is resolved lexically as a function; it is not read from `receiver`.

The forms involving a dot are disambiguated by the next token:

- `value.name(args)` is a tunnel call;
- `value.name` is read-only string-key lookup;
- `value::name` is always read-only string-key lookup, and a later `(args)`
  calls the looked-up value.

Thus `module::run(1)` calls the function exported under `run`, while
`value.run(1)` passes `value` as the first argument to the root name `run`.

### 9. Operators

The following table is complete. Higher rows bind more tightly. Operators on
the same row are left-associative unless stated otherwise.

| Precedence | Operators or form | Associativity and constraints |
|---:|---|---|
| 14 | call `()`, index `[]`, member `.`/`::` | left, chainable |
| 13 | `^` | right-associative |
| 12 | unary `+`, unary `-` | right-associative |
| 11 | `*`, `/`, `%`, `@` | left |
| 10 | `+`, `-` | left |
| 9 | `==`, `!=`, `>`, `<`, `>=`, `<=` | non-associative |
| 8 | `to` | non-associative |
| 7 | `in` | non-associative |
| 6 | `&` | left, non-short-circuiting |
| 5 | `\|` | left, non-short-circuiting |
| 4 | `not` | right-associative |
| 3 | `and` | left, short-circuit |
| 2 | `or` | left, short-circuit |
| 1 | `:` | right-associative |

Parentheses override precedence. In particular:

- `-2^2` means `-(2^2)`;
- `2^3^2` means `2^(3^2)`;
- `a or b and c` means `a or (b and c)`;
- `item in 0 to 10` means `item in (0 to 10)`.
- `a | b & c` means `a | (b & c)`;
- `a and b | c` means `a and (b | c)`;
- `x > 0 & y > 0` means `(x > 0) & (y > 0)`.

Comparison, `to`, and `in` expressions cannot be chained. Write
`0 < x and x < 10`, not `0 < x < 10`.

`==` and `!=` compare integers and floats numerically, including mixed numeric
types. Booleans compare only with booleans. Strings, lists, pairs, iterators,
boolean arrays, and time values compare by contents. Dictionaries, functions,
and libraries compare by object identity. Every comparison between a real array
and a number or equal-shaped real array is element-wise and returns a boolean
array. Ordering comparisons otherwise require numbers. `!=` is the logical
complement of `==` for scalar results and element-wise complement for real
arrays.

Arithmetic on two integers returns an integer except `^`, which returns a
float. Mixed integer/float arithmetic returns a float. Integer `/` truncates
toward zero. Integer division or remainder by zero is an error. Floating-point
operations follow the host C runtime.

`not` is a reserved keyword and cannot be used as a variable or function name.

See the runnable [logical negation example](examples/syntax/logical_not.tap).

In ordinary expressions, `not` accepts only a scalar Bool and returns its logical negation, evaluating the operand exactly once. Statically known non-Bool operands are compile errors; dynamic values are checked at runtime. It does not apply truthiness conversions. Inside a Rule's own expressions, `not RuleInstance` additionally checks and negates instance satisfaction, producing Bool. Ordinary functions do not inherit this extension, and bare Rules are always rejected. See [Rule negation](Rules_en.md#25-logical-negation-not).

`not` binds less tightly than comparisons, `in`, `&`, and `|`, but more tightly than `and` and `or`. Thus `not x > 0` means `not (x > 0)`, `not a and b` means `(not a) and b`, and `not not a` means `not (not a)`. Parenthesize negation when used as an operand of a comparison or a tighter-binding operator, as in `a == (not b)`.

In ordinary functions and expressions, `and` and `or` accept only Bool. Within a Rule's own expressions, they also accept RuleInstance, check satisfaction left to right with short-circuiting, and return Bool. Errors propagate; skipped instances are neither constructed nor checked. Two bare instance items are independent obligations, whereas an `and`/`or` is one combined condition. See [Rule logical composition](Rules_en.md#26-logical-composition-and--or). Conditions in `if` and `while` still require Bool.

`start to end` requires integers and creates a half-open iterator with step
`1`; it is empty when `end <= start`. Use `iter(start, step, end)` for a
descending range. `value in collection` supports iterators, lists, and
dictionary keys; for another right operand it is `false`.

For precedence, integer arithmetic, membership, and short-circuit evaluation
in one program, see the [operator example](examples/syntax/operators.tap).

### 10. Functions and closures

A fixed-arity function lists zero or more distinct parameter names:

```tapas
function tutorial_hypotenuse(x, y)
{
    return math::sqrt(x * x + y * y)
}
print(tutorial_hypotenuse(3, 4))
```
<pre class='Tapas-Return'>
5
</pre>

A statement that requires a block may place its opening brace at the end of
the header or on a later line. Blank lines and comments may occur before `{`;
a semicolon always terminates the current statement:

```tapas
function tutorial_add(left: Int, right: Int) -> Int
{
    return left + right
}
```

An arity mismatch is a compile error when the function signature is statically
known; a dynamic call can check it only at runtime. A variadic function uses
`...` as its entire parameter list. It reads arguments with `__nparam__()` and
`__param__(index)`.

Function parameter annotations use `name: Type`, and a function result
annotation uses `-> Type`. Annotated and unannotated parameters may be mixed:

```tapas
function convert(value: Int | String, strict: Bool, context) -> String
{
    if (strict) {
        return str(value)
    }
    return 'value'
}
```

Signature annotations use the `type-expression` and assignability relation
defined by the [Type System](TypeSystem_en.md). They participate only in
compile-time analysis and do not insert an implicit runtime check. The `...`
form cannot carry a parameter annotation, but a variadic function literal may
still place a `-> Type` result annotation after its parameter list. Without a
result annotation, the function result Type is `Unknown`.

An exact function Type is written as `Function[parameter Types...] -> result
Type` and may annotate higher-order parameters, results, and ordinary bindings.
`Function[] -> T` is a zero-argument function, `Function[...] -> T` is
variadic, and `->` is right-associative.

Named functions use `function` declarations by default and create read-only
bindings. Function literals are anonymous and are reserved for function-value
expressions, callbacks, and closures. Inside a function, `this` evaluates to a callable
copy of the current function and is the standard recursion mechanism. `base`
evaluates to a copy of the parent function environment. `this` and `base` are
only valid where the corresponding environment exists.

```tapas
function tutorial_factorial(n)
{
    if (n <= 1) {
        return 1
    }
    return n * this (n - 1)
}
print(tutorial_factorial(6))
```
<pre class='Tapas-Return'>
720
</pre>

For typed functions, recursion, closures, and variadic arguments in one
program, see the [function example](examples/syntax/functions.tap).

### 11. Rules and child-rule composition

A Rule is a rule value that can be stored, passed, and composed. A parameterized
Rule uses `rule (parameters) { ... }`; a zero-parameter Rule may omit the
parameter list. Every Rule parameter requires a Type annotation. A Bool
expression at the outermost level of a Rule body is a Condition that must hold:

```tapas
let tutorial_positive = rule (value: Int) {
    'value must be positive':
        value > 0
}

let tutorial_small_positive = rule (value: Int) {
    tutorial_positive(value)
    'value must be below ten':
        value < 10
}

assert(tutorial_small_positive(5))
```

Calling a Rule only binds its arguments and produces a RuleInstance; it does
not check the Conditions immediately. Checking begins when `assert`,
`rules::check`, or an evaluator consumes that RuleInstance.

Top-level Bool expressions must be true; bare RuleInstance expressions must hold and retain their dependency and violation paths. The `require` keyword has been removed: migrate `require R(x)` to `R(x)`. A Rule body may also use
local `let` declarations, and a String followed by `:` may describe one
Condition or a block of Conditions. A Rule body does not accept `var`,
assignment, control flow, imports, or direct IO.

See [Rules](Rules_en.md) for exact Rule Types, capture semantics, checking APIs,
the public IR, and evaluators.

### 12. Conditional execution and loops

An `if` chain evaluates conditions in order and executes at most one arm:

```tapas
function tutorial_sign(x)
{
    if (x < 0) {
        return -1
    } elif (x > 0) {
        return 1
    } else {
        return 0
    }
}
```

An `elif` or `else` must belong to the immediately preceding `if` chain.
There may be any number of `elif` arms and at most one final `else` arm.

`while (condition) { body }` repeats while its boolean condition is true.

A `for` loop accepts either a newly declared temporary loop variable or an
existing assignable variable:

```text
for (let item in iterable) { body }
for (item in iterable) { body }
```

The iterable must be an iterator, list, or another extension value that
explicitly supports iteration. Dictionaries are not directly iterable; use
`keys(dictionary)` or `values(dictionary)`. The `let` loop variable exists
only in the loop. An existing loop variable remains visible after the loop.

`break` and `continue` are valid only inside the nearest lexically enclosing
loop and cannot cross a function boundary. `return` is valid only inside a
function or at module top level. A return inside a nested control-flow block
returns from its enclosing function or module.

For branches, `for`, `while`, `break`, and `continue` in one program, see the
[control-flow example](examples/syntax/control_flow.tap).

### 13. Modules and imports

Each `.tap` file is a module and may also be run directly as a script. Running
a script executes its top-level statements in order and does not require a
`main` function. The example name `main.tap` merely identifies the principal
script of that example; it has no special meaning in the language.

A `.md` file may likewise be used as a module or script. Its Tapas source is
the concatenation of fenced blocks tagged `tapas` or `tap`, with original line
positions preserved for diagnostics.

```text
import path/to/module.tap
import path/to/module.tap as module_name
import 'path with spaces/module.tap' as module_name
```

A bare import executes the module for its effects. An aliased import binds a
`Library` value. A module exposes named values by returning a dictionary from
its top level:

```text
function exported_function()
{ return 5 }
return {
    'get_five' : exported_function,
}
```

The importer reads exports with `module::get_five` or
`module['get_five']`. Returning no value produces an empty library. Returning
a non-dictionary, non-`nil` value from a module is an error.

Import resolution is deterministic:

1. an absolute path is used directly;
2. a relative literal path is tried relative to the process working directory;
3. configured module search paths are tried in order;
4. for nested imports, the importing file's directory is prepended to that
   module's search paths;
5. when a candidate is a directory, `candidate/__init__.tap` is used.

The resolved file must end in `.tap` or `.md`. Circular imports are prohibited;
a conforming implementation must diagnose a cycle rather than recurse
indefinitely. Import aliases follow the ordinary identifier rules.

An `__init__.tap` file is the entry point of its directory package. If it
exports `main(arguments: List[String])`, the package can be executed as a
program with `tapas -m package [arguments]`. Tapas calls `main` with the
command-line arguments as a list of strings. An `Int` result becomes the
process exit status, while `Nil` indicates success. Thus, `main` is the entry
function of an executable directory package; an ordinary script enters through
the file itself and its top-level statements.

For both module forms, see the single-file module
[library.tap](examples/modules/library.tap), the directory-package entry point
[greeter/__init__.tap](examples/modules/greeter/__init__.tap), and the
[main.tap](examples/modules/main.tap) executable script that imports both by
relative path. See [Usage](Usage_en.md#execute-a-source-package) for package
execution and the `main` convention.

### 14. Errors and compatibility forms

A language server should distinguish lexical, syntactic, name-resolution, and
runtime errors. It should recover after a lexical or syntactic error and report
as many independent diagnostics as practical.

#### 14.1 Deprecated legacy forms

A deprecated legacy form is syntax that belonged to an earlier version of the
language, remains recognizable only for migration, and is intended to be
removed. This specification currently defines no forms in that category.

#### 14.2 Compatibility forms

The current specification defines no additional compatibility forms.

## Part II — Lexical Rules

### 1. Source text and positions

Tapas source is UTF-8 text. A byte-order mark at the start may be ignored. Line
endings may be LF or CRLF and are normalized to LF for parsing. Diagnostic line
and column numbers are one-based for users. Protocol adapters such as LSP
convert positions to the protocol's required encoding.

Strings are byte sequences and identifiers are ASCII-only in this language
version. Non-ASCII bytes may occur in strings and comments but not identifiers.

### 2. Whitespace, nesting, and statement separators

`SPACE`, horizontal tab, carriage return, and line feed are whitespace.
Whitespace separates tokens when their concatenation would form another token.

The lexer tracks parentheses `()`, brackets `[]`, braces `{}`, and string
delimiters. A newline or semicolon is a `SEPARATOR` only when all delimiter
depths are zero and the lexer is outside a string. Inside delimiters it is
ordinary whitespace, except inside a string where it is content.

Unmatched closing delimiters and end-of-file with an open delimiter or string
are lexical errors. Delimiters inside strings and comments have no nesting
effect.

### 3. Comments

A line comment begins with `//` outside a string and ends before LF or at EOF:

```ebnf
line-comment = "//", { any-code-point-except-LF } ;
```

Comments are replaced by whitespace. Tapas has no block comment syntax.

### 4. Identifiers and keywords

Identifiers are case-sensitive:

```ebnf
identifier-start    = ASCII-letter | "_" ;
identifier-continue = identifier-start | decimal-digit ;
identifier          = identifier-start, { identifier-continue } ;
```

An identifier beginning with `__` is reserved for the implementation and
standard library. User programs should not declare one.

Reserved words are:

```text
and as base break continue elif else false for function if import in
let nil not of or return rule this to true var while
```

`function` begins a named declaration but not an expression; `rule` begins a
Rule expression; bare RuleInstance items compose child rules;
`nil` and `of` also do not begin expressions. A keyword is recognized only when the following
character is not an identifier continuation character.

### 5. Numeric literals

Numeric literals are decimal. `_`, hexadecimal, binary, octal, `NaN`, and
`Infinity` spellings are not literals.

```ebnf
decimal-digit    = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9" ;
nonzero-digit    = "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9" ;
digits           = decimal-digit, { decimal-digit } ;
exponent         = ("e" | "E"), [ "+" | "-" ], digits ;
integer-literal  = "0" | nonzero-digit, { decimal-digit } ;
float-literal    = (digits, ".", [digits] | ".", digits), [exponent]
                 | digits, exponent ;
```

Except for `0`, an integer literal cannot begin with `0`. A leading sign is an
operator, not part of the literal. Integer overflow and a floating literal
outside the runtime's representable range are compile errors.

### 6. String literals

Single- and double-quoted strings have identical semantics:

```ebnf
single-string = "'", { code-point-except-single-quote }, "'" ;
double-string = '"', { code-point-except-double-quote }, '"' ;
```

Content may include newlines and the other quote style. There are no escape
sequences: `\n` is two bytes. Consequently, a string cannot contain its own
delimiter quote. Source generators should choose the other quote style.

### 7. Operators and punctuation

The lexer uses longest-match tokenization:

```text
Multi-character:  ==  !=  >=  <=  ::  ...  ->  //
Single-character: + - * / % @ ^ & | > < = : . , ; ( ) [ ] { }
Word operators:   and or not in to
```

`//` starts a comment. `.` is part of a float only in the numeric forms above;
otherwise it is member/tunnel punctuation.

### 8. Import paths

After `import`, the lexer reads either a string literal or an unquoted path up
to `as` or the statement separator. An unquoted path may contain ASCII letters,
digits, `_`, `-`, `.`, `/`, and `\`. It must not contain whitespace. Use a
quoted path when whitespace is required.

```ebnf
path-character       = ASCII-letter | decimal-digit | "_" | "-" | "."
                     | "/" | "\\" | ":" ;
unquoted-import-path = path-character, { path-character } ;
```

### 9. Markdown source extraction

In a Markdown module, an opening fence is a line whose first non-whitespace
characters are three backticks followed by the exact tag `tap` or `tapas` and
optional trailing whitespace. Source continues until a line containing three
backticks and optional surrounding whitespace. Text outside blocks contributes
blank lines so diagnostics retain Markdown line numbers. Generated
`<pre class='Tapas-Return'>` blocks are not source.

## Part III — Syntactic Grammar (EBNF)

### 1. Notation and lexical terminals

The grammar uses `=` for definition, `|` for alternatives, `[x]` for an
optional item, `{x}` for repetition, and parentheses for grouping. Quoted text
is a terminal. Uppercase rules are tokens from Part II.

```ebnf
IDENTIFIER = identifier excluding reserved words ;
INTEGER    = integer-literal ;
FLOAT      = float-literal ;
STRING     = single-string | double-string ;
PATH       = unquoted-import-path ;
LINE_SEP   = top-level newline ;
SEP        = LINE_SEP | ";" ;
```

Whitespace and comments may occur between tokens unless a lexical rule says
otherwise.

### 2. Modules, statement lists, and blocks

```ebnf
module          = separators, [ statement-list ], separators, EOF ;
statement-list  = statement, { separator-run, statement } ;
separator-run   = SEP, { SEP } ;
separators      = { SEP } ;

statement       = declaration
                | assignment
                | if-statement
                | while-statement
                | for-statement
                | break-statement
                | continue-statement
                | return-statement
                | import-statement
                | expression-statement ;

block           = "{", separators, [ statement-list ], separators, "}" ;
expression-statement = expression ;
```

The separator rule is relative to the current block: newlines at depth zero
inside a block separate that block's statements.

### 3. Declarations and assignments

```ebnf
declaration       = variable-declaration | function-declaration ;
variable-declaration = ("var" | "let"), declarator, { ",", declarator } ;
declarator        = IDENTIFIER, [ ":", type-expression ], [ "=", expression ] ;
function-declaration = "function", IDENTIFIER, parameter-list,
                       [ return-annotation ], block ;
type-expression   = union-type ;
union-type        = primary-type, { "|", primary-type } ;
primary-type      = function-type | instance-type | type-application | qualified-type-name ;
instance-type     = [ "types::" ], "InstanceOf", "[", [ ";" ],
                    qualified-type-name, [ "," ], "]" ;
function-type     = ( "Function" | "types::Function" ), "[",
                    [ type-arguments, [ "," ] | "..." ], [ ";" ], "]",
                    "->", type-expression ;
type-application  = qualified-type-name,
                    "[", [ type-arguments, [ "," ] ], [ ";" ], "]" ;
type-arguments    = type-expression, { ",", type-expression } ;
qualified-type-name = IDENTIFIER, { "::", IDENTIFIER } ;
assignment        = assignment-target, "=", expression ;
assignment-target = IDENTIFIER, [ index-suffix ] ;
```

Declaration location, duplicate-name, `nil`, and assignability restrictions
are semantic constraints from Part I.

### 4. Control flow

```ebnf
if-statement    = "if", "(", expression, ")", block,
                  { { LINE_SEP }, "elif", "(", expression, ")", block },
                  [ { LINE_SEP }, "else", block ] ;

while-statement = "while", "(", expression, ")", block ;
for-statement   = "for", "(", for-binding, "in", expression, ")", block ;
for-binding     = "let", IDENTIFIER | IDENTIFIER ;

break-statement    = "break" ;
continue-statement = "continue" ;
return-statement   = "return", [ expression ] ;
```

### 5. Imports

```ebnf
import-statement = "import", import-path, [ "as", IDENTIFIER ] ;
import-path      = PATH | STRING ;
```

`STRING` contributes its raw contents without quote delimiters.

### 6. Expressions and precedence

Each grammar layer corresponds to the precedence table in Part I.

```ebnf
expression      = pair-expression ;
pair-expression = or-expression, [ ":", pair-expression ] ;

or-expression   = and-expression, { "or", and-expression } ;
and-expression  = not-expression, { "and", not-expression } ;
not-expression  = "not", not-expression | elementwise-or-expression ;

elementwise-or-expression  = elementwise-and-expression,
                             { "|", elementwise-and-expression } ;
elementwise-and-expression = membership-expression,
                             { "&", membership-expression } ;

membership-expression = range-expression, [ "in", range-expression ] ;
range-expression      = comparison-expression, [ "to", comparison-expression ] ;

comparison-expression = additive-expression,
                        [ comparison-operator, additive-expression ] ;
comparison-operator   = "==" | "!=" | ">" | "<" | ">=" | "<=" ;

additive-expression       = multiplicative-expression,
                            { ("+" | "-"), multiplicative-expression } ;
multiplicative-expression = unary-expression,
                            { ("*" | "/" | "%" | "@"), unary-expression } ;

unary-expression = ("+" | "-"), unary-expression | power-expression ;
power-expression = postfix-expression, [ "^", unary-expression ] ;

postfix-expression = primary-expression, { postfix-suffix } ;
postfix-suffix     = call-suffix
                   | index-suffix
                   | readonly-member-suffix
                   | tunnel-suffix ;

call-suffix       = "(", [ argument-list ], ")" ;
argument-list     = expression, { ",", expression }, [ "," ] ;

index-suffix        = "[", index-argument-list, "]" ;
index-argument-list = index-argument, { ",", index-argument } ;
index-argument      = expression | slice ;
slice               = [ or-expression ], ":", [ or-expression ] ;

readonly-member-suffix = "::", IDENTIFIER
                       | ".", IDENTIFIER  (* only when not followed by "(" *) ;
tunnel-suffix     = ".", IDENTIFIER, "(", [ argument-list ], ")" ;
```

Slice bounds use `or-expression` to exclude an unparenthesized pair colon.
Strings and lists require one index argument; arrays require two. These arities
are semantic checks.

### 7. Primary expressions and literals

```ebnf
primary-expression = INTEGER
                   | FLOAT
                   | STRING
                   | "true"
                   | "false"
                   | IDENTIFIER
                   | "this"
                   | "base"
                   | parenthesized-expression
                   | list-literal
                   | dictionary-literal
                   | function-literal
                   | rule-literal ;

parenthesized-expression = "(", expression, ")" ;
list-literal             = "[", [ argument-list ], "]" ;

dictionary-literal = "{", [ dictionary-entry,
                            { ",", dictionary-entry }, [ "," ] ], "}" ;
dictionary-entry   = or-expression, ":", expression ;

function-literal  = parameter-list, [ return-annotation ], block ;
parameter-list    = "(", [ fixed-parameters | "..." ], ")" ;
fixed-parameters  = parameter, { ",", parameter }, [ "," ] ;
parameter         = IDENTIFIER, [ ":", type-expression ] ;
return-annotation = "->", type-expression ;

rule-literal = "rule",
               [ "(", [ rule-parameters ], ")" ],
               rule-block ;
rule-parameters = rule-parameter,
                  { ",", rule-parameter }, [ "," ] ;
rule-parameter = IDENTIFIER, ":", type-expression ;
rule-block = "{", separators, [ rule-item-list ], separators, "}" ;
rule-item-list = rule-item, { separator-run, rule-item } ;
rule-item = rule-let-declaration
          | condition-statement
          | described-condition-statement
          | implication-statement ;
rule-let-declaration = "let", declarator, { ",", declarator } ;
condition-statement = expression ;
implication-statement = [ STRING, ":", separators ],
                        expression, "implies",
                        ( expression | condition-block ) ;
described-condition-statement = STRING, ":", separators,
                                ( expression | condition-block ) ;
condition-block = "{", separators, condition-statement,
                  { separator-run, condition-statement },
                  separators, "}" ;
```

Named declarations and function literals share parameter, result-annotation,
and body rules. A named declaration creates a read-only binding; a function
literal remains an ordinary expression. A Rule literal is also an ordinary
expression, but it uses a specialized body containing only Rule items;
Within a Rule body, an item beginning
with a String literal immediately followed by `:` is parsed as a described
Condition rather than as an ordinary Pair expression. While a block-requiring header is
waiting for `{`, the newline after that header is not a `SEP`; a newline after
an otherwise complete expression still separates statements.

`nil` is intentionally absent. `{}` in expression position is an empty
dictionary; a block occurs only where a compound statement or function expects
one.

### 8. Context-sensitive constraints

Plain EBNF cannot express these required constraints:

1. fixed parameter names are distinct;
2. declaration names are distinct within one scope;
3. `var` does not occur inside a control-flow block;
4. `break` and `continue` occur inside a loop in the same function;
5. `return` occurs inside a function or at module top level;
6. `elif` and `else` occur only in their `if-statement`;
7. an assignment target resolves to a declared, writable name;
8. a root built-in name cannot be assigned;
9. `this` and `base` are used only where their environments exist;
10. an import alias is valid and non-reserved;
11. comparison, range, and membership expressions contain at most one
    respective operator;
12. a value stored in a variable or collection must not be `nil`;
13. every Rule parameter requires a Type annotation, and parameter names are
    distinct;
14. the outermost level of a Rule body accepts only local `let` declarations,
    Bool/RuleInstance items, described items, and `implies`;
15. a Condition produces Bool, and a description is written directly as a
    String literal;
16. bare RuleInstance items retain child dependencies; bare Rule values are invalid items;
17. `implies` is a Rule item with a Bool or RuleInstance antecedent (including their union) and nonempty Bool consequents,
    not a general Bool operator. Consequent blocks reject declarations,
    bare instance obligations, and nested implications. See [Rule semantics](Rules_en.md#24-implication-items).

## Part IV — Default Operator Semantics for Built-in Types

### 1. General rules

This part is the reference for the default behavior of operators on Tapas
built-in types. Compilers, language servers, and other tools may use these
tables directly. The numeric Types are `Int | Float`; real arrays use
`RealArray`, and boolean arrays use `BoolArray`. Equal-shaped arrays have the
same row and column counts.

An operand combination not explicitly listed here is a runtime type error.
Tapas does not perform implicit truth conversion or broadcast between arrays of
different shapes. In an operation between a number and a real array, the number
is applied to every array element.

Some real-array operations must be performed by BLAS. Tapas does not link BLAS
while building or installing. It loads an LP64 CBLAS dynamic library from the
user's environment when the first such operation runs. If no compatible
library is available, that operation raises a runtime error; Tapas provides no
handwritten fallback for it.

### 2. Unary and arithmetic operators

| Operator | Valid operands | Result and behavior | Implementation requirement |
|---|---|---|---|
| unary `+`, `-` | `Int \| Float` | Preserves the numeric Type and produces the original or negated value. | Tapas built-in |
| unary `-` | `RealArray` | Negates each element and produces an equal-shaped `RealArray`. `RealArray` does not support unary `+`. | One fused pass |
| `+`, `-`, `*`, `/` | `Int \| Float`, `Int \| Float` | Two `Int` values produce `Int`; otherwise the result is `Float`. `Int` division truncates toward zero. | Tapas built-in |
| `+` | `Time`, `Int` | Moves an instant forward by whole seconds and produces a new `Time`. `Int + Time` is unsupported. | Tapas built-in and host C time type |
| `-` | `Time`, `Int` | Moves an instant backward by whole seconds and produces a new `Time`. | Tapas built-in and host C time type |
| `-` | `Time`, `Time` | Produces left minus right in seconds as `Float`. | host C `difftime` |
| `+`, `-`, `/` | `RealArray` and `Int \| Float`, in either order | Element-wise operation producing an equal-shaped `RealArray`. Subtraction and division preserve operand order. | Tapas loop |
| `*` | `RealArray` and `Int \| Float`, in either order | Scalar multiplication producing an equal-shaped `RealArray`. | One fused pass |
| `+`, `-` | two equal-shaped `RealArray` values | Adds or subtracts corresponding elements and produces an equal-shaped `RealArray`. | One fused pass |
| `*`, `/` | two equal-shaped `RealArray` values | Multiplies or divides corresponding elements and produces an equal-shaped `RealArray`. | Tapas loop |
| `%` | `Int \| Float`, `Int \| Float` | Two `Int` values produce an `Int` remainder; other combinations produce `Float`. | Tapas built-in |
| `%` | `RealArray` and `Int \| Float`, in either order | Computes an element-wise floating remainder, preserves operand order, and produces an equal-shaped `RealArray`. | Tapas loop and host C `fmod` |
| `%` | two equal-shaped `RealArray` values | Computes the floating remainder of corresponding elements and produces an equal-shaped `RealArray`. | Tapas loop and host C `fmod` |
| `^` | `Int \| Float`, `Int \| Float` | Exponentiation producing `Float`. | host C math library |
| `^` | `RealArray` and `Int \| Float`, in either order | Element-wise exponentiation producing an equal-shaped `RealArray`. | Tapas loop |
| `^` | two equal-shaped `RealArray` values | Exponentiates corresponding elements and produces an equal-shaped `RealArray`. | Tapas loop |
| `@` | two `RealArray` values | Matrix multiplication. The left column count must equal the right row count. | CBLAS `dgemm` |

Zero is an error as the divisor of integer division or integer remainder. When
either `%` operand is a real array, every element uses `fmod`: the result sign
follows the left operand, and floating zero divisors and other special cases
follow the host C math library.

### 3. Comparison operators

| Operator | Valid operands | Result and behavior |
|---|---|---|
| `==`, `!=` | two `Int \| Float` values | Numeric comparison, including mixed `Int` and `Float` operands, producing `Bool`. |
| `==`, `!=` | `RealArray` and `Int \| Float`, in either order | Element-wise comparison producing an equal-shaped `BoolArray`. |
| `==`, `!=` | two equal-shaped `RealArray` values | Element-wise comparison producing an equal-shaped `BoolArray`. |
| `==`, `!=` | an enum value and a String | String content comparison producing `Bool`; a direct String literal must be a member of that enum. |
| `==`, `!=` | two values of the same Type among `Bool`, `String`, `List`, `Pair`, `Iterator`, `BoolArray`, and `Time` | Content comparison producing one `Bool`. `List` and `Pair` compare their contents recursively. |
| `==`, `!=` | two `Dictionary`, `Function`, or `Library` values | Object-identity comparison producing `Bool`. |
| `==`, `!=` | other values of different types | Produces `false` or `true`, respectively. |
| `>`, `<`, `>=`, `<=` | two `Int \| Float` values | Numeric comparison producing `Bool`. |
| `>`, `<`, `>=`, `<=` | two `Time` values | Chronological comparison producing `Bool`. |
| `>`, `<`, `>=`, `<=` | `RealArray` and `Int \| Float`, in either order | Element-wise comparison producing an equal-shaped `BoolArray`. |
| `>`, `<`, `>=`, `<=` | two equal-shaped `RealArray` values | Element-wise comparison producing an equal-shaped `BoolArray`. |

Boolean arrays do not support ordering comparisons. Use `==` or `!=`, which
returns a scalar result, to test whether two boolean arrays have equal contents.
Rule and RuleInstance do not define content equality; use `identical` to test
whether two values refer to the same runtime object.

### 4. Logical, range, membership, and pair operators

| Operator | Valid operands | Result and behavior |
|---|---|---|
| `not` | `Bool`; additionally `RuleInstance` inside a Rule | Produces Bool, evaluating its operand once; instances are evaluated during checking. |
| `and`, `or` | `Bool`; additionally `RuleInstance` inside a Rule | Produces `Bool` and uses short-circuit evaluation for the right operand. |
| `&`, `\|` | `Bool`, `Bool` | Produces `Bool`. Both operands are evaluated; these operators do not short-circuit. |
| `&`, `\|` | `BoolArray` and `Bool`, in either order | Combines the `Bool` with every array element and produces an equal-shaped `BoolArray`. |
| `&`, `\|` | two equal-shaped `BoolArray` values | Computes logical AND or OR for corresponding elements and produces an equal-shaped `BoolArray`. |
| `to` | `Int`, `Int` | Creates a step-`1` half-open `Iterator` that excludes its end. |
| `in` | any value and an iterator | Tests iterator membership. |
| `in` | any value and a list | Searches using built-in content-equality rules. |
| `in` | any value and a dictionary | Searches dictionary keys. |
| `in` | any other right operand | Produces `false`; arrays currently provide no membership test. |
| `:` | any two values | Creates a `Pair` containing the left and right values. |

`&` and `|` evaluate both operands and use a Tapas loop for
boolean arrays. `and` and `or` remain scalar-only and retain short-circuit
semantics.

### 5. Call, index, and member operations

| Form | Default behavior |
|---|---|
| `value(arguments)` | Calls a user or built-in function; calling a Rule binds its arguments and produces a RuleInstance. Other values are not callable. |
| `value[index]` | Indexes a string, list, pair, or dictionary using the index types defined in Part I. |
| `array[row, column]` | Indexes an array with two integers or slices; two integers produce a scalar, while any slice produces an array. |
| `value::name` | Reads the string key `name` as a read-only dictionary or library member. |
| `value.name` | Equivalent to `value::name` when it is not followed by `(`. |
| `receiver.name(arguments)` | Tunnel call equivalent to `name(receiver, arguments)`. |

Arguments and index expressions are evaluated from left to right. An invalid
index, missing member, or attempt to call a non-callable value is a runtime
error.

### 6. BLAS runtime contract

Tapas accepts an LP64 CBLAS dynamic library that provides `cblas_dcopy`,
`cblas_daxpy`, `cblas_dscal`, `cblas_ddot`, `cblas_dnrm2`, and
`cblas_dgemm`. Calls use row-major array layout. ILP64 interfaces and libraries
that expose only Fortran BLAS symbols
are outside the current contract.

When `TAPAS_BLAS_LIBRARY` is set, Tapas tries only the dynamic library path or
loader-recognized name stored in that variable. Otherwise it searches common
library names for the current operating system. The result is cached for the
remainder of the process.

This version intentionally provides no language function such as
`blas_available` or `blas_backend`. Programs may use every BLAS-independent
feature normally. A missing compatible backend becomes an error only when a
BLAS-required operation actually executes.

### 7. Array operator summary

| Capability | Real array | Boolean array |
|---|---|---|
| Two-dimensional integer indexing, slicing, and assignment | supported | supported |
| unary `+` | unsupported | unsupported |
| unary `-` | element-wise; requires CBLAS | unsupported |
| `+`, `-` with an equal-shaped array | element-wise; requires CBLAS | unsupported |
| `*` with a number | scalar multiplication; requires CBLAS | unsupported |
| other `+`, `-`, `*`, `/`, `%`, `^` forms | element-wise with a number or equal-shaped real array | unsupported |
| `@` | matrix multiplication for compatible dimensions; requires CBLAS | unsupported |
| `==`, `!=` | element-wise, producing `BoolArray` | compares complete contents, producing `Bool` |
| `>`, `<`, `>=`, `<=` | element-wise with a number or equal-shaped real array | unsupported |
| `&`, `\|` | unsupported | element-wise with `Bool` or an equal-shaped `BoolArray` |
| `not`, `and`, `or`, `in` | unsupported | unsupported |

An invalid shape or unsupported operand type is a runtime error. The following
program checks the default real-array operator behavior:

```tapas
let operator_demo_matrix = array(2, 3, [1, 2, 3, 4, 5, 6])
pprint(-operator_demo_matrix)
pprint(operator_demo_matrix + 10)
pprint(10 - operator_demo_matrix)
pprint(operator_demo_matrix * 2)
pprint(operator_demo_matrix * operator_demo_matrix)
pprint(operator_demo_matrix % 4)
pprint(10 % operator_demo_matrix)
pprint(operator_demo_matrix % array(2, 3, [2, 2, 2, 3, 3, 3]))
pprint(operator_demo_matrix > 2)
pprint(operator_demo_matrix @ dense::transpose(operator_demo_matrix))
```
<pre class='Tapas-Return'>
[[-1, -2, -3],
 [-4, -5, -6]]
[[11, 12, 13],
 [14, 15, 16]]
[[9, 8, 7],
 [6, 5, 4]]
[[2, 4, 6],
 [8, 10, 12]]
[[1, 4, 9],
 [16, 25, 36]]
[[1, 2, 3],
 [0, 1, 2]]
[[0, 0, 1],
 [2, 0, 4]]
[[1, 0, 1],
 [1, 2, 0]]
[[false, false, true],
 [true, true, true]]
[[14, 32],
 [32, 77]]
</pre>

The following program checks `&` and `|` precedence and boolean-array behavior:

```tapas
let operator_demo_flags = array(1, 3, [true, false, true])
let operator_demo_mask = array(1, 3, [false, true, true])
print(true | false & false)
print(true | false and false)
pprint(operator_demo_flags & true)
pprint(false | operator_demo_flags)
pprint(operator_demo_flags & operator_demo_mask)
pprint(operator_demo_flags | operator_demo_mask)
```
<pre class='Tapas-Return'>
true
false
[[true, false, true]]
[[true, false, true]]
[[false, false, true]]
[[true, true, true]]
</pre>
