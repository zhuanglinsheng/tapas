---
layout: post
title: "Tapas Programming Language"
use_math: false
---



# 1.6.5. Dictionary

Dictionary is inherited from C++ STL class ``std::unsorted_map``. It contains ``key-value`` pairs. The ``key`` in dictionary should be string.

```
let dict = {
    'key_1' : 1,
    'key_2' : 2,
}
```

<br>

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
<br>

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

<br>

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

<br>

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

<br>

Dictionary follows the shallow copy rule: only the pointer is stored if dictionary contains a value of composite type.
