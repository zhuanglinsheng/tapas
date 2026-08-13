# The Tapas Programming Language

This document describes the syntax, core semantics, and built-in facilities of
Tapas. It is written as a language reference: each `tapas` code block is meant
to be accepted by Tapas.

Tapas is an expression-oriented scripting language with explicit variable
declarations, first-class functions, shallow-copy composite values, half-open
iterators, and module imports.

## Quick Syntax

```text
var x = 1
let y = 2

let abs = (x){
    if(x >= 0){
        return x
    }
    return -x
}

for(let i in 0 to 5){
    print(i)
}

let values = [1, 2, 3]
let person = {'name' : 'Tony', 'age' : 20}

import examples/Sort.md as sort_lib
```

Common rules:

- `var` declares environment variables. They live in the current environment and
  can be captured by nested functions.
- `let` declares temporary variables. They are local to the current block and are
  not captured by nested functions.
- Every declared variable must have an initializer.
- `nil` is internal only and cannot be bound to variables.
- `start to end` and `iter(...)` create half-open ranges.
- Slices use half-open ranges and can omit either endpoint: `xs[:end]`,
  `xs[start:]`, and `xs[:]`.
- `a.f(b, c)` is exactly equivalent to `f(a, b, c)`.



## Type System

### Value Types

Generally, there are two kinds of values in Tapas: value types and composite
types.

Value types contain their data directly. Composite types, such as strings,
lists, dictionaries, arrays, functions, libraries, and time values, are
reference values and follow Tapas's shallow-copy rule unless explicitly copied.

#### Nil

A value of nil means null, empty or nothing. Nil is an internal value only: users cannot create a nil literal, bind nil to a variable, or assign nil to an existing variable. Nil may still appear transiently as the return value of functions such as `print`.

```tapas
print('The return of function "print" is a nil
whose type code is:').type()
```
<pre class='Tapas-Return'>
The return of function "print" is a nil
whose type code is:
nil
</pre>



#### Boolean

A boolean value is either `true` or `false`. Boolean literals are written as
`true` and `false`.

```tapas
true
false
```
<pre class='Tapas-Return'>
true
false
</pre>
Boolean values can also be produced by logical operations:

```tapas
print('true and false  = ', true and false)
print('true or  false  = ', true or  false)

print('true  == true   = ', true == true)
print('true  == false  = ', true == false)
print('false == true   = ', false == true)
print('false == false  = ', false == false)
```
<pre class='Tapas-Return'>
true and false  = false
true or  false  = true
true  == true   = true
true  == false  = false
false == true   = false
false == false  = true
</pre>

The logical operators `and` and `or` are strictly short-circuiting. For
`left and right`, Tapas evaluates `right` only when `left` is `true`. For
`left or right`, Tapas evaluates `right` only when `left` is `false`. Both
operands must be boolean values when they are evaluated.

```tapas
let guard = [1]
false and guard[2] > 0
true or guard[2] > 0
```
<pre class='Tapas-Return'>
false
true
</pre>


#### Integer

Integer values can be obtained from integer literals:

```tapas
1; 2; 3
```
<pre class='Tapas-Return'>
1
2
3
</pre>
or by integer operations.

```tapas
2 / 3
```
<pre class='Tapas-Return'>
0
</pre>

Example: Logical operations on integers

```tapas
print('1 == 1        ', 1 == 1)
print('1 == 2        ', 1 == 2)
print('2 == 1        ', 2 == 1)
print('2 == 2        ', 2 == 2)
```
<pre class='Tapas-Return'>
1 == 1        true
1 == 2        false
2 == 1        false
2 == 2        true
</pre>

Boolean values can be compared with integer values:

```tapas
print('true == 1     ', true == 1)
print('false == 0    ', false == 0)
```
<pre class='Tapas-Return'>
true == 1     false
false == 0    false
</pre>


#### Float

Float values can be obtained from float literals or numerical operations.

```tapas
print('2 / 3.0 = ', 2 / 3.0)
```
<pre class='Tapas-Return'>
2 / 3.0 = 0.666667
</pre>

Example: Logical operations on double floats

```tapas
print('2.0 == 2.0     ', 2.0 == 2.0)
print('2   == 2.0     ', 2   == 2.0)
print('2.0 == 2       ', 2.0 == 2)
```
<pre class='Tapas-Return'>
2.0 == 2.0     true
2   == 2.0     true
2.0 == 2       true
</pre>
Note that the integer ``1`` and double float ``1.0`` are the same, given that their absolute difference is less than the float precision in Tapas:

```tapas
print('true == 1.0      ', true == 1.0)
print('false == 0.0     ', false == 0.0)
```
<pre class='Tapas-Return'>
true == 1.0      false
false == 0.0     false
</pre>



### Composite Types

#### Iterator

Iterator can be created by operator ``to`` or by function ``iter``. Iterator ranges are half-open: the start value is included and the end value is not included. For example, `0 to 5` yields `0, 1, 2, 3, 4`.

Example 1: Create an iterator "iter1" from 0 to 5 (not included) with operator `to` :

```tapas
let iter1 = 0 to 5
print('iter1 (abbr) = ', iter1)

for(let i in iter1){ i }
```
<pre class='Tapas-Return'>
iter1 (abbr) = Iterator <0x8fcc04a20>
0
1
2
3
4
</pre>

Example 2: Create an iterator "iter2" from 5 to 0 (not included) with function `iter` :

```tapas
let iter2 = iter(5, -1, 0)

print('iter2 (full) = ', iter2.str())
for(let i in iter2){ i }
```
<pre class='Tapas-Return'>
iter2 (full) = 5 : 0
5
4
3
2
1
</pre>

Example 3: Create an iterator "iter3" from 0 to 5 (not included) with function `iter`:

```tapas
let iter3 = iter(0, 1, 5)

print('iter1 == iter3 = ', iter1 == iter3)
for(let i in iter3){ i }
```
<pre class='Tapas-Return'>
iter1 == iter3 = true
0
1
2
3
4
</pre>


#### String

Strings can be written with single quotes `'...'` or double quotes `"..."`.
A string may span multiple lines until the matching quote is closed.

```tapas
// declare a variable 'mystr'
let mystr = 'Uhis is a string'

// declare a variable 'multistr'
let multistr = 'This is a ...
    string of multiple lines'

mystr
multistr
```
<pre class='Tapas-Return'>
Uhis is a string
This is a ...
    string of multiple lines
</pre>

String is indexable. It can be indexed with integer or slice. A slice is written
as `start:end` and follows half-open range semantics `[start, end)`: `start` is
included and `end` is not included. Negative slice indices are counted from the
end of the string. Slice endpoints can be omitted: `s[:end]` starts at `0`,
`s[start:]` ends at `s.len()`, and `s[:]` copies the whole string.

```tapas
mystr[0] = 'T'
mystr[0:4]
mystr[:4]
mystr[5:]
```
<pre class='Tapas-Return'>
This
This
is a string
</pre>

```tapas
mystr[0:2] = 'th'
mystr
```
<pre class='Tapas-Return'>
this is a string
</pre>

The current implementation does not support wide strings or Unicode-aware
indexing. String indexing may be wrong if the string contains non-ASCII
characters.

Set operation on string: using function `append`.

```tapas
print('>>>Append "mystr" with string "!!!"...')
mystr.append('!!!')
print('>>>Append "mystr" with boolean true...')
mystr.append(true)
print('>>>Append "mystr" with boolean false...')
mystr.append(false)
print('>>>Append "mystr" with integer 0...')
mystr.append(0)
print('>>>Append "mystr" with double float 1.0...')
mystr.append(1.0)
mystr
```
<pre class='Tapas-Return'>
>>>Append "mystr" with string "!!!"...
>>>Append "mystr" with boolean true...
>>>Append "mystr" with boolean false...
>>>Append "mystr" with integer 0...
>>>Append "mystr" with double float 1.0...
this is a string!!!truefalse01
</pre>

String mutation currently supports `append`. The functions `insert`, `pop`,
and `delete` are list or dictionary operations in the current implementation.

Copy string

```tapas
mystr.copy()
mystr[0].copy()
```
<pre class='Tapas-Return'>
this is a string!!!truefalse01
t
</pre>

String equality

```tapas
print("'aaa' == 'bbb'    ", 'aaa' == 'bbb')
print("'aaa' == 'aaa'    ", 'aaa' == 'aaa')
```
<pre class='Tapas-Return'>
'aaa' == 'bbb'    false
'aaa' == 'aaa'    true
</pre>


#### List

Lists can be created by `list(...)` or by brackets `[...]`. Empty brackets
create an empty list. A list can contain values of different types.

```tapas
let empty = []
empty.sprint()

// 'arr1' consists of integer, string, function and float
let arr1 = [1, '2', (x){print(x)}, 2/3]

// print out arr1 directly and as a string
arr1.print()
arr1.str().print()
arr1
```
<pre class='Tapas-Return'>
[]
List <0x8fd000a00>
[1, 2, Function <0x1055a4380>, 0]
[1, 2, Function <0x1055a4380>, 0]
</pre>
```tapas
// create an identical list variables 'arr2' to 'arr1'
let arr2 = list(1, '2', (x){print(x)}, 2/3)
arr2
```
<pre class='Tapas-Return'>
[1, 2, Function <0x1055a4480>, 0]
</pre>

Copy list. The location of the copied object is different from the original:

```tapas
arr1.print()
arr1.copy().print()
```
<pre class='Tapas-Return'>
List <0x8fd000a00>
List <0x8fd000a60>
</pre>

List can be indexed with integer or slice. List slices use the same `start:end`
half-open rule as string slices. Negative slice indices are counted from the end
of the list. Slice endpoints can be omitted: `xs[:end]`, `xs[start:]`, and
`xs[:]`.

```tapas
// change the first element of arr1
arr1[0] = 3

// print out 3 and '2'
arr2[0:2]
arr2[:2]
arr2[1:]
arr2[:]
```
<pre class='Tapas-Return'>
[1, 2]
[1, 2]
[2, Function <0x1055a4480>, 0]
[1, 2, Function <0x1055a4480>, 0]
</pre>
Elements of list can be added to a list by either of functions ``append`` or ``insert``. An example is given below:

```tapas
// add an integer -1 to the beginning of the list
arr1.insert(-1, 0)
arr1.sprint()
```
<pre class='Tapas-Return'>
[-1, 3, 2, Function <0x1055a4380>, 0]
</pre>

```tapas
// add 4 to the end of arr
arr1.append(4)
arr1.sprint()
```
<pre class='Tapas-Return'>
[-1, 3, 2, Function <0x1055a4380>, 0, 4]
</pre>

Elements of list can be removed from a list by either of functions ``pop`` or ``delete``. Here is an example:

```tapas
// remove the element from the beginning of the list
arr1.pop()
arr1.sprint()
```
<pre class='Tapas-Return'>
[-1, 3, 2, Function <0x1055a4380>, 0]
</pre>
```tapas
// remove the last one.
arr1.delete(0)
arr1.sprint()
```
<pre class='Tapas-Return'>
[3, 2, Function <0x1055a4380>, 0]
</pre>

List follows the shallow copy rule: only the pointer is stored if list contains an object of composite type.

```tapas
let num = 3.3
let text = 'Hello'
let li1 = [1,2,3]
let li2 = [num, text, li1]
li1.sprint()
li2.sprint()
```
<pre class='Tapas-Return'>
[1, 2, 3]
[3.3, Hello, [1, 2, 3]]
</pre>

```tapas
num = num - 0.3
li2.sprint()
```
<pre class='Tapas-Return'>
[3.3, Hello, [1, 2, 3]]
</pre>
```tapas
text[0] = 'h'
li1[0] = 4
li2.sprint()
li2[2].sprint()
```
<pre class='Tapas-Return'>
[3.3, hello, [4, 2, 3]]
[4, 2, 3]
</pre>


#### Pair

Pair is the binary tuple in Tapas. It is created by the colon operator `:`. It is used to

- Support slice indexing. The slice in slice indexing is actually a pair.
- Define elements of dictionary. Each key-value pair of dictionary is actually a pair object.

We can think of a pair object as a special list object with only two elements: just like list, the elements of pair can also be indexed, and pair also follows the shallow copy rule.

Example.

Create a pair 'mypair' as ``'jey' : 0``

```tapas
let mypair = 'jey' : 0
mypair
```
<pre class='Tapas-Return'>
jey : 0
</pre>

Check the basic informations of pair

```tapas
mypair.print()
mypair.type() // 4 means composite type
mypair.len()
```
<pre class='Tapas-Return'>
Pair <0x8fcc04a80>
Pair
2
</pre>

Set the first element of 'mypair' as ``key``

```tapas
mypair[0] = 'key'
mypair
```
<pre class='Tapas-Return'>
key : 0
</pre>

Set the value of 'mypair' as '1'

```tapas
mypair[1] = '1'
mypair
```
<pre class='Tapas-Return'>
key : 1
</pre>

Copy 'mypair' as 'mypair2'

```tapas
let mypair2 = mypair.copy()
mypair == mypair2
```
<pre class='Tapas-Return'>
true
</pre>


#### Dictionary

Dictionaries contain key-value pairs. Dictionary keys should be strings.

```tapas
let dict = {
    'key_1' : 1,
    'key_2' : 2,
}
```

Dictionary can be indexed by string.

```tapas
dict['key_1']
```
<pre class='Tapas-Return'>
1
</pre>
```tapas
// change the value of the pair whose key is `key_2`
dict['key_2'] = 0
// add a new pair whose key is `key_3`
dict['key_3'] = 3
dict
```
<pre class='Tapas-Return'>
{
    key_2 : 0,
    key_1 : 1,
    key_3 : 3
}
</pre>

Besides indexing with brackets, dictionary also support the ``read-only indexing`` with two colons, which cannot be used as left value in assignment (see 4.2):

```tapas
// use the string 'key_2' to index dict
dict::key_2

// Wrong! '::' is read-only indexing
// dict::key_2 = 0
```
<pre class='Tapas-Return'>
0
</pre>

Elements can be added to dictionary by function ``append``. Here is an example:

```tapas
dict.append('key_4' : 4)
dict
```
<pre class='Tapas-Return'>
{
    key_2 : 0,
    key_1 : 1,
    key_3 : 3,
    key_4 : 4
}
</pre>

Elements can be removed from dictionary by function ``delete``. An example is given below:

```tapas
// remove a pair whose key is 'key_1'.
dict.delete('key_1')
dict
```
<pre class='Tapas-Return'>
{
    key_2 : 0,
    key_3 : 3,
    key_4 : 4
}
</pre>

Dictionary follows the shallow copy rule: only the pointer is stored if dictionary contains a value of composite type.



#### Dense Array

Tapas provides row-major, two-dimensional dense arrays. An array containing
numbers is a real array; an array containing booleans is a boolean array.

Use `array(rows, cols, value)` to construct an array. A scalar fills every
element. A list fills the array in row-major order and must contain exactly
`rows * cols` elements.

```tapas
let numbers = array(2, 3, [1, 2, 3, 4, 5, 6])
let flags = array(2, 2, false)

sprint(numbers)
sprint(flags)
```
<pre class='Tapas-Return'>
[[1, 2, 3],
 [4, 5, 6]]
[[false, false],
 [false, false]]
</pre>

The same constructor is available as `eig::new`. The `eig` package also
provides shape inspection and transposition:

```tapas
print(eig::rows(numbers), ' x ', eig::cols(numbers))
sprint(eig::transpose(numbers))
```
<pre class='Tapas-Return'>
2 x 3
[[1, 4],
 [2, 5],
 [3, 6]]
</pre>

Arrays use two indices. Each index can be an integer or a half-open slice. A
scalar index returns a scalar; using a slice in either dimension returns a new
array. Negative integer indices and negative slice endpoints count from the end.

```tapas
numbers[1, 2]
numbers[0:2, 1]
```
<pre class='Tapas-Return'>
6
[[2],
 [5]]
</pre>

Elements and slices can be assigned. Slice assignment requires an array of the
same type and shape.

```tapas
numbers[0, 0] = 10
numbers[0:2, 1:2] = array(2, 1, [20, 50])
sprint(numbers)
```
<pre class='Tapas-Return'>
[[10, 20, 3],
 [4, 50, 6]]
</pre>

Real arrays support element-wise `+`, `-`, `*`, `/`, and `^` with a scalar or
another real array of the same shape. Comparisons return boolean arrays.
Matrix multiplication uses `**`; the left column count must equal the right row
count.

```tapas
let a = array(2, 2, [1, 2, 3, 4])
sprint(a + 1)
sprint(a > 2)
sprint(a ** eig::transpose(a))
```
<pre class='Tapas-Return'>
[[2, 3],
 [4, 5]]
[[false, false],
 [true, true]]
[[5, 11],
 [11, 25]]
</pre>

Boolean arrays support element-wise `and` and `or` with a boolean scalar or a
boolean array of the same shape.


#### Time

The function `now()` returns the current local time. Time values can be copied,
compared for identity, formatted with `sprint`, and subtracted. Subtracting two
time values returns their difference in seconds as a float.

```tapas
let started = now()
sprint(started)
```


#### Function

Function is defined by the combination of parentheses and curly braces ``(){}``. Parameters are listed in
parentheses ``()``. Commands are placed between curly braces ``{}``.

Parameter passing in Tapas follows the shallow copy rule: objects of reference type only pass pointers into the function as parameter values.

```tapas
let int_abs = (x){
    if(x >= 0) {
  	    return x
    }
    else{
        return -x
    }
}
int_abs(-10)
```
<pre class='Tapas-Return'>
10
</pre>

Let's play with this newly defined function:

```tapas
print('Length                    = ', int_abs.len())
print('Print out directly        = ', int_abs)
print('Print out as a string     = ', int_abs.str())
print('int_abs == int_abs.copy() = ', int_abs == int_abs.copy())
```
<pre class='Tapas-Return'>
Length                    = 0
Print out directly        = Function <0x1055a2bf0>
Print out as a string     = Function <0x1055a2bf0>
int_abs == int_abs.copy() = false
</pre>

Parameter list written as `...` meaning that the number of parameters is undetermined. We can use function ``__param__(idx)`` to index each parameter, and use function ``__nparam__()`` to get the total number of parameters.

```tapas
let mean = (...){
    if(__nparam__() == 0){
        return 0
    }
    let sum = 0
    for(let i in 0 to __nparam__()){
        sum = sum + __param__(i)
    }
    return sum / __nparam__()
}

mean(2.0, 3.0)
```
<pre class='Tapas-Return'>
2.5
</pre>

Functions in Tapas are ``anonymous``. If we want to refer to the function itself within its body, i.e., if we want to do recursion, we can use the key word ``this``, which will copy the function that it refers to.

```tapas
let cumsum = (x){
    if(x <= 0){
        return 0
    }
    return x + this(x - 1)
}

cumsum(100).print()
```
<pre class='Tapas-Return'>
5050
</pre>

Functions with empty parameter list and contains only a ``return value`` command in its body can be concisely defined by  ``#{}``, as following

```tapas
var x = 10.0
let expr = #{ x/3 }

expr()
```
<pre class='Tapas-Return'>
3.33333
</pre>
The above syntax is equivalent to

```tapas
let expr2 = (){ return x/3 }

expr2()
```
<pre class='Tapas-Return'>
3.33333
</pre>


#### Library

Library is the module type in Tapas. It is created by an `import` statement.

Example: Let's import our unit test file `examples/Sort.md`:

```tapas
import examples/Sort.md as sort_lib
```
<pre class='Tapas-Return'>
original: [2, 7, 3, 4, 3, 1, 9, 1, 8, 4, 6, 9, 5]
bubble: [1, 1, 2, 3, 3, 4, 4, 5, 6, 7, 8, 9, 9]
selection: [1, 1, 2, 3, 3, 4, 4, 5, 6, 7, 8, 9, 9]
insertion: [1, 1, 2, 3, 3, 4, 4, 5, 6, 7, 8, 9, 9]
shell: [1, 1, 2, 3, 3, 4, 4, 5, 6, 7, 8, 9, 9]
merge: [1, 1, 2, 3, 3, 4, 4, 5, 6, 7, 8, 9, 9]
quick: [1, 1, 2, 3, 3, 4, 4, 5, 6, 7, 8, 9, 9]
heap: [1, 1, 2, 3, 3, 4, 4, 5, 6, 7, 8, 9, 9]
original after sorting copies: [2, 7, 3, 4, 3, 1, 9, 1, 8, 4, 6, 9, 5]
</pre>

We can see the program execute the file `examples/Sort.md` and print out all its outputs. Now let's print out the lib

```tapas
sort_lib
```
<pre class='Tapas-Return'>
{}
</pre>
It is empty... That is because we didn't ``return`` anything in the file `examples/Sort.md`. 

More details about module importing are covered in the Environment section.



## Operators

### Arithmetic Operator

Including ``+``, ``-``, ``*``, ``/``, ``%``, ``^``, and ``**``. The `**`
operator performs matrix multiplication on compatible real arrays.

```tapas
2.3 + 4 / 3.0 * 2^2
```
<pre class='Tapas-Return'>
7.63333
</pre>


### Logical Operators (1st level)

Including ``>``, ``<``, ``>=``,  ``<=``,  ``==``,  ``!=``.

```tapas
5 >= 5.0
```
<pre class='Tapas-Return'>
true
</pre>


### Logical Operators (2nd level)

Including ``and``, ``or``. These operators are strictly short-circuiting: the
right-hand side is evaluated only if the left-hand side does not already decide
the result.

```tapas
5 > 4 and (4 in [0,1,2,3,4]) or 3 > 6.0
```
<pre class='Tapas-Return'>
true
</pre>


### Operator `to`

The `to` operator creates a half-open iterator. `start to end` includes `start` and excludes `end`.

```tapas
0 to 5
```
<pre class='Tapas-Return'>
0 : 5
</pre>

```tapas
for (let i in 0 to 5){
    i
}
```
<pre class='Tapas-Return'>
0
1
2
3
4
</pre>


### Logical Operators (3rd level)

Operator `in`.

```tapas
4 in 0 to 10
```
<pre class='Tapas-Return'>
true
</pre>


### Operator colon ``:``

```tapas
{
'name' : 'Tony',
'age' : 20,
}
```
<pre class='Tapas-Return'>
{
    name : Tony,
    age : 20
}
</pre>


Tapas does not provide syntax of operator overloading.

Please inherit the operator interfaces in the source code and implement the corresponding methods manually if you need new operators.



## Expressions

This section will illustrate the priority of different expressions in Tapas.

Here is an example of parsing expressions according to the rule of priority:

```tapas
var arr = list(1,2,3)
var test_pri = -1 * 2 * 2 / int(2^2) + arr[0] in 0 to arr[2]
test_pri
```
<pre class='Tapas-Return'>
true
</pre>
This code prints `true`. When precedence is not clear, use parentheses.



### Value

The expressions with the highest priority are

- Integer
- Float
- Boolean ``true`` or ``false``
- String ``'...'`` or ``"..."``
- Dictionary  ``{ key_1 : value_1, key_2 : value_2 }``
- Function ``(..){ ... }``
- Environment copy ``this`` and ``base``
- Variable name

These expressions directly create or refer to Tapas values. They are the basis
of other operations, and Tapas interprets them first.



### Calling and indexing

Expressions of the second priority are calling and indexing.

Calling expression looks like ``f(a,b,c,...)`` where ``f`` is a callable value, and ``a``, ``b``, ``c``, ``...`` are parameters. Calling expression can be changed into the equivalent ``tunnel`` form, ``a.f(b, c, ...)``. The tunnel form is only syntactic sugar: `a.f(b, c)` is parsed exactly as `f(a, b, c)`.

```tapas
arr.len()
```
<pre class='Tapas-Return'>
3
</pre>

Indexing has the same priority as calling. An indexing expression looks like
`arr[idx]`, where `arr` is an indexable value and `idx` is the index.
Dictionary indexing can also use the read-only form `dict::key`, where `key`
does not need quotes. The read-only indexing form cannot be used as the
left-hand side of an assignment.

```tapas
var d = {
    'k1' : [1],
    'k2' : [2],
    'f1' : (){ return 99999 },
}
```

Now we index the elements of the dictionary above:

```tapas
d::k1.str()
```
<pre class='Tapas-Return'>
[1]
</pre>
```tapas
d::k2.str()
```
<pre class='Tapas-Return'>
[2]
</pre>
```tapas
d::k2[0] / 2
```
<pre class='Tapas-Return'>
1
</pre>
```tapas
d::f1()
```
<pre class='Tapas-Return'>
99999
</pre>


### Arithmetic operations

Arithmetic expressions are evaluated after values, calls, and indexing. They can
be divided into three precedence levels:

- Third order  ``^`` (power calculation)
- Second order  ``*`` (multiplication), ``/`` (division), ``%`` (modulo
  calculation), and ``**`` (matrix multiplication)
- First order  ``+`` (addition) and ``-`` (subtraction)

Higher order arithmetic operations are executed first inside an arithmetic operation.



### Third order logic operations

Comparison expressions include `>`, `<`, `>=`, `<=`, `==`, and `!=`.



### Second order logic operations

Logical expressions use `and` and `or`. Their operands should be boolean values
or expressions that evaluate to boolean values. They are strictly
short-circuiting: `a and b` does not evaluate `b` when `a` is `false`, and
`a or b` does not evaluate `b` when `a` is `true`.



### Expression to

A `to` expression looks like `v1 to v2`, where `v1` and `v2` are integers. The
produced iterator is half-open: it includes `v1` and excludes `v2`.



### Expression pair

A pair expression looks like `v1 : v2`, where `v1` and `v2` are Tapas values.



### Expression in

An `in` expression looks like `v1 in v2`. It tests whether `v1` exists in the
iterable value `v2`, such as an iterator or list, and returns a boolean value.



## Statement

### Environment variable declaration

Use `var` to declare an environment variable. Environment variables live in the
current Tapas environment. A nested function can read and update environment
variables from its parent environment, so `var` is the right choice for values
that must be captured by closures, shared with nested functions, or referenced
by recursive functions.

Every declared variable must be initialized because `nil` cannot be bound to
variables. Multiple variables can be declared in one statement, but each
declarator must provide a value.

```tapas
var a = 0, b = 1, c = 0
print('a = ', a)
print('b = ', b)
print('c = ', c)
```
<pre class='Tapas-Return'>
a = 0
b = 1
c = 0
</pre>

The following declaration is invalid because `b` has no value:

```tapas
// var a = 0, b, c = 0
```

Environment variables are visible to nested functions:

```tapas
var make_counter = (){
    var n = 0
    return (){
        n = n + 1
        return n
    }
}

var counter = make_counter()
counter()
counter()
```
<pre class='Tapas-Return'>
1
2
</pre>
Since duplicate declarations are not allowed and loops do not introduce a new
environment scope, avoid declaring environment variables inside loops.



### Temporary variable declaration

Use `let` to declare a temporary variable. Temporary variables are local to the
current block. Tapas removes temporary variables when the block ends.

Unlike `var`, a `let` variable is not part of the environment captured by nested
functions. Use `let` for short-lived local values such as intermediate
calculations, loop indices, and helper names that do not need to survive outside
the current block.

Like `var`, every `let` declaration must provide a value.

```tapas
let A = 0, B = 1, C = 0
print('A = ', A)
print('B = ', B)
print('C = ', C)
```
<pre class='Tapas-Return'>
A = 0
B = 1
C = 0
</pre>
A `let` variable has block scope. It is visible only from the point where it is
declared to the end of the current block. When the block ends, Tapas removes the
variable. This applies to function bodies, `if` / `elif` / `else` blocks,
`while` blocks, and `for` loop bodies.

The following pattern is invalid because the returned function cannot capture
the temporary variable `x`:

```tapas
// var make_reader = (){
//     let x = 1
//     return (){
//         return x
//     }
// }
```

Use `var` instead when a nested function needs the variable:

```tapas
var make_reader = (){
    var x = 1
    return (){
        return x
    }
}

var reader = make_reader()
reader()
```
<pre class='Tapas-Return'>
1
</pre>



### Assignment

The left-hand side of `=` is the assignable expression, and the right-hand side
is the value assigned to it. Variables must be declared before assignment, and
the assigned value cannot be `nil`.

For composite values, assignment is shallow: after assigning one reference value
to another variable, both variables point to the same object.

Built-in objects cannot be reassigned.

The `nil` value cannot be assigned to variables. For example:

```tapas
// var ret_pnt = print('hello')
```

The code is invalid because `print` returns `nil`.



### Branching statement

> ```
> if(condition_1){
>     commands_1
> }
> elif(condition_2){
>     commands_2
> }
> else{
>     commands_3
> }
> ```

Each condition must be a boolean value or an expression that evaluates to a
boolean value. Tapas executes the first branch whose condition is `true`; if no
condition matches, it executes the `else` block when one is present.

### Loop statement: for

The ``for`` loop statement looks like

> ```
> for(let i in iter){
>     commands
> }
> 
> var ii = 0
> for(ii in iter){
>     commands
> }
> ```

The loop header must contain an `in` expression. The first form creates a
temporary loop variable. The second form reuses an environment variable that was
declared before the loop. Use the second form when the loop variable must be
visible after the loop or shared with nested functions.

### Loop statement: while

The ``while`` loop statement looks like

> ```
> while(condition){
>     commands
> }
> ```

`condition` must be a boolean value or an expression that evaluates to a boolean
value. The loop continues while the condition is `true`.

### break and continue

Tapas supports `break` and `continue` inside loops. `break` exits the loop, and
`continue` jumps to the next iteration.

### Return

The return statement can be written as `return value` or as `return` without a
value. A bare `return` returns `nil`, which can be used internally as a function
result but cannot be assigned to a variable.

### Module import

The import statement looks like

> ```
> import file.tap [as library]
> ```



## Environment

### Environment tree

Environment is the key concept behind function scope, closures, recursion, and
modules in Tapas. A Tapas session maintains a root environment, and functions or
libraries create child environments.

Environments in Tapas are organized as a tree. Defining a function creates a
child environment. Each environment has a parent environment. The keyword `this`
copies the current function environment, and `base` copies the parent
environment.

Each environment stores the variables declared in its scope.



### Function scope

Commands in a child environment can read and modify variables declared in a
parent environment. Commands in a parent environment cannot directly access
variables declared only inside a child environment; those values must be
returned explicitly.

Statements such as `if`, `while`, and `for` do not create function scope.



### Executable environment

When a function is called, Tapas executes the function body in its own
environment and returns the function result to the caller.

Function environments keep their captured variables, which enables closures.
This is useful, but it also means reference values should be copied explicitly
when independent state is required.



### Higher-order functions

Functions are values, so a function can return another function. A returned
function can keep access to variables from the environment where it was created;
this is a closure.

```tapas
var f1 = (){
    var x = 0
    return (){
        x = x + 1
        print(x)
    }
}
var f2 = f1()
f2
f2()
var f3 = f1()
f3
f2()
f3()
```
<pre class='Tapas-Return'>
Function <0x8fd144180>
1
Function <0x8fd1441e0>
2
1
</pre>
`f2` and `f3` are different function values. Each call to `f1()` creates a fresh
captured `x`, so calling `f2()` does not update the `x` captured by `f3()`.



### Function calling and recursion

Functions in Tapas are anonymous values. To write recursion, call `this(...)`
inside the function body; `this` refers to a copy of the current function.

```tapas
// 'this' refers to the function

var sum = (x){
    if(x > 1){
        return x + this(x - 1)
    }
    else{
        return 1
    }
}
sum(100)
101 * 50
```
<pre class='Tapas-Return'>
5050
5050
</pre>
Since ``this`` is a copy of the current environment, it has an empty variable list of the same length as the current environment, and it shares the same father environment as the current environment. Thus, the original function is intact when the copied one is executed.



### Further explanations about recursion

We mentioned at the end of Section 5 that local reference variables cannot be returned in recursion. Here is another example:

```tapas
// Line 4: try to return the local variable y
// Line 7: returning a locally created expression is fine

var expand_list = (x){
    var y = x.copy()
    if(y.len() <= 1){
        return y
    }
    y.pop(0)
    return union(y, this(y))
}
[1,2,3,4,5,6,7].expand_list()
```
<pre class='Tapas-Return'>
[2, 3, 4, 5, 6, 7, 3, 4, 5, 6, 7, 4, 5, 6, 7, 5, 6, 7, 6, 7, 7, 7]
</pre>

The correct version is

```tapas
// Line 3: returning x which is passed in from outside is fine

var expand_list_1 = (x){
    if(x.len() <= 1){
        return x
    }
    var y = x.copy()
    y.pop()
    return union(y, this(y))
}
[1,2,3,4,5,6,7].expand_list_1()
```
<pre class='Tapas-Return'>
[1, 2, 3, 4, 5, 6, 1, 2, 3, 4, 5, 1, 2, 3, 4, 1, 2, 3, 1, 2, 1, 1]
</pre>
Or you can write it in this way:

```tapas
// Line 4: returning an expression is fine,
//         yet too many copies.

var expand_list_2 = (x){
    var y = x.copy()
    if(y.len() <= 1){
        return y.copy()
    }
    y.pop()
    return union(y, this(y))
}
[1,2,3,4,5,6,7].expand_list_2()
```
<pre class='Tapas-Return'>
[1, 2, 3, 4, 5, 6, 1, 2, 3, 4, 5, 1, 2, 3, 4, 1, 2, 3, 1, 2, 1, 1]
</pre>


### tlib: Import a single file

A file stands for a module. By importing a module we get a value of ``tlib``.

```tapas
// import a file module and create a library value
import examples/modules/demo-lib-1.tap as lib
lib::get_5()
```
<pre class='Tapas-Return'>
5
</pre>

```tapas
// execute a file module without binding it in the current environment
import examples/modules/demo-lib-2.tap
```
<pre class='Tapas-Return'>
3
5
</pre>

The values defined in one file is not observable by other files because they belong to different branches of the environment tree, unless we return the values that we want at the end of the file. The returned values should be wrapped in a ``tdict`` value.

> ```
> return {
>     'obj_1' : obj_1,  // return obj_1 by naming it as 'obj_1'
>     'obj_2' : obj_2,  // return obj_2 by naming it as 'obj_2'
> }
> ```

Here is a concrete example of the module importing work flow. In
`examples/modules/demo-lib-1.tap`, we define two functions and return them.

> ```
> var print_three = (){
>     print(3.0)
> }
> var get_five = (){
>     return 5.0
> }
> return {
>     'print_3' : print_three,
>     'get_5'   : get_five,
> }
> ```

In `examples/modules/demo-lib-2.tap`, we import the module
`examples/modules/demo-lib-1.tap` and create a variable ``m1``.

```tapas
import examples/modules/demo-lib-1.tap as m1
m1::print_3()
m1::get_5().print()
```
<pre class='Tapas-Return'>
3
5
</pre>

Here ``m1`` is a variable of ``tlib`` type value. Thus, we can index the returned values by using read-only string indexing on ``m1``.

Note that modules cannot be imported by each other, causing the looping-importing problem. Tapas will not check for the looping-importing problem in compilation. Modules cannot be copied, so we cannot use ``this`` or ``base`` in modules.

### tlib: How to import a folder?

If your modules are put under a folder where there is a file ``__init__.tap``, then Tapas also supports to "import" a folder. For example, `examples/modules/mymodule` has the structure

> ```
> |---- examples/modules/mymodule/
>       |---- __init__.tap
>       |---- file_1.tap
> ```

We can directly use ``import`` statement to import the whole folder.

```tapas
import examples/modules/mymodule as A
```

In compilation, Tapas will check whether ``mymodule`` is a file or a folder. If it is a file, Tapas will import it as a file. If it is a folder, Tapas will only import the ``__init__.tap`` file under this folder. Thus, the above command is actually equivalent to

```tapas
import examples/modules/mymodule/__init__.tap as A_init
A_init['abs'](-10)
```
<pre class='Tapas-Return'>
10
</pre>

We need to edit the ``__init__.tap`` file manually. For example, below is the
file `examples/modules/mymodule/file_1.tap`:

> ```
> var abs = (x){
>     if(x >= 0){
>         return x
>     }
>     else{
>         return -x
>     }
> }
> var inv = (x){
>     return 1/x
> }
> return {
>     'abs' : abs,
>     'inv' : inv,
> }
> ```

where we return methods ``inv`` and ``abs`` from ``file_1.tap``. Suppose we want to expose these methods to other users. We can edit ``__init__.tap`` in this way:

> ```
> import file_1.tap as file_1
> 
> return {
>     'author'  : 'author name',
>     'module'  : 'test',
>     'version' : 1,
>     'mail'    : 'author@server',
>     'file_1'  : file_1,
>     'inv'     : file_1['inv'],
>     'abs'     : file_1['abs'],
> }
> ```

Finally, let's try to import ``mymodule`` and call the ``inv`` and ``abs`` methods.

```tapas
import examples/modules/mymodule as M
M['file_1']['abs'](-10).print()
M['file_1']['inv'](-10).print()
M['inv'](-10).print()
```
<pre class='Tapas-Return'>
10
0
0
</pre>



## Built-In Functions

Tapas registers standard functions directly in the root environment. Use the
function name without a package prefix.

### Core

Output and inspection:

> ```
> print(...)
> sprint(...)
> len(...)
> type(...)
> copy(...)
> identical(a, b)
> clock()
> now()
> ```

Conversions:

> ```
> int(value)
> float(value)
> bool(value)
> str(value)
> ```

Composite constructors and helpers:

> ```
> iter(start, end)
> iter(start, step, end)
> pair(first, second)
> list(...)
> array(rows, cols, scalar_or_list)
> 
> push(list, value)
> append(target, value)
> insert(target, value, index)
> pop(target)
> pop(target, index)
> delete(target, index_or_key)
> idx(target, index)
> keys(dict)
> dkeys(dict)
> dvalues(dict)
> union(left, right)
> sort(list)
> ```

Session functions:

> ```
> __ls__([lib])
> __path__([lib])
> __param__(index)
> __nparam__()
> __binary__([env])
> ```


### Dense Arrays

Dense-array helpers are registered in the default `eig` package:

> ```
> eig::new(rows, cols, scalar_or_list)
> eig::rows(array)
> eig::cols(array)
> eig::transpose(array)
> ```

`array(...)` and `eig::new(...)` are equivalent. See the Dense Array section
under Composite Types for indexing, slicing, assignment, and operators.



### Math

Scalar math functions are registered in the default `math` package. Use the
package-qualified form `math::name(...)`.

Scalar math functions:

> ```
> math::abs(x)
> math::fabs(x)
> math::sqrt(x)
> math::rsqrt(x)
> math::cbrt(x)
> math::pow(x, y)
> math::hypot(x, y)
> math::sin(x)
> math::cos(x)
> math::tan(x)
> math::asin(x)
> math::acos(x)
> math::atan(x)
> math::atan2(y, x)
> math::sinh(x)
> math::cosh(x)
> math::tanh(x)
> math::asinh(x)
> math::acosh(x)
> math::atanh(x)
> math::exp(x)
> math::exp2(x)
> math::expm1(x)
> math::log(x)
> math::log2(x)
> math::log10(x)
> math::log1p(x)
> math::logb(x)
> math::ilogb(x)
> math::frexp(x)
> math::modf(x)
> math::ldexp(x, n)
> math::scalbn(x, n)
> math::scalbln(x, n)
> math::erf(x)
> math::erfc(x)
> math::lgamma(x)
> math::tgamma(x)
> math::ceil(x)
> math::floor(x)
> math::nearbyint(x)
> math::rint(x)
> math::lrint(x)
> math::llrint(x)
> math::round(x)
> math::lround(x)
> math::llround(x)
> math::trunc(x)
> math::fmod(x, y)
> math::remainder(x, y)
> math::remquo(x, y)
> math::copysign(x, y)
> math::nextafter(x, y)
> math::fdim(x, y)
> math::fmax(x, y)
> math::fmin(x, y)
> math::fma(x, y, z)
> math::eleinv(x)
> math::make_nan()
> ```

Scalar math predicates:

> ```
> math::isfinite(x)
> math::isinf(x)
> math::isnan(x)
> math::isnormal(x)
> math::fpclassify(x)
> math::signbit(x)
> math::isgreater(x, y)
> math::isgreaterequal(x, y)
> math::isless(x, y)
> math::islessequal(x, y)
> math::islessgreater(x, y)
> math::isunordered(x, y)
> ```

These math functions accept scalar `int` and `float` values. Dense arrays have
their own element-wise operators and the helpers in the `eig` package.
