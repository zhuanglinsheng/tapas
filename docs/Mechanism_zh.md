# Tapas 运行机制

简体中文 | [English](Mechanism_en.md) | [项目主页](../README.md)

可以从四个层次理解 Tapas：编译、运行时状态、虚拟机和 C 交互 API。

总体而言，Tapas 通过一次遍历把源码编译为虚拟机指令，再由栈式虚拟机执行。

运行时通过环境系统管理变量；编译期间还会建立整数、浮点数和字符串常量表。



## 概述

与 Lua 类似，Tapas 使用递归下降编译器遍历源码并生成字节码，细节以编译器实现
为准。

编译期间，Tapas 维护寄存器计数器（``treg_ctr``）和变量名表（``tobj_ctr``），
二者共同确定每个变量在所属环境中的存储位置。

编译后，变量名被替换为环境树中的相对位置；运行时不会再按源码文本解析普通
变量名。

执行时，虚拟机可从 ``.tapc`` 文件读取字节码，加载常量表和指令列表，再按照
指令出现的顺序逐条执行。

表达式执行后会把结果留在虚拟机栈顶；语句使用完这些中间结果后会将它们清除。

下面是一个可执行示例：

```tapas
var a = 0
a = 1 + 2
```

第一行是变量声明语句。虚拟机在作为根环境的库中为该变量分配槽位；声明完成后
栈上不保留值。

第二行是包含加法表达式的赋值语句。

虚拟机先从整数常量表把 ``1`` 和 ``2`` 压栈，执行加法并移除操作数，把结果
``3`` 留在栈顶。

赋值再把栈顶值移入 ``a`` 的槽位并清空栈；语句结束后栈重新为空。



## 垃圾回收

Tapas 使用引用计数管理引用类型值。该机制以 ``tcompo_v`` 为核心，每个复合值
保存一个整数，记录指向它的所有权引用数量。

Tapas 值主要可存放在四处：

- 环境的变量列表；
- 集合值；
- 虚拟机运行栈；
- 虚拟机结果寄存器 ``tvm::rev``。

原则上，引用值存入任一位置时计数都应加一。实际实现只跟踪前两种长期存储；
虚拟机栈和返回寄存器只保存计算过程中的临时状态。

引用计数规则如下：

- 新建引用值的计数从零开始，随后必须交给变量、集合或正在执行的代码使用。
- 变量引用该值时加一，变量释放它时减一。
- ``tpair``、``tlist``、``tdict`` 等集合保存复合值时加一，释放元素时减一；
  稠密数组直接拥有连续标量存储。
- 返回指令结束后，函数返回值被压入虚拟机栈。
- 每条语句结束时，虚拟机会清除栈中的临时值。
- 每次计数减少都要检查；计数到零时释放该值。



## 字节码说明

Tapas 字节码是抽象虚拟机指令，不是真实 CPU 指令。与 Lua 虚拟机指令类似，
它们用无符号整数表示；当前共有 48 条指令。

每条字节码长 32 位：前 6 位保存操作码，最多容纳 64 种指令；其余 26 位保存
参数。

指令按位布局分组，细节参阅 ``tbycode`` 实现。



### 无参数

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

- ``OP_PASS``：不执行操作。
- ``OP_THIS``：把当前环境值压栈。
- ``OP_BASE``：把父环境值压栈。
- ``OP_BREAK``：跳到下一条 ``OP_JUMPB`` 之后。
- ``OP_CONTI``：跳到下一条 ``OP_JUMPB`` 之前。
- ``OP_RET``：返回栈顶，清空栈并跳到指令列表末尾。
- ``OP_IN``：弹出两个参数，调用 ``in`` 运算并压入结果。
- ``OP_PAIR``：弹出两个参数，调用对运算并压入结果。
- ``OP_TO``：弹出两个参数，调用 ``to`` 运算并压入结果。



### U（1 个参数）

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

- ``OP_VCRT isenv``：在当前环境或临时区创建变量。
- ``OP_TMPDEL oloc``：删除当前虚拟机中的临时变量。
- ``OP_JPF ncmd`` / ``OP_JPB ncmd``：向前或向后跳转 ``ncmd`` 条指令。
- ``OP_CJPFPOP ncmd`` / ``OP_CJPBPOP ncmd``：要求栈顶为布尔值；为假时弹出并向前或向后跳转。
- ``OP_PUSHI cloc``：压入 ``cloc`` 处整数常量。
- ``OP_PUSHFLT cloc``：压入 ``cloc`` 处浮点常量。
- ``OP_PUSHB b``：压入布尔值 ``b``。
- ``OP_PUSHS cloc``：压入 ``cloc`` 处字符串常量。
- ``OP_PUSHDICT n``：弹出顶部 ``n`` 个值并构造 ``tdict``。
- ``OP_PUSHINFO u``：压入无符号整数元数据 ``u``。
- ``OP_IMPORT cloc``：导入路径存于字符串常量 ``cloc`` 的 Tapas 文件。
- ``OP_IDXR n``：以顶部 ``n`` 个值为索引参数、下一个值为对象，弹出后压入索引结果。
- ``OP_EVAL n``：以顶部 ``n`` 个值为参数、下一个值为可调用对象，弹出后压入调用结果。



### LR（2 个参数）

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

- ``OP_POPN nreg, interactive``：从栈顶弹出 ``nreg`` 个值。
- ``OP_POPCOV oloc, isenv``：弹出栈顶并赋给 ``oloc`` 处变量。
- ``OP_LOOPAS oloc, isenv``：推进栈顶迭代值，把当前项赋给 ``oloc`` 处变量，并压入是否继续迭代的布尔值。
- ``OP_PUSHX oloc, isenv``：把 ``oloc`` 处变量压栈。



### CP（2 个参数）

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

- ``OP_PUSHF ncmds nparams``：用后续 ``ncmds`` 条指令创建含 ``nparams`` 个参数的 ``tfunc`` 并压栈。



### iLR（3 个参数）

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

三参数指令主要是二元算术和逻辑运算，布局相同。以加法为例：

- ``OP_ADD i L R`` 中 ``i = 0, 1, 2, 3`` 分别表示：

  - 0：两侧都是字面量；
  - 1：左侧是变量，右侧是字面量；
  - 2：左侧是字面量，右侧是变量；
  - 3：两侧都是变量。

  ``L`` 和 ``R`` 是左右值的位置：字面量位于常量表，变量位于变量列表。

- ``OP_IDXL oloc, nparams, isenv``：对 ``oloc`` 处的变量进行索引；栈顶 ``nparams`` 个值是索引参数，再下一个值是要写入的新值。完成写入后，弹出这些已使用的栈值。



## 示例

下面查看这个函数生成的字节码：

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
该程序输出 0 至 9 中所有能被 3 整除的奇数。

也可以从 `examples/test_bycodes.tap` 导入同一函数：

```tapas
import examples/test_bycodes.tap as lib
var fn = lib['odd_multiples_of_three']
fn()
```
<pre class='Tapas-Return'>
3
9
</pre>


### 命令行

查看生成字节码最简单的方法是使用命令行选项 ``-cr``；它先编译源文件，再显示
编译后的字节码：

```sh
./build/bin/tapas -cr docs/examples/test_bycodes.tap
```



### C API

C 会话 API 也提供相同操作：

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



### 在 Tapas 内部

在 Tapas 中，可使用调试函数 ``__binary__([env])`` 打印字节码。它接受可选的
环境值；不传参数时，打印当前库或模块对应的完整字节码信息。

传入库时，``__binary__`` 打印该库对应的完整字节码信息：

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
传入函数时，``__binary__`` 只打印该函数体占用的字节码范围：

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
结合上面的指令说明，即可阅读字节码并理解 Tapas 脚本的执行流程。
