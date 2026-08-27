# Tapas 编程语言规范

简体中文 | [English](Syntax_en.md)

本文是 Tapas 的规范性语言定义。它独立于当前编译器实现；符合规范的编译器、
格式化工具和语言服务器都必须遵守本文规则。

本文分为五部分：

1. 教程式语法说明及语言语义；
2. 明确的词法规则；
3. 完整的 EBNF 语法；
4. 内置函数目录；
5. 运算符对内置类型的默认语义。

文中的“必须”“不得”“应当”和“可以”具有规范意义。每个标记为 `tapas` 的
代码块都可以执行。Tapas 二进制程序会按文档顺序把这些代码块作为同一个程序
读取，因此后面的代码块可以使用前面声明的名称。标记为 `text` 的代码只用于
展示写法，可能有意包含不完整或无效的 Tapas 源码。



## 第一部分——教程与语言语义

### 1. 第一个程序

Tapas 是一门动态类型、以表达式为核心的脚本语言。源文件由一条条语句组成。
通常情况下，换行或分号表示一条语句结束；但如果换行出现在圆括号、方括号、
花括号或字符串内部，它只相当于普通空白，不会结束语句。

```tapas
var tutorial_limit = 5

let tutorial_square = (x){
    return x * x
}

for(let tutorial_i in 0 to tutorial_limit){
    print(tutorial_i, ' -> ', tutorial_square(tutorial_i))
}
```
<pre class='Tapas-Return'>
0 -> 0
1 -> 1
2 -> 4
3 -> 9
4 -> 16
</pre>

`var` 和 `let` 都用于声明变量，但生命周期和捕获规则不同。函数字面量写成
`(参数){ 函数体 }`。`0 to 5` 产生半开迭代器 `0, 1, 2, 3, 4`。

### 2. 如何分隔语句

只要当前不在括号、列表、字典、代码块或字符串内部，换行和 `;` 都表示当前
语句已经结束。连续写多个换行或分号不会产生额外语句。下面两种写法等价：

```text
let a = 1
let b = 2

let a = 1; let b = 2
```

`if`、`for`、`while` 和函数等结构使用 `{ ... }` 包住内部代码。这类结构在
右花括号后不需要再写分号。`if`、`elif` 和 `else` 之间可以换行。

在字符串之外，`//` 表示从这里到本行末尾都是注释。如果表达式没有放在括号、
方括号或花括号中，就不能在注释后的下一行继续书写同一个表达式。

### 3. 值与动态类型

每个表达式执行后都会得到一个值。变量不需要声明类型，同一个变量以后也可以
改为保存另一种类型的值。

直接存储的值类型如下：

| 类型 | 字面量示例 | 说明 |
|---|---|---|
| `bool` | `true`, `false` | 条件中不进行隐式转换。 |
| `int` | `0`, `-12`, `42` | 由运行时 C `long` 表示的有符号整数。 |
| `float` | `0.5`, `.5`, `5.`, `1e3` | 由 C `double` 表示。 |
| `nil` | 无 | 表示“没有值”的内部值，不存在 `nil` 字面量。 |

复合值是堆对象：

| 类型 | 构造方式 |
|---|---|
| 字符串 | `'text'`, `"text"` |
| 列表 | `[1, 2, 3]`, `list(1, 2, 3)` |
| 二元组（Pair） | `'key' : 1`, `pair('key', 1)` |
| 字典 | `{'key1' : 1, 'key2' : 2}` |
| 迭代器 | `0 to 10`, `iter(0, 10)` |
| 函数 | `(x){ return x }` |
| 库 | `import module.tap as module` |
| 实数数组 | `array(2, 2, 0.0)` |
| 布尔数组 | `array(2, 2, false)` |
| 时间 | `now()`, `time::from_unix(0)` |

`Time` 表示一个绝对时间点，精度为整秒。默认转换成字符串时，Tapas 按运行环境
的本地时区显示时间；时区不是 `Time` 值自身的一部分。

```tapas
let tutorial_epoch_time = time::from_unix(100)
let tutorial_later_time = tutorial_epoch_time + 20
print(time::unix(tutorial_later_time))
print(tutorial_later_time - tutorial_epoch_time)
print(tutorial_epoch_time < tutorial_later_time)
print(len(time::format(tutorial_epoch_time, '%Y')) > 0)
```
<pre class='Tapas-Return'>
120
20
true
true
</pre>

`print` 等只执行操作、不产生有用结果的函数会返回内部值 `nil`。不能用变量
保存 `nil`，也不能把它赋给已有变量；这样做会在程序运行时报错。单独写
`return` 是合法的，此时函数向调用方返回 `nil`。

整数字面量采用严格的十进制写法：`0` 可以单独使用，其他整数必须以 `1` 到
`9` 开头。因此 `10` 合法，而 `00`、`010`、`0x10`、`0b10` 和 `0o10` 都是
编译错误。浮点数字面量可以写成 `.5`、`5.`、`1e3` 或 `1.5e-3`。

当前正式语法中的变量声明不包含类型标注。

### 4. 变量、作用域与赋值

声明变量时必须同时给出初始值。一条语句可以声明多个变量：

```tapas
var tutorial_origin = 0, tutorial_step = 1
let tutorial_message = 'hello'
tutorial_message = 42
```

变量必须先声明，之后才能读取或赋值。同一作用域内不能重复声明同名变量，
`var` 与 `let` 之间也不能重名。嵌套函数可以声明与外层同名的变量，此时内层
名称会暂时遮住外层名称。程序最外层的内置名称不能重新赋值。

`var` 声明的变量从声明位置开始生效，一直存在到当前模块或函数结束。内层函数
可以引用并修改同一个变量。不能在 `if`、`elif`、`else`、`for` 或 `while`
的 `{ ... }` 内声明 `var`；需要这样的变量时，应在当前函数或模块的最外层
提前声明。

`let` 声明的变量只在它所在的最小 `{ ... }` 范围内有效。程序离开这个代码块
后，变量随即消失，内层函数也不能继续引用它。函数参数则在本次函数调用期间
一直存在，并且可以被内层函数引用。

赋值目标只能是变量，或对变量进行一次直接索引：

```tapas
var tutorial_value = 1
tutorial_value = 2

let tutorial_items = [10, 20]
tutorial_items[0] = 11
```

不能向只读成员访问表达式赋值。语言也不支持向临时计算结果或多层索引直接
赋值，例如 `make_list()[0] = 1` 和 `matrix[0][1] = 1` 都是无效写法。

### 5. 复制与同一性

赋值或传递 `int`、`float`、`bool` 会复制值；赋值、传递或插入复合值只复制
引用，因此通过任一别名进行的修改对其他别名可见。

```tapas
let tutorial_inner = [1, 2]
let tutorial_outer = [tutorial_inner]
tutorial_inner[0] = 9
sprint(tutorial_outer)
```
<pre class='Tapas-Return'>
[[9, 2]]
</pre>

`copy(value)` 创建独立的外层复合对象，但列表、二元组或字典内部的元素仍采用浅
复制。`identical(a, b)` 检查运行时同一性；`==` 调用类型自身的相等运算，
不能替代同一性检查。

### 6. 字符串、列表、对和字典

字符串可使用任一种引号，并可跨物理行。Tapas 字符串是字节串，索引按字节而
不是 Unicode 码点计数。语言没有转义序列；反斜杠是普通字符，字符串内部不能
出现作为分隔符的同种引号。

字符串和列表接受一个整数索引，负索引从末尾反向计数。二者也接受半开切片
`start:end`，任一端点都可以省略。负数端点从末尾开始计算，例如 `-1` 表示
最后一个位置。

```tapas
let tutorial_text = 'Tapas'
let tutorial_numbers = [0, 1, 2, 3, 4]
print(tutorial_text[1:4])
sprint(tutorial_numbers[:3])
sprint(tutorial_numbers[-2:])
```
<pre class='Tapas-Return'>
apa
[0, 1, 2]
[3, 4]
</pre>

字符串整数索引返回单字节字符串。向字符串索引或切片赋字符串会替换对应范围。
列表元素赋值接受任何非 `nil` 值；列表切片赋值不受支持。

冒号运算符构造二元组（Pair）。连续使用冒号时从右向左分组，因此
`a : b : c` 等于 `a : (b : c)`。二元组固定包含两个元素，只接受索引
`0`、`1`、`-2` 和 `-1`。

字典字面量由逗号分隔的“键 : 值”项目组成。键可以是运行时支持的任意可哈希值；模块 API 和
命名成员应使用字符串键。重复键覆盖先前值。字典迭代顺序未规定。

```tapas
let tutorial_person = {
    'name' : 'Tony',
    'age' : 20,
}
print(tutorial_person['name'])
print(tutorial_person::age)
```
<pre class='Tapas-Return'>
Tony
20
</pre>

`dictionary::name` 等同于 `dictionary['name']`。不跟参数列表的
`dictionary.name` 也表示相同的只读查找。

### 7. 稠密数组

数组需要两个索引，每个索引可以是整数或切片。两个整数返回标量；任一索引为
切片时返回新数组。

```tapas
let tutorial_matrix = array(2, 3, [1, 2, 3, 4, 5, 6])
print(tutorial_matrix[1, 2])
sprint(tutorial_matrix[0:2, 1:3])
```
<pre class='Tapas-Return'>
6
[[2, 3],
 [5, 6]]
</pre>

为单个数组元素赋值时，新值的类型必须与数组元素类型一致。为数组切片赋值时，
新数组的元素类型和形状都必须与目标切片一致。实数数组支持与
数字或同形实数数组进行逐元素 `+`、`-`、`*`、`/`、`%`、`^` 和比较；`@`
表示矩阵乘法。其中，矩阵乘法、同形数组加减、数组与标量相乘和数组取负需要
在运行时使用 CBLAS；其余逐元素运算由 Tapas 自己实现。安装 Tapas 时不要求
系统已经安装 BLAS，详细规则见第五部分。布尔数组支持使用 `&`、`|` 与布尔值
或同形布尔数组进行逐元素逻辑运算；这两个运算符不采用短路规则。

### 8. 调用、索引、成员与隧道调用

调用函数时，Tapas 先确定要调用的函数，再从左到右计算各个参数。用户函数和
内置函数都可以像普通值一样保存和传递。`value(args)` 表示调用，
`value[indices]` 表示索引，`value::name` 表示读取名称为 `name` 的成员，
但不能通过这种写法修改成员。

Tapas 还提供隧道调用：

```text
receiver.function_name(arg1, arg2)
```

其定义为：

```text
function_name(receiver, arg1, arg2)
```

因此 `values.append(3)` 等于 `append(values, 3)`。这里的 `append` 是当前
作用域中的函数名，并不是从 `values` 对象中取出的成员。点号后的名称如果紧跟
`(`，就是隧道调用；否则表示只读成员访问。调用、索引和成员访问可以连续书写，
并按从左到右的顺序执行。

### 9. 运算符

优先级从高到低如下：

| 层级 | 运算符 | 结合性 |
|---|---|---|
| 后缀 | `()`, `[]`, `::`, `.` | 左结合 |
| 幂 | `^` | 右结合 |
| 一元 | `+`, `-` | 右结合 |
| 乘法 | `*`, `/`, `%`, `@` | 左结合 |
| 加法 | `+`, `-` | 左结合 |
| 比较 | `>`, `<`, `>=`, `<=`, `==`, `!=` | 不结合 |
| 范围 | `to` | 不结合 |
| 成员测试 | `in` | 不结合 |
| 逐元素逻辑与 | `&` | 左结合 |
| 逐元素逻辑或 | `\|` | 左结合 |
| 逻辑与 | `and` | 左结合 |
| 逻辑或 | `or` | 左结合 |
| 对 | `:` | 右结合 |

比较、`to` 和 `in` 不能直接连续书写；需要组合时必须用括号明确分组。
`&` 和 `|` 总会计算两个操作数，不会短路。`and` 和 `or` 采用短路
规则：只在结果尚不能确定时计算右侧。`and`、`or` 的实际操作数必须是布尔值；
`&`、`|` 还可以处理布尔数组。每种值类型自行规定它支持的算术和比较操作；
类型组合不受支持时，程序会在运行时报错。
整数除法向零截断。除数为零、整数溢出以及无效数组形状均为运行时错误。

例如，`a | b & c` 按 `a | (b & c)` 分组，`a and b | c` 按
`a and (b | c)` 分组，而 `x > 0 & y > 0` 按 `(x > 0) & (y > 0)` 分组。

### 10. 函数与闭包

固定参数函数列出零个或多个互不相同的参数名：

```tapas
let tutorial_hypotenuse = (x, y){
    return math::sqrt(x * x + y * y)
}
print(tutorial_hypotenuse(3, 4))
```
<pre class='Tapas-Return'>
5
</pre>

参数数量不匹配是运行时错误。变参函数以 `...` 作为完整参数列表，并通过
`__nparam__()` 和 `__param__(index)` 读取参数。

函数本身没有固定名称。函数内部可以用 `this` 再次调用当前函数，因此递归不
依赖外部变量名。`base` 指向创建当前函数的上一层函数环境。只有确实存在当前
函数或父函数环境时，才能分别使用 `this` 或 `base`。

```tapas
let tutorial_factorial = (n){
    if(n <= 1){
        return 1
    }
    return n * this(n - 1)
}
print(tutorial_factorial(6))
```
<pre class='Tapas-Return'>
720
</pre>

### 11. 条件与循环

`if` 结构会从上到下检查条件，只执行第一个条件为真的分支。`elif` 必须紧跟在
同一个 `if` 结构中，
可以有任意多个；`else` 最多一个且必须位于最后。

```tapas
let tutorial_sign = (x){
    if(x < 0){
        return -1
    }
    elif(x > 0){
        return 1
    }
    else{
        return 0
    }
}
```

`while(condition){ body }` 在布尔条件为真时重复执行。`for` 有两种形式：

```text
for(let item in iterable){ body }
for(item in iterable){ body }
```

可迭代值必须是迭代器、列表，或明确实现迭代协议的扩展值。字典不能直接迭代，
应使用 `keys(dictionary)` 或 `dvalues(dictionary)`。`let` 循环变量只在循环内
存在；使用已有变量时，该变量在循环后仍可见。

`break` 和 `continue` 只能出现在同一函数内的循环中，不能越过嵌套函数边界。
`return` 结束当前函数；模块顶层的 `return` 定义模块导出值。其他顶层控制流
无效。

### 12. 模块与导入

`import` 单独占一条语句。解析相对路径时，Tapas 依次在当前文件所在目录、
会话搜索路径和程序的当前工作目录中查找。路径没有文件后缀时，依次尝试原路径、`.tap`、`.md` 和目录中的
`__init__.tap`。Markdown 模块只编译 `tap` 或 `tapas` 代码围栏，并保持其余
行的位置以便报告诊断。

```text
import path/to/module.tap
import path/to/module.tap as module_name
import 'path with spaces/module.tap' as module_name
```

不写 `as` 的导入只执行模块，不保存模块结果；写了 `as name` 时，名称 `name`
会引用一个 `Library` 值。模块在最外层返回一个字典，以字典中的键作为导出名称。
模块没有返回值时得到空库；返回值既不是字典也不是 `nil` 时会报错。循环导入
必须被检测并报告，不得无限递归或产生部分初始化的库。

### 13. 错误与兼容写法

词法或语法错误必须包含文件、行、列和出错标记；上下文约束在编译期报告；动态
类型和边界问题在运行时报错。错误不得被静默改写为不同程序。

#### 13.1 已弃用的旧写法

“已弃用的旧写法”是指过去曾属于正式语言、现在只为迁移旧代码而保留，并且
计划在未来删除的语法。本规范目前没有把任何写法归入这一类。

#### 13.2 暂时保留的兼容写法

“兼容写法”不属于当前正式语法，但当前编译器会接受，而且不会报错。保留它们
是因为未来版本可能会赋予这些写法正式语义。因此，它们不应被称为“旧写法”或
“已弃用语法”。

| 写法 | 当前版本的兼容行为 | 未来可能的发展方向 |
|---|---|---|
| `var name: Type = value` 和 `let name: Type = value` | 声明可以通过编译，但 `Type` 不参与类型检查，也不会影响运行结果。 | 类型标注将来可能成为类型系统的一部分。 |
| `function(parameters){ body }` | 可以通过编译，行为与 `(parameters){ body }` 相同。 | `function` 关键字将来可能成为正式的函数字面量写法。 |
| `#{ expression }` | 生成一个可以接收任意数量参数的匿名函数；调用该函数时会计算并返回 `expression`。它近似于 `(...){ return expression }`，但函数体只能包含一个表达式。 | 这种简写将来可能成为正式语法，也可能被废弃。 |

编译器和语言服务器应当正常解析这些写法，不应把它们报告为错误。工具可以
给出非错误性质的提示，说明这些语法在当前版本中只作为兼容写法。格式化工具应当
保留原写法，不应在未告知用户的情况下自动改写。第三部分的 EBNF 仍然只描述
当前正式语法，不包含这些兼容扩展。



## 第二部分——词法规则

### 1. 源文本与位置

编译器按字节读取源文件。语法中的标点、关键字、标识符和数字都使用 ASCII
字符；字符串可以包含除自身结束引号外的其他字节。LF、CRLF 和单独的 CR 都
表示一次换行。错误位置的行号和列号从 1 开始，其中列号按字节计算。

### 2. 空白、嵌套与语句分隔

空格、水平制表符、垂直制表符和换页符都属于普通空白。圆括号、方括号、
花括号和字符串之外的换行会生成语句分隔标记 `SEP`；它们内部的换行只算
普通空白。字符串之外的 `;` 也会生成 `SEP`。连续出现多个分隔符时，效果与
一个分隔符相同。

### 3. 注释

```ebnf
line-comment = "//", { any-code-point-except-LF } ;
```

编译器把注释当作空白处理。Tapas 不支持块注释。

### 4. 标识符与关键字

```ebnf
identifier-start    = ASCII-letter | "_" ;
identifier-continue = identifier-start | decimal-digit ;
identifier          = identifier-start, { identifier-continue } ;
```

标识符区分大小写。以 `__` 开头的名称为实现和标准库保留，用户程序不应声明。
保留字如下：

```text
and as base break continue elif else false for function if import in
let nil of or return this to true var while
```

`function`、`nil` 和 `of` 都是保留字，但目前都不能作为表达式的开头。
关键字必须是完整单词，例如 `format` 中的 `for` 不会被识别为关键字。

### 5. 数值字面量

只允许十进制；不接受十六进制、二进制、八进制、数字分隔符及 NaN/Infinity。

```ebnf
decimal-digit    = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9" ;
nonzero-digit    = "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9" ;
digits           = decimal-digit, { decimal-digit } ;
exponent         = ("e" | "E"), [ "+" | "-" ], digits ;
integer-literal  = "0" | nonzero-digit, { decimal-digit } ;
float-literal    = (digits, ".", [digits] | ".", digits), [exponent]
                 | digits, exponent ;
```

除 `0` 外，整数字面量不能以 `0` 开头。数字前面的正负号是单独的运算符，不是
数值字面量的一部分。整数超出可表示范围，或浮点数字面量超出运行时支持的范围，
都会导致编译错误。

### 6. 字符串字面量

```ebnf
single-string = "'", { code-point-except-single-quote }, "'" ;
double-string = '"', { code-point-except-double-quote }, '"' ;
```

两种引号语义相同。字符串可以包含换行和另一种引号，没有转义序列。

### 7. 运算符和标点

当多个符号拥有相同前缀时，编译器总是优先识别最长的那个：

```text
多字符： ==  !=  >=  <=  ::  ...  //
单字符： + - * / % @ ^ & | > < = : . , ; ( ) [ ] { }
单词运算符：and or in to
```

`//` 用于开始注释。`.` 只有符合前面的浮点数字面量格式时才属于数字；其他
情况下，它用于成员访问或隧道调用。

### 8. 导入路径

`import` 后的路径如果没有引号，只能包含下列字符；遇到空白、分号、注释或
关键字 `as` 时，路径结束。路径本身包含空格时，必须用引号把它写成字符串。

```ebnf
path-character       = ASCII-letter | decimal-digit | "_" | "-" | "."
                     | "/" | "\\" | ":" ;
unquoted-import-path = path-character, { path-character } ;
```

### 9. Markdown 源码提取

在 Markdown 中，一行去掉行首空白后，如果以三个反引号开头，并紧接 `tap`
或 `tapas`，就表示 Tapas 代码块开始；语言标签后只能有空白。结束行同样允许
行首空白，但必须包含至少三个反引号，之后也只能有空白。编译器只读取围栏内的
代码，其他行按空行处理，以保留正确的报错行号。`Tapas-Return` 输出区域不会
参与编译。



## 第三部分——语法规则（EBNF）

### 1. 记号与词法终结符

在下面的 EBNF 中，`=` 用于定义规则，`,` 表示前后相接，`|` 表示任选其一，
`[ ]` 表示内容可以省略，`{ }` 表示内容可以重复。带引号的文本必须原样出现在
源码中；大写名称表示第二部分定义的词法标记。

```ebnf
IDENTIFIER = identifier excluding reserved words ;
INTEGER    = integer-literal ;
FLOAT      = float-literal ;
STRING     = single-string | double-string ;
PATH       = unquoted-import-path ;
SEP        = top-level newline | ";" ;
```

除词法规则特别限制的地方外，各个词法标记之间都可以插入空白或注释。

### 2. 模块、语句列表与代码块

```ebnf
module          = separators, [ statement-list ], separators, EOF ;
statement-list  = statement, { separator-run, statement } ;
separator-run   = SEP, { SEP } ;
separators      = { SEP } ;

statement       = declaration
                | assignment
                | if-statement
                | while-statement
                | for-statement
                | break-statement
                | continue-statement
                | return-statement
                | import-statement
                | expression-statement ;

block           = "{", separators, [ statement-list ], separators, "}" ;
expression-statement = expression ;
```

在 `{ ... }` 代码块中，只要换行没有位于更内层的括号、列表、字典、代码块或
字符串中，它就会分隔前后两条语句。

### 3. 声明与赋值

```ebnf
declaration       = ("var" | "let"), declarator, { ",", declarator } ;
declarator        = IDENTIFIER, "=", expression ;
assignment        = assignment-target, "=", expression ;
assignment-target = IDENTIFIER, [ index-suffix ] ;
```

### 4. 控制流

```ebnf
if-statement    = "if", "(", expression, ")", block,
                  { separators, "elif", "(", expression, ")", block },
                  [ separators, "else", block ] ;
while-statement = "while", "(", expression, ")", block ;
for-statement   = "for", "(", for-binding, "in", expression, ")", block ;
for-binding     = [ "let" ], IDENTIFIER ;
break-statement    = "break" ;
continue-statement = "continue" ;
return-statement   = "return", [ expression ] ;
```

### 5. 导入

```ebnf
import-statement = "import", import-path, [ "as", IDENTIFIER ] ;
import-path      = PATH | STRING ;
```

`STRING` 提供去除引号后的原始内容。

### 6. 表达式与优先级

```ebnf
expression      = pair-expression ;
pair-expression = or-expression, [ ":", pair-expression ] ;
or-expression   = and-expression, { "or", and-expression } ;
and-expression  = elementwise-or-expression,
                  { "and", elementwise-or-expression } ;
elementwise-or-expression  = elementwise-and-expression,
                             { "|", elementwise-and-expression } ;
elementwise-and-expression = membership-expression,
                             { "&", membership-expression } ;
membership-expression = range-expression, [ "in", range-expression ] ;
range-expression      = comparison-expression, [ "to", comparison-expression ] ;
comparison-expression = additive-expression,
                        [ (">" | "<" | ">=" | "<=" | "==" | "!="),
                          additive-expression ] ;
additive-expression       = multiplicative-expression,
                            { ("+" | "-"), multiplicative-expression } ;
multiplicative-expression = unary-expression,
                            { ("*" | "/" | "%" | "@"), unary-expression } ;
unary-expression = ("+" | "-"), unary-expression | power-expression ;
power-expression = postfix-expression, [ "^", unary-expression ] ;

postfix-expression = primary-expression, { postfix-suffix } ;
postfix-suffix     = call-suffix | index-suffix
                   | readonly-member-suffix | tunnel-suffix ;
call-suffix       = "(", [ argument-list ], ")" ;
argument-list     = expression, { ",", expression }, [ "," ] ;
index-suffix        = "[", index-argument-list, "]" ;
index-argument-list = index-argument, { ",", index-argument } ;
index-argument      = expression | slice ;
slice               = [ or-expression ], ":", [ or-expression ] ;
readonly-member-suffix = "::", IDENTIFIER
                       | ".", IDENTIFIER  (* only when not followed by "(" *) ;
tunnel-suffix = ".", IDENTIFIER, "(", [ argument-list ], ")" ;
```

切片端点使用 `or-expression`，从而排除无括号的对运算符。字符串和列表要求一个
索引参数，数组要求两个；这些数量限制属于语义检查。

### 7. 基本表达式与字面量

```ebnf
primary-expression = INTEGER | FLOAT | STRING | "true" | "false"
                   | IDENTIFIER | "this" | "base"
                   | parenthesized-expression | list-literal
                   | dictionary-literal | function-literal ;
parenthesized-expression = "(", expression, ")" ;
list-literal = "[", [ argument-list ], "]" ;
dictionary-literal = "{", [ dictionary-entry,
                            { ",", dictionary-entry }, [ "," ] ], "}" ;
dictionary-entry = or-expression, ":", expression ;
function-literal = parameter-list, block ;
parameter-list   = "(", [ fixed-parameters | "..." ], ")" ;
fixed-parameters = IDENTIFIER, { ",", IDENTIFIER }, [ "," ] ;
```

语法中有意不包含 `nil`。表达式位置的 `{}` 是空字典；代码块只出现在复合语句
或函数要求的位置。

### 8. 上下文相关约束

EBNF 只能描述代码的结构，下面这些规则还需要编译器单独检查：

1. 同一函数的参数名不能重复，同一作用域内也不能重复声明名称；
2. `if`、`for`、`while` 等控制流代码块内不能声明 `var`；
3. `break` 和 `continue` 必须位于当前函数自己的循环中；
4. `return` 只能出现在函数内或模块最外层；
5. 赋值目标必须已经声明，并且允许写入；
6. 程序最外层的内置名称不能赋值；
7. 使用 `this` 或 `base` 时，必须存在对应的当前函数或父函数环境；
8. 导入别名必须是合法标识符，且不能是保留字；
9. 同一个未加括号的表达式中，比较、范围和成员测试运算符各自最多出现一次；
10. 变量和集合不能保存 `nil`。



## 第四部分——内置函数和包

### 1. 签名说明

`A | B` 表示两种运行时类型之一，`Any` 表示任意 Tapas 值，`...Any` 表示零个
或多个参数。返回 `nil` 表示过程，其结果不能保存。根函数可以在第一个参数为
接收者时使用隧道调用，如 `append(items, value)` 等于 `items.append(value)`。

### 2. 输出、检查与时间

| 签名 | 返回 | 行为 |
|---|---|---|
| `print(...Any)` | `nil` | 打印简略表示并换行。 |
| `sprint(...Any)` | `nil` | 打印完整表示并换行。 |
| `len(value: Any)` | `int` | 复合值长度；`nil` 为 0，其他标量为 1。 |
| `type(value: Any)` | `String` | 返回运行时类型名。 |
| `copy(value: Any)` | 同类别 | 复制标量或浅复制复合值。 |
| `identical(a, b)` | `bool` | 检查运行时同一性。 |
| `clock()` | `float` | 进程 CPU 时间（秒）。 |
| `now()` | `Time` | 返回调用时刻对应的绝对时间点。默认显示使用本地时区。 |

### 3. 转换与构造

| 签名 | 返回 | 行为 |
|---|---|---|
| `int(bool | int | float | String)` | `int` | 转换为整数；字符串必须是完整十进制输入。 |
| `float(bool | int | float | String)` | `float` | 转换为浮点数。 |
| `bool(Any)` | `bool` | 显式真值转换。 |
| `str(Any)` | `String` | 完整文本表示。 |
| `list(...Any)` | `List` | 用参数建立新列表。 |
| `pair(Any, Any)` | `Pair` | 建立对。 |
| `iter(start, end)` | `Iterator` | 自动推断步长的半开范围。 |
| `iter(start, step, end)` | `Iterator` | 指定非零步长的半开范围。 |
| `array(rows, cols, fill)` | `Array` | 建立稠密数组。 |

数组维度不得为负，显式迭代步长不得为零。

### 4. 集合操作

| 签名 | 返回 | 修改与结果 |
|---|---|---|
| `push(List, Any)` | `nil` | 追加元素。 |
| `append(String | List | Dictionary, Any)` | `nil` | 追加文本、元素或对。 |
| `insert(List, Any, int)` | `nil` | 在索引前插入。 |
| `pop(List)` | `nil` | 删除最后一个元素。 |
| `pop(List, int)` | `nil` | 删除指定元素。 |
| `delete(List | Dictionary, Any)` | `nil` | 删除元素。 |
| `idx(String | List | Pair | Dictionary, Any)` | `Any` | 单参数索引。 |
| `keys(Dictionary)` | `List` | 按未规定顺序返回键。 |
| `dkeys(Dictionary)` | `List` | `keys` 的别名。 |
| `dvalues(Dictionary)` | `List` | 按相应顺序返回值。 |
| `union(List, List)` | `List` | 返回浅复制拼接结果。 |
| `sort(List)` | `nil` | 按运行时全序原地排序。 |

对字典调用 `append` 时，追加的值必须是 `Pair`。集合中不能保存 `nil`。

### 5. 会话函数

| 签名 | 返回 | 作用 |
|---|---|---|
| `__ls__([Library])` | `List` | 当前根库或指定库中的名称。 |
| `__path__([Library])` | `List` | 当前库或指定库的搜索路径。 |
| `__param__(index)` | `Any` | 当前变参调用的参数。 |
| `__nparam__()` | `int` | 当前变参调用的参数数量。 |
| `__binary__([Library | Function])` | `nil` | 打印当前或指定值的字节码。 |

以 `__` 开头的名称由实现保留。


### 6. 时间包 `time`

| 签名 | 返回 | 行为 |
|---|---|---|
| `time::from_unix(seconds: int)` | `Time` | 从 Unix 时间戳创建时间点。时间戳是自 Unix 纪元起的整数秒数。 |
| `time::unix(value: Time)` | `int` | 返回时间点对应的 Unix 整数秒数。 |
| `time::format(value: Time, pattern: String)` | `String` | 使用本地时区和宿主 C `strftime` 格式字符串生成文本。 |

时间戳和时间偏移必须落在宿主平台的 `time_t` 范围以及 Tapas `int` 范围内。
格式化模式及受支持的转换符由宿主 C 库决定。当前版本没有保存时区信息，也没有
提供 UTC 格式化或日期解析。

### 7. 标量数学包 `math`

除特别说明外，`number` 表示 `int | float`，函数接受数字并返回 `float`。定义域、
溢出、无穷和 NaN 行为遵循宿主 C 数学库。以下名称都通过 `math::` 访问。

| 分组 | 签名 |
|---|---|
| 绝对值与根 | `abs(number) -> int | float`, `fabs`, `sqrt`, `rsqrt`, `cbrt`: `(number) -> float` |
| 幂与几何 | `pow(number, number)`, `hypot(number, number) -> float` |
| 三角函数 | `sin`, `cos`, `tan`, `asin`, `acos`, `atan`: `(number) -> float`; `atan2(number, number)` |
| 双曲函数 | `sinh`, `cosh`, `tanh`, `asinh`, `acosh`, `atanh`: `(number) -> float` |
| 指数 | `exp`, `exp2`, `expm1`: `(number) -> float` |
| 对数 | `log`, `log2`, `log10`, `log1p`, `logb`: `(number) -> float`; `ilogb(number) -> int` |
| 分解 | `frexp(number) -> Pair(float, int)`, `modf(number) -> Pair(float, float)` |
| 缩放 | `ldexp`, `scalbn`, `scalbln`: `(number, number) -> float` |
| 误差与伽马 | `erf`, `erfc`, `lgamma`, `tgamma`: `(number) -> float` |
| 浮点舍入 | `ceil`, `floor`, `nearbyint`, `rint`, `round`, `trunc`: `(number) -> float` |
| 整数舍入 | `lrint`, `llrint`, `lround`, `llround`: `(number) -> int` |
| 余数 | `fmod`, `remainder`: `(number, number) -> float`; `remquo -> Pair(float, int)` |
| 浮点操作 | `copysign`, `nextafter`, `fdim`, `fmax`, `fmin`: `(number, number) -> float`; `fma(number, number, number)` |
| 倒数与 NaN | `eleinv(number) -> float`, `make_nan() -> float` |
| 分类 | `isfinite`, `isinf`, `isnan`, `isnormal`, `signbit`: `(number) -> bool`; `fpclassify(number) -> int` |
| 有序谓词 | `isgreater`, `isgreaterequal`, `isless`, `islessequal`, `islessgreater`, `isunordered`: `(number, number) -> bool` |

缩放函数的整数参数会将浮点数向零截断。

### 8. 稠密数组包 `dense`

| 签名                           | 返回    | 行为             |
| ------------------------------ | ------- | ---------------- |
| `dense::new(rows, cols, fill)` | `Array` | `array` 的别名。 |
| `dense::rows(Array)`           | `int`   | 行数。           |
| `dense::cols(Array)`           | `int`   | 列数。           |
| `dense::transpose(Array)`      | `Array` | 新的转置数组。   |

```tapas
let builtin_dense_matrix = dense::new(2, 3, [1, 2, 3, 4, 5, 6])
print(dense::rows(builtin_dense_matrix), ' x ', dense::cols(builtin_dense_matrix))
sprint(dense::transpose(builtin_dense_matrix))
```

<pre class='Tapas-Return'>
2 x 3
[[1, 4],
 [2, 5],
 [3, 6]]
</pre>


## 第五部分——运算符对内置类型的默认语义

### 1. 通用规则

本部分集中规定运算符对 Tapas 内置类型的默认行为，供编译器、语言服务器和其他
工具直接查阅。`number` 表示 `int | float`；“实数数组”表示元素为数字的
`Array`，“布尔数组”表示元素为布尔值的 `Array`。“同形数组”要求行数和列数
都相同。

除本部分明确列出的组合外，其他操作数组合均在运行时报类型错误。Tapas 不进行
隐式真值转换，也不在两个不同形状的数组之间进行广播。数字与实数数组运算时，
该数字会应用到数组的每个元素。

实数数组的部分运算必须通过 BLAS 完成。Tapas 编译和安装时不链接 BLAS；程序
第一次执行这类运算时，Tapas 才尝试加载用户环境中的 LP64 CBLAS 动态库。
如果找不到兼容的动态库，程序会在该运算处报运行时错误，不会改用 Tapas 内部的
循环实现。

### 2. 一元和算术运算符

| 运算符 | 合法操作数 | 结果与行为 | 实现要求 |
|---|---|---|---|
| 一元 `+`, `-` | `number` | 保持数值类型，分别返回原值或相反数。 | Tapas 内置 |
| 一元 `-` | 实数数组 | 逐元素取相反数，返回同形实数数组。实数数组不支持一元 `+`。 | CBLAS `dcopy`、`dscal` |
| `+`, `-`, `*`, `/` | `number`, `number` | 两个整数得到整数；只要一侧是浮点数就得到浮点数。整数除法向零截断。 | Tapas 内置 |
| `+` | `Time`, `int` | 按指定秒数平移时间点；正数得到更晚的时间，负数得到更早的时间。返回新的 `Time`，且不支持 `int + Time`。 | Tapas 内置和宿主 C 时间类型 |
| `-` | `Time`, `int` | 从时间点减去指定秒数；正数得到更早的时间，负数得到更晚的时间。返回新的 `Time`。 | Tapas 内置和宿主 C 时间类型 |
| `-` | `Time`, `Time` | 返回左侧减右侧的秒数，结果为 `float`。 | 宿主 C `difftime` |
| `+`, `-`, `/` | 实数数组与 `number`，顺序不限 | 逐元素运算，返回同形实数数组。减法和除法保留左右操作数顺序。 | Tapas 内置循环 |
| `*` | 实数数组与 `number`，顺序不限 | 标量乘法，返回同形实数数组。 | CBLAS `dcopy`、`dscal` |
| `+`, `-` | 两个同形实数数组 | 对对应元素做加法或减法，返回同形实数数组。 | CBLAS `dcopy`、`daxpy` |
| `*`, `/` | 两个同形实数数组 | 对对应元素做乘法或除法，返回同形实数数组。 | Tapas 内置循环 |
| `%` | `number`, `number` | 两个整数得到整数余数；其他数字组合得到浮点余数。 | Tapas 内置 |
| `%` | 实数数组与 `number`，顺序不限 | 逐元素计算浮点余数，返回同形实数数组，并保留左右操作数顺序。 | Tapas 内置循环和宿主 C 数学库的 `fmod` |
| `%` | 两个同形实数数组 | 对对应元素计算浮点余数，返回同形实数数组。 | Tapas 内置循环和宿主 C 数学库的 `fmod` |
| `^` | `number`, `number` | 幂运算，返回浮点数。 | 宿主 C 数学库 |
| `^` | 实数数组与 `number`，顺序不限 | 逐元素幂运算，返回同形实数数组。 | Tapas 内置循环 |
| `^` | 两个同形实数数组 | 对对应元素做幂运算，返回同形实数数组。 | Tapas 内置循环 |
| `@` | 两个实数数组 | 矩阵乘法。左侧列数必须等于右侧行数。 | CBLAS `dgemm` |

两个整数做除法或取余时，除数为零会报错。只要 `%` 的任一操作数是实数数组，
每个元素都会按 `fmod` 计算：结果符号跟随左操作数，浮点零除数等特殊情况遵循
宿主 C 数学库。

### 3. 比较运算符

| 运算符 | 合法操作数 | 结果与行为 |
|---|---|---|
| `==`, `!=` | 两个数字 | 按数值比较，允许整数与浮点数混合，返回 `bool`。 |
| `==`, `!=` | 实数数组与数字，顺序不限 | 逐元素比较，返回同形布尔数组。 |
| `==`, `!=` | 两个同形实数数组 | 逐元素比较，返回同形布尔数组。 |
| `==`, `!=` | 两个同类型的布尔值、字符串、列表、二元组、迭代器、布尔数组或时间值 | 按内容比较，返回一个 `bool`。列表和二元组递归比较其内容。 |
| `==`, `!=` | 两个字典、函数或库 | 按对象同一性比较，返回 `bool`。 |
| `==`, `!=` | 其他不同类型的值 | 分别返回 `false`、`true`。 |
| `>`, `<`, `>=`, `<=` | 两个数字 | 按数值比较，返回 `bool`。 |
| `>`, `<`, `>=`, `<=` | 两个时间值 | 按时间先后顺序比较，返回 `bool`。 |
| `>`, `<`, `>=`, `<=` | 实数数组与数字，顺序不限 | 逐元素比较，返回同形布尔数组。 |
| `>`, `<`, `>=`, `<=` | 两个同形实数数组 | 逐元素比较，返回同形布尔数组。 |

布尔数组不支持顺序比较。需要判断两个布尔数组内容是否完全相同时，可以使用
返回标量结果的 `==` 或 `!=`。

### 4. 逻辑、范围、成员测试和二元组

| 运算符 | 合法操作数 | 结果与行为 |
|---|---|---|
| `and`, `or` | `bool`, `bool` | 返回 `bool`，并按照短路规则决定是否计算右侧。 |
| `&`, `\|` | `bool`, `bool` | 返回 `bool`。两个操作数都会计算，不采用短路规则。 |
| `&`, `\|` | 布尔数组与 `bool`，顺序不限 | 将布尔值与每个数组元素运算，返回同形布尔数组。 |
| `&`, `\|` | 两个同形布尔数组 | 对对应元素进行逻辑与或逻辑或，返回同形布尔数组。 |
| `to` | `int`, `int` | 创建步长为 `1`、不包含终点的半开迭代器。 |
| `in` | 任意值与迭代器 | 判断该值是否属于迭代器。 |
| `in` | 任意值与列表 | 按内置值的内容相等规则查找元素。 |
| `in` | 任意值与字典 | 查找字典键。 |
| `in` | 右侧为其他类型 | 返回 `false`；数组目前不提供成员测试。 |
| `:` | 任意两个值 | 创建保存左右两个值的 `Pair`。 |

`&` 和 `|` 会计算两个操作数，并使用 Tapas 内置循环处理布尔数组。
`and` 和 `or` 仍然只支持标量布尔值，并保留短路语义。

### 5. 调用、索引和成员运算

| 写法 | 默认行为 |
|---|---|
| `value(arguments)` | 调用用户函数或内置函数；其他值不可调用。 |
| `value[index]` | 索引字符串、列表、二元组或字典，具体索引类型见第一部分。 |
| `array[row, column]` | 用两个整数或切片索引数组；两个整数返回标量，包含切片时返回数组。 |
| `value::name` | 按字符串键 `name` 读取字典或库的只读成员。 |
| `value.name` | 不跟 `(` 时等同于 `value::name`。 |
| `receiver.name(arguments)` | 隧道调用，等同于 `name(receiver, arguments)`。 |

调用参数和索引表达式均从左到右计算。索引越界、索引数量错误、成员不存在或值
不可调用时，程序会在运行时报错。

### 6. BLAS 运行时约定

Tapas 接受提供 `cblas_dcopy`、`cblas_daxpy`、`cblas_dscal` 和
`cblas_dgemm` 的 LP64 CBLAS 动态库。Tapas 按行优先的数组布局调用这些函数。
ILP64 接口以及只提供 Fortran BLAS 符号的动态库不属于当前支持范围。

如果环境变量 `TAPAS_BLAS_LIBRARY` 已设置，Tapas 只尝试加载它指定的动态库；
该值应当是动态库的路径或系统加载器能够识别的名称。如果没有设置，Tapas 会按
当前操作系统的常用名称查找可用实现。加载结果会在进程内缓存。

本版本不提供 `blas_available` 或 `blas_backend` 一类的语言接口。程序可以正常
使用不依赖 BLAS 的功能；只有实际执行依赖 BLAS 的运算时，缺少兼容后端才会
成为错误。

### 7. 数组运算汇总

| 能力 | 实数数组 | 布尔数组 |
|---|---|---|
| 二维整数索引、切片及赋值 | 支持 | 支持 |
| 一元 `+` | 不支持 | 不支持 |
| 一元 `-` | 逐元素；需要 CBLAS | 不支持 |
| 与同形数组进行 `+`, `-` | 逐元素；需要 CBLAS | 不支持 |
| 与数字进行 `*` | 标量乘法；需要 CBLAS | 不支持 |
| 其他 `+`, `-`, `*`, `/`, `%`, `^` | 与数字或同形实数数组逐元素运算 | 不支持 |
| `@` | 支持维度兼容的矩阵乘法；需要 CBLAS | 不支持 |
| `==`, `!=` | 逐元素，返回布尔数组 | 按完整内容比较，返回 `bool` |
| `>`, `<`, `>=`, `<=` | 与数字或同形实数数组逐元素比较 | 不支持 |
| `&`, `\|` | 不支持 | 与 `bool` 或同形布尔数组逐元素运算 |
| `and`, `or`, `in` | 不支持 | 不支持 |

形状不符合要求或操作数类型不受支持时，程序会在运行时报错。下面的程序验证
实数数组的默认运算行为：

```tapas
let operator_demo_matrix = array(2, 3, [1, 2, 3, 4, 5, 6])
sprint(-operator_demo_matrix)
sprint(operator_demo_matrix + 10)
sprint(10 - operator_demo_matrix)
sprint(operator_demo_matrix * 2)
sprint(operator_demo_matrix * operator_demo_matrix)
sprint(operator_demo_matrix % 4)
sprint(10 % operator_demo_matrix)
sprint(operator_demo_matrix % array(2, 3, [2, 2, 2, 3, 3, 3]))
sprint(operator_demo_matrix > 2)
sprint(operator_demo_matrix @ dense::transpose(operator_demo_matrix))
```
<pre class='Tapas-Return'>
[[-1, -2, -3],
 [-4, -5, -6]]
[[11, 12, 13],
 [14, 15, 16]]
[[9, 8, 7],
 [6, 5, 4]]
[[2, 4, 6],
 [8, 10, 12]]
[[1, 4, 9],
 [16, 25, 36]]
[[1, 2, 3],
 [0, 1, 2]]
[[0, 0, 1],
 [2, 0, 4]]
[[1, 0, 1],
 [1, 2, 0]]
[[false, false, true],
 [true, true, true]]
[[14, 32],
 [32, 77]]
</pre>

下面的程序验证 `&`、`|` 的优先级和布尔数组语义：

```tapas
let operator_demo_flags = array(1, 3, [true, false, true])
let operator_demo_mask = array(1, 3, [false, true, true])
print(true | false & false)
print(true | false and false)
sprint(operator_demo_flags & true)
sprint(false | operator_demo_flags)
sprint(operator_demo_flags & operator_demo_mask)
sprint(operator_demo_flags | operator_demo_mask)
```
<pre class='Tapas-Return'>
true
false
[[true, false, true]]
[[true, false, true]]
[[false, false, true]]
[[true, true, true]]
</pre>
