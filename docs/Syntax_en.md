# The Tapas Programming Language

[简体中文](Syntax_zh.md) | English | [Project Home](../README_en.md)

This document is the normative language specification for Tapas. It defines
the source language independently of the current compiler implementation. A
conforming compiler, formatter, and language server must follow these rules.

The document has five parts:

1. a tutorial introduction with the language semantics;
2. lexical rules;
3. a complete syntactic grammar in EBNF;
4. the built-in function catalogue;
5. default operator semantics for built-in types.

The words **must**, **must not**, **should**, and **may** are normative. Every
code block marked `tapas` is executable. The Tapas binary reads those blocks in
document order as one program, so a block may use declarations from an earlier
`tapas` block in the same document. Code marked `text` is only a notation
example and may intentionally be incomplete or invalid Tapas.

## Part I — Tutorial and Language Semantics

### 1. A first program

Tapas is a dynamically typed, expression-oriented scripting language. A source
file contains statements separated by a newline or semicolon. Newlines inside
parentheses, brackets, braces, or strings do not separate statements.

```tapas
var tutorial_limit = 5

let tutorial_square = (x){
    return x * x
}

for(let tutorial_i in 0 to tutorial_limit){
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
capture rules. Function literals are written `(parameters){ body }`. `0 to 5`
creates the half-open iterator `0, 1, 2, 3, 4`.

### 2. Statements and separators

A newline or `;` terminates a simple statement at delimiter depth zero. Empty
statements are ignored. These are equivalent:

```text
let a = 1
let b = 2

let a = 1; let b = 2
```

A compound statement owns a braced block and does not need a semicolon after
its closing brace. A separator may appear between the arms of an `if` chain.

Comments begin with `//` outside a string and continue through the end of the
physical line. A comment ends the statement text on that line; an expression
cannot continue after a line comment unless it is already inside delimiters.

### 3. Values and dynamic types

Every Tapas expression evaluates to one value. Variables do not have declared
types, and a later assignment may store a value of a different type.

Tapas has four directly stored value types:

| Type | Literal examples | Notes |
|---|---|---|
| `bool` | `true`, `false` | No implicit conversion in conditions. |
| `int` | `0`, `-12`, `42` | Signed integer represented by runtime C `long`. |
| `float` | `0.5`, `.5`, `5.`, `1e3` | Runtime C `double`. |
| `nil` | none | Internal absence value; there is no `nil` literal. |

Composite values are heap objects:

| Type | Construction |
|---|---|
| String | `'text'`, `"text"` |
| List | `[1, 2, 3]`, `list(1, 2, 3)` |
| Pair | `'key' : 1`, `pair('key', 1)` |
| Dictionary | `{'key' : 1}` |
| Iterator | `0 to 10`, `iter(0, 10)` |
| Function | `(x){ return x }` |
| Library | `import module.tap as module` |
| Real array | `array(2, 2, 0.0)` |
| Boolean array | `array(2, 2, false)` |
| Time | `now()`, `time::from_unix(0)` |

`Time` represents an absolute instant with whole-second precision. Its default
text representation uses the local time zone of the running environment; the
time zone is not stored in the `Time` value itself.

```tapas
let tutorial_epoch_time = time::from_unix(100)
let tutorial_later_time = tutorial_epoch_time + 20
print(time::unix(tutorial_later_time))
print(tutorial_later_time - tutorial_epoch_time)
print(tutorial_epoch_time < tutorial_later_time)
print(len(time::format(tutorial_epoch_time, '%Y')) > 0)
```
<pre class='Tapas-Return'>
120
20
true
true
</pre>

`nil` is used internally as the result of procedures such as `print`. A
declaration or assignment must not store `nil`; attempting to do so is a
runtime error. A bare `return` is allowed and returns `nil` to its caller.

Integer literals use strict decimal spelling: `0` may stand alone, while every
other integer starts with `1` through `9`. Thus `10` is valid, while `00`,
`010`, `0x10`, `0b10`, and `0o10` are compile errors. Floating literals include
forms such as `.5`, `5.`, `1e3`, and `1.5e-3`.

Declarations in the current normative grammar do not include type annotations.

### 4. Variables, scope, and assignment

Every declaration has an initializer. Multiple declarations may share one
statement:

```tapas
var tutorial_origin = 0, tutorial_step = 1
let tutorial_message = 'hello'
tutorial_message = 42
```

An identifier must be declared before it is read or assigned. Two declarations
must not introduce the same name in the same scope, including one `var` and one
`let` declaration. A nested function may shadow a name from an outer function.
Built-ins in the root environment cannot be reassigned.

`var` creates an **environment variable**. Its scope begins after its
declarator and extends to the end of the current module or function body.
Nested functions capture it by reference. A `var` declaration is not permitted
inside an `if`, `elif`, `else`, `for`, or `while` block; declare it in
the surrounding function or module instead.

`let` creates a **temporary variable**. Its scope begins after its declarator
and extends to the end of the innermost current block. It is removed when that
block exits and cannot be captured by a nested function.

Function parameters behave like environment variables belonging to the
function call and may be captured by a nested function.

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

Assigning or passing an `int`, `float`, or `bool` copies the value. Assigning,
passing, or inserting a composite value copies its reference. Mutating the
object is therefore visible through all aliases.

```tapas
let tutorial_inner = [1, 2]
let tutorial_outer = [tutorial_inner]
tutorial_inner[0] = 9
sprint(tutorial_outer)
```
<pre class='Tapas-Return'>
[[9, 2]]
</pre>

`copy(value)` creates an independent outer composite object, but elements held
inside a copied list, pair, or dictionary are still copied shallowly. Use
`identical(a, b)` to test runtime identity. The `==` operator follows the
value-specific equality operation and is not a substitute for identity testing.

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
sprint(tutorial_numbers[:3])
sprint(tutorial_numbers[-2:])
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

### 7. Dense arrays

Tapas arrays are row-major and two-dimensional. `array(rows, columns, fill)`
creates a real array when `fill` is an `int` or `float`, and a boolean array
when `fill` is a `bool`. A list fill must contain exactly
`rows * columns` values of one appropriate scalar category.

Arrays require two indices. Each index is an integer or slice. Two integer
indices return a scalar; if either index is a slice, the result is a new array.

```tapas
let tutorial_matrix = array(2, 3, [1, 2, 3, 4, 5, 6])
print(tutorial_matrix[1, 2])
sprint(tutorial_matrix[0:2, 1:3])
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
| 13 | call `()`, index `[]`, member `.`/`::` | left, chainable |
| 12 | `^` | right-associative |
| 11 | unary `+`, unary `-` | right-associative |
| 10 | `*`, `/`, `%`, `@` | left |
| 9 | `+`, `-` | left |
| 8 | `==`, `!=`, `>`, `<`, `>=`, `<=` | non-associative |
| 7 | `to` | non-associative |
| 6 | `in` | non-associative |
| 5 | `&` | left, non-short-circuiting |
| 4 | `\|` | left, non-short-circuiting |
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

`and` and `or` require booleans and short-circuit. They do not apply truthiness
conversion. Conditions in `if` and `while` likewise require an actual `bool`.

`start to end` requires integers and creates a half-open iterator with step
`1`; it is empty when `end <= start`. Use `iter(start, step, end)` for a
descending range. `value in collection` supports iterators, lists, and
dictionary keys; for another right operand it is `false`.

### 10. Functions and closures

A fixed-arity function lists zero or more distinct parameter names:

```tapas
let tutorial_hypotenuse = (x, y){
    return math::sqrt(x * x + y * y)
}
print(tutorial_hypotenuse(3, 4))
```
<pre class='Tapas-Return'>
5
</pre>

Calling it with a different number of arguments is a runtime error. A variadic
function uses `...` as its entire parameter list. It reads arguments with
`__nparam__()` and `__param__(index)`.

Functions are anonymous. Inside a function, `this` evaluates to a callable copy
of the current function and is the standard recursion mechanism. `base`
evaluates to a copy of the parent function environment. `this` and `base` are
only valid where the corresponding environment exists.

```tapas
let tutorial_factorial = (n){
    if(n <= 1){
        return 1
    }
    return n * this(n - 1)
}
print(tutorial_factorial(6))
```
<pre class='Tapas-Return'>
720
</pre>

### 11. Conditional execution and loops

An `if` chain evaluates conditions in order and executes at most one arm:

```tapas
let tutorial_sign = (x){
    if(x < 0){
        return -1
    }
    elif(x > 0){
        return 1
    }
    else{
        return 0
    }
}
```

An `elif` or `else` must belong to the immediately preceding `if` chain.
There may be any number of `elif` arms and at most one final `else` arm.

`while(condition){ body }` repeats while its boolean condition is true.

A `for` loop accepts either a newly declared temporary loop variable or an
existing assignable variable:

```text
for(let item in iterable){ body }
for(item in iterable){ body }
```

The iterable must be an iterator, list, or another extension value that
explicitly supports iteration. Dictionaries are not directly iterable; use
`keys(dictionary)` or `dvalues(dictionary)`. The `let` loop variable exists
only in the loop. An existing loop variable remains visible after the loop.

`break` and `continue` are valid only inside the nearest lexically enclosing
loop and cannot cross a function boundary. `return` is valid only inside a
function or at module top level. A return inside a nested control-flow block
returns from its enclosing function or module.

### 12. Modules and imports

Each `.tap` file is a module. A `.md` file is also a module whose Tapas source
is the concatenation of fenced blocks tagged `tapas` or `tap`, with original
line positions preserved for diagnostics.

```text
import path/to/module.tap
import path/to/module.tap as module_name
import 'path with spaces/module.tap' as module_name
```

A bare import executes the module for its effects. An aliased import binds a
`Library` value. A module exposes named values by returning a dictionary from
its top level:

```text
var exported_function = (){ return 5 }
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

### 13. Errors and compatibility forms

A language server should distinguish lexical, syntactic, name-resolution, and
runtime errors. It should recover after a lexical or syntactic error and report
as many independent diagnostics as practical.

#### 13.1 Deprecated legacy forms

A deprecated legacy form is syntax that belonged to an earlier version of the
language, remains recognizable only for migration, and is intended to be
removed. This specification currently defines no forms in that category.

#### 13.2 Provisional compatibility forms

A provisional compatibility form is not part of the current normative grammar,
but the current compiler accepts it without an error. It is kept because a
future language version may give it formal semantics. It must not be described
as deprecated or as legacy syntax.

| Form | Current compatibility behavior | Possible future direction |
|---|---|---|
| `var name: Type = value` and `let name: Type = value` | The declaration is accepted, but `Type` is not checked and has no runtime effect. | Type annotations may become part of the type system. |
| `function(parameters){ body }` | The declaration is accepted with the same behavior as `(parameters){ body }`. | The `function` keyword may become a formal function-literal spelling. |
| `#{ expression }` | Creates an anonymous function that accepts any number of arguments and returns the result of `expression` when called. It approximates `(...){ return expression }`, but its body is limited to one expression. | This shorthand may become normative syntax or be deprecated. |

Compilers and language servers should parse these forms without an error. A
tool may show a non-error informational hint explaining that the syntax is a
compatibility form in the current version. Formatters should preserve it
rather than silently rewrite it. The EBNF in Part III continues to describe
only the current normative grammar.

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
let nil of or return this to true var while
```

`function`, `nil`, and `of` are reserved although they do not begin a current
expression. A keyword is recognized only when the following character is not
an identifier continuation character.

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
Multi-character:  ==  !=  >=  <=  ::  ...  //
Single-character: + - * / % @ ^ & | > < = : . , ; ( ) [ ] { }
Word operators:   and or in to
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
SEP        = top-level newline | ";" ;
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
declaration       = ("var" | "let"), declarator, { ",", declarator } ;
declarator        = IDENTIFIER, "=", expression ;
assignment        = assignment-target, "=", expression ;
assignment-target = IDENTIFIER, [ index-suffix ] ;
```

Declaration location, duplicate-name, `nil`, and assignability restrictions
are semantic constraints from Part I.

### 4. Control flow

```ebnf
if-statement    = "if", "(", expression, ")", block,
                  { separators, "elif", "(", expression, ")", block },
                  [ separators, "else", block ] ;

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
and-expression  = elementwise-or-expression,
                  { "and", elementwise-or-expression } ;

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
                   | function-literal ;

parenthesized-expression = "(", expression, ")" ;
list-literal             = "[", [ argument-list ], "]" ;

dictionary-literal = "{", [ dictionary-entry,
                            { ",", dictionary-entry }, [ "," ] ], "}" ;
dictionary-entry   = or-expression, ":", expression ;

function-literal = parameter-list, block ;
parameter-list   = "(", [ fixed-parameters | "..." ], ")" ;
fixed-parameters = IDENTIFIER, { ",", IDENTIFIER }, [ "," ] ;
```

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
7. an assignment target resolves to a declared, mutable name;
8. a root built-in name cannot be assigned;
9. `this` and `base` are used only where their environments exist;
10. an import alias is valid and non-reserved;
11. comparison, range, and membership expressions contain at most one
    respective operator;
12. a value stored in a variable or collection must not be `nil`.

## Part IV — Built-in Functions

### 1. Reading the signatures

`A | B` means either runtime type. `Any` means any Tapas value.
`...Any` means zero or more arguments. A `nil` return denotes a procedure
whose result cannot be stored.

Every root function can be used as a tunnel call when its first parameter is
the receiver. For example, `append(items, value)` and
`items.append(value)` are equivalent.

### 2. Output, inspection, and time

| Signature | Returns | Behavior |
|---|---|---|
| `print(...Any)` | `nil` | Prints abbreviated representations, then LF. |
| `sprint(...Any)` | `nil` | Prints full representations, then LF. |
| `len(value: Any)` | `int` | Composite length; 0 for `nil`; 1 for other scalars. |
| `type(value: Any)` | `String` | Runtime type name. |
| `copy(value: Any)` | same category | Scalar copy or shallow composite copy. |
| `identical(a: Any, b: Any)` | `bool` | Runtime identity/value identity. |
| `clock()` | `float` | Process CPU time in seconds. |
| `now()` | `Time` | Absolute instant at the time of the call. Default display uses local time. |

### 3. Conversion and construction

| Signature | Returns | Behavior |
|---|---|---|
| `int(value: bool | int | float | String)` | `int` | Numeric conversion; string is complete base-10 input. |
| `float(value: bool | int | float | String)` | `float` | Numeric conversion. |
| `bool(value: Any)` | `bool` | Explicit truth conversion. |
| `str(value: Any)` | `String` | Full textual representation. |
| `list(...Any)` | `List` | New list containing the arguments. |
| `pair(first: Any, second: Any)` | `Pair` | New pair. |
| `iter(start: int, end: int)` | `Iterator` | Half-open range with inferred step. |
| `iter(start: int, step: int, end: int)` | `Iterator` | Half-open range with explicit nonzero step. |
| `array(rows: int, cols: int, fill: bool | int | float | List)` | `Array` | New dense array. |

Array dimensions are non-negative. An explicit iterator step of zero is an
error.

### 4. Collection operations

| Signature | Returns | Mutation and result |
|---|---|---|
| `push(list: List, value: Any)` | `nil` | Appends `value`. |
| `append(target: String | List | Dictionary, value: Any)` | `nil` | Appends text, an item, or a pair. |
| `insert(list: List, value: Any, index: int)` | `nil` | Inserts before `index`. |
| `pop(list: List)` | `nil` | Removes the last item. |
| `pop(list: List, index: int)` | `nil` | Removes the indexed item. |
| `delete(target: List | Dictionary, index_or_key: Any)` | `nil` | Deletes an item. |
| `idx(target: String | List | Pair | Dictionary, index: Any)` | `Any` | One-argument indexing. |
| `keys(dict: Dictionary)` | `List` | Keys in unspecified order. |
| `dkeys(dict: Dictionary)` | `List` | Alias of `keys`. |
| `dvalues(dict: Dictionary)` | `List` | Values in corresponding order. |
| `union(left: List, right: List)` | `List` | New shallow concatenation. |
| `sort(list: List)` | `nil` | Sorts in place using the runtime total order. |

`append(dictionary, value)` requires a `Pair`. Negative list indices count
from the end where an operation accepts them. Collections cannot own `nil`.

### 5. Session functions

| Signature | Returns | Context |
|---|---|---|
| `__ls__()` | `List` | Names in the current root library. |
| `__ls__(library: Library)` | `List` | Names in `library`. |
| `__path__()` | `List` | Current library search paths. |
| `__path__(library: Library)` | `List` | Search paths of `library`. |
| `__param__(index: int)` | `Any` | Argument of the current variadic call. |
| `__nparam__()` | `int` | Argument count of the current variadic call. |
| `__binary__()` | `nil` | Prints current bytecode. |
| `__binary__(value: Library | Function)` | `nil` | Prints bytecode for `value`. |

Names beginning with `__` are implementation-reserved.


### 6. Time package `time`

| Signature | Returns | Behavior |
|---|---|---|
| `time::from_unix(seconds: int)` | `Time` | Creates an instant from whole seconds since the Unix epoch. |
| `time::unix(value: Time)` | `int` | Returns whole Unix seconds for an instant. |
| `time::format(value: Time, pattern: String)` | `String` | Formats in local time using the host C `strftime` pattern. |

Timestamps and offsets must fit both the host `time_t` range and the Tapas
`int` range. Supported formatting conversions are defined by the host C
library. This version stores no time-zone information and provides neither UTC
formatting nor date parsing.

### 7. Scalar mathematics package `math`

Unless stated otherwise, `number` means `int | float`; functions accept
numbers and return `float`. Domain, overflow, infinity, and NaN behavior
follows the host C math library.

| Group | Signatures |
|---|---|
| Absolute and roots | `abs(number) -> int | float`, `fabs(number) -> float`, `sqrt(number) -> float`, `rsqrt(number) -> float`, `cbrt(number) -> float` |
| Power and geometry | `pow(number, number) -> float`, `hypot(number, number) -> float` |
| Trigonometric | `sin`, `cos`, `tan`, `asin`, `acos`, `atan`: `(number) -> float`; `atan2(number, number) -> float` |
| Hyperbolic | `sinh`, `cosh`, `tanh`, `asinh`, `acosh`, `atanh`: `(number) -> float` |
| Exponential | `exp`, `exp2`, `expm1`: `(number) -> float` |
| Logarithmic | `log`, `log2`, `log10`, `log1p`, `logb`: `(number) -> float`; `ilogb(number) -> int` |
| Decomposition | `frexp(number) -> Pair(float, int)`, `modf(number) -> Pair(float, float)` |
| Scaling | `ldexp(number, number) -> float`, `scalbn(number, number) -> float`, `scalbln(number, number) -> float` |
| Error and gamma | `erf`, `erfc`, `lgamma`, `tgamma`: `(number) -> float` |
| Rounding to float | `ceil`, `floor`, `nearbyint`, `rint`, `round`, `trunc`: `(number) -> float` |
| Rounding to int | `lrint`, `llrint`, `lround`, `llround`: `(number) -> int` |
| Remainder | `fmod(number, number) -> float`, `remainder(number, number) -> float`, `remquo(number, number) -> Pair(float, int)` |
| Floating manipulation | `copysign`, `nextafter`, `fdim`, `fmax`, `fmin`: `(number, number) -> float`; `fma(number, number, number) -> float` |
| Reciprocal and NaN | `eleinv(number) -> float`, `make_nan() -> float` |
| Classification | `isfinite`, `isinf`, `isnan`, `isnormal`, `signbit`: `(number) -> bool`; `fpclassify(number) -> int` |
| Ordered predicates | `isgreater`, `isgreaterequal`, `isless`, `islessequal`, `islessgreater`, `isunordered`: `(number, number) -> bool` |

Every name in this table is accessed through `math::`, for example
`math::sqrt(2)`. Scaling-function integer arguments convert floats toward
zero.

### 8. Dense-array package `dense`

| Signature                        | Returns | Behavior              |
| -------------------------------- | ------- | --------------------- |
| `dense::new(rows, cols, fill)`   | `Array` | Alias of `array`.     |
| `dense::rows(array: Array)`      | `int`   | Row count.            |
| `dense::cols(array: Array)`      | `int`   | Column count.         |
| `dense::transpose(array: Array)` | `Array` | New transposed array. |

```tapas
let builtin_dense_matrix = dense::new(2, 3, [1, 2, 3, 4, 5, 6])
print(dense::rows(builtin_dense_matrix), ' x ', dense::cols(builtin_dense_matrix))
sprint(dense::transpose(builtin_dense_matrix))
```

<pre class='Tapas-Return'>
2 x 3
[[1, 4],
 [2, 5],
 [3, 6]]
</pre>


###

## Part V — Default Operator Semantics for Built-in Types

### 1. General rules

This part is the reference for the default behavior of operators on Tapas
built-in types. Compilers, language servers, and other tools may use these
tables directly. `number` means `int | float`; a real array is an `Array` whose
elements are numeric, and a boolean array is an `Array` whose elements are
booleans. Equal-shaped arrays have the same row and column counts.

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
| unary `+`, `-` | `number` | Preserves the numeric type and produces the original or negated value. | Tapas built-in |
| unary `-` | real array | Negates each element and produces an equal-shaped real array. Real arrays do not support unary `+`. | CBLAS `dcopy`, `dscal` |
| `+`, `-`, `*`, `/` | `number`, `number` | Two integers produce an integer; otherwise the result is a float. Integer division truncates toward zero. | Tapas built-in |
| `+` | `Time`, `int` | Moves an instant forward by whole seconds and produces a new `Time`. `int + Time` is unsupported. | Tapas built-in and host C time type |
| `-` | `Time`, `int` | Moves an instant backward by whole seconds and produces a new `Time`. | Tapas built-in and host C time type |
| `-` | `Time`, `Time` | Produces left minus right in seconds as a `float`. | host C `difftime` |
| `+`, `-`, `/` | real array and `number`, in either order | Element-wise operation producing an equal-shaped real array. Subtraction and division preserve operand order. | Tapas loop |
| `*` | real array and `number`, in either order | Scalar multiplication producing an equal-shaped real array. | CBLAS `dcopy`, `dscal` |
| `+`, `-` | two equal-shaped real arrays | Adds or subtracts corresponding elements and produces an equal-shaped real array. | CBLAS `dcopy`, `daxpy` |
| `*`, `/` | two equal-shaped real arrays | Multiplies or divides corresponding elements and produces an equal-shaped real array. | Tapas loop |
| `%` | `number`, `number` | Two integers produce an integer remainder; other numeric combinations produce a floating remainder. | Tapas built-in |
| `%` | real array and `number`, in either order | Computes an element-wise floating remainder, preserves operand order, and produces an equal-shaped real array. | Tapas loop and host C `fmod` |
| `%` | two equal-shaped real arrays | Computes the floating remainder of corresponding elements and produces an equal-shaped real array. | Tapas loop and host C `fmod` |
| `^` | `number`, `number` | Exponentiation producing a float. | host C math library |
| `^` | real array and `number`, in either order | Element-wise exponentiation producing an equal-shaped real array. | Tapas loop |
| `^` | two equal-shaped real arrays | Exponentiates corresponding elements and produces an equal-shaped real array. | Tapas loop |
| `@` | two real arrays | Matrix multiplication. The left column count must equal the right row count. | CBLAS `dgemm` |

Zero is an error as the divisor of integer division or integer remainder. When
either `%` operand is a real array, every element uses `fmod`: the result sign
follows the left operand, and floating zero divisors and other special cases
follow the host C math library.

### 3. Comparison operators

| Operator | Valid operands | Result and behavior |
|---|---|---|
| `==`, `!=` | two numbers | Numeric comparison, including mixed integer and float operands, producing `bool`. |
| `==`, `!=` | real array and number, in either order | Element-wise comparison producing an equal-shaped boolean array. |
| `==`, `!=` | two equal-shaped real arrays | Element-wise comparison producing an equal-shaped boolean array. |
| `==`, `!=` | two values of the same type among booleans, strings, lists, pairs, iterators, boolean arrays, and times | Content comparison producing one `bool`. Lists and pairs compare their contents recursively. |
| `==`, `!=` | two dictionaries, functions, or libraries | Object-identity comparison producing `bool`. |
| `==`, `!=` | other values of different types | Produces `false` or `true`, respectively. |
| `>`, `<`, `>=`, `<=` | two numbers | Numeric comparison producing `bool`. |
| `>`, `<`, `>=`, `<=` | two times | Chronological comparison producing `bool`. |
| `>`, `<`, `>=`, `<=` | real array and number, in either order | Element-wise comparison producing an equal-shaped boolean array. |
| `>`, `<`, `>=`, `<=` | two equal-shaped real arrays | Element-wise comparison producing an equal-shaped boolean array. |

Boolean arrays do not support ordering comparisons. Use `==` or `!=`, which
returns a scalar result, to test whether two boolean arrays have equal contents.

### 4. Logical, range, membership, and pair operators

| Operator | Valid operands | Result and behavior |
|---|---|---|
| `and`, `or` | `bool`, `bool` | Produces `bool` and uses short-circuit evaluation for the right operand. |
| `&`, `\|` | `bool`, `bool` | Produces `bool`. Both operands are evaluated; these operators do not short-circuit. |
| `&`, `\|` | boolean array and `bool`, in either order | Combines the boolean with every array element and produces an equal-shaped boolean array. |
| `&`, `\|` | two equal-shaped boolean arrays | Computes logical AND or OR for corresponding elements and produces an equal-shaped boolean array. |
| `to` | `int`, `int` | Creates a step-`1` half-open iterator that excludes its end. |
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
| `value(arguments)` | Calls a user or built-in function; other values are not callable. |
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
`cblas_daxpy`, `cblas_dscal`, and `cblas_dgemm`. Calls use row-major array
layout. ILP64 interfaces and libraries that expose only Fortran BLAS symbols
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
| `==`, `!=` | element-wise, producing a boolean array | compares complete contents, producing `bool` |
| `>`, `<`, `>=`, `<=` | element-wise with a number or equal-shaped real array | unsupported |
| `&`, `\|` | unsupported | element-wise with `bool` or an equal-shaped boolean array |
| `and`, `or`, `in` | unsupported | unsupported |

An invalid shape or unsupported operand type is a runtime error. The following
program checks the default real-array operator behavior:

```tapas
let operator_demo_matrix = array(2, 3, [1, 2, 3, 4, 5, 6])
sprint(-operator_demo_matrix)
sprint(operator_demo_matrix + 10)
sprint(10 - operator_demo_matrix)
sprint(operator_demo_matrix * 2)
sprint(operator_demo_matrix * operator_demo_matrix)
sprint(operator_demo_matrix % 4)
sprint(10 % operator_demo_matrix)
sprint(operator_demo_matrix % array(2, 3, [2, 2, 2, 3, 3, 3]))
sprint(operator_demo_matrix > 2)
sprint(operator_demo_matrix @ dense::transpose(operator_demo_matrix))
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
sprint(operator_demo_flags & true)
sprint(false | operator_demo_flags)
sprint(operator_demo_flags & operator_demo_mask)
sprint(operator_demo_flags | operator_demo_mask)
```
<pre class='Tapas-Return'>
true
false
[[true, false, true]]
[[true, false, true]]
[[false, false, true]]
[[true, true, true]]
</pre>
