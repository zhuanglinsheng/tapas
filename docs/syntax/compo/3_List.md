---
layout: post
title: "Tapas Programming Language"
use_math: false
---



# 1.6.3. List

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
<br>

Copy list. The location of the copied object is different from the original:

```
arr1.std::print()
arr1.std::copy().std::print()
```
<pre class='Tapas-Return'>
"List at 0x600003a51300"
"List at 0x600003a51480"
</pre>
<br>

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
<br>

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
<br>

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

<br>

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
<br>

