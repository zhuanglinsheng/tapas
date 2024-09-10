---
layout: post
title: "Tapas Programming Language"
use_math: false
---



# 1.4. Statements 

Briefly speaking, an expression will leave no object at the top of virtual machine stack. 

In this section, we will introduce the statements in Tapas.

<br>

## 1.4.1. Environmental variable declaration

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

<br>

## 1.4.2. Temporary variable declaration

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

<br>

## 1.4.3. Assignment

The left hand side of ``=``  is the variable name (lvalue), and the right hand side of ``=`` is the respective rvalue to be assigned to the lvalue. Right value CANNOT be ``nil``. Before assignment, variables must be declared.

For reference types, value assignment is not copy. After reference variable as the rvalue in assignment, the two variable values will point to the same memory space.

All default objects CANNOT be assigned values.

The ``nil`` value cannot be assigned to other variables as an rvalue. For example:

<pre>
var ret_pnt = std::print('hello')
</pre>

The code gives us error since the return of `print` function is `nil`.

<br>

## 1.4.4. Branching statement

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

Here ``condition_1`` and ``condition_2`` are logical expression or boolean value. If they are  ``false``, jump happens. ``command_1``, ``command_2`` and ``command_3`` are code blocks. Any one of them were executed and the program would jump out of the branching statement. Note that curly braces must be putted where they are in the example.

<br>

## 1.4.5. Loop statement: for

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

<br>

## 1.4.6. Loop statement: while

The ``while`` loop statement looks like

```
let condition_3 = false

while (condition_3) {
	// commands
}
```

``condition`` is logically expression or boolean value. Jump will happen if it is ``false``, otherwise the program keep executing the ``commands``.

<br>

## 1.4.7. break and continue

Tapas supports ``break`` and ``continue`` to control the looping process. ``break`` means to jump out of loop while ``continue`` means to jump to the next loop.

<br>

## 1.4.8. Return

The return value statement can be parameterized like ``return value`` or of no parameter like ``return``. In return value statement, Tapas would clear all values on the virtual machine stack, copy ``value`` from environment to ``tapas::tvm::__rev``, and jump out of the current environment.

<br>

## 1.4.9. Module import

The import statement looks like `import file.tap [as library]`. 

See Section 5 for more details of this part. 
