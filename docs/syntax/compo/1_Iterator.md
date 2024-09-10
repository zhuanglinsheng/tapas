---
layout: post
title: "Tapas Programming Language"
use_math: false
---



# 1.6.1. Iterator

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
