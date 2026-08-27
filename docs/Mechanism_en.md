# How Tapas Works

[简体中文](Mechanism_zh.md) | English

Tapas can be understood through four layers: compilation, runtime state, the
virtual machine, and the C interaction API.

At a high level, Tapas compiles source code into virtual-machine instructions
with a single traversal, then executes those instructions on a stack-based
virtual machine.

At runtime, Tapas maintains an environment system for variables. During
compilation, it also builds constant tables for integers, floating-point values,
and strings.

<br>

## Introduction

Like Lua, Tapas uses a recursive descent compiler to traverse source code and
generate bycode. See the compiler implementation for the details.

During compilation, Tapas maintains a register counter (``treg_ctr``) and a
variable-name table (``tobj_ctr``). These structures determine where each
variable is stored in the corresponding environment.

After compilation, variable names are replaced by relative locations in the
environment tree. The runtime does not resolve ordinary variable names by their
source text.

During execution, the Tapas virtual machine can read bycodes from ``.tapc``
binary files. It loads the constant tables and instruction list, then interprets
the instructions in order.

After an expression is executed, its result is left on top of the VM stack.
Statements consume or clear their intermediate stack values.

Here is an executable example:

```tapas
var a = 0
a = 1 + 2
```

The first line is a variable declaration statement.

The virtual machine allocates a slot in the root environment, which is a
library, for this variable. After this declaration statement runs, no value is
left on the VM stack.

The second line is an assignment statement, which contains an addition expression.

For the right-hand side of the assignment, the virtual machine first pushes
``1`` and ``2`` from the integer constant table onto the VM stack. It then adds
them, removes the operands, and leaves the result ``3`` on top of the stack.

The assignment then moves the value from the top of the VM stack into the slot
for ``a`` and clears the stack. After the assignment statement finishes, the VM
stack is empty again.

<br>

## Garbage collection

Tapas uses reference counting to manage reference-type values.

The reference-counting mechanism is built around ``tcompo_v``. Each composite
value stores an integer counter that records how many owning references point
to the value.

Tapas values can be stored in four main places:

- **Case 1.** the variable list in an environment
- **Case 2.** collection values
- **Case 3.** the virtual machine runtime stack
- **Case 4.** the virtual machine result register, ``tvm::rev``

In principle, when a reference value is stored in any of these places, its
reference count should increase by one. In practice, Tapas tracks the first two
storage locations. The VM stack and return register are treated as transient
execution state rather than long-term storage.

The reference counting rules are very simple:

- **Case 1.** A newly created reference value starts with a reference count of zero.
- **Case 2.** A newly created reference value must then be placed somewhere that owns or uses it.
- **Case 3.** When a variable refers to a reference value, the reference count increases by one. When the variable releases it, the count decreases by one.
- **Case 4.** When a collection, such as ``tpair``, ``tlist``, or ``tdict``, stores a reference value, the reference count increases by one. Dense arrays instead own their contiguous scalar storage directly. When a collection releases a composite element, its reference count decreases by one.
- **Case 5.** A function return value is pushed onto the VM stack after the return instruction finishes.
- **Case 6.** When a statement finishes, the VM clears its transient stack values.
- **Case 7.** Whenever a reference count decreases, Tapas checks the value. If the count reaches zero, the value is released.

<br>

## Bycode Description

Tapas bycode instructions are abstract VM instructions, not real CPU
instructions.

Like Lua VM instructions, Tapas bycodes are represented as unsigned integers.
Tapas currently has 48 instructions.

Each bycode is 32 bits long. The first 6 bits store the instruction code, which
allows up to 64 instructions. The remaining 26 bits store instruction
parameters.

Instructions are grouped by bit layout. See the ``tbycode`` implementation for
details.



### No Parameters

<embed>
<p></p>
<div style="width:120px;height:26px;border-width: thin;border-style:solid;display:inline-block;flex:none;text-align:center;">
  Ins (6 bit)
</div>
<div style="width:400px;height:26px;border-width: thin;border-style:dashed;display:inline-block;text-align:center;">
  Unused (26 bit)
</div>
<p></p>
</embed>

- ``OP_PASS`` Do nothing.
- ``OP_THIS`` Push the value representing the current environment onto the stack.
- ``OP_BASE`` Push the value representing the parent environment onto the stack.
- ``OP_BREAK`` Jump to the location after the next ``OP_JUMPB``.
- ``OP_CONTI`` Jump to the location before the next ``OP_JUMPB``.
- ``OP_RET`` Return the stack top, clear the stack, and jump to the end of the instruction list.
- ``OP_IN`` Pop the top two stack values as parameters, call the ``in`` operator, and push the result.
- ``OP_PAIR`` Pop the top two stack values as parameters, call the pair operator, and push the result.
- ``OP_TO`` Pop the top two stack values as parameters, call the ``to`` operator, and push the result.



### U (1 parameter)

<embed>
<p></p>
<div style="width:120px; height:26px; border-width:thin; border-style:solid; display:inline-block;text-align:center;">
  Ins (6 bit)
</div>
<div style="width:400px; height:26px; border-width:thin; border-style:solid; display:inline-block;text-align:center;">
  U (26 bit)
</div>
<p></p>
</embed>

- ``OP_VCRT isenv`` Create a new variable in the current environment or temporary area.
- ``OP_TMPDEL oloc`` Delete temporary variables from the current virtual machine.
- ``OP_JPF ncmd`` Jump forward by ``ncmd`` instructions.
- ``OP_JPB ncmd`` Jump backward by ``ncmd`` instructions.
- ``OP_CJPFPOP ncmd`` Require a boolean on top of the stack. If it is ``false``, pop it and jump forward by ``ncmd`` instructions.
- ``OP_CJPBPOP ncmd`` Require a boolean on top of the stack. If it is ``false``, pop it and jump backward by ``ncmd`` instructions.
- ``OP_PUSHI cloc`` Push the integer constant at ``cloc`` onto the stack.
- ``OP_PUSHFLT cloc`` Push the floating-point constant at ``cloc`` onto the stack.
- ``OP_PUSHB b`` Push boolean ``b`` onto the stack.
- ``OP_PUSHS cloc`` Push the string constant at ``cloc`` onto the stack.
- ``OP_PUSHDICT n`` Pop the top ``n`` stack values and use them to create a ``tdict`` value.
- ``OP_PUSHINFO u`` Push unsigned integer metadata ``u`` onto the stack.
- ``OP_IMPORT cloc`` Import a Tapas file whose path is stored as string constant ``cloc``.
- ``OP_IDXR n`` Use the top ``n`` stack values as index arguments and the next value as the indexable object. Pop them and push the indexing result.
- ``OP_EVAL n`` Use the top ``n`` stack values as call arguments and the next value as the callable object. Pop them and push the call result.



### LR (2 parameters)

<embed>
<p></p>
<div style="width:120px; height:26px; border-width:thin; border-style:solid; display:inline-block;text-align:center;">
  Ins (6 bit)
</div>
<div style="width:205px; height:26px; border-width:thin; border-style:solid; display:inline-block;text-align:center;">
  L (13 bit)
</div>
<div style="width:205px; height:26px; border-width:thin; border-style:solid; display:inline-block;text-align:center;">
  R (13 bit)
</div>
<p></p>
</embed>

- ``OP_POPN nreg, interactive`` Pop ``nreg`` values from the top of the stack.
- ``OP_POPCOV oloc, isenv`` Pop the stack top and assign it to the variable at ``oloc``.
- ``OP_LOOPAS oloc, isenv`` Require an iterable value on top of the stack. Advance the iterator, assign the current item to the variable at ``oloc``, and push a boolean indicating whether iteration should continue.
- ``OP_PUSHX oloc, isenv`` Push the variable at ``oloc`` onto the stack.



### CP (2 parameters)

<embed>
<p></p>
<div style="width:120px; height:26px; border-width:thin; border-style:solid; display:inline-block;text-align:center;">
  Ins (6 bit)
</div>
<div style="width:280px; height:26px; border-width:thin; border-style:solid; display:inline-block;text-align:center;">
  C (18 bit)
</div>
<div style="width:120px; height:26px; border-width:thin; border-style:solid; display:inline-block;text-align:center;">
  P (8 bit)
</div>
<p></p>
</embed>

- ``OP_PUSHF ncmds nparams`` Create a ``tfunc`` with ``nparams`` parameters from the next ``ncmds`` instructions and push it onto the stack.



### iLR (3 parameters)

<embed>
<p></p>
<div style="width:120px; height:26px; border-width:thin; border-style:solid; display:inline-block;text-align:center;">
  Ins (6 bit)
</div>
<div style="width:70px; height:26px; border-width:thin; border-style:solid; display:inline-block;text-align:center;">
  i (2 bit)
</div>
<div style="width:170px; height:26px; border-width:thin; border-style:solid; display:inline-block;text-align:center;">
  L (12 bit)
</div>
<div style="width:170px; height:26px; border-width:thin; border-style:solid; display:inline-block;text-align:center;">
  R (12 bit)
</div>
<p></p>
</embed>

Instructions with three parameters are mostly binary arithmetic and logical
operations, and they share the same structure. The addition instruction is a
representative example.

- ``OP_ADD i L R`` Here ``i = 0, 1, 2, 3`` means:

  - 0 - both sides are literals
  - 1 - the left-hand side is a variable and the right-hand side is a literal
  - 2 - the left-hand side is a literal and the right-hand side is a variable
  - 3 - both sides are variables

  ``L`` and ``R`` are the locations of the left-hand and right-hand values in
  the constant table, when the value is a literal, or in the variable list, when
  the value is a variable.

- ``OP_IDXL oloc, nparams, isenv`` Use the variable at ``oloc`` as an indexable value. Use the top ``nparams`` stack values as index arguments, assign the target location with the next stack value, and pop the consumed values.

<br>

## Example

We try to check the bycodes of the following function:

```tapas
var odd_multiples_of_three = (){
    for(let i in 0 to 10){
        if(i % 2 != 0 and i % 3 == 0){
            print(i)
        }
    }
}

odd_multiples_of_three()
```
<pre class='Tapas-Return'>
3
9
</pre>
This program prints out all odd numbers that are multiples of 3 among 0 and 9.

The same function can be imported from `examples/test_bycodes.tap`:

```tapas
import examples/test_bycodes.tap as lib
var fn = lib['odd_multiples_of_three']
fn()
```
<pre class='Tapas-Return'>
3
9
</pre>


### Command Line

The simplest way to inspect the generated bycodes is the command-line option
``-cr``, which compiles a source file and then displays the compiled bycodes:

```sh
./build/bin/tapas -cr docs/examples/test_bycodes.tap
```



### C API

The same operation is also available from the C session API:

```c
#include "tapas/tapas.h"

int main(void)
{
    tsession *sess = tsession_new();
    const char *src_codes = "docs/examples/test_bycodes.tap";

    tsession_compile_file(sess, src_codes, 1);
    tsession_show_bycodes(sess, src_codes);
    tsession_free(sess);
    return 0;
}
```



### Inside Tapas

Inside Tapas, use the debugging function ``__binary__([env])`` to print
bytecode. The function accepts an optional environment value. Without arguments,
``__binary__`` prints the current library or module wrapper.

With a library argument, ``__binary__`` prints that library's wrapper:

```tapas
__binary__(lib)
```
<pre class='Tapas-Return'>
[0]OP_VCRT     0  1
[1]OP_PUSHINFO 0
[2]OP_PUSHINFO 1
[3]OP_PUSHINFO 5
[4]OP_PUSHINFO 0
[5]OP_PUSHF    35
[6]OP_PUSHI    0
[7]OP_PUSHI    1
[8]OP_TO
[9]OP_VCRT     1  0
[10]OP_LOOPIAS  0  0
[11]OP_CJPFPOP  27
[12]OP_PUSHI    1
[13]OP_PUSHI    2
[14]OP_PUSHX    0  tmp
[15]OP_PUSHINFO 0
[16]OP_MOD      0  1
[17]OP_PUSHINFO 0
[18]OP_NE       0  1
[19]OP_CJPFPOP  11
[20]OP_PUSHI    1
[21]OP_PUSHI    3
[22]OP_PUSHX    0  tmp
[23]OP_PUSHINFO 0
[24]OP_MOD      0  1
[25]OP_PUSHINFO 0
[26]OP_EQ       0  1
[27]OP_PUSHB    1
[28]OP_PUSHINFO 0
[29]OP_AND      0  1
[30]OP_JPF      1
[31]OP_PUSHB    0
[32]OP_CJPFPOP  5
[33]OP_PUSHX    0  tmp
[34]OP_PUSHX    0  upval 1
[35]OP_EVAL     1
[36]OP_POPN     1  1
[37]OP_PASS
[38]OP_JPB      29
[39]OP_POPN     1  0
[40]OP_TMPDEL   1
[41]OP_POPCOV   34  1
[42]OP_PUSHX    34  local
[43]OP_PUSHS    0
[44]OP_PAIR
[45]OP_PUSHDICT 1
[46]OP_RET
Max Obj. Number: 35
Max Tmp. Number: 0
Max Reg. Number: 4
Const Value List (Integers): 10, 0, 2, 3
Const Value List (Double Floats):
Const Value List (Character Strings): odd_multiples_of_three, i
</pre>
With a function argument, ``__binary__`` prints only the bytecode range occupied
by that function body:

```tapas
__binary__(fn)
```
<pre class='Tapas-Return'>
[6]OP_PUSHI    0
[7]OP_PUSHI    1
[8]OP_TO
[9]OP_VCRT     1  0
[10]OP_LOOPIAS  0  0
[11]OP_CJPFPOP  27
[12]OP_PUSHI    1
[13]OP_PUSHI    2
[14]OP_PUSHX    0  tmp
[15]OP_PUSHINFO 0
[16]OP_MOD      0  1
[17]OP_PUSHINFO 0
[18]OP_NE       0  1
[19]OP_CJPFPOP  11
[20]OP_PUSHI    1
[21]OP_PUSHI    3
[22]OP_PUSHX    0  tmp
[23]OP_PUSHINFO 0
[24]OP_MOD      0  1
[25]OP_PUSHINFO 0
[26]OP_EQ       0  1
[27]OP_PUSHB    1
[28]OP_PUSHINFO 0
[29]OP_AND      0  1
[30]OP_JPF      1
[31]OP_PUSHB    0
[32]OP_CJPFPOP  5
[33]OP_PUSHX    0  tmp
[34]OP_PUSHX    0  upval 1
[35]OP_EVAL     1
[36]OP_POPN     1  1
[37]OP_PASS
[38]OP_JPB      29
[39]OP_POPN     1  0
[40]OP_TMPDEL   1
Max Obj. Number: 35
Max Tmp. Number: 0
Max Reg. Number: 4
Const Value List (Integers): 10, 0, 2, 3
Const Value List (Double Floats):
Const Value List (Character Strings): odd_multiples_of_three, i
</pre>
Using the instruction descriptions above, we can read the bycode and understand
the execution flow of a Tapas script.
