# Tapas 类型系统

简体中文 | [English](TypeSystem_en.md) | [项目主页](../README.md)

本文定义 Tapas 当前支持的类型系统，包括类型标注、运行时 `Type` 值、静态检查、
`types` 包和实现约束。本文是 `Syntax_zh.md` 中语言定义的类型系统补充；涉及类型
标注、Type 构造、静态检查和运行时 Type 操作时，以本文规则为准。

当前类型系统遵循四项原则：

- 类型标注只参与编译期分析，不改变值的运行时表示，也不自动插入检查或转换；
- `Type` 同时是普通运行时值，可以保存、传递、返回和导出；
- 用户定义的 Type 不可修改，且完整定义必须在编译期可见；
- 无法静态判断的值仍可流入带标注的位置，程序在动态边界使用
  `types::matches` 显式验证。

所有 Type 在逻辑上都是递归且不可变的映射：

```text
Dictionary[String, Type]
```

这只是统一的语义模型，不要求底层 C 对象包含普通 `tdict`。



## 1. Type 值与静态 Type

### 1.1 运行时表示

`Type` 与 String、List、Dictionary 和 Time 一样，是独立的复合值类别。普通
Dictionary 即使内容符合 Type 的逻辑形状，也不会自动成为 Type。

Type 建立后不可修改。索引、反射和遍历只能读取定义或取得普通容器副本，不能
取得内部可写位置。

Type 定义会递归引用其他 Type。唯一的递归终点是：

```text
types::AnyType -> {}
```

`types::AnyType` 是唯一具有空定义的 Type，同时表示任意值。用户不能建立空 Type：

```text
types::make_type() // 编译错误：字段 Type 不能为空
```

非递归定义的每条路径最终到达预定义 Type。递归 Type 使用具名定义和回引用形成
有限 Type 图；直接递归和同一作用域中的相互递归均受支持。运行时模块导入循环仍
属于模块错误，不因 Type 递归而开放。

### 1.2 预定义 Type

`types` 包导出以下 Type 值：

| 名称 | 表示的值 |
|---|---|
| `types::AnyType` | 任意值，也是 Type 定义的递归终点 |
| `types::Nil` | 内部空值 |
| `types::Bool` | 布尔值 |
| `types::Int` | 整数 |
| `types::Float` | 浮点数 |
| `types::String` | 字符串 |
| `types::List` | 不约束元素的 List |
| `types::Pair` | 不约束两个成员的 Pair |
| `types::Dictionary` | 不约束字段、键和值的 Dictionary |
| `types::Iterator` | 迭代器 |
| `types::Function` | 函数 |
| `types::Library` | 库 |
| `types::RealArray` | 实数数组 |
| `types::BoolArray` | 布尔数组 |
| `types::Time` | 时间点 |
| `types::Type` | Type 值 |

`types::List`、`types::Pair` 和 `types::Dictionary` 称为原始容器 Type。

### 1.3 静态 Type 表达式

运行时值为 Type，不代表该值可以用于类型标注。编译器只把以下表达式视为静态
Type 表达式：

1. `types` 包中的预定义 Type；
2. 当前作用域中已确定、且此后没有被重新赋值的静态 Type 绑定；
3. 模块接口导出的静态 Type 绑定；
4. 符合第 2 节要求的 `types::make_type`、`types::union`、`types::list`、
   `types::pair` 或 `types::dictionary` 直接调用。

类型标注还可以使用第 3.1 节定义的参数化 Type 应用。该语法只在类型标注中
构造 Type，不是普通索引表达式，也不会执行运行时代码。

静态 Type 可以通过 `let` 建立任意层别名：

```tapas
let Integer = types::Int
let AlsoInteger = Integer
let value: AlsoInteger = 1
```

普通函数调用、函数参数、索引、条件表达式、`var` 绑定和其他运行期运算都不是
静态 Type 表达式。编译器不会执行或内联普通函数来求得 Type：

```tapas
let choose = (flag){
    if(flag){
        return types::Int
    }
    return types::String
}

let Chosen = choose(true)
// Chosen 的运行时值是 Type，但 Chosen 不是静态 Type 绑定。
```

Type 可以在运行时保存和选择：

```tapas
var current = types::Int
current = types::String
```

`current` 的运行时类型是 `types::Type`，但不能用于类型标注。

五个 Type 构造器采用函数调用的语法，但属于编译期内建形式，不是可保存、传递
或间接调用的一等函数。它们只能出现在编译器可直接识别的调用中，并且参数必须是
静态 Type 表达式。编译器为结果生成运行时 Type 常量或模块初始化代码；程序不能
根据文件、网络或其他运行期数据建立新 Type 定义。



## 2. 建立 Type

### 2.1 字段 Type

`types::make_type` 接收一个或多个直接写在调用中的 Pair。每个 Pair 的左侧是字段
名，右侧是字段 Type：

```tapas
let Person = types::make_type(
    'name' : types::String,
    'age' : types::Int,
)
```

Pair 只是构造输入；结果在逻辑上仍是不可变的 `Dictionary[String, Type]`。
编译器按以下规则检查调用：

1. 至少包含一个参数；
2. 每个顶层参数都是直接写出的 Pair；
3. Pair 左侧是 String 字面量；
4. 字段名不重复，也不以保留字符 `@` 开头；
5. Pair 右侧是静态 Type 表达式。

逗号分隔字段。Pair 运算符按右结合解析，因此嵌套 Pair 不能代替多个参数：

```text
types::make_type('x' : types::Int : types::String)
// 编译错误：字段 x 的右侧不是 Type。
```

Dictionary 和保存 Pair 的变量也不能作为构造参数：

```text
let fields = {'name' : types::String}
let name_field = 'name' : types::String

let Person = types::make_type(fields)          // 编译错误
let OtherPerson = types::make_type(name_field) // 编译错误
```

字段名区分大小写，并按原始 UTF-8 字节序列区分，不执行 Unicode 规范化。字段都是
必需字段；当前类型系统没有可选字段。

一个值满足字段 Type，当且仅当它是 Dictionary、包含全部必需字段，并且每个必需
字段的值递归满足对应 Type。未声明的额外字段不影响匹配。

字段 Type 采用结构等价。名称、声明位置和字段书写顺序均不参与等价判断；字段名
及对应 Type 完全相同的两个定义等价。一个 Dictionary 因此可以同时满足多个字段
Type，不存在全局继承树。

### 2.2 参数化容器

List、Pair 和统一键值 Dictionary 分别由以下形式描述：

```tapas
let IntList = types::list(types::Int)
let Entry = types::pair(types::String, types::Int)
let Scores = types::dictionary(types::String, types::Float)
let Matrix = types::list(types::list(types::Float))
```

构造器参数必须是静态 Type 表达式，可以有限嵌套。构造结果可以先绑定名称，
也可以在类型标注中使用参数化 Type 应用直接书写：

```tapas
let values: IntList = [1, 2, 3]
let direct_values: List[Int] = [1, 2, 3]
let matrix: List[List[Float]] = []
let scores: Dictionary[String, Float] = {}
```

`List[T]`、`Pair[A, B]` 和 `Dictionary[K, V]` 分别与
`types::list(T)`、`types::pair(A, B)` 和 `types::dictionary(K, V)`
产生等价 Type。方括号形式只改善标注表达，不引入用户定义泛型模板。

原始容器 Type 只检查运行时类别，与使用 `types::AnyType` 的参数化容器不等价：

```tapas
let raw_list_is_distinct = types::List != types::list(types::AnyType)
```

字段 Type 和统一键值 Dictionary Type 是两种独立约束。前者要求一组固定字段，
后者检查容器中的所有键和值；当前类型系统不能在同一个 Type 中组合这两种约束。

Iterator 暂不支持元素 Type，因为 `types::matches` 无法在不消耗 Iterator 的
情况下检查所有元素。数组继续使用 `types::RealArray` 和 `types::BoolArray`；
形状和维数等整数参数不属于当前只接受 Type 参数的设计。

### 2.3 联合

`types::union` 接收至少两个静态 Type 参数：

```tapas
let Identifier = types::union(
    types::Int,
    types::String,
)
```

构造联合时，编译器依次执行：

1. 展开已有联合；
2. 删除等价的重复成员；
3. 按规范表示排序；
4. 去重后只剩一个成员时，直接使用该成员；
5. 包含 `types::AnyType` 时，直接使用 `types::AnyType`。

因此，下列两个联合等价，`types::members` 也会以相同顺序返回其成员：

```tapas
let canonical_identifier_a = types::union(types::Int, types::String)
let canonical_identifier_b = types::union(types::String, types::Int)
```

`types::union(T, T)` 合法，结果为 `T`；公开调用的原始参数少于两个则属于编译
错误。

### 2.4 递归 Type

递归 Type 使用无初始值、显式标注为 `Type` 的 `let` 声明建立定义位置，再由一次
静态 Type 赋值完成定义：

```text
let Tree: Type

Tree = types::make_type(
    'value': types::Int,
    'children': types::list(Tree),
)
```

声明后、定义完成前，名称只能出现在完成递归定义所需的静态 Type 表达式中，不能
作为普通运行时值读取。同一作用域可以先声明多个名称，再建立相互引用：

```text
let Left: Type
let Right: Type

Left = types::make_type('rights': types::list(Right))
Right = types::make_type('lefts': types::list(Left))
```

每个定义必须由字段、容器、联合或函数等 Type 构造形成实际结构；`A = A` 以及
`A = B; B = A` 这样的纯别名循环是编译错误。完成后的 Type 图不可修改。Type
变量以后重新赋值不会修改已经封闭的递归图，已有回引用仍指向原定义。

递归 Type 采用结构等价。等价、可赋值和匹配均记录已经访问的节点对或
“值—Type”组合，因此不会无限展开。打印只显示有限的递归标识，不尝试展开整个
图。模块接口保存结构化定义，而不是依赖展开后的显示文本。

### 2.5 规范表示与构造顺序

非递归 Type 的规范表示用于等价、排序和哈希，按以下规则生成：

1. `types::AnyType` 使用固定终点标识；
2. 其他预定义 Type 使用固定编号；
3. 字段 Type 按字段名的 UTF-8 字节顺序编码字段名长度、字段名和字段 Type；
4. 参数化容器编码原始容器编号、固定参数名和参数 Type；
5. 联合展开并去重，再按成员规范表示的字节顺序编码；
6. 每一段都包含类别和长度，不能直接拼接显示文本。

递归 Type 不生成无限规范字符串，而是直接比较带回引用的有限图。比较过程中的
节点编号只属于本次遍历，不由声明名称或对象地址决定。递归 Type 使用与结构等价
兼容的稳定哈希；等价 Type 必须具有相同哈希，但不同 Type 可以共享哈希值。规范
表示与图比较结果都不是打印格式，也不由 `types::definition` 返回。

字段 Type 另有一份构造顺序，即 `make_type` 中 Pair 的书写顺序。编译器和模块
接口保存这份顺序，供位置结构字面量使用；它不属于 Type 的结构定义，不参与
等价、可赋值或哈希判断。Type 别名和模块导入保留原 Type 的构造顺序。

两个等价字段 Type 可以具有不同构造顺序。它们的位置结构字面量分别按各自的
构造顺序解释，但仍表示同一个结构 Type。



## 3. 类型标注与对象构造

### 3.1 标注语法与名称解析

声明中的标注语法为：

```text
declarator       = IDENTIFIER, [ ":", type-expression ], [ "=", expression ] ;
type-expression  = function-type | type-application | qualified-type-name ;
function-type    = function-constructor, "[",
                   [ type-arguments, [ "," ] | "..." ], "]",
                   "->", type-expression ;
function-constructor = "Function" | "types::Function" ;
type-application = qualified-type-name,
                   "[", [ type-arguments, [ "," ] ], "]" ;
type-arguments   = type-expression, { ",", type-expression } ;
qualified-type-name = IDENTIFIER, { "::", IDENTIFIER } ;
```

参数化 Type 应用只接受下列内建构造器：

| 形式 | 参数数量 | 等价构造形式 |
|---|---:|---|
| `List[T]` | 1 | `types::list(T)` |
| `Pair[A, B]` | 2 | `types::pair(A, B)` |
| `Dictionary[K, V]` | 2 | `types::dictionary(K, V)` |
| `Union[A, B, ...]` | 至少 2 | `types::union(A, B, ...)` |
| `Function[A, B, ...] -> R` | 0 个以上参数及 1 个结果 | 无运行时构造形式 |

构造器可以写成 `types::List[T]` 等限定形式。参数本身是递归的
`type-expression`，因此可以有限嵌套。名称区分大小写；参数数量错误、基类型不支持
参数，或者任一参数不是有效 Type 表达式时，均为编译错误。参数列表允许尾随逗号；
空参数列表只对 `Function[] -> R` 有效。`Function[...] -> R` 表示变参函数；当前
不支持给变参项单独标注 Type。

未限定构造器名称遵守普通遮蔽规则。当前作用域存在同名绑定时不再回退到内建
构造器。当前版本不支持用户定义参数化 Type 构造器。

标注位置不能调用构造器或执行其他普通表达式：

```text
let Identifier = types::union(types::Int, types::String)
let value: Identifier = read()
let direct: Union[Int, String] = read()

let other: types::union(types::Int, types::String) = read() // 编译错误
let invalid: Int[String] = 1                                // 编译错误
```

预定义 Type 在标注位置可以省略 `types::`：

```tapas
let count: Int = 1
let name: String = 'Tapas'
```

冒号后的未限定名称按以下顺序解析：

1. 查找当前词法作用域中的同名绑定；
2. 绑定是静态 Type 时使用该 Type；
3. 绑定是普通值或非静态的运行时 Type 时报告错误，不再回退；
4. 当前作用域没有同名绑定时，查找 `types` 包中的预定义 Type；
5. 仍未找到时报告未知类型。

限定名称不执行回退。`model::Person` 必须从 `model` 的模块接口解析到静态 Type。
普通表达式也不执行预定义 Type 回退，因此取得运行时 Type 值时必须写完整名称：

```tapas
let schema = types::Int
```

普通值遮蔽预定义名称后，可以使用完整限定名：

```text
let Int = 42
let value: Int = 1        // 编译错误：Int 是普通值
let other: types::Int = 1
```

类型标注只向编译器提供目标 Type，不执行隐式转换，也不保存在运行时变量槽中。

### 3.2 函数签名标注

函数参数和返回值类型标注采用与声明相同的 `type-expression`：

```text
parameter        = IDENTIFIER, [ ":", type-expression ] ;
fixed-parameters = parameter, { ",", parameter }, [ "," ] ;
return-annotation = "->", type-expression ;
function-literal = parameter-list, [ return-annotation ], block ;
```

具名声明 `function name(parameters) -> Type { ... }` 使用同一套签名语法，语义
等同于把对应函数字面量绑定给只读名称 `name`。

固定参数可以分别标注，也可以保留为未标注参数：

```text
let find = (values: List[Int], target: Int, start) -> Int {
    // ...
}
```

精确函数 Type 可以标注高阶函数参数、结果和普通绑定：

```tapas
function apply(
        callback: Function[Int] -> String,
        value: Int,
) -> String {
    return callback(value)
}

let formatter: Function[Int] -> String = (value: Int) -> String {
    return str(value)
}
```

`->` 右结合，因此 `Function[] -> Function[Int] -> String` 表示一个不接收参数、
返回格式化函数的函数。函数 Type 的固定参数数量必须相同；参数 Type 逆变，结果
Type 协变。固定参数和变参签名不互相赋值。

参数标注在函数字面量的定义环境中解析，不受参数名称遮蔽。标注得到的 Type 属于
函数签名，并作为参数绑定的目标 Type 参与函数体分析。静态已知的调用目标也按
第 4.1 节的可赋值关系检查对应实参；`this` 使用当前函数的同一签名。

`-> Type` 标注函数结果。编译器按第 4.1 节检查每个可达出口：带表达式的
`return` 检查该表达式，无表达式的 `return` 和执行到函数体末尾均视为返回
`Nil`。无法证明所有路径返回时，结果中因此包含 `Nil`。显式结果 Type 使递归
调用的结果可在分析函数体之前确定；没有结果标注时，函数结果为 `Unknown`，
当前设计不要求自动推断。

参数标注只提供编译期约束。实参或调用目标为 `Unknown` 时允许编译，编译器不在
函数入口自动插入运行时检查。动态边界需要保证值满足更具体条件时，应显式使用
`types::matches` 或 `assert(rule)`。变参函数的 `...` 不能携带参数标注，但可以
在参数列表之后使用返回值标注。

编译器使用精确函数签名检查函数体、静态已知调用和模块接口。该签名属于编译期
Type 元数据，不改变运行时函数对象；`types::of(function_value)` 仍返回原始
`types::Function`。当前不提供精确函数 Type 的运行时构造或反射接口。

### 3.3 Dictionary 与结构字面量

字段 Type 描述的运行时值仍是普通 Dictionary。完整构造方式是 Dictionary
字面量加类型标注：

```tapas
let Point = types::make_type(
    'x' : types::Float,
    'y' : types::Float,
)

let origin: Point = {
    'x' : 0.0,
    'y' : 0.0,
}
```

`types::of(origin)` 返回 `types::Dictionary`，而
`types::matches(origin, Point)` 返回 `true`。

目标是编译器已知的单一字段 Type 时，可以使用结构字面量简写：

```tapas
let positional_origin: Point = {0.0, 0.0}
let same_origin: Point = {0.0, y=0.0}
let named_origin: Point = {x=0.0, y=0.0}
```

结构字面量遵循以下规则：

1. 目标必须是单一字段 Type；没有标注、目标为联合或
   `types::Dictionary` 时不能使用；
2. 位置项目写在命名项目之前，按构造顺序填入尚未指定的字段；
3. 命名项目写成 `IDENTIFIER = expression`，只能引用名称可作为 Identifier 的
   已声明字段；
4. 同一字段不能指定两次，所有必需字段都必须获得值；
5. 简写不能包含未声明字段；额外字段或不能作为 Identifier 的字段名应使用普通
   Dictionary 字面量；
6. 表达式按源码顺序从左到右求值，结果是以字段名 String 为键的普通 Dictionary。

在 `{0.0, y=0.0}` 中，位置项目填写 `x`，命名项目填写 `y`。这项语法依赖目标
Type，不会建立具有运行时身份的“Point 对象”。



## 4. 静态检查

### 4.1 可赋值关系

变量初始化、后续赋值、字段写入以及函数参数和返回值标注统一使用可赋值关系。

已知表达式 Type `A` 可以赋值给目标 Type `B`，当且仅当以下规则之一成立：

1. `A` 与 `B` 等价；
2. `B` 是 `types::AnyType`；
3. `A` 是联合，且它的每个成员都可以赋值给 `B`；
4. `B` 是联合，且 `A` 可以赋值给它的至少一个成员；
5. `A` 是参数化容器，`B` 是对应的原始容器 Type；
6. `A` 与 `B` 是同类参数化容器，且所有对应参数 Type 等价；
7. `A` 是字段 Type，`B` 是 `types::Dictionary`；
8. `A` 与 `B` 都是字段 Type，`A` 包含 `B` 的全部字段，且同名字段 Type 等价。

字段 Type 与统一键值 Dictionary Type 之间不能互相赋值。除上述规则外，两个
已知 Type 之间不能赋值；不存在 Int 到 Float 等隐式转换。

字段 Type 的同名字段和参数化容器的类型参数都按不变规则比较，因为对应的
Dictionary、List 和 Pair 可以修改。例如，若允许 `List[Int]` 赋值给
`List[Int | Float]`，后者就能写入 Float，使原有标注失效：

```text
let Number = types::union(types::Int, types::Float)
let IntList = types::list(types::Int)
let NumberList = types::list(Number)

let ints: IntList = [1, 2]
let numbers: NumberList = ints // 编译错误
```

字段 Type 支持宽度赋值：字段更多的 `A` 可以赋值给字段较少的 `B`，但双方同名
字段的 Type 必须等价。通过字段 Type 引用写入未声明字段属于编译错误，否则较窄
别名可以改坏较宽 Type 的额外字段。需要增加字段时，应先显式赋值给
`types::Dictionary`，同时接受静态字段约束丢失的结果。

参数化容器和字段 Type 都可以赋值给对应的原始容器 Type。赋值给原始容器或
`types::AnyType` 会丢失部分静态约束；值通过较宽别名修改后，编译器不保证原有
标注仍然成立，也不会插入运行时检查。

编译器无法确定表达式 Type 时使用内部状态 `Unknown`。`Unknown` 不是 Type，
但可以进入任何目标位置，不触发隐式运行时检查：

```text
var amount: Int = 1
amount = 2
amount = 'three'          // 编译错误
amount = foreign_value()  // Unknown，允许编译
```

`let` 和 `var` 都沿用当前语言的赋值规则；只要绑定带有标注，后续每次可静态分析
的赋值都按原标注检查。未标注的静态 Type 绑定被重新赋值后，不再作为静态 Type
名称使用，即使新值在运行时仍是 Type。名称解析、参数数量、重复字段和删除必需
字段等错误由各自规则检查，不属于可赋值关系。

声明后的首次赋值还受确定初始化分析约束：只有所有继续执行的控制流路径都完成
赋值，后续读取才合法。循环体不能单独建立循环后的初始化事实。递归 Type 的完成
定义必须在其声明代码块的无条件执行路径上进行，不能放入条件分支或循环。

### 4.2 字面量检查与推断

新鲜字面量具有目标 Type 时直接进行上下文检查，不先固定为更窄的可变容器 Type。
每个字段、元素、键或成员表达式都必须可以赋值给对应目标 Type。

普通 Dictionary 字面量必须包含全部必需字段，可以包含额外字段；结构字面量
简写只接受目标 Type 中声明的字段：

```tapas
let Number = types::union(types::Int, types::Float)
let Measurement = types::make_type('value' : Number)

let sample: Measurement = {'value' : 1}
let concise_sample: Measurement = {value=1}

let NumberList = types::list(Number)
let number_values: NumberList = [1, 2.5]
```

新鲜字面量可以直接赋值给较宽的目标容器；已经保存到变量中的可变容器必须遵守
不变规则。

没有目标 Type 时，容器字面量按以下规则推断：

1. 非空 List 的所有元素 Type 已知时，以元素 Type 的规范联合为 Item；
2. Pair 的两个成员 Type 已知时，分别作为 First 和 Second；
3. 非空 Dictionary 的键都是 String 字面量，且所有值 Type 已知时，推断字段
   Type；
4. 其他非空 Dictionary 的键和值 Type 已知时，分别以规范联合作为 Key 和 Value；
5. 空容器或任一必要成员为 `Unknown` 时，推断为对应的原始容器 Type。

推断得到的规范联合只剩一个成员时直接使用该成员，不受 `types::union` 公开调用
至少需要两个参数的限制。

编译器可以推断字段 Type 中已声明字段的读取结果。对这类值进行可静态识别的
写入时，新值必须可以赋值给已声明字段；删除必需字段或写入未声明字段属于编译
错误：

```text
var person: Person = load_person()
person['age'] = 37
person['age'] = 'old'       // 编译错误
person['nickname'] = 'Ada'  // 编译错误：Person 未声明 nickname
```

通过原始容器别名、原生扩展或其他 `Unknown` 路径发生的修改不会触发隐式检查，
可能使值不再满足原标注。跨动态边界需要保证结构时，应显式调用
`types::matches`。



## 5. `types` 包与运行时行为

### 5.1 接口概览

编译期 Type 构造形式如下：

| 签名 | 作用 |
|---|---|
| `types::make_type(...Pair(String, Type)) -> Type` | 建立非空字段 Type |
| `types::union(Type, Type, ...) -> Type` | 建立联合 |
| `types::list(Type) -> Type` | 建立参数化 List Type |
| `types::pair(Type, Type) -> Type` | 建立参数化 Pair Type |
| `types::dictionary(Type, Type) -> Type` | 建立参数化 Dictionary Type |

类型标注另提供 `List[T]`、`Pair[A, B]`、`Dictionary[K, V]` 和
`Union[A, B, ...]`。这些形式直接建立与上表构造器相同的 Type，不是普通运行时
索引，也不定义新的 Type 模板。

运行时检查接口如下：

| 签名 | 作用 |
|---|---|
| `types::of(AnyType) -> Type` | 返回值的固有运行时 Type |
| `types::matches(AnyType, Type) -> Bool` | 检查值是否满足指定 Type |

只读反射接口如下：

| 签名 | 作用 |
|---|---|
| `types::fields(Type) -> Dictionary` | 返回字段 Type 的用户字段副本 |
| `types::members(Type) -> List` | 返回联合成员 |
| `types::base(Type) -> Type` | 返回参数化容器的原始容器 Type |
| `types::parameters(Type) -> Dictionary` | 返回参数化容器的参数副本 |
| `types::definition(Type) -> Dictionary` | 返回仅供调试的定义副本 |

### 5.2 `types::of` 与 `types::matches`

`types::of` 只返回值的固有运行时类别：

| 值 | 结果 |
|---|---|
| Nil | `types::Nil` |
| Bool | `types::Bool` |
| Int | `types::Int` |
| Float | `types::Float` |
| String | `types::String` |
| List | `types::List` |
| Pair | `types::Pair` |
| Dictionary | `types::Dictionary` |
| Iterator | `types::Iterator` |
| 函数 | `types::Function` |
| 库 | `types::Library` |
| 实数数组 | `types::RealArray` |
| 布尔数组 | `types::BoolArray` |
| 时间点 | `types::Time` |
| Type | `types::Type` |

它不会返回联合、参数化容器或用户定义的字段 Type：

```tapas
let dictionary_runtime_type_is_generic = types::of({'name' : 'Ada'}) == types::Dictionary
let incomplete_person_does_not_match = types::matches({'name' : 'Ada'}, Person)
```

`types::matches(value, expected)` 按以下顺序判断：

1. `expected` 是 `types::AnyType` 时返回 `true`，包括 Nil；
2. `expected` 是联合时，任一成员匹配即返回 `true`；
3. `expected` 是参数化 List 时，值必须是 List，且每个元素匹配 Item；
4. `expected` 是参数化 Pair 时，值必须是 Pair，两个成员分别匹配 First 和 Second；
5. `expected` 是参数化 Dictionary 时，值必须是 Dictionary，且每个键和值分别
   匹配 Key 和 Value；
6. `expected` 是字段 Type 时，值必须是 Dictionary，包含全部必需字段，且字段值
   分别匹配；
7. 其他情况按第 4.1 节判断 `types::of(value)` 是否可以赋值给 `expected`。

空 List 和空 Dictionary 满足相应参数化容器 Type。容器检查需要遍历当前内容，
时间复杂度与成员数量成正比。检查不修改值，也不把结果附着到值上。

### 5.3 反射、相等与错误

`types::fields(T)` 对字段 Type 返回用户字段副本，对其他 Type 返回空 Dictionary。
`types::members(T)` 对联合返回规范顺序的成员，对其他 Type 返回只含 `T` 的 List。
`types::base(T)` 对参数化容器返回 `types::List`、`types::Pair` 或
`types::Dictionary`，对其他 Type 返回 `T` 自身。

`types::parameters(T)` 对参数化容器返回普通 Dictionary 副本，键固定为：

- List：`item`；
- Pair：`first`、`second`；
- Dictionary：`key`、`value`。

其他 Type 返回空 Dictionary。修改任何反射结果都不会影响原 Type。程序可以用
`types::base(T) != T` 判断 `T` 是否为参数化容器；Type 等价直接使用 `==`。

Type 的普通只读操作只观察用户字段：

- `T['name']` 返回字段 Type，不存在时报告索引错误；
- `len(T)` 返回用户字段数量；
- `keys(T)` 和遍历只返回用户字段；
- 预定义 Type、参数化容器和联合的用户字段数量均为零。

`types::definition(T)` 每次返回新的普通 Dictionary。结果可以包含内部编码，不
保证跨版本稳定，也不能传回 `types::make_type`。Type 的打印结果同样只用于诊断。

非递归 Type 的 `==` 比较规范表示；递归 Type 的 `==` 比较有限 Type 图，`!=` 是
其否定。Type 与非 Type 比较时，`==` 返回
`false`，`!=` 返回 `true`。`identical(A, B)` 只判断两个值是否引用同一 Type
对象；等价 Type 可能共享对象，也可能不共享，程序不能依赖其中一种情况。

Type 作为 Dictionary 键时，键相等规则与 `==` 一致。非递归 Type 的哈希由规范
表示计算，递归 Type 使用与图等价兼容的稳定哈希。等价 Type 必须具有相同哈希，
并对应同一个 Dictionary 条目。

`types::of` 接受任意值。`types::matches` 的第二个参数必须是 Type。其他反射
函数收到非 Type 参数时报告运行时参数类型错误。Type 构造形式的参数数量、形状
或静态性质不正确时，由编译器报告错误。



## 6. 模块与工具

模块可以导出静态 Type：

```text
// geometry.tap
let Point = types::make_type(
    'x' : types::Float,
    'y' : types::Float,
)

return {'Point' : Point}
```

```text
// app.tap
import geometry.tap as geometry

let origin: geometry::Point = {
    'x' : 0.0,
    'y' : 0.0,
}
```

模块接口保存静态 Type 图；字段 Type 还保存位置结构字面量所需的构造顺序。接口
不依赖 `types::definition` 的结果或打印文本。导入方复用或物化该 Type 图；构造
顺序只供编译期展开结构字面量。

模块也可以导出运行时值为 Type、但来源不是静态 Type 表达式的绑定。这类绑定在
导入方只能作为普通运行时值，不能用于标注。模块接口必须区分“静态 Type 绑定”
和“运行时类型为 Type 的普通绑定”。

可复用前端以不可变的 `TypeId` 和 Type arena 保存节点、符号及函数签名 Type；
编译器和语言服务器读取同一份静态分析结果。编译器只在生成程序时把 `TypeId`
物化为运行时 `ttypeval`，语言服务器不链接或执行 VM。两者只对规定的 Type 构造
形式执行静态求值，不执行普通 Tapas 函数。



## 7. 实现约束

### 7.1 内部对象

Type 使用独立复合类型码 `compo_ttypeval`。为避免与现有 `ttypes` 混淆，C
结构命名为 `ttypeval`：

```c
typedef struct ttypeval {
    tcompo_v compo_base;
    thashtbl *definition;
} ttypeval;
```

`compo_base` 是复合值共有的对象头，保存 Type 虚表指针和引用计数。
`definition` 是不可变的 `String -> Type` 映射，只保存当前 Type 的一层定义；
其中的 Type 直接引用已有 `ttypeval`，不复制整棵定义树。实现直接使用
`thashtbl`，不包装可修改的 `tdict`。

`definition` 只允许以下内部形状：

| Type | 逻辑内容 |
|---|---|
| `types::AnyType` | `{}` |
| 其他预定义 Type | `{'@builtin/名称' : types::AnyType}` |
| 字段 Type | `{'字段名' : 字段Type, ...}` |
| 参数化 List | `{'@base' : types::List, '@item' : Item}` |
| 参数化 Pair | `{'@base' : types::Pair, '@first' : First, '@second' : Second}` |
| 参数化 Dictionary | `{'@base' : types::Dictionary, '@key' : Key, '@value' : Value}` |
| 联合 | `{'@union/0' : Member0, '@union/1' : Member1, ...}` |

例如：

```tapas
let int_definition_example = types::Int
// {'@builtin/Int' : types::AnyType}

let list_definition_example = types::list(types::Int)
// {'@base' : types::List, '@item' : types::Int}

let point_definition_example = Point
// {'x' : types::Float, 'y' : types::Float}
```

所有键必须是 String，所有值必须是 Type。`types::AnyType` 是唯一空表。普通
字段名不能以 `@` 开头，不同内部形状也不能混合。

这些保留键是实现细节。程序不能依赖 `types::definition` 或打印结果中的具体
键名和排列顺序。联合成员在写入前必须规范化并排序；`@union/N` 中的编号表示
规范成员顺序，不能使用哈希表遍历顺序代替。

原始容器和使用 `types::AnyType` 的参数化容器具有不同定义。例如，
`types::List` 使用 `@builtin/List`，而 `types::list(types::AnyType)` 使用
`@base` 和 `@item`，因此两者不等价。

### 7.2 不可变性与对象协议

`ttypeval` 只在内部构造期间写入。对象发布后，不得增加、删除或替换
`definition` 条目。哈希表持有键和值的引用；释放 Type 时必须释放哈希表，并按
现有引用计数规则释放其中的 String 和 Type。运行时入口仍应防御性检查空定义、
非 String 键、非 Type 值、保留键和无效内部形状。

完成后的递归 Type 图可能形成引用环。当前运行时将这类不可变图保留到进程结束；
这不改变可观察语义。以后若引入图所有权或循环回收，必须保持同一公开行为。

公开操作不能取得内部可写位置：

- 普通索引、`len`、`keys` 和遍历只观察用户字段并隐藏所有 `@` 键；
- `types::fields`、`types::members`、`types::parameters` 和
  `types::definition` 返回副本或新容器；
- `types::base` 根据内部类别返回原始容器 Type；
- `==` 和哈希使用规范表示或递归图算法，不依赖对象地址或哈希表遍历顺序。

`ttypeval` 的虚表提供固定类型名称 `Type`、复合类型码 `compo_ttypeval`、释放、
只读访问、字符串表示、`==`、`!=` 和同一性。Type 作为 Dictionary 键时必须按
规范表示或递归图语义计算哈希和比较等价，不能沿用普通复合对象的地址比较。

字段构造顺序不保存在 `ttypeval` 中，由编译器和模块接口单独保存。当前结构也不
限制未来实现；可以增加类别或哈希缓存，也可以改用排序数组、小对象内联或对象
驻留，只要公开行为和 `Dictionary[String, Type]` 逻辑模型保持不变。



## 8. 当前限制

当前类型系统尚不表示：

- 可选字段；
- Iterator 元素 Type；
- 数组形状、维数和其他数值参数；
- 可在运行时构造或反射的精确函数签名 Type；
- 交集、名义继承和方法类型。

这些能力以后需要分别设计，不得改变本文已有 Type 的含义。
