---
layout: post
title: Tapas Programming Language
use_math: false
---



# 1.1. Primitive Types

Generally speaking, there are two kind of primitive types in Tap, `value type` and `composite type`.

Consistent with other popular programming languages with virtual machines (such as Java), object of value types refers to specific values while composite types refers to a pointer.

<br>

## 1.1.1. Nil

A value of `nil` means null, empty or nothing. It cannot be explicitly created by users.

```
std::print('The return of function "print" is a "nil" whose type is').std::type()
```

<pre class='Tapas-Return'>
The return of function "print" is a "nil" whose type is
Nil
</pre>

<br>

## 1.1.2. Bool


A boolean value could be either `true` or `false`. It can be obtained by literal `true` and `false` or by logical calculations (1.2.2).

```
std::type(2 > 1)
false; true
```

<pre class='Tapas-Return'>
Bool
false
true
</pre>


<br>

## 1.1.3. Int

Int type in Tapas is in essence `long int` in C++.

```
std::type(0)
2 / 3
```

<pre class='Tapas-Return'>
Int
0
</pre>


Note that the boolean `true` and `false` are in essence integers `1` and `0`, which is the same as in Python.

```
true == 1
false == 0
```

<pre class='Tapas-Return'>
true
true
</pre>

<br>

## 1.1.4. Float

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


<br>

## 1.1.5. Primitive Composite Types

Tapas provides the following compositive types by default:

- Itarator
- String
- List
- Pair
- Dictionary
- Function
- Array of Values Types
- Library

```
std::type('String is a composite type')
```

<pre class='Tapas-Return'>
String
</pre>


Please see Chapter 1.6 for this part.
