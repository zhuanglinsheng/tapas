# Tapas 编程语言规范

简体中文 | [English](Syntax_en.md) | [项目主页](../README.md)

本文是 Tapas 的规范性语言定义。
它独立于当前编译器实现；符合规范的编译器、格式化工具和语言服务器都必须遵守本文规则。

本文分为四部分：

1. 教程式语法说明及语言语义；
2. 明确的词法规则；
3. 完整的 EBNF 语法；
4. 运算符对内置类型的默认语义。

根内建函数、原生包和源码包的 API 见[标准库](Stdlib_zh.md)。

文中的“必须”“不得”“应当”和“可以”具有规范意义。
每个标记为`tapas`的代码块都可以执行。
Tapas 二进制程序会按文档顺序把这些代码块作为同一个程序读取，因此后面的代码块可以使用前面声明的名称。
标记为`text`的代码只用于展示写法，可能有意包含不完整或无效的 Tapas 源码。

## 第一部分——语言语义教程

### 1. 第一个程序

Tapas 是一门以表达式为核心、支持可选 Type 标注和编译期检查的脚本语言。
源文件由一条条语句组成。
通常情况下，换行或分号表示一条语句结束；但如果换行出现在圆括号、方括号、花括号或字符串内部，它只相当于普通空白，不会结束语句。

```tapas
var tutorial_limit = 5

function tutorial_square(x)
{
    return x * x
}

for (let tutorial_i in 0 to tutorial_limit) {
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

`var`和`let`都用于声明变量，但生命周期和捕获规则不同。
具名函数以`function`开头，后接函数名、参数列表和函数体；匿名函数字面量没有函数名，直接由参数列表和函数体组成。
`0 to 5`产生半开迭代器`0, 1, 2, 3, 4`。

可运行的入门程序见[Tapas 入门示例](examples/Basics_zh.md)。

### 2. 如何分隔语句

只要当前不在括号、列表、字典、代码块或字符串内部，换行和`;`都表示当前语句已经结束。
连续写多个换行或分号不会产生额外语句。
下面两种写法等价：

```text
let a = 1
let b = 2

let a = 1; let b = 2
```

`if`、`for`、`while`和函数等结构使用`{ ... }`包住内部代码。
这类结构在右花括号后不需要再写分号。
`if`、`elif`和`else`可以接在前一个分支的`}`后，也可以用换行、空行和注释隔开；分号会结束整条条件语句，不能放在分支之间。

在字符串之外，`//`表示从这里到本行末尾都是注释。
如果表达式没有放在括号、方括号或花括号中，就不能在注释后的下一行继续书写同一个表达式。

### 3. 值类型、引用类型与类型标注

Tapas 按赋值和传参时的行为区分值类型与引用类型。
`Bool`、`Int`和`Float`是值类型，赋值或传参会复制值本身。
其他内建对象是引用类型，赋值或传参只复制引用，因此多个变量可以指向同一个对象；第 5 节进一步说明复制与同一性。

Type 标注是可选的。
没有标注的变量可以先后保存不同 Type 的值；如果写了 Type 标注，编译器会检查初始化和后续赋值是否符合该约束。

```tapas
let tutorial_number: Int | Float = 1
tutorial_number = 1.5
```

上例中的标注允许`tutorial_number`保存`Int`或`Float`，因此两次赋值都能通过编译期检查。

源码直接提供以下字面量：

| Type | 字面量示例 | 说明 |
|---|---|---|
| `Bool` | `true`, `false` | 条件只接受 `Bool`，不进行隐式真值转换。 |
| `Int` | `0`, `12`, `42` | 严格的十进制整数。 |
| `Float` | `0.5`, `.5`, `5.`, `1e3`, `1.5e-3` | 普通小数或科学计数法。 |
| `String` | `'text'`, `"text"` | 单引号和双引号写法含义相同。 |

整数不接受前导零和进制前缀，因此`00`、`010`、`0x10`、`0b10`和`0o10`都是编译错误。
浮点数的指数以`e`或`E`开头，可以带正负号，并且必须包含数字，例如`1E+6`、`.5e2`和`5.e-1`。
数字前的`+`或`-`是一元运算符，不属于字面量本身。

内建引用 Type 包括`String`、`List`、`Pair`、`Dictionary`、`Iterator`、`Function`、`Library`、`RealArray`、`BoolArray`、`Time`和`Type`。
Rule 系统还提供`Rule`、`RuleInstance`、`RuleIR`、`RuleTerm`、`RuleItem`和`Evaluator`等引用 Type。
原生扩展可以通过 Tapas 对象接口提供新的引用 Type。

`Nil`是函数没有可用结果时返回的内部值，不属于可存储的值类型或引用类型。
源码中没有`nil`字面量，变量和集合也不能保存`Nil`；单独写`return`会向调用方返回`Nil`。

Type 的构造和静态检查详见[类型系统](TypeSystem_zh.md)，Rule 相关值详见[Rule](Rules_zh.md)，集合、数组和时间等能力详见[标准库](Stdlib_zh.md)，扩展值详见[C 交互](Foreign_zh.md)。
包含联合 Type、枚举 Type、结构 Type 和参数化容器的完整程序见[Type 示例](examples/syntax/types.tap)。

有限字符串集合使用`types::enum`建立静态枚举 Type；`enum`不是声明关键字：

```tapas
let TutorialOrderStatus = types::enum(
    'Pending',
    'Shipped',
    'Delivered',
)
let tutorial_status: TutorialOrderStatus = TutorialOrderStatus['Delivered']
```

构造参数必须是一个或多个直接 String 字面量，不能重复。
枚举成员在运行时仍是 String；枚举 Type 的 String 索引返回经过成员检查的值。
目标 Type 已知时可以直接使用成员字面量，非成员字面量是编译错误；普通 String 表达式必须先通过`types::matches`才能收窄为枚举。
成员集合相同的枚举结构等价，较小成员集合可赋值给包含它的枚举，任意枚举可赋值给`String`。
完整的等价、可赋值、动态索引、反射和运行时擦除规则见[枚举 Type](TypeSystem_zh.md#24-枚举-type)。

### 4. 变量、作用域与赋值

声明可以省略初始值。
一条语句可以声明多个变量：

```tapas
var tutorial_origin = 0, tutorial_step = 1
let tutorial_message = 'hello'
tutorial_message = 42

let tutorial_count: Int
tutorial_count = 3
```

变量必须先声明并初始化，之后才能读取；第一次赋值可以完成初始化。
同一作用域内不能重复声明同名变量，`var`与`let`之间也不能重名。
嵌套函数可以声明与外层同名的变量，此时内层名称会暂时遮住外层名称。
程序最外层的内置名称不能重新赋值。

初始化按实际控制流判断。
变量只有在到达读取位置的每条继续执行路径上都已赋值，才视为已初始化。
完整的`if`／`elif`／`else`可以共同完成初始化；缺少`else`时还存在所有条件均不成立的路径。
`while`和`for`可能一次也不执行，因此循环体中的赋值不能证明循环后的变量已初始化。
已有变量作为`for`目标时，在每次循环体入口处已初始化，但循环结束后仍恢复循环前的确定初始化状态。

`var`是环境变量，从声明位置开始存在到当前模块或函数结束。
嵌套函数可以按引用捕获并修改同一个变量。
不能在`if`、`elif`、`else`、`for`或`while`的`{ ... }`内声明`var`；需要这样的变量时，应在当前函数或模块的最外层提前声明。

`let`是临时变量，只在它所在的最小`{ ... }`范围内有效。
程序离开这个代码块后，变量随即消失，嵌套函数不能捕获它。
这里需要区分词法作用域与闭包捕获：嵌套函数的定义可能位于`let`的词法作用域内，但它形成的闭包可能在当前代码块结束后继续存在。
`let`使用代码块所属的临时存储，不进入闭包环境；禁止捕获使其能够在离开代码块时确定地释放，而不需要自动延长生命周期。
需要闭包共享的状态应声明为`var`。
函数参数属于本次函数调用的环境，可以被嵌套函数捕获。

具名`function`声明是只读的环境绑定。
后续具名函数和匿名闭包可以捕获它，但任何作用域都不能给该名称重新赋值。
它因此适合模块级函数之间的稳定引用，同时不改变普通`let`的临时存储规则。

赋值目标只能是变量，或对变量进行一次直接索引：

```tapas
var tutorial_value = 1
tutorial_value = 2

let tutorial_items = [10, 20]
tutorial_items[0] = 11
```

不能向只读成员访问表达式赋值。
语言也不支持向临时计算结果或多层索引直接赋值，例如`make_list()[0] = 1`和`matrix[0][1] = 1`都是无效写法。

### 5. 复制与同一性

赋值或传递值类型会复制值本身；赋值、传递或插入引用类型的值只复制引用，因此通过任一别名进行的修改对其他别名可见。

```tapas
let tutorial_inner = [1, 2]
let tutorial_outer = [tutorial_inner]
tutorial_inner[0] = 9
pprint(tutorial_outer)
```
<pre class='Tapas-Return'>
[[9, 2]]
</pre>

对引用类型调用`copy(value)`会创建独立的外层对象，但列表、二元组或字典内部的元素仍采用浅复制。
`identical(a, b)`检查运行时同一性；`==`调用 Type 自身的相等运算，不能替代同一性检查。

### 6. 字符串、列表、对和字典

字符串可使用任一种引号，并可跨物理行。
Tapas 字符串是字节串，索引按字节而不是 Unicode 码点计数。
语言没有转义序列；反斜杠是普通字符，字符串内部不能出现作为分隔符的同种引号。

字符串和列表接受一个整数索引，负索引从末尾反向计数。
二者也接受半开切片`start:end`，任一端点都可以省略。
负数端点从末尾开始计算，例如`-1`表示最后一个位置。

```tapas
let tutorial_text = 'Tapas'
let tutorial_numbers = [0, 1, 2, 3, 4]
print(tutorial_text[1:4])
pprint(tutorial_numbers[:3])
pprint(tutorial_numbers[-2:])
```
<pre class='Tapas-Return'>
apa
[0, 1, 2]
[3, 4]
</pre>

字符串整数索引返回单字节字符串。
向字符串索引或切片赋字符串会替换对应范围。
列表元素赋值接受任何非`nil`值；列表切片赋值不受支持。

冒号运算符构造二元组（Pair）。
连续使用冒号时从右向左分组，因此`a : b : c`等于`a : (b : c)`。
二元组固定包含两个元素，只接受索引`0`、`1`、`-2`和`-1`。

字典字面量由逗号分隔的“键 : 值”项目组成。
键可以是运行时支持的任意可哈希值；模块 API 和命名成员应使用字符串键。
重复键覆盖先前值。
字典迭代顺序未规定。

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

`dictionary::name`等同于`dictionary['name']`。
不跟参数列表的`dictionary.name`也表示相同的只读查找。

切片、列表修改、二元组、字典和浅复制的组合用法见[值与容器示例](examples/syntax/values_and_collections.tap)。

### 7. 稠密数组

数组需要两个索引，每个索引可以是整数或切片。
两个整数返回标量；任一索引为切片时返回新数组。

```tapas
let tutorial_matrix = array(2, 3, [1, 2, 3, 4, 5, 6])
print(tutorial_matrix[1, 2])
pprint(tutorial_matrix[0:2, 1:3])
```
<pre class='Tapas-Return'>
6
[[2, 3],
 [5, 6]]
</pre>

为单个数组元素赋值时，新值的类型必须与数组元素类型一致。
为数组切片赋值时，新数组的元素类型和形状都必须与目标切片一致。
实数数组支持与数字或同形实数数组进行逐元素`+`、`-`、`*`、`/`、`%`、`^`和比较；`@`表示矩阵乘法。
其中，矩阵乘法、同形数组加减、数组与标量相乘和数组取负需要在运行时使用 CBLAS；其余逐元素运算由 Tapas 自己实现。
安装 Tapas 时不要求系统已经安装 BLAS，详细规则见第四部分。
布尔数组支持使用`&`、`|`与布尔值或同形布尔数组进行逐元素逻辑运算；这两个运算符不采用短路规则。

矩阵范数、外积、原地更新和矩阵乘法见[稠密数组示例](examples/syntax/dense_arrays.tap)。

### 8. 调用、索引、成员与隧道调用

调用函数时，Tapas 先确定要调用的函数，再从左到右计算各个参数。
用户函数和内置函数都可以像普通值一样保存和传递。
`value(args)`表示调用，`value[indices]`表示索引，`value::name`表示读取名称为`name`的成员，但不能通过这种写法修改成员。

Tapas 还提供隧道调用：

```text
receiver.function_name(arg1, arg2)
```

其定义为：

```text
function_name(receiver, arg1, arg2)
```

因此`values.append(3)`等于`append(values, 3)`。
这里的`append`是当前作用域中的函数名，并不是从`values`对象中取出的成员。
点号后的名称如果紧跟`(`，就是隧道调用；否则表示只读成员访问。
调用、索引和成员访问可以连续书写，并按从左到右的顺序执行。

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
| 逻辑非 | `not` | 右结合 |
| 逻辑与 | `and` | 左结合 |
| 逻辑或 | `or` | 左结合 |
| 对 | `:` | 右结合 |

比较、`to`和`in`不能直接连续书写；需要组合时必须用括号明确分组。
`&`和`|`总会计算两个操作数，不会短路。
`not`是保留关键字，不能再用作变量名或函数名。

可运行示例见[逻辑取反](examples/syntax/logical_not.tap)。

普通表达式中的`not`只接受标量 Bool，返回其逻辑取反；操作数恰好求值一次。静态已知的非 Bool 操作数在编译时报错，动态值在运行时检查；不进行隐式真假转换。Rule 自身的表达式中还允许`not RuleInstance`，检查实例后取反，结果仍为 Bool；普通函数不获得此扩展，裸 Rule 始终不接受。详见 [Rule 否定](Rules_zh.md#25-逻辑否定-not)。

`not`的优先级低于比较、`in`、`&`、`|`，高于`and`和`or`。例如，`not x > 0`表示`not (x > 0)`，`not a and b`表示`(not a) and b`，`not not a`表示`not (not a)`。作为比较或更高优先级运算的操作数时需要括号，例如`a == (not b)`。

`and`和`or`采用短路规则：只在结果尚不能确定时计算右侧。
普通函数与表达式中的`and`、`or`只接受 Bool。Rule 自身表达式中的`and`、`or`也接受 RuleInstance，按从左到右短路检查成立性，返回 Bool；错误正常传播，跳过的实例不构造、不检查。两行裸实例是独立要求，一个`and`/`or`则是组合条件。详见 [Rule 逻辑组合](Rules_zh.md#26-逻辑组合-and--or)。

`&`、`|`还可以处理布尔数组。
每种值类型自行规定它支持的算术和比较操作；类型组合不受支持时，程序会在运行时报错。
整数除法向零截断。
除数为零、整数溢出以及无效数组形状均为运行时错误。

例如，`a | b & c`按`a | (b & c)`分组，`a and b | c`按`a and (b | c)`分组，而`x > 0 & y > 0`按`(x > 0) & (y > 0)`分组。

优先级、整数运算、成员测试和短路求值见[运算符示例](examples/syntax/operators.tap)。

### 10. 函数与闭包

固定参数函数列出零个或多个互不相同的参数名：

```tapas
function tutorial_hypotenuse(x, y)
{
    return math::sqrt(x * x + y * y)
}
print(tutorial_hypotenuse(3, 4))
```
<pre class='Tapas-Return'>
5
</pre>

需要代码块的语句允许把左花括号放在头部末尾或后续独立行。
头部与`{`之间可以出现空行和注释；分号始终结束当前语句：

```tapas
function tutorial_add(left: Int, right: Int) -> Int
{
    return left + right
}
```

静态已知函数签名的参数数量不匹配是编译错误；动态调用只能在运行时检查。
变参函数以`...`作为完整参数列表，并通过`__nparam__()`和`__param__(index)`读取参数。

函数参数类型标注写成`name: Type`，返回值类型标注写成`-> Type`。
参数允许混合标注与未标注形式：

```tapas
function convert(value: Int | String, strict: Bool, context) -> String
{
    if (strict) {
        return str(value)
    }
    return 'value'
}
```

签名标注使用[类型系统](TypeSystem_zh.md)定义的`type-expression`和可赋值关系，只参与编译期分析，不自动插入运行时检查。
`...`不能携带参数标注，但变参函数字面量仍可以在参数列表后使用`-> Type`标注返回值。
没有返回值标注时，函数返回值 Type 为`Unknown`。

精确函数 Type 写成`Function[参数 Type...] -> 返回值 Type`，可以用于高阶函数参数、返回值和普通绑定。
`Function[] -> T`表示无参数函数，`Function[...] -> T`表示变参函数；`->`右结合。

具名函数默认使用`function`声明，并建立一个只读绑定。
函数字面量本身没有固定名称，只在确实需要函数值表达式、回调或闭包时使用。
函数内部可以用`this`再次调用当前函数，因此递归不依赖外部变量名。
`base`指向创建当前函数的上一层函数环境。
只有确实存在当前函数或父函数环境时，才能分别使用`this`或`base`。

```tapas
function tutorial_factorial(n)
{
    if (n <= 1) {
        return 1
    }
    return n * this (n - 1)
}
print(tutorial_factorial(6))
```
<pre class='Tapas-Return'>
720
</pre>

类型标注、递归、闭包和变参函数的组合用法见[函数示例](examples/syntax/functions.tap)。

### 11. Rule 与子规则组合

Rule 是可以保存、传递和组合的规则值。
带参数的 Rule 使用`rule (参数) { ... }`，而无参数 Rule 可以省略参数列表。
Rule 参数必须带有 Type 标注，Rule 体最外层的 Bool 表达式表示必须满足的 Condition：

```tapas
let tutorial_positive = rule (value: Int) {
    'value must be positive':
        value > 0
}

let tutorial_small_positive = rule (value: Int) {
    tutorial_positive(value)
    'value must be below ten':
        value < 10
}

assert(tutorial_small_positive(5))
```

调用 Rule 只会绑定参数并产生 RuleInstance，不会立即检查 Condition。
`assert`、`rules::check`或 evaluator 消费 RuleInstance 时才会执行检查。

Rule 体最外层的 Bool 表达式要求结果为 true，裸 RuleInstance 表达式要求子规则成立并保留依赖与违规路径。`require`关键字已移除，旧写法`require R(x)`迁移为`R(x)`。
Rule 体还可以使用局部`let`，并可以用 String 和冒号为一个 Condition 或 Condition 代码块添加说明。
Rule 体不接受`var`、赋值、控制流、导入或直接 IO。

Rule 的精确 Type、捕获规则、检查接口、公共 IR 和 evaluator 详见[Rule 文档](Rules_zh.md)。

### 12. 条件与循环

`if`结构会从上到下检查条件，只执行第一个条件为真的分支。
`elif`必须紧跟在同一个`if`结构中，可以有任意多个；`else`最多一个且必须位于最后。

```tapas
function tutorial_sign(x)
{
    if (x < 0) {
        return -1
    } elif (x > 0) {
        return 1
    } else {
        return 0
    }
}
```

`while (condition) { body }`在布尔条件为真时重复执行。
`for`有两种形式：

```text
for (let item in iterable) { body }
for (item in iterable) { body }
```

可迭代值必须是迭代器、列表，或明确实现迭代协议的扩展值。
字典不能直接迭代，应使用`keys(dictionary)`或`values(dictionary)`。
`let`循环变量只在循环内存在；使用已有变量时，该变量在循环后仍可见。

`break`和`continue`只能出现在同一函数内的循环中，不能越过嵌套函数边界。
`return`结束当前函数；模块顶层的`return`定义模块导出值。
其他顶层控制流无效。

分支、`for`、`while`、`break`和`continue`的完整程序见[控制流示例](examples/syntax/control_flow.tap)。

### 13. 模块与导入

每个`.tap`文件都是一个模块，也可以作为脚本直接运行。
直接运行脚本时，Tapas 从上到下执行文件中的顶层语句，不要求脚本定义`main`函数；示例文件名`main.tap`只表示它是程序的主脚本，并不是语言规定的特殊文件名。
`.md`文件也可以作为模块或脚本，其中只有标记为`tap`或`tapas`的代码围栏会被编译，其他行的位置则保留下来以便报告诊断。

```text
import path/to/module.tap
import path/to/module.tap as module_name
import 'path with spaces/module.tap' as module_name
```

`import`单独占一条语句。
解析相对路径时，Tapas 依次在当前文件所在目录、会话搜索路径和程序的当前工作目录中查找。
路径没有文件后缀时，Tapas 依次尝试原路径、`.tap`、`.md`和目录中的`__init__.tap`。

不写`as`的导入只执行模块，不保存模块结果；写成`as name`时，名称`name`会引用一个`Library`值。
模块可以在顶层返回一个字典，并以字典中的键作为导出名称。
模块没有返回值时得到空库；返回值既不是字典也不是`nil`时会报错。
循环导入必须被检测并报告，不得无限递归或产生部分初始化的库。

目录中的`__init__.tap`是该目录包的入口；如果它导出`main(arguments: List[String])`，该包便可以通过`tapas -m package [arguments]`作为程序运行。
此时 Tapas 会调用`main`，把命令行参数作为字符串列表传入；`main`返回`Int`时，该值用作进程退出码，返回`Nil`时表示成功。
因此，`main`是可执行目录包的入口函数，而普通脚本的入口是文件本身及其顶层语句。

两种模块形式的完整示例见单文件模块[library.tap](examples/modules/library.tap)、目录包入口[greeter/__init__.tap](examples/modules/greeter/__init__.tap)和使用相对路径导入二者的可执行脚本[main.tap](examples/modules/main.tap)。
目录包的运行方式和`main`函数约定详见[使用说明](Usage_zh.md#执行源码包)。

### 14. 错误与兼容写法

词法或语法错误必须包含文件、行、列和出错标记；上下文约束在编译期报告；动态类型和边界问题在运行时报错。
错误不得被静默改写为不同程序。

#### 14.1 已弃用的旧写法

“已弃用的旧写法”是指过去曾属于正式语言、现在只为迁移旧代码而保留，并且计划在未来删除的语法。
本规范目前没有把任何写法归入这一类。

#### 14.2 兼容写法

当前规范没有额外的兼容写法。

## 第二部分——词法规则

### 1. 源文本与位置

编译器按字节读取源文件。
语法中的标点、关键字、标识符和数字都使用 ASCII 字符；字符串可以包含除自身结束引号外的其他字节。
LF、CRLF 和单独的 CR 都表示一次换行。
错误位置的行号和列号从 1 开始，其中列号按字节计算。

### 2. 空白、嵌套与语句分隔

空格、水平制表符、垂直制表符和换页符都属于普通空白。
圆括号、方括号、花括号和字符串之外的换行会生成语句分隔标记`SEP`；它们内部的换行只算普通空白。
字符串之外的`;`也会生成`SEP`。
连续出现多个分隔符时，效果与一个分隔符相同。

### 3. 注释

```ebnf
line-comment = "//", { any-code-point-except-LF } ;
```

编译器把注释当作空白处理。
Tapas 不支持块注释。

### 4. 标识符与关键字

```ebnf
identifier-start    = ASCII-letter | "_" ;
identifier-continue = identifier-start | decimal-digit ;
identifier          = identifier-start, { identifier-continue } ;
```

标识符区分大小写。
以`__`开头的名称为实现和标准库保留，用户程序不应声明。
保留字如下：

```text
and as base break continue elif else false for function if import in
let nil not of or return rule this to true var while
```

`function`用于具名声明，但不能作为表达式的开头；`rule`用于 Rule 表达式，裸 RuleInstance 表达式用于在 Rule 体中组合子规则；`nil`和`of`同样不能作为表达式的开头。
关键字必须是完整单词，例如`format`中的`for`不会被识别为关键字。

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

除`0`外，整数字面量不能以`0`开头。
数字前面的正负号是单独的运算符，不是数值字面量的一部分。
整数超出可表示范围，或浮点数字面量超出运行时支持的范围，都会导致编译错误。

### 6. 字符串字面量

```ebnf
single-string = "'", { code-point-except-single-quote }, "'" ;
double-string = '"', { code-point-except-double-quote }, '"' ;
```

两种引号语义相同。
字符串可以包含换行和另一种引号，没有转义序列。

### 7. 运算符和标点

当多个符号拥有相同前缀时，编译器总是优先识别最长的那个：

```text
多字符： ==  !=  >=  <=  ::  ...  ->  //
单字符： + - * / % @ ^ & | > < = : . , ; ( ) [ ] { }
单词运算符：and or not in to
```

`//`用于开始注释。
`.`只有符合前面的浮点数字面量格式时才属于数字；其他情况下，它用于成员访问或隧道调用。

### 8. 导入路径

`import`后的路径如果没有引号，只能包含下列字符；遇到空白、分号、注释或关键字`as`时，路径结束。
路径本身包含空格时，必须用引号把它写成字符串。

```ebnf
path-character       = ASCII-letter | decimal-digit | "_" | "-" | "."
                     | "/" | "\\" | ":" ;
unquoted-import-path = path-character, { path-character } ;
```

### 9. Markdown 源码提取

在 Markdown 中，一行去掉行首空白后，如果以三个反引号开头，并紧接`tap`或`tapas`，就表示 Tapas 代码块开始；语言标签后只能有空白。
结束行同样允许行首空白，但必须包含至少三个反引号，之后也只能有空白。
编译器只读取围栏内的代码，其他行按空行处理，以保留正确的报错行号。
`Tapas-Return`输出区域不会参与编译。

## 第三部分——语法规则（EBNF）

### 1. 记号与词法终结符

在下面的 EBNF 中，`=`用于定义规则，`,`表示前后相接，`|`表示任选其一，`[ ]`表示内容可以省略，`{ }`表示内容可以重复。
带引号的文本必须原样出现在源码中；大写名称表示第二部分定义的词法标记。

```ebnf
IDENTIFIER = identifier excluding reserved words ;
INTEGER    = integer-literal ;
FLOAT      = float-literal ;
STRING     = single-string | double-string ;
PATH       = unquoted-import-path ;
LINE_SEP   = top-level newline ;
SEP        = LINE_SEP | ";" ;
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

在`{ ... }`代码块中，只要换行没有位于更内层的括号、列表、字典、代码块或字符串中，它就会分隔前后两条语句。

### 3. 声明与赋值

```ebnf
declaration       = variable-declaration | function-declaration ;
variable-declaration = ("var" | "let"), declarator, { ",", declarator } ;
declarator        = IDENTIFIER, [ ":", type-expression ], [ "=", expression ] ;
function-declaration = "function", IDENTIFIER, parameter-list,
                       [ return-annotation ], block ;
type-expression   = union-type ;
union-type        = primary-type, { "|", primary-type } ;
primary-type      = function-type | instance-type | type-application | qualified-type-name ;
instance-type     = [ "types::" ], "InstanceOf", "[", [ ";" ],
                    qualified-type-name, [ "," ], "]" ;
function-type     = ( "Function" | "types::Function" ), "[",
                    [ type-arguments, [ "," ] | "..." ], [ ";" ], "]",
                    "->", type-expression ;
type-application  = qualified-type-name,
                    "[", [ type-arguments, [ "," ] ], [ ";" ], "]" ;
type-arguments    = type-expression, { ",", type-expression } ;
qualified-type-name = IDENTIFIER, { "::", IDENTIFIER } ;
assignment        = assignment-target, "=", expression ;
assignment-target = IDENTIFIER, [ index-suffix ] ;
```

### 4. 控制流

```ebnf
if-statement    = "if", "(", expression, ")", block,
                  { { LINE_SEP }, "elif", "(", expression, ")", block },
                  [ { LINE_SEP }, "else", block ] ;
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

`STRING`提供去除引号后的原始内容。

### 6. 表达式与优先级

```ebnf
expression      = pair-expression ;
pair-expression = or-expression, [ ":", pair-expression ] ;
or-expression   = and-expression, { "or", and-expression } ;
and-expression  = not-expression, { "and", not-expression } ;
not-expression  = "not", not-expression | elementwise-or-expression ;
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

切片端点使用`or-expression`，从而排除无括号的对运算符。
字符串和列表要求一个索引参数，数组要求两个；这些数量限制属于语义检查。

### 7. 基本表达式与字面量

```ebnf
primary-expression = INTEGER | FLOAT | STRING | "true" | "false"
                   | IDENTIFIER | "this" | "base"
                   | parenthesized-expression | list-literal
                   | dictionary-literal | function-literal | rule-literal ;
parenthesized-expression = "(", expression, ")" ;
list-literal = "[", [ argument-list ], "]" ;
dictionary-literal = "{", [ dictionary-entry,
                            { ",", dictionary-entry }, [ "," ] ], "}" ;
dictionary-entry = or-expression, ":", expression ;
function-literal  = parameter-list, [ return-annotation ], block ;
parameter-list    = "(", [ fixed-parameters | "..." ], ")" ;
fixed-parameters  = parameter, { ",", parameter }, [ "," ] ;
parameter         = IDENTIFIER, [ ":", type-expression ] ;
return-annotation = "->", type-expression ;

rule-literal = "rule",
               [ "(", [ rule-parameters ], ")" ],
               rule-block ;
rule-parameters = rule-parameter,
                  { ",", rule-parameter }, [ "," ] ;
rule-parameter = IDENTIFIER, ":", type-expression ;
rule-block = "{", separators, [ rule-item-list ], separators, "}" ;
rule-item-list = rule-item, { separator-run, rule-item } ;
rule-item = rule-let-declaration
          | condition-statement
          | described-condition-statement
          | implication-statement ;
rule-let-declaration = "let", declarator, { ",", declarator } ;
condition-statement = expression ;
implication-statement = [ STRING, ":", separators ],
                        expression, "implies",
                        ( expression | condition-block ) ;
described-condition-statement = STRING, ":", separators,
                                ( expression | condition-block ) ;
condition-block = "{", separators, condition-statement,
                  { separator-run, condition-statement },
                  separators, "}" ;
```

具名声明和函数字面量共用参数、返回值标注与函数体规则。
具名声明建立只读绑定，函数字面量仍是普通表达式。
Rule 字面量同样是普通表达式，但使用只接受 Rule 项目的专用代码块。
Rule 项目以 String 字面量开头且后面直接跟`:`时，按带说明的 Condition 解析，而不按普通 Pair 表达式解析。
需要代码块的头部处于等待`{`的状态时，头部后的换行不构成`SEP`；普通完整表达式后的换行仍然分隔语句。

语法中有意不包含`nil`。
表达式位置的`{}`是空字典；代码块只出现在复合语句或函数要求的位置。

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
10. 变量和集合不能保存 `nil`；
11. Rule 参数必须带有 Type 标注，且参数名称不能重复；
12. Rule 体最外层只允许局部 `let`、Bool/RuleInstance 项、带说明的项目和 `implies`；
13. Condition 必须产生 Bool，说明必须直接写成 String 字面量；
14. 裸 RuleInstance 项保留子规则依赖；裸 Rule 值不是合法规则项；
15. `implies` 只能作为 Rule 项，前件为 Bool 或 RuleInstance 表达式（可为两者的联合 Type），非空后件仍为 Bool 表达式；后件块不接受声明、裸实例要求或嵌套蕴含。它不是普通 Bool 运算符，详细语义见 [Rule 文档](Rules_zh.md#24-蕴含规则项-implies)。

## 第四部分——运算符对内置类型的默认语义

### 1. 通用规则

本部分集中规定运算符对 Tapas 内置类型的默认行为，供编译器、语言服务器和其他工具直接查阅。
数字 Type 为`Int | Float`；实数数组使用`RealArray`，布尔数组使用`BoolArray`。
“同形数组”要求行数和列数都相同。

除本部分明确列出的组合外，其他操作数组合均在运行时报类型错误。
Tapas 不进行隐式真值转换，也不在两个不同形状的数组之间进行广播。
数字与实数数组运算时，该数字会应用到数组的每个元素。

实数数组的部分运算必须通过 BLAS 完成。
Tapas 编译和安装时不链接 BLAS；程序第一次执行这类运算时，Tapas 才尝试加载用户环境中的 LP64 CBLAS 动态库。
如果找不到兼容的动态库，程序会在该运算处报运行时错误，不会改用 Tapas 内部的循环实现。

### 2. 一元和算术运算符

| 运算符 | 合法操作数 | 结果与行为 | 实现要求 |
|---|---|---|---|
| 一元 `+`, `-` | `Int \| Float` | 保持数值 Type，分别返回原值或相反数。 | Tapas 内置 |
| 一元 `-` | `RealArray` | 逐元素取相反数，返回同形 `RealArray`。`RealArray` 不支持一元 `+`。 | 单遍融合 |
| `+`, `-`, `*`, `/` | `Int \| Float`, `Int \| Float` | 两个 `Int` 得到 `Int`；只要一侧是 `Float` 就得到 `Float`。`Int` 除法向零截断。 | Tapas 内置 |
| `+` | `Time`, `Int` | 按指定秒数平移时间点；正数得到更晚的时间，负数得到更早的时间。返回新的 `Time`，且不支持 `Int + Time`。 | Tapas 内置和宿主 C 时间类型 |
| `-` | `Time`, `Int` | 从时间点减去指定秒数；正数得到更早的时间，负数得到更晚的时间。返回新的 `Time`。 | Tapas 内置和宿主 C 时间类型 |
| `-` | `Time`, `Time` | 返回左侧减右侧的秒数，结果为 `Float`。 | 宿主 C `difftime` |
| `+`, `-`, `/` | `RealArray` 与 `Int \| Float`，顺序不限 | 逐元素运算，返回同形 `RealArray`。减法和除法保留左右操作数顺序。 | Tapas 内置循环 |
| `*` | `RealArray` 与 `Int \| Float`，顺序不限 | 标量乘法，返回同形 `RealArray`。 | 单遍融合 |
| `+`, `-` | 两个同形 `RealArray` | 对对应元素做加法或减法，返回同形 `RealArray`。 | 单遍融合 |
| `*`, `/` | 两个同形 `RealArray` | 对对应元素做乘法或除法，返回同形 `RealArray`。 | Tapas 内置循环 |
| `%` | `Int \| Float`, `Int \| Float` | 两个 `Int` 得到 `Int` 余数；其他数字组合得到 `Float` 余数。 | Tapas 内置 |
| `%` | `RealArray` 与 `Int \| Float`，顺序不限 | 逐元素计算浮点余数，返回同形 `RealArray`，并保留左右操作数顺序。 | Tapas 内置循环和宿主 C 数学库的 `fmod` |
| `%` | 两个同形 `RealArray` | 对对应元素计算浮点余数，返回同形 `RealArray`。 | Tapas 内置循环和宿主 C 数学库的 `fmod` |
| `^` | `Int \| Float`, `Int \| Float` | 幂运算，返回 `Float`。 | 宿主 C 数学库 |
| `^` | `RealArray` 与 `Int \| Float`，顺序不限 | 逐元素幂运算，返回同形 `RealArray`。 | Tapas 内置循环 |
| `^` | 两个同形 `RealArray` | 对对应元素做幂运算，返回同形 `RealArray`。 | Tapas 内置循环 |
| `@` | 两个 `RealArray` | 矩阵乘法。左侧列数必须等于右侧行数。 | CBLAS `dgemm` |

两个整数做除法或取余时，除数为零会报错。
只要`%`的任一操作数是实数数组，每个元素都会按`fmod`计算：结果符号跟随左操作数，浮点零除数等特殊情况遵循宿主 C 数学库。

### 3. 比较运算符

| 运算符 | 合法操作数 | 结果与行为 |
|---|---|---|
| `==`, `!=` | 两个 `Int \| Float` | 按数值比较，允许 `Int` 与 `Float` 混合，返回 `Bool`。 |
| `==`, `!=` | `RealArray` 与 `Int \| Float`，顺序不限 | 逐元素比较，返回同形 `BoolArray`。 |
| `==`, `!=` | 两个同形 `RealArray` | 逐元素比较，返回同形 `BoolArray`。 |
| `==`, `!=` | 枚举值与 String | 按 String 内容比较，返回 `Bool`；直接 String 字面量必须是该枚举的成员。 |
| `==`, `!=` | 两个同 Type 的 `Bool`、`String`、`List`、`Pair`、`Iterator`、`BoolArray` 或 `Time` | 按内容比较，返回一个 `Bool`。`List` 和 `Pair` 递归比较其内容。 |
| `==`, `!=` | 两个 `Dictionary`、`Function` 或 `Library` | 按对象同一性比较，返回 `Bool`。 |
| `==`, `!=` | 其他不同类型的值 | 分别返回 `false`、`true`。 |
| `>`, `<`, `>=`, `<=` | 两个 `Int \| Float` | 按数值比较，返回 `Bool`。 |
| `>`, `<`, `>=`, `<=` | 两个 `Time` | 按时间先后顺序比较，返回 `Bool`。 |
| `>`, `<`, `>=`, `<=` | `RealArray` 与 `Int \| Float`，顺序不限 | 逐元素比较，返回同形 `BoolArray`。 |
| `>`, `<`, `>=`, `<=` | 两个同形 `RealArray` | 逐元素比较，返回同形 `BoolArray`。 |

布尔数组不支持顺序比较。
需要判断两个布尔数组内容是否完全相同时，可以使用返回标量结果的`==`或`!=`。
Rule 和 RuleInstance 不定义内容相等运算；需要判断是否为同一个运行时对象时，应使用`identical`。

### 4. 逻辑、范围、成员测试和二元组

| 运算符 | 合法操作数 | 结果与行为 |
|---|---|---|
| `not` | `Bool`；Rule 内另接受 `RuleInstance` | 返回取反后的 `Bool`，操作数只求值一次；实例在检查时求值。 |
| `and`, `or` | `Bool`；Rule 内另接受 `RuleInstance` | 返回 `Bool`，并按照短路规则决定是否计算右侧。 |
| `&`, `\|` | `Bool`, `Bool` | 返回 `Bool`。两个操作数都会计算，不采用短路规则。 |
| `&`, `\|` | `BoolArray` 与 `Bool`，顺序不限 | 将 `Bool` 与每个数组元素运算，返回同形 `BoolArray`。 |
| `&`, `\|` | 两个同形 `BoolArray` | 对对应元素进行逻辑与或逻辑或，返回同形 `BoolArray`。 |
| `to` | `Int`, `Int` | 创建步长为 `1`、不包含终点的半开 `Iterator`。 |
| `in` | 任意值与迭代器 | 判断该值是否属于迭代器。 |
| `in` | 任意值与列表 | 按内置值的内容相等规则查找元素。 |
| `in` | 任意值与字典 | 查找字典键。 |
| `in` | 右侧为其他类型 | 返回 `false`；数组目前不提供成员测试。 |
| `:` | 任意两个值 | 创建保存左右两个值的 `Pair`。 |

`&`和`|`会计算两个操作数，并使用 Tapas 内置循环处理布尔数组。
`and`和`or`仍然只支持标量布尔值，并保留短路语义。

### 5. 调用、索引和成员运算

| 写法 | 默认行为 |
|---|---|
| `value(arguments)` | 调用用户函数或内置函数；调用 Rule 时绑定参数并返回 RuleInstance。其他值不可调用。 |
| `value[index]` | 索引字符串、列表、二元组或字典，具体索引类型见第一部分。 |
| `array[row, column]` | 用两个整数或切片索引数组；两个整数返回标量，包含切片时返回数组。 |
| `value::name` | 按字符串键 `name` 读取字典或库的只读成员。 |
| `value.name` | 不跟 `(` 时等同于 `value::name`。 |
| `receiver.name(arguments)` | 隧道调用，等同于 `name(receiver, arguments)`。 |

调用参数和索引表达式均从左到右计算。
索引越界、索引数量错误、成员不存在或值不可调用时，程序会在运行时报错。

### 6. BLAS 运行时约定

Tapas 接受提供`cblas_dcopy`、`cblas_daxpy`、`cblas_dscal`、`cblas_ddot`、`cblas_dnrm2`和`cblas_dgemm`的 LP64 CBLAS 动态库。
Tapas 按行优先的数组布局调用这些函数。
ILP64 接口以及只提供 Fortran BLAS 符号的动态库不属于当前支持范围。

如果环境变量`TAPAS_BLAS_LIBRARY`已设置，Tapas 只尝试加载它指定的动态库；该值应当是动态库的路径或系统加载器能够识别的名称。
如果没有设置，Tapas 会按当前操作系统的常用名称查找可用实现。
加载结果会在进程内缓存。

本版本不提供`blas_available`或`blas_backend`一类的语言接口。
程序可以正常使用不依赖 BLAS 的功能；只有实际执行依赖 BLAS 的运算时，缺少兼容后端才会成为错误。

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
| `==`, `!=` | 逐元素，返回 `BoolArray` | 按完整内容比较，返回 `Bool` |
| `>`, `<`, `>=`, `<=` | 与数字或同形实数数组逐元素比较 | 不支持 |
| `&`, `\|` | 不支持 | 与 `Bool` 或同形 `BoolArray` 逐元素运算 |
| `not`, `and`, `or`, `in` | 不支持 | 不支持 |

形状不符合要求或操作数类型不受支持时，程序会在运行时报错。
下面的程序验证实数数组的默认运算行为：

```tapas
let operator_demo_matrix = array(2, 3, [1, 2, 3, 4, 5, 6])
pprint(-operator_demo_matrix)
pprint(operator_demo_matrix + 10)
pprint(10 - operator_demo_matrix)
pprint(operator_demo_matrix * 2)
pprint(operator_demo_matrix * operator_demo_matrix)
pprint(operator_demo_matrix % 4)
pprint(10 % operator_demo_matrix)
pprint(operator_demo_matrix % array(2, 3, [2, 2, 2, 3, 3, 3]))
pprint(operator_demo_matrix > 2)
pprint(operator_demo_matrix @ dense::transpose(operator_demo_matrix))
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

下面的程序验证`&`、`|`的优先级和布尔数组语义：

```tapas
let operator_demo_flags = array(1, 3, [true, false, true])
let operator_demo_mask = array(1, 3, [false, true, true])
print(true | false & false)
print(true | false and false)
pprint(operator_demo_flags & true)
pprint(false | operator_demo_flags)
pprint(operator_demo_flags & operator_demo_mask)
pprint(operator_demo_flags | operator_demo_mask)
```
<pre class='Tapas-Return'>
true
false
[[true, false, true]]
[[true, false, true]]
[[false, false, true]]
[[true, true, true]]
</pre>
