---
layout: post
title: "Tapas Programming Language"
use_math: false
---



# 2.1. Mechanism

## 2.1.1. Overview

Tapas source code can be divided into four parts: compilation, runtime, virtual machine and C++ interaction APIs.

Roughly speaking, Tapas uses a single traversal to generate virtual machine instructions, and then uses a stack-type virtual machine to load, interprets and executes those instructions.

In addition, Tapas maintains an environment system at runtime to manage variables. Also, Tapas keeps a constant-value table that is generated in compilation for constants (integers, floating-point numbers, and strings) indexing.

Similar to Lua, Tapas uses a recursive descent algorithm to traverse the source code in order to generate bycode. Please refer to the ``tsyner`` class for compilation algorithms.

In compilation, a register counter (in class ``treg_ctr``) and a list of variable names (in class ``tobj_ctr``) are maintained. Based on the two recorders, we determine the location of each variable in the corresponding environment.

After compilation, all variable names will be replaced with its relative location in the environment tree. Tapas will not store the names of any variable names in the runtime.

In the execution period, the Tapas virtual machine reads bycodes from binary files with suffix ``.tapc``, and loads the constant table and instruction lists from the binary file. Then, virtual machine interprets the instructions in order.

After execution, expressions would leave a returned value on the top of the virtual machine stack, while statements clear the stack.

Here is an example:

```tapas
var a : int
a = 1 + 2
```

The first line is a variable declaration statement.

The virtual machine claims a memory space in the root environment (library) to store the value of this variable. After the execution of this statement, there is no value left in the virtual machine stack.

The second line is an assignment statement, which contains an addition expression.

For the right hand side of the assignment statement, the virtual machine first pushes values ``1`` and ``2`` from integer constant table into the top of virtual machine (VM) stack, and adds them, and clear the top two elements of VM stack, then stores the returned value of addition operation ``1+2`` on the top of VM stack. After the execution of this addition expression, there is a returned value ``3`` left at the top of VM stack.

Then, program will execute the assignment statement. Pick the value from the top of VM stack and move it into the memory address of the variable ``a``, and then clear the value at the top of VM stack. After assignment statement, no value is left in VM stack.

<br>

## 2.1.2. Garbage collection

Tapas uses reference counting for garbage collection of reference type values.

The reference counting algorithm is implemented in the class ``tcompo_v``: each reference type maintains a 16-bit short integer unsigned number to record the number of references to the value.

There are four places in Tapas that can be used to store values,

- **Case 1.** variable list in the environment
- **Case 2.** values of collection types
- **Case 3.** runtime stack of virtual machine
- **Case 4.** returned value register of virtual machine ``tvm::__ret``

Generally speaking, wherever a reference type is placed in any of the above 4 positions, the reference count of this value needs to be increased by 1. (In practice, only the first two positions are tracked, and the latter two are ignored, since the virtual machine is only regarded as a computing process, not a warehouse for data storage.)

The reference counting rules are very simple:

- **Case 1.** When a reference type value is just created in memory, its reference count is zero.
- **Case 2.** When a reference type value is just created in memory, it must be put in one of the above four places.
- **Case 3.** When a reference type value is referred by a variable name, the reference count is increased by one. Correspondingly, if it is discarded by a variable name, the reference count is reduced by one.
- **Case 4.** When a reference type value is enclosed by a collection (such as ``tpair``, ``tlist`` or ``tdict``), the references count is increased by one. Correspondingly, if discarded by the collection, the reference count is reduced by one.
- **Case 5.** The returned value of the returned value register in virtual machine is always pushed on the top the stack after the return command ends.
- **Case 6.** Whenever a period of virtual machine running process (that is, a statement) ends, the running stack is always been cleared (reference values' reference count are deducted by one). The reference type value is released when its reference count reaches 0.
- **Case 7.** Reference type values will be checked whenever its reference count decreases. When its reference count is reduced to 0, the value is released.

