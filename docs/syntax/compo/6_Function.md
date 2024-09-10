---
layout: post
title: "Tapas Programming Language"
use_math: false
---



# 1.6.6. Function

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
<br>

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
<br>

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
<br>

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
<br>
