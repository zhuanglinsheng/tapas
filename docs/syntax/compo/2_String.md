---
layout: post
title: "Tapas Programming Language"
use_math: false
---



# 1.6.2. String

String is inherited from C++ STL string class ``std::string``. It has the following attributes

- 
- 

It can be obtained by single quote '...' or double quote "...". 

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

<br>

## 1.6.2.1. Indexing

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

<br>

## 1.6.2.2. Append

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

<br>

## 1.6.2.3. Insert

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


<br>

## 1.6.2.4. Pop

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

<br>

## 1.6.2.5. Delete

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

<br>

## 1.6.2.6. Compare strings

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
