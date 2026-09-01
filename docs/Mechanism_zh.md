# Tapas 运行机制

简体中文 | [English](Mechanism_en.md) | [项目主页](../README.md)

本文说明 Tapas 如何把源码编译成字节码，以及虚拟机如何管理变量、调用函数并
执行这些字节码。语言本身的规则见 `Syntax_zh.md`；这里描述的是当前实现。



## 从源码到字节码

编译器先把 `.tap` 文件或 Markdown 中的 Tapas 代码解析成抽象语法树，然后完成
名称解析和静态 Type 检查，最后生成字节码。

编译器把结果保存在字节码包装器中。包装器包括：

- 32 位指令列表；
- 整数、浮点数和字符串常量表；
- 每条指令对应的源码位置；
- 环境槽位、临时槽位和运行栈所需的最大容量。

编译器同时确定名称所在的槽位。虚拟机执行普通变量访问时直接使用槽位编号，
不再查找源码中的变量名。虚拟机可以立即执行这个结果，也可以把它写入 `.tapc`
文件后再加载。



## 运行时状态

库是程序的根环境。调用 Tapas 函数时，虚拟机会在函数定义时保存的父环境下建立
调用环境。环境保存需要跨表达式存在的值；当前表达式的中间结果放在运行栈中，
局部临时值放在单独的临时槽位中。`tvm::rev` 暂存函数返回值。

例如：

```tapas
var a = 0
a = 1 + 2
```

第一条语句创建 `a` 的环境槽位，并把整数常量 `0` 写入该槽位。第二条语句从
常量表读取 `1` 和 `2`，在运行栈上完成加法，再把结果移入 `a` 的槽位。赋值
结束后，这次计算使用的栈值已经清除。



## 指令执行与函数调用

虚拟机按程序计数器依次读取指令。操作码和参数由内联位运算直接解码，指令逻辑
位于同一个解释循环中；跳转指令直接修改程序计数器。

每次 Tapas 函数调用都有独立的调用帧，其中保存程序计数器、返回位置、调用环境、
临时槽位、运行栈和循环游标。虚拟机在同一个解释循环中切换调用帧，不会为 Tapas
函数递归进入新的 C 解释函数。函数返回后，虚拟机会清除帧中持有的值，但保留已经
分配的存储空间；相同调用深度上的后续调用可以复用这些空间。普通调用的参数在
进入函数时写入调用环境预留的参数槽位；动态参数列表才在调用期间直接读取调用者
的运行栈。同一深度再次调用同一个函数时，函数对象本身就是校验条件，虚拟机直接
复用已经验证的帧布局；调用其他函数时仍会检查父环境、参数、局部槽位和运行栈容量。

``return this(...)`` 形式的直接尾递归不需要增加调用深度。虚拟机会先保留新参数，
再清空并复用当前调用帧，从函数入口继续执行。

函数值可以引用定义位置的父环境。返回的函数需要继续使用当前调用环境时，运行时
会复制需要保留的环境，使它不依赖随后复用的调用帧。各层循环游标同样属于各自的
调用帧，因此递归进入同一条循环指令不会改变外层循环的位置。

字节码只记录操作本身，不记录某次执行观察到的值类型，也不会在执行过程中改写。
虚拟机为每份正在执行的代码维护独立缓存，并按指令位置直接访问缓存项。循环、
索引和函数调用第一次执行时走通用路径；同一位置反复遇到相同类型或相同函数后，
缓存让后续执行直接进入对应实现。实际值仍会接受必要的检查；同一位置出现不同情况时，
虚拟机保留原指令并回到通用分派。

缓存不写入 `.tapc` 文件，也不属于函数调用帧。列表循环的当前位置则相反：它属于
当前调用帧，并通过缓存预先分配的槽位直接访问。这样，同一份字节码可以由不同虚拟机
安全执行，递归调用也不会共享循环位置。

编译器已经确定局部名称和临时值的槽位。虚拟机执行直接槽位访问指令时读取相应数组，
只保留边界检查，不再经过通用数组接口。创建和删除槽位时优先复用数组已经分配的
容量；清除标量槽位只重置值，复合值才进入引用计数释放。索引指令直接消费原有栈区；
取得结果后一次清除容器和参数，不会为了清栈而把容器重新压栈。

缓存确认索引位置一直使用同一种容器后，列表和 String 的单整数索引，以及稠密
数值数组和布尔数组的双整数索引，会直接执行对应的数据访问。稠密数组路径直接计算
`row * columns + column`，仍保留负索引、边界和写入类型检查；切片或不同类型会回到
通用路径。缓存只选择实现，不改变 `OP_IDXR` 和 `OP_IDXL`。

运行时错误的位置也来自字节码包装器。虚拟机只更新当前指令编号；真正发生错误时，
错误系统才读取并复制对应的文件名、源码位置和源码文本。函数嵌套调用不会为每条
指令重复建立错误上下文。



## 复合值的生命周期

字符串、列表、字典、函数等复合值以 `tcompo_v` 为共同基础，并通过引用计数管理
生命周期。环境槽位、集合元素、运行栈和返回值都可能持有复合值。

复制一个持有关系时，运行时增加引用计数；覆盖或清除该位置时减少引用计数。值从
运行栈移入环境槽位或返回值位置时，可以直接转移原有的持有关系，不必先增加再减少。
整数、浮点数和布尔值不持有复合值；从运行栈弹出它们时只需移动栈顶位置，下一次
压栈会覆盖旧槽位。已经确认操作数为标量的算术和比较也直接写入结果，不再调用带
引用清理的通用写值函数。

引用计数降到零后，复合值通过自身的 vtable 释放。列表、二元组和字典还会依次
释放它们持有的元素；稠密数组的标量数据由数组本身直接管理。

列表切片的长度在创建结果前已经确定。运行时按最终长度一次分配元素数组，连续复制
元素，再只为其中的复合值增加引用计数；它不会通过逐项追加反复检查容量和扩容。

字符串较短时，字符数据直接保存在 `tstring` 内部；超过内部容量后才分配独立
缓冲区。运行时 String 把 `tstring` 存储头嵌入自身，因此短字符串只需要分配
一次，同时仍然是可以独立修改的普通 String。已知字符长度时，运行时直接按长度
构造 String；索引不再扫描结尾，切片也不再经过临时 `tstring`。

引用计数不能发现不可达的循环引用。当前运行时没有额外的追踪式循环回收器，因此
程序应避免让容器形成不再使用的引用环。



## 字节码说明

Tapas 字节码是抽象虚拟机指令，不是真实 CPU 指令。与 Lua 虚拟机指令类似，
它们用无符号整数表示；当前共有 50 条指令。

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
- ``OP_EVALTF n``：以顶部 ``n`` 个值为参数，直接调用当前函数，弹出参数后压入调用结果。

编译器仍以 ``OP_PUSHINFO`` 保存二元运算的寻址方式。执行时，如果它后面紧接
二元运算，虚拟机会融合执行这两条指令，不再把寻址信息实际压栈。整数和浮点数的
算术、数值比较可以在融合路径中直接完成；需要自定义运算的复合值仍交给相应的
vtable 方法处理。这项优化不改变字节码，也不为二元运算增加运行时缓存。

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
- ``OP_LOOPAS oloc, isenv``：推进栈顶迭代值，把当前项赋给 ``oloc`` 处变量，并压入是否继续迭代的布尔值。整数区间、列表和 Type 的具体执行路径由运行时缓存选择，字节码保持为 ``OP_LOOPAS``。
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



## 查看字节码

以下函数用于展示编译结果：

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
[10]OP_LOOPAS   0  0
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
[10]OP_LOOPAS   0  0
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
