---
layout: post
title: Tapas Programming Language
use_math: false
---



# 1.3. Expressions

Briefly speaking, an expression will leave an object at the top of virtual machine stack.

In this section, we will illustrate the priority of different expressions in Tapas. Here is an example of parsing expressions according to the rule of priority:

```
let arr = [1, 2, 3]  // create a list of integers
-1 * 2 * 2 / std::toint(2^2) + arr[0] in 0 to arr[2]
```

<pre class='Tapas-Return'>
true

When the propriety is not clear, always use parenthesis `()`.

<br>

## 1.3.1. Value

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

<br>

## 1.3.2. Calling and indexing

Expressions of the second priority are calling and indexing expressions.

### 1.3.2.1. Indexing

The "Indexing" expression is of the same priority as calling expression, which looks like `arr[idx]`. Here `arr` is a indexable value, and `idx` is the index.

```
arr[0]
```

<pre>
1
</pre>


### 1.3.2.2. Read-only indexing

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

```
d::add_one
```

<pre>
"Function at 0x107600f20"

### 1.3.2.3. Calling

The "Calling" expression looks like `f(a,b,c,...)`, where `f` is a callable value and `a`, `b`, `c`, `...` are parameters.

```
d::add_one(d::arr[0])
```

<pre class='Tapas-Return'>
2
</pre>


### 1.3.2.4. Tunnel expression

Calling expression `f(a,b,c,...)` can be reformed into the equivalent `tunnel` form `a.f(b, c, ...)`.

```
d::arr[0].d::add_one()
```

<pre class='Tapas-Return'>
2
</pre>


<br>

## 1.3.3. Arithmetic operations

Expressions of the third priority are arithmetic operations.

- Third order:  `^` (power)
- Second order:  `*` (multiplication), `/` (devision), `%` (modulo calculation) and `@` (matrix multiplication)
- First order:  `+` (addition) and `-` (subtraction)

Higher order arithmetic operations have higher priorities than lower order operations.

```
2.3 + 4 / 3.0 * 2^2
```

<pre class='Tapas-Return'>
7.633333
</pre>


<br>

## 1.3.4. Logic operations: order 3

Expression of the 4th priority is third order logical expression. The logical operators in this level include `>`, `<`, `>=`, `<=`, `==`, `!=`. Note that floats cannot be applied into `==` for equality judgment.

```
5 >= 5.0
```

<pre class='Tapas-Return'>
true
</pre>


<br>

## 1.3.5. Logic operations: order 2

Expression of the 5th priority is second order logical expression. The logical operators in this level include `and` and `or`, the logical and operation and logical or operation. A `nil` would be returned if either side is not logical expression (boolean value or the expression that returns boolean value).

```
5 > 4 or 3 > 6.0
5 > 4 and 3 > 6.0
```

<pre class='Tapas-Return'>
true
false
</pre>


<br>

## 1.3.6. To expression

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


<br>

## 1.3.7. Pair expression

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

<br>

## 1.3.8. Logic operations: order 1

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

