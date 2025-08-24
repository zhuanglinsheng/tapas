
# The Tapas Programming Language



## 1. Primitive Types

Generally speaking, Tapas provides two kinds of primitive types: **value types** and **composite types**.

Consistent with other popular programming languages with virtual machines (such as Java), object of value types refers to specific values while composite types refers to a pointer.



### 1.1. Nil

The value nil represents *null*, *empty*, or *nothing*. It cannot be explicitly created by users.

```
std::print('The return of function "print" is a "nil" whose type is').std::type()
```

<pre class='Tapas-Return'>
The return of function "print" is a "nil" whose type is
Nil
</pre>


### 1.2. Bool


A boolean value could be either `true` or `false`. It can be obtained by literal `true` and `false` or by logical calculations (see Section [3.4](#34-logic-operations-order-3), [3.5](#35-logic-operations-order-2) and [3.8](#38-logic-operations-order-1)).

```
std::type(2 > 1)
false; true
```

<pre class='Tapas-Return'>
Bool
false
true
</pre>
Note that the boolean `true` and `false` are in essence integers `1` and `0`:

```
true == 1
false == 0
```

<pre class='Tapas-Return'>
true
true
</pre>




### 1.3. Int

Int type in Tapas is in essence `long int` in C++.

```
std::type(0)
2 / 3
```

<pre class='Tapas-Return'>
Int
0
</pre>


### 1.4. Float

Float type in Tapas is in essence `double float` in C++.

```
std::type(0.0)
2 / 3.0
1.414e-2
```

<pre class='Tapas-Return'>
Float
0.666667
0.014140
</pre>



### 1.5. Primitive Composite Types

Tapas provides the following compositive types by default:

- [Itarator](#61-iterator)
- [String](#62-string)
- [List](#63-list)
- [Pair](#64-pair)
- [Dictionary](#65-dictionary)
- [Function](#66-function)
- [Array of Values Types](#67-array)
- [Library](#68-library)



## 2. Type System

Type is a single directed graph with a root node.

The root node is `Any` type while the leaf nodes are primitives or user defined types.



## 3. Expressions

In short, an expression places an object on top of the virtual machine stack.

In this section, we will illustrate the priority of different expressions in Tapas. Here is an example of parsing expressions according to the rule of priority:

```
let arr = [1, 2, 3]  // create a list of integers
-1 * 2 * 2 / std::toint(2^2) + arr[0] in 0 to arr[2]
```

<pre class='Tapas-Return'>
true
</pre>

When the propriety is not clear, always use parenthesis `()`.


### 3.1. Value

The expressions with the highest priority are

- Integer `1; 2; 3`
- Float `1.0; 1e-5`
- Boolean `true` or `false`
- String `'...'` or `"..."`
- Dictionary  `{ key_1 : value_1, key_2 : value_2 }`
- Function `(..) { ... }`
- Environment copy `this` and `base`
- Variable name

These expressions directly create or refer to objects in Tapas.



### 3.2. Calling and indexing

Expressions of the second priority are calling and indexing expressions.



#### 3.2.1. Indexing

The "Indexing" expression is of the same priority as calling expression, which looks like `arr[idx]`. Here `arr` is a indexable value, and `idx` is the index.

```
arr[0]
```

<pre>
1
</pre>


#### 3.2.2. Read-only indexing

When indexing elements of `dict` objects, and the key is a string, we can use `Read-only indexing`, which looks like `arr::idx`.

```
// Create a dict `d` that contains a list `arr` and a function `add_one`
let d = {
    'arr' : [1, 2, 3],
    'add_one' : (x) { return x + 1 },
}
```

Now we index the elements of the dictionary above:

```
d::arr
```

<pre class='Tapas-Return'>
[1, 2, 3]
</pre>

```
d::add_one
```

<pre>
"Function at 0x107600f20"
</pre>


#### 3.2.3. Calling

The "Calling" expression looks like `f(a,b,c,...)`, where `f` is a callable value and `a`, `b`, `c`, `...` are parameters.

```
d::add_one(d::arr[0])
```

<pre class='Tapas-Return'>
2
</pre>


#### 3.2.4. Tunnel expression

Calling expression `f(a,b,c,...)` can be reformed into the equivalent `tunnel` form `a.f(b, c, ...)`.

```
d::arr[0].d::add_one()
```

<pre class='Tapas-Return'>
2
</pre>


### 3.3. Arithmetic operations

Expressions of the third priority are arithmetic operations.

- Third order:  `^` (power)
- Second order:  `*` (multiplication), `/` (devision), `%` (modulo) and `@` (matrix multiplication)
- First order:  `+` (addition) and `-` (subtraction)

Higher order arithmetic operations have higher priorities than lower order operations.

```
2.3 + 4 / 3.0 * 2^2
```

<pre class='Tapas-Return'>
7.633333
</pre>



### 3.4. Logic operations: order 3

Expression of the 4th priority is third order logical expression. The logical operators in this level include `>`, `<`, `>=`, `<=`, `==`, `!=`. Note that floats cannot be applied into `==` for equality judgment.

```
5 >= 5.0
```

<pre class='Tapas-Return'>
true
</pre>



### 3.5. Logic operations: order 2

Expression of the 5th priority is second order logical expression. The logical operators in this level include `and` and `or`, the logical and operation and logical or operation. A `nil` would be returned if either side is not logical expression (boolean value or the expression that returns boolean value).

```
5 > 4 or 3 > 6.0
5 > 4 and 3 > 6.0
```

<pre class='Tapas-Return'>
true
false
</pre>



### 3.6. To expression

Expression of the 6th priority is `to` expression, which looks like `v1 to v2`. Here `to` is an operator and `v1` and `v2` are integers.

```
0 to 5
```

<pre>
0 to 5 (by 1)
</pre>


The "To" expression is used to generate iterators for loops.

```
for (let idx in 0 to 5) {
	idx
}
```

<pre>
0
1
2
3
4
</pre>



### 3.7. Pair expression

Expression of the 7th priority is `pair` expression, which looks like `v1 : v2`. Here `:` is an operator and `v1` and `v2` are Tapas objects.

```
1 : 1
```

<pre>
1 : 1
</pre>


The `dict` type in Tapas consists of a set of pairs in curly-brackets, seperated by commas:

```
{
	'name' : 'Tony',
	'age' : 20,
}
```

<pre class='Tapas-Return'>
{
	"age" : 20,
	"name" : Tony,
}
</pre>


### 3.8. Logic operations: order 1

The expression at the lowest position of the priority system is `in` expression which looks like `v1 in v2`, where `in` is logic operator, `v1` is an element and `v2` is an iterable object (subclass of `tcompo_iter`, including `titer` and `tlist`).

Operator `in` returns a boolean, standing for whether `v1` is in `v2`.

```
4 in [0,1,2,3,4]
4 in 0 to 5
```

<pre class='Tapas-Return'>
true
true
</pre>



## 4. Statements

Briefly speaking, an expression will leave no object at the top of virtual machine stack.

In this section, we will introduce the statements in Tapas.



### 4.1. Environmental variable declaration

To declare an environmental variable, we use `var` statement.

All environmental variables need to be declared before used.

When an environmental variable is declared but not initialized, Tapas automatically initializes it to the ``nil`` type. Variables can be initialized directly as they are declared.

For example:

```
var a: int = 0
var b: int
var c = 0.0

std::print('a = ', a)
std::print('b = ', b)
std::print('c = ', c)
sys::__ls__()  // listing all environmental variables
```
<pre class='Tapas-Return'>
a = 0
b = nil
c = 0.000000
[sys, std, eig, a, b, c]
</pre>


Note that:

1. Since duplicated declaration is not allowed and loop has no environment-level scope, it is not allowed to declare environmental variables in sub blocks of functions. If users want to declare some temporary variables, just use the `let` statement below.
2. Type hints are optional.



### 4.2. Temporary variable declaration

To declare a temporary variable we use `let` statement

```
let A: int = 0
let B: int
let C = 0.0

std::print('A = ', A)
std::print('B = ', B)
std::print('C = ', C)
sys::__ls__()  // listing all environmental variables
```
<pre class='Tapas-Return'>
A = 0
B = nil
C = 0.000000
[sys, std, eig, a, b, c]
</pre>


Note that:

1. Temporary variable has block scope. All temporary variables will be deleted at the end of block.
2. Type hints are optional.



### 4.3. Assignment

The left hand side of ``=``  is the variable name (lvalue), and the right hand side of ``=`` is the respective rvalue to be assigned to the lvalue. Right value CANNOT be ``nil``. Before assignment, variables must be declared.

For reference types, value assignment is not copy. After reference variable as the rvalue in assignment, the two variable values will point to the same memory space.

All default objects CANNOT be assigned values.

The ``nil`` value cannot be assigned to other variables as an rvalue. For example:

<pre>
var ret_pnt = std::print('hello')
</pre>

The code gives us error since the return of `print` function is `nil`.



### 4.4. Branching statement

```
let condition_1 = true
let condition_2 = true

if (condition_1) {
	// commonds_1
}
elif (condition_2) {
	// commonds_2
}
else {
	// commonds_3
}
```

Here ``condition_1`` and ``condition_2`` are logical expression or boolean value. If they are  ``false``, jumpping happens.

Then, ``command_1``, ``command_2`` and ``command_3`` are code blocks. Any one of them were executed and the program would jump out of the branching statement.

Note that curly braces must always be placed exactly as shown in the example.



### 4.5. Loop statement: for

The ``for`` loop statement looks like

```
for (let i in 0 to 10) {  // i is a temporary variable in the loop block
	// ...
}

var ii  // environmental variable has to be declared outside of loop blocks
for (ii in 0 to 10) {
	// ...
}
```

Their meanings are similar to other general programming languages. It should be noted that the condition of the ``for`` statement must be an ``in`` expression. ``ii`` is an environmental variable and must be declared outside the loop body. If there is an outer loop outside the ``for`` loop, you need to declare ``ii`` outside the outermost loop. Inside the ``for`` loop, the program will use ``ii`` to traverse the iterable type value ``iter``.



### 4.6. Loop statement: while

The ``while`` loop statement looks like

```
let condition_3 = false

while (condition_3) {
	// commands
}
```

``condition`` is logically expression or boolean value. Jump will happen if it is ``false``, otherwise the program keep executing the ``commands``.



### 4.7. break and continue

Tapas supports ``break`` and ``continue`` to control the looping process. ``break`` means to jump out of loop while ``continue`` means to jump to the next loop.



### 4.8. Return

The return value statement can be parameterized like ``return value`` or of no parameter like ``return``. In return value statement, Tapas would clear all values on the virtual machine stack, copy ``value`` from environment to ``tapas::tvm::__rev``, and jump out of the current environment.



### 4.9. Module import

The import statement looks like `import file.tap [as library]`.

See Section [5](#5-environment) for more details of this part.



## 5. Environment

Environment is a key concept in Tapas. Concepts like function scope, closure, recursion, and modules are all related to the environment.

Environment is an abstract type in Tapas. `tfunc` and `tlib` are sub-types of Environment. All these sub-types are child classes of `tcompo_env` in the C++ source code. When we launch a Tapas session, it maintains a root environment.



### 5.1. Environment tree

Environments in Tapas are organized as a tree. When we define a Tapas function, it creates a child environment. Each environment contains a pointer to its parent environment. We can use `this` to get a copy of the current environment and `base` to get a copy of the parent environment.

Environment maintains a variable list for storage and reference. All variables declared in the scope of an environment are stored in the respective variable list.



### 5.2. Environment Scope

In Tapas, commands in child environments can directly read and modify variables declared in father environment, while commands in the father environment can not directly affects those variables declared in child environments. If we want to modify some variables of child environment in father environment, we have to ``return`` them.

Note that, the statements like ``if``, ``while`` and ``for`` don't have environment scope.



### 5.3. Executable environment

For executable environment values (such as functions, etc.), a new virtual machine will be temporarily created each time it is executed. The commands in the sub-environment will be executed on this temporary virtual machine. The temporary virtual machine is only responsible for the operation of the sub-environment program instructions. Once execution ends, the returned value of the child environment program will be pushed onto the top of the stack of the virtual machine running in the parent environment program, and the temporary virtual machine will be destroyed.

For functions, after each function calling occurs, the values of local variables in the function will not be released immediately, but be cached in the variable list. These values will be overwritten when the next function call occurs. This brings some conveniences, such as the support of closures. However, the problem with this design is also very obvious: The memory footprint of the environment is very large, which may cause efficiency problems.



### 5.4. Higher-order functions

Since function is also a value in Tap, the function itself can return a function. Functions that return functions are called higher-order functions. When Tapas tries to search a variable while the returned function is executed, that variable is indexed according to the environment tree, which is called the "closure". Here is an example of a higher-order function:

```
var f1: fn = () {
	var x: int = 0
	return () {
		x = x + 1
		std::print(x)
	}
}
var f2: fn = f1()
f2
f2()
var f3: fn = f1()
f3
f2()
f3()
```

<pre class='Tapas-Return'>
"Function at 0x600000144180"
1
"Function at 0x6000001442a0"
1
2
</pre>
Note that ``f2`` and ``f3``, as the returned value of ``f1``, are not identical, because their locations in memory are different. Yet their father environments are both ``f1``. Thus, when we execute the two functions, the variable ``x`` in environment ``f1`` will be added by 1 on 0, while when ``f1`` is executed again, it is re-assigned to be 0.



### 5.5. Function calling and recursion

Tapas has two kind of functions, ``tfunc`` (defined by Tapas script) and ``cppfunc`` (defined by C++). They are all sub classes of ``tcompo_eval``. ``tfunc`` is also the child class of ``tcompo_env``, meaning that it is an environment.

Since all functions in Tapas are anonymous, we cannot implement the recursion by calling them by name. Yet, we can use ``this`` (the keyword for function environment copying) to represent the recursion function.

```
// 'this' refers to the function
var sum: fn = (x) {
	if (x > 1) {
		return x + this(x - 1)
	}
	else{
		return 1
	}
}
sum(500)
501 * 250
```

<pre class='Tapas-Return'>
125250
125250
</pre>
Since ``this`` is a copy of the current environment, it has an empty variable list of the same length as the current environment, and it shares the same father environment as the current environment. Thus, the original function is intact when the copied one is executed.



### 5.6. Further explanations about recursion

We mentioned at the end of Section 5 that local reference variables cannot be returned in recursion. Here is another example:

<pre>
// Line 4: try to return the local variable y
// Line 7: returning a locally created expression is fine
var expand_list: fn = (x) {
	var y: any = x.std::copy()
	if (y.std::len() <= 1) {
		return y
	}
	y.std::pop(0)
	return std::union(y, this(y))
}
[1,2,3,4,5,6,7].expand_list()
</pre>

The correct version is

```
// Line 3: returning x which is passed in from outside is fine
var expand_list_1: fn = (x) {
	if (x.std::len() <= 1) {
		return x
	}
	var y: any = x.std::copy()
	y.std::pop()
	return std::union(y, this(y))
}
[1,2,3,4,5,6,7].expand_list_1()
```


<pre class='Tapas-Return'>
[1, 2, 3, 4, 5, 6, 1, 2, 3, 4, 5, 1, 2, 3, 4, 1, 2, 3, 1, 2, 1, 1]
</pre>

Or you can write it in this way:

```
// Line 4: returning an expression is fine,
//         yet too many copies.
var expand_list_2: fn = (x) {
	var y: any = x.std::copy()
	if (y.std::len() <= 1) {
		return y.std::copy()
	}
	y.std::pop()
	return std::union(y, this(y))
}
[1,2,3,4,5,6,7].expand_list_2()
```


<pre class='Tapas-Return'>
[1, 2, 3, 4, 5, 6, 1, 2, 3, 4, 5, 1, 2, 3, 4, 1, 2, 3, 1, 2, 1, 1]
</pre>


### 5.7. How to import a single file

A file stands for a module. By importing a module we get an instance of `Library`:

```
// execute `model_1.tap` without import it into current environment
import module_1.tap

// import `model_1.tap` and create a library instance
import module_1.tap as lib_1
std::print('The type of `lib_1` is: ', std::type(lib_1))
std::print('The contents of `lib_1`: ')
lib_1
```

<pre class='Tapas-Return'>
Library
</pre>

The values defined in one file is not observable by other files because they belong to different branches of the environment tree, unless we **return** the values that we want at the end of the file.

The returned values should be wrapped in a ``tdict`` value:

<pre>
return {
	// return obj_1 by naming it as 'obj_1'
	'obj_1' : 'some object 1',
	// return obj_2 by naming it as 'obj_2'
	'obj_2' : 'some object 2',
}
</pre>

Here is a concrete example of the module importing work flow.

In file `module_1.tap`, we define two functions and return them:

<pre>
// file `module_1.tap`

var print_three = () {
	std::print(3.0)
}
var get_five = () {
	return 5.0
}
return {
	'print_3' : print_three,
	'get_5'   : get_five,
}
</pre>


In file `module_2.tap`, we import the module `module_1.tap` and excute them:

<pre>
// file `module_2.tap`

import module_1.tap as mylib
mylib::get_5()
mylib::print_3()
</pre>


In file `module_2.tap`, the variable `mylib` is of `Library` type value.
Thus, we can index the returned values by using read-only string indexing on `mylib`.

```
import module_2.tap
```

Note that modules cannot be imported by each other, causing the looping-importing problem.
Tapas will **not** check for the looping-importing problem in compilation, so users should make sure it not happen themselves.

Modules cannot be copied, and we cannot use ``this`` or ``base`` in modules.



### 5.8. How to import a folder?

If your modules are put under a folder where there is a file `__init__.tap`, then Tapas also supports to "import" a folder.

For example, let us say currently your folder has the structure

<pre>
|---- module_1.tap
|---- module_2.tap
|---- module_3/
    |---- __init__.tap
    |---- file_1.tap
    |---- file_2.tap
    |---- ....
    |---- LICENSE
    |---- README.md
</pre>

We can directly use ``import`` statement to import the whole folder:

```
import module_3 as lib_3
```

In compilation, Tapas will check whether `module_3` is a file or a folder.

If it is a file, Tapas will import it as a file.

If it is a folder, Tapas will only import the file `__init__.tap` under this folder.

Thus, the above command is actually equivalent to

```
import module_3/__init__.tap as lib_3_again
```

We need to edit the `__init__.tap` file manually.

For example, below is my file `module_3/file_1.tap`,

<pre>
var abs: fn = (x) {
	if (x >= 0) {
		return x
	}
	else{
		return -x
	}
}
var inv: fn = (x) {
	return 1/x
}
return {
	'abs' : abs,
	'inv' : inv,
}
</pre>

where we return methods ``inv`` and ``abs`` from ``file_1.tap``.

Suppose we want to expose these methods to other users, we can edit ``__init__.tap`` in this way:

<pre>
// file `__init__.tap`

import file_1.tap as file_1
return {
	'author'  : 'author name',
	'module'  : 'module name',
	'version' : 1.0.0,
	'mail'    : 'author@server',
	'file_1'  : file_1,
	'inv'     : file_1::inv,
	'abs'     : file_1::abs
}
</pre>

Finally, let's try to import `module_3` and call the `inv` and `abs` methods:

```
import module_3 as library_3
std::type(library_3)
library_3::abs(-10)
library_3::inv(-10)
```

<pre class='Tapas-Return'>
10
0
0
</pre>


## 6. Composite Types



### 6.1. Iterator

Iterator can be created by operator ``to`` or by function ``std::toiter``.

**Example.** Create an iterator `iter1` from 0 to 5 (not included) with operator `to`.

```
let iter1 = 0 to 5
iter1

for (let i in iter1) { i }
```
<pre class='Tapas-Return'>
0 to 5 (by 1)
0
1
2
3
4
</pre>

Create an iterator `iter2` from 5 to 0 (not included) with function `toiter`.

```
let iter2 = std::toiter(5, -1, 0)
iter2

for (let i in iter2) { i }
```
<pre class='Tapas-Return'>
iter2 (full) = 5 to 0 (by -1)
5
4
3
2
1
</pre>
Create an iterator `iter3` from 0 to 5 (not included) with function `toiter`.

```
let iter3 = std::toiter(0, 1, 5)

std::print('iter1 == iter3 = ', iter1 == iter3)
for (let i in iter3) { i }
```
<pre class='Tapas-Return'>
iter1 == iter3 = true
0
1
2
3
4
</pre>


### 6.2. String

String is inherited from C++ STL string class ``std::string``. It can be obtained by single quote '...' or double quote "...".

String could be multi-lines until where the quote is closed.

```
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



#### 6.2.1. Indexing

String is indexable. It can be indexed with integer or slice.

```
mystr[0] = 'T'
mystr[0:4]
```
<pre class='Tapas-Return'>
This
</pre>
```
mystr[0:2] = 'th'
mystr
```
<pre class='Tapas-Return'>
this is a string
</pre>
Currently, wide string and Unicode are NOT supported. Indexing could be wrong if your string contains non-ASCII characters.



#### 6.2.2. Append

Set operation on string: using function `append()`.

```
let mystr2 = ''
mystr2.std::append(true)   // append a boolean value to string
mystr2.std::append('!!!')  // append a string
mystr2

mystr2 = ''
mystr2.std::append(1)   // append an integer
mystr2.std::append(1.)  // append a float
mystr2
```
<pre class='Tapas-Return'>
true!!!
11.000000
</pre>



#### 6.2.3. Insert

Set operation on string: using function `insert`.

```
let mystr3 = ''
mystr3.std::insert(false, mystr3.std::len())
mystr3.std::insert('!!!', mystr3.std::len())
mystr3
```
<pre class='Tapas-Return'>
false!!!
</pre>


#### 6.2.4. Pop

Set operations on string: using function `pop`.

```
let mystr4 = 'true!!!'
mystr4.std::pop()
mystr4.std::pop()
mystr4
```
<pre class='Tapas-Return'>
true!
</pre>



#### 6.2.5. Delete

Set operations on string: using function `delete`.

```
let mystr5 = '1234567'
std::print('mystr5 is ', mystr5)

mystr5.std::delete(mystr5.std::len()-1) // delete the last character
mystr5

mystr5.std::delete(4 to 7) // further delete last 2 characters
mystr5
```
<pre class='Tapas-Return'>
123456
1234
</pre>



#### 6.2.6. Compare strings

String equality

```
'aaa' == 'bbb'
'aaa' == 'aaaa'
'aaa' == 'aaa'
```
<pre class='Tapas-Return'>
false
false
true
</pre>


### 6.3. List

List is inherited from C++ STL vector class ``std::vector``. It can be created by the Tapas function ``tolist(...)`` or simply by brackets ``[...]``.  It can contain elements of different types.

```
// 'arr1' consists of integer, string, function and float
let arr1 = [1, '2', (x) {std::print(x)}, 2/3]

// print out arr1 directly and as a string
arr1.std::print()
arr1.std::tostr().std::print()
arr1
```
<pre class='Tapas-Return'>
"List at 0x600003a51300"
[1, 2, "Function at 0x600000b50240", 0]
[1, 2, "Function at 0x600000b50240", 0]
</pre>

```
// create an identical list variables 'arr2' to 'arr1'
let arr2 = std::tolist(1, '2', (x) {std::print(x)}, 2/3)
arr2
```
<pre class='Tapas-Return'>
[1, 2, "Function at 0x600000b50300", 0]
</pre>


Copy list. The location of the copied object is different from the original:

```
arr1.std::print()
arr1.std::copy().std::print()
```
<pre class='Tapas-Return'>
"List at 0x600003a51300"
"List at 0x600003a51480"
</pre>


List can be indexed with integer or slice.

```
// change the first element of arr1
arr1[0] = 3

// print out 3 and '2'
arr2[0:2]
```
<pre class='Tapas-Return'>
[1, 2]
</pre>


Elements of list can be added to a list by either of functions ``append`` or ``insert``. An example is given below:

```
// add an integer -1 to the beginning of the list
arr1.std::insert(-1, 0)
arr1.std::sprt()
```
<pre class='Tapas-Return'>
[-1, 3, 2, "Function at 0x600000b50240", 0]
</pre>
```
// add 4 to the end of arr
arr1.std::append(4)
arr1.std::sprt()
```
<pre class='Tapas-Return'>
[-1, 3, 2, "Function at 0x600000b50240", 0, 4]
</pre>


Elements of list can be removed from a list by either of functions ``pop`` or ``delete``. Here is an example:

```
// remove the element from the beginning of the list
arr1.std::pop()
arr1.std::sprt()
```
<pre class='Tapas-Return'>
[-1, 3, 2, "Function at 0x600000b50240", 0]
</pre>

```
// remove the last one.
arr1.std::delete(0)
arr1.std::sprt()
```
<pre class='Tapas-Return'>
[3, 2, "Function at 0x600000b50240", 0]
</pre>



List follows the shallow copy rule: only the pointer is stored if list contains an object of composite type.

```
let num = 3.3
let str = 'Hello'
let li1 = [1,2,3]
let li2 = [num, str, li1]
li1.std::sprt()
li2.std::sprt()
```
<pre class='Tapas-Return'>
[1, 2, 3]
[3.300000, Hello, "List at 0x600003a515c0"]
</pre>
```
num = num - 0.3
li2.std::sprt()
```
<pre class='Tapas-Return'>
[3.300000, Hello, "List at 0x600003a515c0"]
</pre>

```
str[0] = 'h'
li1[0] = 4
li2.std::sprt()
li2[2].std::sprt()
```
<pre class='Tapas-Return'>
[3.300000, hello, "List at 0x600003a515c0"]
[4, 2, 3]
</pre>




### 6.4. Pair

Pair is the binary tuple in Tape. It is created by the operator colon `:`. It is used to

- Support slice indexing. The slice in slice indexing is actually a pair.
- Define elements of dictionary. Each key-value pair of dictionary is actually a pair object.

We can think of a pair object as a special list object with only two elements: just like list, the elements of pair can also be indexed, and pair also follows the shallow copy rule.


Example.

Create a pair 'mypair' as ``'jey' : 0``

```
let mypair = 'jey' : 0
mypair
```
<pre class='Tapas-Return'>
jey : 0
</pre>


Check the basic informations of pair

```
mypair.std::print()
mypair.std::type() // 4 means composite type
mypair.std::len()
```
<pre class='Tapas-Return'>
"Pair at 0x60000123d340"
4
2
</pre>


Set the first element of 'mypair' as ``key``

```
mypair[0] = 'key'
mypair
```
<pre class='Tapas-Return'>
key : 0
</pre>


Set the value of 'mypair' as '1'

```
mypair[1] = '1'
mypair
```
<pre class='Tapas-Return'>
key : 1
</pre>


Copy 'mypair' as 'mypair2'

```
let mypair2 = mypair.std::copy()
mypair == mypair2
```
<pre class='Tapas-Return'>
true
</pre>


### 6.5. Dictionary

Dictionary is inherited from C++ STL class ``std::unsorted_map``. It contains ``key-value`` pairs. The ``key`` in dictionary should be string.

```
let dict = {
    'key_1' : 1,
    'key_2' : 2,
}
```


Dictionary can be indexed by string.

```
dict['key_1']
```
<pre class='Tapas-Return'>
1
</pre>

```
// change the value of the pair whose key is `key_2`
dict['key_2'] = 0
// add a new pair whose key is `key_3`
dict['key_3'] = 3
dict
```
<pre class='Tapas-Return'>
{
"key_3" : 3,
"key_2" : 0,
"key_1" : 1,
}
</pre>


Besides indexing with brackets, dictionary also support the ``read-only indexing`` with two colons, which cannot be used as left value in assignment (see 4.2):

```
// use the string 'key_2' to index dict
dict::key_2

// Wrong! '::' is read-only indexing
// dict::key_2 = 0
```
<pre class='Tapas-Return'>
0
</pre>


Elements can be added to dictionary by function ``append``. Here is an example:

```
dict.std::append('key_4' : 4)
dict
```
<pre class='Tapas-Return'>
{
"key_3" : 3,
"key_2" : 0,
"key_4" : 4,
"key_1" : 1,
}
</pre>


Elements can be removed from dictionary by function ``delete``. An example is given below:

```
// remove a pair whose key is 'key_1'.
dict.std::delete('key_1')
dict
```
<pre class='Tapas-Return'>
{
"key_3" : 3,
"key_2" : 0,
"key_4" : 4,
}
</pre>



Dictionary follows the shallow copy rule: only the pointer is stored if dictionary contains a value of composite type.


### 6.6. Function

Function is defined by the combination of parentheses and curly braces ``() {}``. Parameters are listed in
parentheses ``()``. Commands are placed between curly braces ``{}``.

Parameter passing in Tapas follows the shallow copy rule: objects of reference type only pass pointers into the function as parameter values.

```
let int_abs = (x) {
	if (x >= 0) {
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

```
std::print('Length                                = ', int_abs.std::len())
std::print('Print out directly           = ', int_abs)
std::print('Print out as a string      = ', int_abs.std::tostr())
std::print('int_abs == int_abs.std::copy() = ', int_abs == int_abs.std::copy())
```
<pre class='Tapas-Return'>
Length                                = 0
Print out directly           = "Function at 0x6000000882a0"
Print out as a string      = "Function at 0x6000000882a0"
int_abs == int_abs.std::copy() = false
</pre>


Parameter list written as `...` meaning that the number of parameters is undetermined. We can use function ``__param__(idx)`` to index each parameter, and use function ``__nparam__()`` to get the total number of parameters.

```
let mean = (...) {
	if (sys::__nparam__() == 0) {
		return 0
	}
	let sum = 0
	for (let i in 0 to sys::__nparam__()) {
		sum = sum + sys::__param__(i)
	}
	return sum / sys::__nparam__()
}
mean(2.0, 3.0)
```
<pre class='Tapas-Return'>
2.500000
</pre>


Functions in Tapas are ``anonymous``. If we want to refer to the function itself within its body, i.e., if we want to do recursion, we can use the key word ``this``, which will copy the function that it refers to.

```
let cumsum = (x) {
	if (x <= 0) {
		return 0
	}
	return x + this(x - 1)
}
cumsum(100).std::print()
```
<pre class='Tapas-Return'>
5050
</pre>


Functions with empty parameter list and contains only a ``return value`` command in its body can be concisely defined by  ``#{}``, as following

```
var x = 10.0
let expr = #{ x/3 }
expr()
```
<pre class='Tapas-Return'>
3.333333
</pre>

The above syntax is equivalent to

```
let expr2 = () { return x/3 }
expr2()
```
<pre class='Tapas-Return'>
3.333333
</pre>



### 6.7. Array

We use C++ ``eigen`` package for the implementation of 2 dimensional array. (n dimensional tensors are
not supported in yet). Tap support boolean array and double array.



#### 6.7.1. Creation

Array can be created by the function ``eig::toarr(rows, cols, value)``. For example, you can set all elements of your array to be the same value:

```
let ones = eig::toarr(3, 3, 1)
ones
```
<pre class='Tapas-Return'>
1 1 1
1 1 1
1 1 1
</pre>
```
let trues = eig::toarr(2, 3, true)
trues
```
<pre class='Tapas-Return'>
1 1 1
1 1 1
</pre>

or you can transform a list into an array

```
let seqs = eig::toarr(2, 3, [1, 2, 3, 6, 5, 4])
seqs
```
<pre class='Tapas-Return'>
1 2 3
6 5 4
</pre>
```
let booleans = eig::toarr(2, 3, [true, false, false, true, true, false])
booleans
```
<pre class='Tapas-Return'>
1 0 0
1 1 0
</pre>

For double array, integers would be transformed into double floats. For example,

```
seqs[0, 0]
```
<pre class='Tapas-Return'>
1.000000
</pre>
You can also use the function ``eig::random(rows, cols)`` to create a random matrix

```
let rad = eig::random(3, 3)
rad
```
<pre class='Tapas-Return'>
7.82637e-06     0.45865   0.0470446
   0.131538    0.532767    0.678865
   0.755605    0.218959    0.679296
</pre>
All elements of the random matrix follows uniform distribution in $[0, 1]$.



#### 6.7.2. Indexing

Array is indexable. You can get the elements an by indexing. Array supports value indexing and slice indexing.

```
seqs[1, 1]
seqs[1, 0:3] // the second row and columns 0, 1, 2
seqs[0:2, 1] // rows 0, 1 and the second column
seqs[0:2, 1:3]
```
<pre class='Tapas-Return'>
5.000000
6 5 4
2
5
2 3
5 4
</pre>
Also, you can set the elements of array by indexing

```
seqs[0, 0] = 0
seqs[0, 1:3] = eig::toarr(1, 2, [3, 5])
seqs[0:2, 0] = eig::toarr(2, 1, [1, 2])
seqs[1:2, 1:3] = eig::toarr(1, 2, [4, 6])
seqs
```
<pre class='Tapas-Return'>
1 3 5
2 4 6
</pre>


#### 6.7.3. Transpose and Pick Sub Array

Next, we will show the basic array transposing and sub array picking methods. Matrix transformation:

```
rad.eig::t()
```
<pre class='Tapas-Return'>
7.82637e-06    0.131538    0.755605
    0.45865    0.532767    0.218959
  0.0470446    0.678865    0.679296
</pre>
Use the function ``eig::top(n)`` to pick the first n rows of array as a sub array

```
rad.eig::top(2)
```
<pre class='Tapas-Return'>
7.82637e-06     0.45865   0.0470446
   0.131538    0.532767    0.678865
</pre>
Use the function ``eig::bottom(n)`` to pick the last n rows of array as a sub array

```
rad.eig::left(2)
```
<pre class='Tapas-Return'>
7.82637e-06     0.45865
   0.131538    0.532767
   0.755605    0.218959
</pre>
Use the function ``eig::left(n)`` to pick the left n columns of array as a sub array

```
rad.eig::left(2)
```
<pre class='Tapas-Return'>
7.82637e-06     0.45865
   0.131538    0.532767
   0.755605    0.218959
</pre>
Use the function ``eig::right(n)`` to pick the right n columns of array as a sub array

```
rad.eig::right(2)
```
<pre class='Tapas-Return'>
  0.45865 0.0470446
 0.532767  0.678865
 0.218959  0.679296
</pre>
Use the function ``eig::topleft(m, n)`` to pick the sub array of top-left corner by m rows and n columns

```
rad.eig::topleft(2, 2)
```
<pre class='Tapas-Return'>
7.82637e-06     0.45865
   0.131538    0.532767
</pre>

Use the function ``eig::topright(m, n)`` to pick the sub array of top-right corner by m rows and n columns

```
rad.eig::topright(2, 2)
```
<pre class='Tapas-Return'>
  0.45865 0.0470446
 0.532767  0.678865
</pre>
Use the function ``eig::bottomleft(m, n)`` to pick the sub array of bottom-left corner by m rows and n columns

```
rad.eig::bottomleft(2, 2)
```
<pre class='Tapas-Return'>
0.131538 0.532767
0.755605 0.218959
</pre>

Use the function ``eig::bottomright(m, n)`` to pick the sub array of bottom-right corner by m rows and n columns

```
rad.eig::bottomright(2, 2)
```
<pre class='Tapas-Return'>
0.532767 0.678865
0.218959 0.679296
</pre>


#### 6.7.4. Operators for double array

In this section we will show the supported operators for double array. We first create two random matrix

```
let rad1 = eig::random(3, 3)
let rad2 = eig::random(3, 3)
```

Addition

```
rad1 + rad2
```
<pre class='Tapas-Return'>
  1.31811   1.51774   1.37587
 0.450344  0.623549   1.19808
 0.936902  0.983898 0.0996631
</pre>

Subtraction

```
rad1 - rad2
```
<pre class='Tapas-Return'>
  0.551277   0.144193  -0.316467
   0.31666  -0.554405   0.144221
   0.10193  -0.876975 -0.0842667
</pre>

Multiplication

```
rad1 * rad2
```
<pre class='Tapas-Return'>
   0.358376    0.570684    0.448215
  0.0256341   0.0203622    0.353648
   0.216849   0.0497427 0.000707963
</pre>

Division

```
rad1 / rad2
```
<pre class='Tapas-Return'>
  2.43781   1.20996     0.626
  5.73742 0.0586986    1.2737
  1.24415 0.0574587 0.0837079
</pre>

Power

```
rad1 ^ rad2
```
<pre class='Tapas-Return'>
 0.974438  0.880586  0.584095
 0.937947   0.13783  0.810487
 0.760732 0.0655427  0.639178
</pre>

Matrix multiplication

```
rad1 @ rad2
```
<pre class='Tapas-Return'>
0.635062  1.62419  1.27748
0.429547 0.908203 0.404446
 0.20594 0.395371 0.468391
</pre>
Greater

```
rad1 > rad2
```
<pre class='Tapas-Return'>
1 1 0
1 0 1
1 0 0
</pre>

Less

```
rad1 < rad2
```
<pre class='Tapas-Return'>
0 0 1
0 1 0
0 1 1
</pre>

Equal

```
rad1 == rad2
```
<pre class='Tapas-Return'>
0 0 0
0 0 0
0 0 0
</pre>

Not equal

```
rad1 != rad2
```
<pre class='Tapas-Return'>
1 1 1
1 1 1
1 1 1
</pre>

Greater or equal to

```
rad1 >= rad2
```
<pre class='Tapas-Return'>
1 1 0
1 0 1
1 0 0
</pre>

Less or equal to

```
rad1 <= rad2
```
<pre class='Tapas-Return'>
0 0 1
0 1 0
0 1 1
</pre>


#### 6.7.5. Operators for boolean array

Operator and

```
rad1 > rad2 and rad1 <= rad2
```
<pre class='Tapas-Return'>
0 0 0
0 0 0
0 0 0
</pre>

Operator or

```
rad1 > rad2 or rad1 <= rad2
```
<pre class='Tapas-Return'>
1 1 1
1 1 1
1 1 1
</pre>


### 6.8. Library

Library is the module type in Tapas. It is created by ``import`` statement.

Example: Let's import our unit test file `3_Examples.md`:

```
import 3_Examples.md as lib2
```
<pre class='Tapas-Return'>
10
Length                                = 0
Print out directly           = "Function at 0x600003e103c0"
Print out as a string      = "Function at 0x600003e103c0"
int_abs == int_abs.std::copy() = false
2.500000
5050
3.333333
3.333333
</pre>
We can see the program execute the file `6_Function.md` and print out all its outputs. Now let's print out the lib

```
lib2
```
<pre class='Tapas-Return'>
{
}
</pre>
It is empty...

That is because we didn't ``return`` anything in the file `6_Function.md`.

More details about module importing will be introduced in the Section [5](#5-environment) of Environment.



