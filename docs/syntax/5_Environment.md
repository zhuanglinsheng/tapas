---
layout: post
title: "Tapas Programming Language"
use_math: false
---



# 1.5. Environment

Environment is a key concept in Tapas. Concepts like function scope, closure, recursion, and modules are all related to the environment.

Environment is an abstract type in Tapas. `tfunc` and `tlib` are sub-types of Environment. All these sub-types are child classes of `tcompo_env` in the C++ source code. When we launch a Tapas session, it maintains a root environment.

<br>

## 1.5.1. Environment tree

Environments in Tapas are organized as a tree. When we define a Tapas function, it creates a child environment. Each environment contains a pointer to its parent environment. We can use `this` to get a copy of the current environment and `base` to get a copy of the parent environment.

Environment maintains a variable list for storage and reference. All variables declared in the scope of an environment are stored in the respective variable list. 

<br>

## 1.5.2. Environment Scope

In Tapas, commands in child environments can directly read and modify variables declared in father environment, while commands in the father environment can not directly affects those variables declared in child environments. If we want to modify some variables of child environment in father environment, we have to ``return`` them.

Note that, the statements like ``if``, ``while`` and ``for`` don't have environment scope.

<br>

## 1.5.3. Executable environment

For executable environment values (such as functions, etc.), a new virtual machine will be temporarily created each time it is executed. The commands in the sub-environment will be executed on this temporary virtual machine. The temporary virtual machine is only responsible for the operation of the sub-environment program instructions. Once execution ends, the returned value of the child environment program will be pushed onto the top of the stack of the virtual machine running in the parent environment program, and the temporary virtual machine will be destroyed.

For functions, after each function calling occurs, the values of local variables in the function will not be released immediately, but be cached in the variable list. These values will be overwritten when the next function call occurs. This brings some conveniences, such as the support of closures. However, the problem with this design is also very obvious: The memory footprint of the environment is very large, which may cause efficiency problems.

<br>

## 1.5.4. Higher-order functions

Since function is also a value in Tap, the function itself can return a function. Functions that return functions are called higher-order functions. When Tapas tries to search a variable while the returned function is executed, that variable is indexed according to the environment tree, which is called the "closure". Here is an example of a higher-order function:

```
var f1: fn = () {
	var x: int = 0
	return () {
		x = x + 1
		std::print(x)
	}
}
var f2: fn = f1()
f2
f2()
var f3: fn = f1()
f3
f2()
f3()
```

<pre class='Tapas-Return'>
"Function at 0x600000144180"
1
"Function at 0x6000001442a0"
1
2
</pre>
Note that ``f2`` and ``f3``, as the returned value of ``f1``, are not identical, because their locations in memory are different. Yet their father environments are both ``f1``. Thus, when we execute the two functions, the variable ``x`` in environment ``f1`` will be added by 1 on 0, while when ``f1`` is executed again, it is re-assigned to be 0.

<br>

## 1.5.5. Function calling and recursion

Tapas has two kind of functions, ``tfunc`` (defined by Tapas script) and ``cppfunc`` (defined by C++). They are all sub classes of ``tcompo_eval``. ``tfunc`` is also the child class of ``tcompo_env``, meaning that it is an environment.

Since all functions in Tapas are anonymous, we cannot implement the recursion by calling them by name. Yet, we can use ``this`` (the keyword for function environment copying) to represent the recursion function.

```
// 'this' refers to the function
var sum: fn = (x) {
	if (x > 1) {
		return x + this(x - 1)
	}
	else{
		return 1
	}
}
sum(500)
501 * 250
```

<pre class='Tapas-Return'>
125250
125250
</pre>
Since ``this`` is a copy of the current environment, it has an empty variable list of the same length as the current environment, and it shares the same father environment as the current environment. Thus, the original function is intact when the copied one is executed.

<br>

## 1.5.6. Further explanations about recursion

We mentioned at the end of Section 5 that local reference variables cannot be returned in recursion. Here is another example:

<pre>
// Line 4: try to return the local variable y
// Line 7: returning a locally created expression is fine
var expand_list: fn = (x) {
	var y: any = x.std::copy()
	if (y.std::len() <= 1) {
		return y
	}
	y.std::pop(0)
	return std::union(y, this(y))
}
[1,2,3,4,5,6,7].expand_list()
</pre>

The correct version is 

```
// Line 3: returning x which is passed in from outside is fine
var expand_list_1: fn = (x) {
	if (x.std::len() <= 1) {
		return x
	}
	var y: any = x.std::copy()
	y.std::pop()
	return std::union(y, this(y))
}
[1,2,3,4,5,6,7].expand_list_1()
```


<pre class='Tapas-Return'>
[1, 2, 3, 4, 5, 6, 1, 2, 3, 4, 5, 1, 2, 3, 4, 1, 2, 3, 1, 2, 1, 1]
</pre>

Or you can write it in this way: 

```
// Line 4: returning an expression is fine,
//         yet too many copies.
var expand_list_2: fn = (x) {
	var y: any = x.std::copy()
	if (y.std::len() <= 1) {
		return y.std::copy()
	}
	y.std::pop()
	return std::union(y, this(y))
}
[1,2,3,4,5,6,7].expand_list_2()
```


<pre class='Tapas-Return'>
[1, 2, 3, 4, 5, 6, 1, 2, 3, 4, 5, 1, 2, 3, 4, 1, 2, 3, 1, 2, 1, 1]
</pre>
<br>


## 1.5.7. tlib: Import a single file

A file stands for a module. By importing a module we get a value of ``tlib``.

```
// execute `model_1.tap` without import it into current environment
import module_1.tap

// import a file (module) `model_1.tap` and create a variable lib
import module_1.tap as lib
lib.std::type()
```

<pre class='Tapas-Return'>
Library
</pre>

The values defined in one file is not observable by other files because they belong to different branches of the environment tree, unless we return the values that we want at the end of the file. The returned values should be wrapped in a ``tdict`` value.

<pre>
return {
	'obj_1' : 'some object 1',  // return obj_1 by naming it as 'obj_1'
	'obj_2' : 'some object 2',  // return obj_2 by naming it as 'obj_2'
}
</pre>

Here is a concrete example of the module importing work flow. In ``module_1.tap``, we define two functions and return them.

<pre>
var print_three: fn = () {
	std::print(3.0)
}
var get_five: fn = () {
	return 5.0
}
return {
	'print_3' : print_three,
	'get_5'   : get_five,
}
</pre>


In ``demo-lib-2.tap``, we import the module ``demo-lib-1.tap`` and create a variable ``m1``.

```
import module_1.tap as m1
m1::print_3()
m1::get_5().std::print()
```


Here ``m1`` is a variable of ``tlib`` type value. Thus, we can index the returned values by using read-only string indexing on ``m1``.

Note that modules cannot be imported by each other, causing the looping-importing problem. Tapas will not check for the looping-importing problem in compilation. Modules cannot be copied, so we cannot use ``this`` or ``base`` in modules.

<br>

## 1.5.8. tlib: How to import a folder?

If your modules are put under a folder where there is a file ``__init__.tap``, then Tapas also supports to "import" a folder. For example, let us say currently your folder has the structure

<pre>
|---- module_1.tap
|---- mymodule/
    |---- __init__.tap
    |---- ....
    |---- file_2.tap
    |---- file_1.tap
    |---- LICENSE
    |---- README.md
</pre>
We can directly use ``import`` statement to import the whole folder.

```
import mymodule as A0
```


In compilation, Tapas will check whether ``mymodule`` is a file or a folder. If it is a file, Tapas will import it as a file. If it is a folder, Tapas will only import the ``__init__.tap`` file under this folder. Thus, the above command is actually equivalent to

```
import mymodule/__init__.tap as A1
```


We need to edit the ``__init__.tap`` file manually. For example, below is my file ``mymodule/file_1.tap``,

<pre>
var abs: fn = (x) {
	if (x >= 0) {
		return x
	}
	else{
		return -x
	}
}
var inv: fn = (x) {
	return 1/x
}
return {
	'abs' : abs,
	'inv' : inv,
}
</pre>

where we return methods ``inv`` and ``abs`` from ``file_1.tap``. Suppose we want to expose these methods to other users. We can edit ``__init__.tap`` in this way:

<pre>
import file_1.tap as file_1
return {
	'author'  : 'author name',
	'module'  : 'test',
	'version' : 1,
	'mail'    : 'author@server',
	'file_1'  : file_1,
	'inv'     : file_1::inv,
	'abs'     : file_1::abs
}
</pre>

Finally, let's try to import ``mymodule`` and call the ``inv`` and ``abs`` methods.

```
import mymodule as A
A::file_1::abs(-10)
A::file_1::inv(-10)
A::inv(-10)
```

<pre class='Tapas-Return'>
10
0
0
</pre>
