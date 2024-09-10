---
layout: post
title: "Tapas Programming Language"
use_math: false
---



# 1.6.8. Library

Library is the module type in Tapas. It is created by ``import`` statement.

Example: Let's import our unit test file `1.2.6_Function.md`:

```
import 6_Function.md as lib
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
lib
```
<pre class='Tapas-Return'>
{
}
</pre>
It is empty... That is because we didn't ``return`` anything in the file `6_Function.md`. This part will be covered soon in Chapter 4. More details about module importing will be introduced in the Chapter 4 of Environment.

