---
layout: post
title: "Tapas Programming Language"
use_math: false
---



# 1.6.4. Pair

Pair is the binary tuple in Tape. It is created by the operator colon `:`. It is used to

- Support slice indexing. The slice in slice indexing is actually a pair.
- Define elements of dictionary. Each key-value pair of dictionary is actually a pair object.

We can think of a pair object as a special list object with only two elements: just like list, the elements of pair can also be indexed, and pair also follows the shallow copy rule.

<br>

Example.

Create a pair 'mypair' as ``'jey' : 0``

```
let mypair = 'jey' : 0
mypair
```
<pre class='Tapas-Return'>
jey : 0
</pre>

<br>

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
<br>

Set the first element of 'mypair' as ``key``

```
mypair[0] = 'key'
mypair
```
<pre class='Tapas-Return'>
key : 0
</pre>
<br>

Set the value of 'mypair' as '1'

```
mypair[1] = '1'
mypair
```
<pre class='Tapas-Return'>
key : 1
</pre>
<br>

Copy 'mypair' as 'mypair2'

```
let mypair2 = mypair.std::copy()
mypair == mypair2
```
<pre class='Tapas-Return'>
true
</pre>
