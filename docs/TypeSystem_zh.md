# Tapas 类型系统

简体中文 | [English](TypeSystem_en.md) | [项目主页](../README.md)

本文说明 Tapas 当前支持的类型标注、Type 值、静态检查以及`types`包，并补充[语言规范](Syntax_zh.md)中的类型系统定义。
涉及这些内容时，以本文规则为准；Rule 的精确 Type 及其构造形式另见[Rule 文档](Rules_zh.md)。

类型标注只参与编译期分析，不改变值的运行时表示，也不会自动插入检查或转换。
Type 本身也可以作为普通值保存、传递、返回和导出，本文将这样的值称为 Type 值。
用户定义的 Type 不可修改，而且完整定义必须在编译期可见。
编译器无法确定一个值的 Type 时仍允许它进入带标注的位置；如果程序需要在动态边界确认其实际结构，应显式调用`types::matches`。

## 1. 基本概念

### 1.1 Type 值与运行时 Type

Type 值是一类独立的值，与 String、List、Dictionary 和 Time 等值类别并列。
普通 Dictionary 即使包含类似 Type 定义的内容，也不会自动成为 Type 值。
Type 值建立后不可修改；索引、反射和遍历只能读取其定义或取得普通容器副本，不能获得内部可写位置。

Type 值与运行时 Type 表示不同的概念。
Type 值是程序可以保存和传递的值，例如`types::Int`；运行时 Type 则表示某个值在执行时所属的类别，例如整数`1`的运行时 Type 是`types::Int`。
`types::of(value)`返回值的运行时 Type，因此`types::of(types::Int)`的结果是`types::Type`。

### 1.2 预定义 Type

`types`包导出以下 Type 值：

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
| `types::Indexable` | 支持索引读取的对象 |
| `types::IndexSettable` | 支持索引写入的对象 |
| `types::Appendable` | 支持追加的对象 |
| `types::Deletable` | 支持删除的对象 |
| `types::Contains` | 支持 `in` 成员测试的对象 |
| `types::Iterable` | 支持 `for` 遍历的对象 |
| `types::Rule` | 任意 Rule |
| `types::RuleInstance` | 任意 Rule 实例 |

文档和源码中的 Type 名称使用上表所示的大小写；省略`types::`后仍写作`Nil`、`Bool`、`Int`、`Float`等。
小写`nil`专指内部空值，小写`int()`、`float()`、`bool()`等专指转换函数。
`Any`、`Array`和`number`不是标准 Type 名称；应分别使用`AnyType`、`RealArray | BoolArray`和`Int | Float`。

`types::List`、`types::Pair`、`types::Dictionary`和`types::Iterator`只描述值的基本类别，本文称为原始容器 Type。
`Indexable`、`Appendable`等能力 Type 只要求值支持相应操作，不约束操作参数和返回值的 Type。
各值类别支持的具体操作详见[标准库](Stdlib_zh.md)；Rule 及 RuleInstance 的精确 Type 详见[Rule 文档](Rules_zh.md)。

### 1.3 静态 Type 表达式

一个表达式在运行时产生 Type 值，并不意味着它可以用于类型标注。
编译器只把以下表达式视为静态 Type 表达式：

1. `types` 包中的预定义 Type；
2. 当前作用域中已确定、且此后没有被重新赋值的静态 Type 绑定；
3. 模块接口导出的静态 Type 绑定；
4. 符合第 2 节要求的 Type 构造形式，以及 Rule 文档定义的精确 Rule Type 构造形式。

类型标注还可以使用第 3.1 节定义的参数化 Type 应用。
该语法只在类型标注中构造 Type，不是普通索引表达式，也不会执行运行时代码。

静态 Type 可以通过`let`建立任意层别名：

```tapas
let Integer = types::Int
let AlsoInteger = Integer
let value: AlsoInteger = 1
```

普通函数调用、函数参数、索引、条件表达式、`var`绑定和其他运行期运算都不是静态 Type 表达式。
编译器不会执行或内联普通函数来求得 Type：

```tapas
function choose(flag)
{
    if (flag) {
        return types::Int
    }
    return types::String
}

let Chosen = choose(true)
// Chosen 的运行时值是 Type，但 Chosen 不是静态 Type 绑定。
```

程序仍然可以在运行时保存或选择 Type 值：

```tapas
var current = types::Int
current = types::String
```

`current`保存的是 Type 值，其运行时 Type 是`types::Type`，但它不能用于类型标注。

Type 构造形式采用函数调用语法，但它们是编译期内建形式，不是可以保存、传递或间接调用的一等函数。
构造形式必须由编译器直接识别，其 Type 参数也必须是静态 Type 表达式。
编译器会为构造结果生成 Type 值常量或模块初始化代码；程序不能根据文件、网络或其他运行期数据建立新的 Type 定义。

## 2. 建立 Type

### 2.1 字段 Type

`types::make_type`接收一个或多个直接写在调用中的 Pair。
每个 Pair 的左侧是字段名，右侧是字段 Type：

```tapas
let Person = types::make_type(
    'name' : types::String,
    'age' : types::Int,
)
```

Pair 只用于描述字段；构造结果是独立且不可变的 Type 值，不是普通 Dictionary。
编译器按以下规则检查调用：

1. 至少包含一个参数；
2. 每个顶层参数都是直接写出的 Pair；
3. Pair 左侧是 String 字面量；
4. 字段名不重复，也不以保留字符 `@` 开头；
5. Pair 右侧是静态 Type 表达式；
6. 可选字段可以用 `types::optional` 包裹一个直接写出的 Pair。

逗号分隔字段。
Pair 运算符按右结合解析，因此嵌套 Pair 不能代替多个参数：

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

字段名区分大小写，并按原始 UTF-8 字节序列区分，不执行 Unicode 规范化。
字段默认为必需字段，也可以使用`types::optional`标记为可选：

```tapas
let User = types::make_type(
    'name' : types::String,
    types::optional('age' : types::Int),
)
```

`types::optional`只能直接出现在`types::make_type`的参数中，并且只能包裹一个直接写出的 Pair。
可选字段可以缺少；字段存在时，其值仍须满足声明的 Type。
`types::optional_fields(T)`返回字段 Type `T`的可选字段名。
删除可选字段合法，删除必需字段是编译错误。

一个值满足字段 Type，当且仅当它是 Dictionary、包含全部必需字段，并且每个已有的声明字段都满足对应 Type。
未声明的额外字段不影响匹配。

字段 Type 采用结构等价。
Type 的绑定名称和声明位置不参与等价判断，字段书写顺序也不参与等价判断；字段名称、字段 Type 和可选状态都相同的两个定义才等价。
同一个 Dictionary 因此可以满足多个字段 Type，类型系统不为字段 Type 建立全局继承树。

### 2.2 参数化容器

参数化容器进一步约束 List 的元素、Pair 的两个成员、Dictionary 的键和值，或者 Iterator 产生的元素：

```tapas
let IntList = types::list(types::Int)
let Entry = types::pair(types::String, types::Int)
let Scores = types::dictionary(types::String, types::Float)
let IntIterator = types::iterator(types::Int)
let Matrix = types::list(types::list(types::Float))
```

构造器参数必须是静态 Type 表达式，可以有限嵌套。
构造结果可以先绑定名称，也可以在类型标注中使用参数化 Type 应用直接书写：

```tapas
let values: IntList = [1, 2, 3]
let direct_values: List[Int] = [1, 2, 3]
let matrix: List[List[Float]] = []
let scores: Dictionary[String, Float] = {}
let indices: Iterator[Int] = 0 to 10
```

`List[T]`、`Pair[A, B]`、`Dictionary[K, V]`和`Iterator[T]`分别与`types::list(T)`、`types::pair(A, B)`、`types::dictionary(K, V)`和`types::iterator(T)`产生等价 Type。
方括号形式只用于简化类型标注，不引入用户定义的泛型模板。

原始容器 Type 只检查运行时类别，与使用`types::AnyType`的参数化容器不等价：

```tapas
let raw_list_is_distinct = types::List != types::list(types::AnyType)
```

字段 Type 和统一键值 Dictionary Type 是两种独立约束。
前者要求一组固定字段，后者检查容器中的所有键和值；当前类型系统不能在同一个 Type 中组合这两种约束。

当前运行时的 Iterator 只产生 Int，因此整数范围 Iterator 满足`Iterator[Int]`。
`types::matches`可以在不消耗 Iterator 的情况下验证这一约束；其他元素 Type 不匹配。

### 2.3 联合

`types::union`接收至少两个静态 Type 参数：

```tapas
let Identifier = types::union(
    types::Int,
    types::String,
)
```

构造联合时，编译器依次执行：

1. 展开已有联合；
2. 删除等价的重复成员；
3. 按稳定的规范顺序排序；
4. 去重后只剩一个成员时，直接使用该成员；
5. 包含 `types::AnyType` 时，直接使用 `types::AnyType`。

因此，下列两个联合等价，`types::members`也会以相同顺序返回其成员：

```tapas
let canonical_identifier_a = types::union(types::Int, types::String)
let canonical_identifier_b = types::union(types::String, types::Int)
```

`types::union(T, T)`合法，结果为`T`；公开调用的原始参数少于两个则属于编译错误。

### 2.4 递归 Type

递归 Type 使用无初始值、显式标注为`Type`的`let`声明建立定义位置，再由一次静态 Type 赋值完成定义：

```text
let Tree: Type

Tree = types::make_type(
    'value' : types::Int,
    'children' : types::list(Tree),
)
```

声明后、定义完成前，名称只能出现在完成递归定义所需的静态 Type 表达式中，不能作为普通运行时值读取。
同一作用域可以先声明多个名称，再建立相互引用：

```text
let Left: Type
let Right: Type

Left = types::make_type('rights' : types::list(Right))
Right = types::make_type('lefts' : types::list(Left))
```

每个定义必须由字段、容器、联合或函数等 Type 构造形成实际结构；`A = A`以及`A = B; B = A`这样的纯别名循环是编译错误。
完成后的 Type 图不可修改。
Type 变量以后重新赋值不会修改已经封闭的递归图，已有回引用仍指向原定义。

递归 Type 采用结构等价。
等价、可赋值和匹配均记录已经访问的节点对或“值—Type”组合，因此不会无限展开。
打印只显示有限的递归标识，不尝试展开整个图。
模块接口保存结构化定义，而不是依赖展开后的显示文本。

### 2.5 Type 等价与字段构造顺序

Type 的绑定名称、声明位置和对象地址都不决定 Type 是否等价。
预定义 Type 按自身类别比较，联合在展开和去重后比较成员，参数化 Type 比较基 Type 及其参数，字段 Type 比较字段名称、字段 Type 和可选状态。
递归 Type 作为带回引用的有限图进行比较，因此比较过程不会无限展开。

等价 Type 具有相同的哈希值，可以作为同一个 Dictionary 键使用；不同 Type 仍可能偶然具有相同哈希值。
Type 的打印结果和`types::definition`返回的调试信息都不参与等价判断。

字段 Type 还保留`types::make_type`中字段的书写顺序，供位置结构字面量使用。
构造顺序不属于 Type 的结构定义，也不参与等价、可赋值或哈希判断。
因此，两个等价字段 Type 可以具有不同的构造顺序，并分别用自己的顺序解释位置结构字面量。
Type 别名和模块导入会保留原 Type 的构造顺序。

## 3. 类型标注与值构造

### 3.1 标注语法与名称解析

类型标注写在声明名称或函数参数之后，也可以用在函数返回值上：

```tapas
let annotation_count: Int = 1
let annotation_values: List[Int] = [1, 2, 3]
let annotation_identifier: Int | String = 'T-1'

function annotation_size(text: String) -> Int
{
    return len(text)
}
```

完整语法如下：

```text
declarator       = IDENTIFIER, [ ":", type-expression ], [ "=", expression ] ;
type-expression  = union-type ;
union-type       = primary-type, { "|", primary-type } ;
primary-type     = function-type | type-application | qualified-type-name ;
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
| `Iterator[T]` | 1 | `types::iterator(T)` |
| `Union[A, B, ...]` | 至少 2 | `types::union(A, B, ...)` |
| `A \| B \| ...` | 至少 2 | `Union[A, B, ...]` |
| `Function[A, B, ...] -> R` | 0 个以上参数 Type 及 1 个返回值 Type | 无运行时构造形式 |
| `Rule[A, B, ...]` | 0 个以上参数 | `types::rule(A, B, ...)` |
| `RuleInstance[A, B, ...]` | 0 个以上参数 | `types::rule_instance(A, B, ...)` |

构造器可以写成`types::List[T]`等限定形式，参数本身也可以是 Type 应用或联合 Type。
名称区分大小写；参数数量错误、基 Type 不支持参数，或者任一参数不是有效 Type 表达式时，均为编译错误。
参数列表允许尾随逗号。
`Function[] -> R`表示无参数函数，`Function[...] -> R`表示变参函数；当前不能单独标注变参项的 Type。
Rule Type 的具体规则见[Rule 文档](Rules_zh.md)。

源码中的联合 Type 通常使用更简洁的`|`，它是`Union`的 Type 语法糖，优先级低于 Type 应用和函数 Type。
`List[Int | Float]`等价于`List[Union[Int, Float]]`，而`Function[Int | Float] -> String | Nil`的参数和结果分别都是联合 Type。
连续书写`A | B | C`会直接建立三成员联合，不会生成嵌套的二元联合。
普通表达式中的`|`仍表示逐元素 OR；工具目前使用稳定的完整形式`Union[A, B]`显示 Type。

未限定的构造器名称遵守普通遮蔽规则；当前作用域存在同名绑定时，不再回退到内建构造器。
当前版本不支持用户定义参数化 Type 构造器。

标注位置不能调用构造器或执行其他普通表达式：

```text
let Identifier = types::union(types::Int, types::String)
let value: Identifier = read()
let direct: Int | String = read()

let other: types::union(types::Int, types::String) = read() // 编译错误
let invalid: Int[String] = 1                                // 编译错误
```

预定义 Type 在标注位置可以省略`types::`：

```tapas
let count: Int = 1
let name: String = 'Tapas'
```

冒号后的未限定名称按以下顺序解析：

1. 查找当前词法作用域中的同名绑定；
2. 绑定是静态 Type 时使用该 Type；
3. 绑定是普通值，或者保存着不能作为静态 Type 表达式解析的 Type 值时，报告错误且不再回退；
4. 当前作用域没有同名绑定时，查找 `types` 包中的预定义 Type；
5. 仍未找到时报告未知类型。

限定名称不执行回退；`model::Person`必须从`model`的模块接口解析到静态 Type。
普通表达式也不执行预定义 Type 回退，因此取得 Type 值时必须写完整名称：

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

函数参数和返回值类型标注采用与声明相同的`type-expression`：

```text
parameter        = IDENTIFIER, [ ":", type-expression ] ;
fixed-parameters = parameter, { ",", parameter }, [ "," ] ;
return-annotation = "->", type-expression ;
function-literal = parameter-list, [ return-annotation ], block ;
```

具名声明`function name(parameters) -> Type
{ ... }`使用同一套签名语法，并把函数值保存在可由其他闭包捕获的只读环境绑定`name`中。

固定参数可以分别标注，也可以保留为未标注参数：

```text
function find(values: List[Int], target: Int, start) -> Int
{
    // ...
}
```

精确函数 Type 可以标注高阶函数参数、结果和普通绑定：

```tapas
function apply(
        callback: Function[Int] -> String,
        value   : Int,
) -> String {
    return callback(value)
}

let formatter: Function[Int] -> String = (value: Int) -> String
{
    return str(value)
}
```

`->`右结合，因此`Function[] -> Function[Int] -> String`表示一个不接收参数、返回格式化函数的函数。
函数 Type 的固定参数数量必须相同；参数 Type 逆变，返回值 Type 协变。
固定参数和变参签名不互相赋值。

参数标注在函数字面量的定义环境中解析，不受参数名称遮蔽。
标注得到的 Type 属于函数签名，并作为参数绑定的目标 Type 参与函数体分析。
静态已知的调用目标也按第 4.1 节的可赋值关系检查对应实参；`this`使用当前函数的同一签名。

`-> Type`标注函数结果。
编译器按第 4.1 节检查每个可达出口：带表达式的`return`检查该表达式，无表达式的`return`和执行到函数体末尾均视为返回`Nil`。
无法证明所有路径返回时，结果中因此包含`Nil`。
显式的返回值 Type 标注使递归调用的返回值可以在分析函数体之前确定；没有返回值标注时，函数返回值为`Unknown`，当前设计不要求自动推断。

参数标注只提供编译期约束。
实参或调用目标为`Unknown`时允许编译，编译器不在函数入口自动插入运行时检查。
动态边界需要保证值满足更具体条件时，应显式使用`types::matches`或内联的`assert(rule { ... })`。
变参函数的`...`不能携带参数标注，但可以在参数列表之后使用返回值标注。

编译器使用精确函数签名检查函数体、静态已知调用和模块接口。
该签名属于编译期 Type 元数据，不改变运行时函数对象；`types::of(function_value)`仍返回原始`types::Function`。
当前不提供精确函数 Type 的运行时构造或反射接口。

### 3.3 字段 Type 与结构字面量

字段 Type 描述的运行时值仍是普通 Dictionary。
完整构造方式是 Dictionary 字面量加类型标注：

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

`types::of(origin)`返回`types::Dictionary`，而`types::matches(origin, Point)`返回`true`。

目标是编译器已知的单一字段 Type 时，可以使用结构字面量简写：

```tapas
let positional_origin: Point = {0.0, 0.0}
let same_origin: Point = {0.0, y=0.0}
let named_origin: Point = {x=0.0, y=0.0}
```

结构字面量遵循以下规则：

1. 目标必须是单一字段 Type；没有标注、目标为联合或 `types::Dictionary` 时不能使用；
2. 位置项目写在命名项目之前，按构造顺序填入尚未指定的字段；
3. 命名项目写成 `IDENTIFIER = expression`，只能引用名称可作为 Identifier 的已声明字段；
4. 同一字段不能指定两次，所有必需字段都必须获得值；
5. 简写不能包含未声明字段；额外字段或不能作为 Identifier 的字段名应使用普通 Dictionary 字面量；
6. 表达式按源码顺序从左到右求值，结果是以字段名 String 为键的普通 Dictionary。

在`{0.0, y=0.0}`中，位置项目填写`x`，命名项目填写`y`。
这项语法依赖目标 Type，不会建立具有运行时身份的“Point 对象”。

## 4. 静态检查

### 4.1 可赋值关系

变量初始化、后续赋值、字段写入以及函数参数和返回值标注统一使用可赋值关系。

已知表达式 Type `A`可以赋值给目标 Type `B`，当且仅当以下规则之一成立：

1. `A` 与 `B` 等价；
2. `B` 是 `types::AnyType`；
3. `A` 是联合，且它的每个成员都可以赋值给 `B`；
4. `B` 是联合，且 `A` 可以赋值给它的至少一个成员；
5. `A` 是参数化容器，`B` 是对应的原始容器 Type；
6. `A` 与 `B` 是同类参数化容器，且所有对应参数 Type 等价；
7. `A` 是字段 Type，`B` 是 `types::Dictionary`；
8. `A` 与 `B` 都是字段 Type，`A` 满足 `B` 对必需字段、可选字段及对应字段 Type 的要求；
9. `A` 与 `B` 是精确函数 Type，并满足第 3.2 节定义的参数逆变和结果协变规则；
10. `A` 与 `B` 是 Rule 相关 Type，并满足 Rule 文档定义的可赋值规则。

字段 Type 与统一键值 Dictionary Type 之间不能互相赋值。
除上述规则外，两个已知 Type 之间不能赋值；不存在 Int 到 Float 等隐式转换。

字段 Type 的同名字段和参数化容器的类型参数都按不变规则比较，因为对应的 Dictionary、List 和 Pair 可以修改。
例如，若允许`List[Int]`赋值给`List[Int | Float]`，后者就能写入 Float，使原有标注失效：

```text
let Number = types::union(types::Int, types::Float)
let IntList = types::list(types::Int)
let NumberList = types::list(Number)

let ints: IntList = [1, 2]
let numbers: NumberList = ints // 编译错误
```

字段 Type 支持宽度赋值：字段更多的`A`可以赋值给字段较少的`B`，但双方同名字段的 Type 必须等价。
`B`的必需字段必须在`A`中同样是必需字段；`B`的可选字段可以在`A`中缺少，也可以在`A`中是必需或可选字段。
通过字段 Type 引用写入未声明字段属于编译错误，否则较窄别名可能破坏较宽 Type 的额外字段。
需要增加字段时，应先显式赋值给`types::Dictionary`，同时接受静态字段约束丢失的结果。

参数化容器和字段 Type 都可以赋值给对应的原始容器 Type。
赋值给原始容器或`types::AnyType`会丢失部分静态约束；值通过较宽别名修改后，编译器不保证原有标注仍然成立，也不会插入运行时检查。

编译器无法确定表达式的 Type 时，会在静态分析中使用内部状态`Unknown`。
`Unknown`不是程序可见的 Type 值，也不能写在类型标注中；它可以进入任何目标位置，但不会触发隐式运行时检查：

```text
var amount: Int = 1
amount = 2
amount = 'three'          // 编译错误
amount = foreign_value()  // Unknown，允许编译
```

`let`和`var`都沿用当前语言的赋值规则；只要绑定带有标注，后续每次可静态分析的赋值都按原标注检查。
未标注的静态 Type 绑定被重新赋值后，不再作为静态 Type 名称使用，即使新值在运行时仍是 Type。
名称解析、参数数量、重复字段和删除必需字段等错误由各自规则检查，不属于可赋值关系。

声明后的首次赋值还受确定初始化分析约束：只有所有继续执行的控制流路径都完成赋值，后续读取才合法。
循环体不能单独建立循环后的初始化事实。
递归 Type 的完成定义必须在其声明代码块的无条件执行路径上进行，不能放入条件分支或循环。

### 4.2 字面量检查与推断

新鲜字面量具有目标 Type 时直接进行上下文检查，不先固定为更窄的可变容器 Type。
每个字段、元素、键或成员表达式都必须可以赋值给对应目标 Type。

普通 Dictionary 字面量必须包含全部必需字段，可以包含额外字段；结构字面量简写只接受目标 Type 中声明的字段：

```tapas
let Number = types::union(types::Int, types::Float)
let Measurement = types::make_type('value' : Number)

let sample: Measurement = {'value' : 1}
let concise_sample: Measurement = {value=1}

let NumberList = types::list(Number)
let number_values: NumberList = [1, 2.5]
```

新鲜字面量可以直接赋值给较宽的目标容器；已经保存到变量中的可变容器必须遵守不变规则。

没有目标 Type 时，容器字面量按以下规则推断：

1. 非空 List 的所有元素 Type 已知时，以元素 Type 规范化后的联合为 Item；
2. Pair 的两个成员 Type 已知时，分别作为 First 和 Second；
3. 非空 Dictionary 的键都是 String 字面量，且所有值 Type 已知时，推断字段 Type；
4. 其他非空 Dictionary 的键和值 Type 已知时，分别以规范化后的联合作为 Key 和 Value；
5. 空容器或任一必要成员为 `Unknown` 时，推断为对应的原始容器 Type。

推断得到的联合经过规范化后如果只剩一个成员，就直接使用该成员，不受`types::union`公开调用至少需要两个参数的限制。

编译器可以推断字段 Type 中已声明字段的读取结果。
对这类值进行可静态识别的写入时，新值必须可以赋值给已声明字段；删除必需字段或写入未声明字段属于编译错误：

```text
var person: Person = load_person()
person['age'] = 37
person['age'] = 'old'       // 编译错误
person['nickname'] = 'Ada'  // 编译错误：Person 未声明 nickname
```

通过原始容器别名、原生扩展或其他`Unknown`路径发生的修改不会触发隐式检查，可能使值不再满足原标注。
跨动态边界需要保证结构时，应显式调用`types::matches`。

## 5. `types` 包与运行时行为

### 5.1 接口概览

编译期 Type 构造形式如下：

| 签名 | 作用 |
|---|---|
| `types::make_type(...Pair(String, Type)) -> Type` | 建立非空字段 Type |
| `types::union(Type, Type, ...) -> Type` | 建立联合 |
| `types::list(Type) -> Type` | 建立参数化 List Type |
| `types::iterator(Type) -> Type` | 建立参数化 Iterator Type |
| `types::pair(Type, Type) -> Type` | 建立参数化 Pair Type |
| `types::dictionary(Type, Type) -> Type` | 建立参数化 Dictionary Type |
| `types::optional(Pair(String, Type))` | 在字段 Type 中标记可选字段 |

类型标注还提供`List[T]`、`Iterator[T]`、`Pair[A, B]`、`Dictionary[K, V]`、`Union[A, B, ...]`、`A | B`和精确函数 Type。
这些形式不是普通运行时索引，也不定义新的 Type 模板。
Rule 相关构造形式由[Rule 文档](Rules_zh.md)定义。

运行时检查接口如下：

| 签名 | 作用 |
|---|---|
| `types::of(AnyType) -> Type` | 返回给定值的运行时 Type |
| `types::matches(AnyType, Type) -> Bool` | 检查值是否满足指定 Type |

只读反射接口如下：

| 签名 | 作用 |
|---|---|
| `types::fields(Type) -> Dictionary` | 返回字段 Type 的用户字段副本 |
| `types::optional_fields(Type) -> List` | 返回字段 Type 的可选字段名 |
| `types::members(Type) -> List` | 返回联合成员 |
| `types::base(Type) -> Type` | 返回参数化容器的原始容器 Type |
| `types::parameters(Type) -> Dictionary` | 返回参数化容器的参数副本 |
| `types::definition(Type) -> Dictionary` | 返回仅供调试的定义副本 |

### 5.2 `types::of` 与 `types::matches`

`types::of`只返回值的运行时 Type：

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
| Rule | `types::Rule` |
| RuleInstance | `types::RuleInstance` |

表中列出核心值类别；Rule IR 和 evaluator 等扩展值的运行时 Type 见[Rule 文档](Rules_zh.md)。
`types::of`不会返回联合、参数化容器、精确函数 Type 或用户定义的字段 Type：

```tapas
let dictionary_runtime_type_is_generic = types::of({'name' : 'Ada'}) == types::Dictionary
let incomplete_person_does_not_match = types::matches({'name' : 'Ada'}, Person)
```

`types::matches(value, expected)`按以下顺序判断：

1. `expected` 是 `types::AnyType` 时返回 `true`，包括 Nil；
2. `expected` 是联合时，任一成员匹配即返回 `true`；
3. `expected` 是参数化 List 时，值必须是 List，且每个元素匹配 Item；
4. `expected` 是参数化 Iterator 时，值必须是 Iterator，且其元素 Type 匹配 Item；
5. `expected` 是参数化 Pair 时，值必须是 Pair，两个成员分别匹配 First 和 Second；
6. `expected` 是参数化 Dictionary 时，值必须是 Dictionary，且每个键和值分别匹配 Key 和 Value；
7. `expected` 是字段 Type 时，值必须是 Dictionary、包含全部必需字段，而且每个已有的声明字段都匹配对应 Type；
8. `expected` 是精确 Rule、RuleInstance 或 RuleTerm Type 时，按 Rule 文档定义的签名或求值结果 Type 进行匹配；
9. 其他情况按第 4.1 节判断 `types::of(value)` 是否可以赋值给 `expected`。

空 List 和空 Dictionary 满足相应参数化容器 Type。
容器检查需要遍历当前内容，时间复杂度与成员数量成正比。
检查不修改值，也不把结果附着到值上。

### 5.3 反射、相等与错误

`types::fields(T)`对字段 Type 返回用户字段副本，对其他 Type 返回空 Dictionary。
`types::members(T)`对联合返回规范顺序的成员，对其他 Type 返回只含`T`的 List。
`types::base(T)`对参数化容器返回对应的`types::List`、`types::Iterator`、`types::Pair`或`types::Dictionary`。
它对精确 Rule 和 RuleInstance Type 返回相应的原始 Type，对其他 Type 返回`T`自身。

`types::parameters(T)`对参数化容器返回普通 Dictionary 副本，键固定为：

- List：`item`；
- Iterator：`item`；
- Pair：`first`、`second`；
- Dictionary：`key`、`value`。

其他 Type 返回空 Dictionary。
修改任何反射结果都不会影响原 Type。
程序可以用`types::base(T) != T`判断`T`是否为参数化容器；Type 等价直接使用`==`。

Type 的普通只读操作只观察用户字段：

- `T['name']` 返回字段 Type，不存在时报告索引错误；
- `len(T)` 返回用户字段数量；
- `keys(T)` 和遍历只返回用户字段；
- 预定义 Type、参数化容器和联合的用户字段数量均为零。

`types::definition(T)`每次返回新的普通 Dictionary。
结果可以包含内部编码，不保证跨版本稳定，也不能传回`types::make_type`。
Type 的打印结果同样只用于诊断。

非递归 Type 的`==`按第 2.5 节的等价规则比较；递归 Type 的`==`比较有限 Type 图，`!=`是其否定。
Type 与非 Type 比较时，`==`返回`false`，`!=`返回`true`。
`identical(A, B)`只判断两个值是否引用同一 Type 对象；等价 Type 可能共享对象，也可能不共享，程序不能依赖其中一种情况。

Type 作为 Dictionary 键时，键相等规则与`==`一致。
非递归 Type 的哈希由其结构定义计算，递归 Type 使用与图等价兼容的稳定哈希。
等价 Type 必须具有相同哈希，并对应同一个 Dictionary 条目。

`types::of`接受任意值。
`types::matches`的第二个参数必须是 Type。
其他反射函数收到非 Type 参数时报告运行时参数类型错误。
Type 构造形式的参数数量、形状或静态性质不正确时，由编译器报告错误。

## 6. 模块中的 Type

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

模块接口保存静态 Type 的完整结构；对于字段 Type，接口还会保留结构字面量所需的字段构造顺序。
导入方不依赖 Type 的打印文本或`types::definition`的结果来恢复这些信息。

模块也可以导出一个恰好保存 Type 值的普通绑定。
如果该绑定的来源不是静态 Type 表达式，导入方可以读取和传递它，但不能把它用在类型标注中。
因此，模块接口会区分静态 Type 绑定和运行时 Type 为`types::Type`的普通绑定。

## 7. 当前限制与实现边界

当前类型系统尚不表示：

- 数组形状、维数和其他数值参数；
- 可在运行时构造或反射的精确函数签名 Type；
- 交集、名义继承和方法类型。

数组继续使用`types::RealArray`和`types::BoolArray`，其形状与维数不属于 Type 参数。
上述能力如果以后加入，不会改变本文已有 Type 的含义。

本文规定公开的语言行为，不规定`ttypeval`、Type arena、规范编码或缓存等内部数据结构。
`types::definition`和 Type 的打印文本只用于诊断，其内容可能随实现变化，程序不应依赖其中的保留键或排列顺序。
编译器、语言服务器和运行时可以采用不同的内部表示，但必须保持本文规定的构造、等价、可赋值、匹配和反射行为。
