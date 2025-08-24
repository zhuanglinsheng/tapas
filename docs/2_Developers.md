
# Developer Documentation

## 1. Mechanism

### 1.1. Overview

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


## 1.2. Garbage collection

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


## 2. C++ Interaction

### 2.1. Calling Tapas in C++

The APIs of calling Tapas in C++ are defined in the class ``tapas::tsession`` in ``tap.h`` file, which provides a set of methods. The most important two are:

- `tsession::compile_file(const std::string & file_path)`
- `tsession::eval_bycodes(const std::string & file_path)`

Tapas source code file should be stored in a text file with suffix ``.tap``. By providing the location of Tapas source code file, we just

- Import the header ``tap.h`` from the ``src`` folder, and
- Call in C++ the above two method two execute it.

For example, suppose you have a Tapas script ``test_calling.tap`` inside which looks like

```tapas
var abs = (x) {
	if (x >= 0) {
		return x
	}
	else {
		return -x
	}
}
abs(-2).std::print()
```

Then we call this ``test_calling.tap`` in C++ by

```c++
#include "tap.h"

int main(int argc, char ** args)
{
	// create a Tapas session
	tapas::tsession sess;

	// compile Tapas source codes
	sess.compile_file("test_calling.tap");

	// evaluate Tapas source codes
	sess.eval_bycodes("test_calling.tap");
	return 0;
}
```


### 2.2. Extend Tapas with C++ (1)

You can expand the default functions of Tapas script by adding C++ functions into it. In order to do this, you need to

- Create your C++ function of ``tapas::cppf`` type, that is a function type defined as

```c++
typedef void (*cppf)(Tap::Tapv *const, unsigned char, Tap::Tapv&);
```

- Register your C++ function to your Tapas session by calling the method

```c++
  tsession::add_cppf(const std::string & fname, const cppf & f)
```

As an example, we define a C++ function in ``test_Tap_extension.h``, which looks like

```c++
#include "./tap-script/src/tap.h"
using namespace tap;

void cpp_int_sum(tapv * const params, unsigned char len, tapv & vre)
{
	long s = 0;
	tapas::tapv * param_i = params;

	for (unsigned char i = 0; i < len; i++) {
		if (params->get_type() != tapas::tint) {
			vre.set_type(tapas::tnil);
			return;
		} else
			s += param_i->get_v_tint();
		param_i++;
	}
	vre.set_type(tapas::tint);
	vre.set_v_tint(s);
}
```

Clearly, the function ``cpp_int_sum`` defined above follows the signature of functions of ``cppf`` type. We want to use this C++ function in our Tapas file ``test_Tap_extension.tap``, which is given below

```tapas
// calling int_sum linking to 'cpp_int_sum'
var s = int_sum(1,2,3,4,5)

// print out thr result
print(s)
```

All we need to do is to register this C++ function in the current Tapas session. So, let's do it. Let's execute ``test_Tap_extension.tap`` which calls ``int_sum``,

```c++
#include <string>
#include "tap.h"
#include "test_Tap_extension.h"

int main()
{
	// create a Tapas session
	tapas::tsession sess;

	// register this C++ function
	sess.add_cppf("int_sum", cpp_int_sum);
	std::string src_codes = "test_Tap_extension.tap";

	// compile the Tapas source code
	sess.compile_file(src_codes);

	// execute the Tapas source code
	sess.eval_bycodes(src_codes);
	return 0;
}
```

And we should get the return ``15``.


### 2.3. Extend Tapas with C++ (2)

In order to extend the data structures in Tapas script, we need to make C++ class to inherit ``tapas::tcompo_v``, which is a virtual class asking for the implementations of methods ``copy`` and ``print``. If further attributes are needed (callable, indexable, iterable, etc.), then we can inherit ``tcompo_eval``, ``tcompo_idx`` and ``tcompo_iter``, and implement their methods. After creating the class that we need, we also need to create a ``cppfunc`` so that we can use this data structure in Tapas script (see above).


## 3. Bycodes

The so-called Tapas bycode instructions are abstract instructions, not real CPU instructions.

Similar to Lua virtual machine instructions, Tapas bycodes are represented by unsigned integers. Currently, Tapas has 48 instructions.

The length of a bycodes is 32 bits, where the first 6 bits are the instruction name (a total of 64 instructions can be accommodated), and the remaining 26 bits are filled by parameters.

According to different bit layouts, instructions are divided into four types. Please refer to ``tbycode`` class for the implementation.


### 3.1. No Params

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

- ``OP_PASS`` Do nothing;
- ``OP_THIS`` Push the value representing the current environment to the top of the stack;
- ``OP_BASE`` Push the value representing the father environment to the top of the stack;
- ``OP_BREAK``  Jump to the place after the next ``OP_JUMPB``;
- ``OP_CONTI``  Jump to the place before the next ``OP_JUMPB``;
- ``OP_RET`` Return the stack top, clear the data in stack, and jump to the end of the instruction;
- ``OP_IN`` Pop the top two of stack as parameters, call ``tops::operator_in``, and push the returned value to stack top;
- ``OP_PAIR`` Pop the top two of stack as parameters, call ``tops::operator_pair``, and push the returned value to stack top;
- ``OP_TO`` Pop the top two of stack as parameters, call ``tops::operator_to``, and push the returned value to stack top;

<br>

### 3.2. U (1 parameter)



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


- ``OP_TMPDEL oloc`` Delete the temporary variable located at ``oloc`` from the current virtual machine;
- ``OP_JPF ncmd`` Jump forward by ``n`` steps;
- ``OP_JPB ncmd`` Jump back by ``n`` steps;
- ``OP_CJPFPOP ncmd`` When this instruction is executed, the stack top must be an boolean value. Jump forward by ``ncmd`` steps when stack top is ``false``;
- ``OP_CJPBPOP ncmd`` When this instruction is executed, the stack top must be an boolean value. Jump back by ``ncmd`` steps when stack top is ``false``;
- ``OP_PUSHI cloc`` Push the constant integer in ``cloc`` of the constant integer list to stack top;
- ``OP_PUSHD cloc`` Push the constant double float in ``cloc`` of the constant double float list to stack top;
- ``OP_PUSHB b`` Push boolean ``b`` to stack top;
- ``OP_PUSHS cloc`` Push the constant string in ``cloc`` of the constant string list to stack top;
- ``OP_PUSHDICT n`` Pop the top ``n`` values of stack as parameters and push a ``tdict`` value generated by those parameters;
- ``OP_PUSHINFO u`` Push an unsigned integer ``u`` to stack top;
- ``OP_IMPORT cloc`` Import a Tapas file whose location is stored at ``cloc`` of the constant string list;
- ``OP_IDXR n`` Take the first ``n`` value at stack top as the index, the ``n+1`` value as the indexable value, and pop the first ``n+1`` values, and push the indexing return to stack top;
- ``OP_EVAL n`` Take the first ``n`` value at stack top as parameters, the ``n+1`` value as the callable value, and pop the first ``n+1`` values, and push the calling return to stack top;

<br>

### 3.3. LR (2 parameters)



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


- ``OP_POPN nreg, interactive`` Pop the n data at the top of the stack;
- ``OP_POPCOV oloc, isenv`` Pop the data on stack top and assign it to the variable located in ``oloc``;
- ``OP_LOOPAS oloc, isenv`` When this instruction is executed, the stack top must be an ``iterable`` value. This instruction will update the iteration, assign the pointed value of the stack top in the current iteration to the variable located in ``oloc``, and push a boolean value to the top of the stack to indicate whether the iteration is over;
- ``OP_PUSHX oloc, isenv`` Push the variables in ``oloc``;


### 3.4. CP (2 parameters)



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


- ``OP_VCRT cloc, isenv`` Create a new variable at `cloc` in environment or stack;

- ``OP_PUSHF ncmds nparams`` Create a ``tfunc`` of ``nparams`` parameters by the next ``ncmd`` instructions in the current instruction list and, and push it to stack top;

- ``OP_ADD L R``   Before `OP_ADD`, there must be an `OP_PUSHINFO` to push an indicator ``i = 0, 1, 2, 3`` standing for

    - 0 - Both sides are literal;
    - 1 - Left hand side is variable, right hand side is literal;
    - 2 - Left hand side is literal, right hand side is variable;
    - 3 - Both sides are variable;

    Then, in `OP_ADD`, the elements ``L`` and ``R`` stand for the locations of values of left hand side and right hand side in constant table (if either is literal) or variable list (if either is vairable).

- ``OP_IDXL oloc, nparams`` Call the variable at ``oloc`` (must be of indexable type), use the ``nparams`` value of stack top as index, indexing and find the corresponding location, assign it by the data at the top ``nparams+1`` of stack, and pop ``nparams+1`` data on the top of the stack; Before `OP_IDXL`, there must be an `OP_PUSHINFO` to push a boolean indicator `isenv`.



### 3.5. Example

We try to check the bycodes of this file ``test_bycodes.tap``:

```tapas
for (let i in 0 to 10) {
	if (i % 2 != 0 and i % 3 == 0) {
		std::print(i)
	}
}
```

<pre class='Tapas-Return'>
3
9
</pre>
This program prints out all odd numbers that are multiples of 3 among 0 and 9.


We use ``tsession::show_bycodes`` to check the bycodes of  ``test_bycodes.tap``:

```c++
#include "Tap.h"

int main()
{
	Tap::tsession sess;
	std::string src_codes = "test_bycodes.tap";
	sess.compile_file(src_codes);
	sess.show_bycdes(src_codes);
	return 0;
}
```

Thus, the bycodes of ``test_Tap_extension.tap`` are printed out as follows:

<pre class='Tapas-Return'>
[0]OP_PUSHI    0
[1]OP_PUSHI    1
[2]OP_TO
[3]OP_VCRT     0
[4]OP_LOOPAS   0  0
[5]OP_CJPFPOP  23
[6]OP_PUSHI    1
[7]OP_PUSHI    2
[8]OP_PUSHINFO 4
[9]OP_MOD      0  0
[10]OP_PUSHINFO 0
[11]OP_EQ       0  1
[12]OP_PUSHI    1
[13]OP_PUSHI    3
[14]OP_PUSHINFO 4
[15]OP_MOD      0  0
[16]OP_PUSHINFO 0
[17]OP_NE       0  1
[18]OP_PUSHINFO 0
[19]OP_AND      0  1
[20]OP_CJPFPOP  7
[21]OP_PUSHX    0  0
[22]OP_PUSHS    0
[23]OP_PUSHX    4  1
[24]OP_IDXR     1
[25]OP_EVAL     1
[26]OP_POPN     1  1
[27]OP_PASS
[28]OP_JPB      25
[29]OP_POPN     1  0
[30]OP_TMPDEL   1
Max Obj. Number: 6
Max Tmp. Number: 1
Max Reg. Number: 5
Const Value List (Integers): 10, 0, 3, 2
Const Value List (Double Floats):
Const Value List (Character Strings): print
</pre>
Referring to the explanations of each instruction above, we can read the bycode and understand the execution flow of Tapas script.

