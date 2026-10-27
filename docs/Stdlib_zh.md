# Tapas 标准库

简体中文 | [English](Stdlib_en.md) | [项目主页](../README.md)

本文记录随 Tapas 提供的根内建函数、原生包和源码包。
语言语法与运算符语义见[语言规范](Syntax_zh.md)，Type 与`types`包见[类型系统](TypeSystem_zh.md)，Rule 与 evaluator 接口见[Rule](Rules_zh.md)。

根函数直接按名称调用；包成员使用`package::name`。
根函数的第一个参数可以作为隧道调用的接收者。
标准库 API 会随语言版本演进，不属于词法或语法定义。

> **签名约定：** `A | B` 表示可接受的 Type，`AnyType` 表示任意值，`T` 表示保留输入 Type；
> `...` 表示变参，`?` 或参数外的方括号表示可省略，`*` 后的参数必须按名称传入。
> 返回 `Nil` 的过程结果不能保存。包内签名省略当前包前缀，实际调用仍写作 `dense::rows(...)`；
> 跨包 Type 引用保留前缀。隧道调用 `items.append(value)` 等价于 `append(items, value)`。

当前提供 18 个原生包和 1 个源码包 `format`，按与语言核心的关系和用途分为四类：

| 分类 | 包 |
|---|---|
| 语言补充包 | [`types`](#package-types), [`syntax`](#package-syntax), [`rules`](#package-rules), [`evaluators`](#package-evaluators) |
| 基础工具包 | [`bytes`](#package-bytes), [`strings`](#package-strings), [`json`](#package-json), [`heap`](#package-heap), [`os`](#package-os), [`path`](#package-path), [`fs`](#package-fs), [`io`](#package-io), [`time`](#package-time) |
| 数学与概率包 | [`math`](#package-math), [`dense`](#package-dense), [`random`](#package-random), [`finite`](#package-finite) |
| 应用包 | [`solve`](#package-solve), [`format`](#package-format) |

`optimize` 目前只有[设计稿](../src/stdlib/optimize/README.md)，尚未作为可用包提供。

## 预定义函数

这些函数直接存在于程序根作用域，无需包名前缀。

### 控制台、检查与时间

| 签名 | 返回 | 行为 |
|---|---|---|
| `print(...values: AnyType)` | `Nil` | 依次打印简略表示，不自动插入分隔符，最后换行。 |
| `pprint(...values: AnyType)` | `Nil` | 依次打印完整表示，不自动插入分隔符，最后换行。 |
| `input()` | `String \| Nil` | 从标准输入读取一行；EOF 且未读到内容时返回 `nil`。 |
| `input(prompt: String)` | `String \| Nil` | 不换行打印并刷新提示后读取一行；EOF 且无内容时返回 `nil`。 |
| `len(value: AnyType)` | `Int` | 复合值长度；`nil` 为 0，其他标量为 1。 |
| `type(value: AnyType)` | `String` | 返回运行时类型名。 |
| `copy(value: T)` | `T` | 按该对象的复制协议返回副本。 |
| `identical(left: AnyType, right: AnyType)` | `Bool` | 按运行时类型及对象自身的匹配规则比较。 |
| `parameters(value: Rule \| RuleInstance \| RuleIR)` | `List[Pair[String, Type]]` | 按声明顺序读取 Rule IR 的参数名与描述性 Type，不执行规则。 |
| `arguments(instance: RuleInstance)` | `List[AnyType]` | 按声明顺序读取已绑定实参，不执行或检查规则。 |
| `assert(rule: RuleInstance \| Rule)` | `Nil` | 检查 Rule；Condition 不满足时产生运行时错误。完整语义见 [Rule](Rules_zh.md#3-检查接口)。 |
| `clock()` | `Float` | 进程 CPU 时间（秒）。 |
| `clock_ns()` | `Int` | 进程 CPU 时间（纳秒），适合测量较短的代码。 |
| `now()` | `Time` | 返回调用时刻对应的绝对时间点。默认显示使用本地时区。 |

`input` 移除末尾 LF 以及其前的 CR；EOF 前已读到的内容仍作为字符串返回。

`pprint`的名称表示结构化的完整打印；生成字符串使用`str`，不再把通常表示“格式化为字符串”的`sprint`用作打印函数。

### 转换与构造

| 签名 | 返回 | 行为 |
|---|---|---|
| `int(value: AnyType)` | `Int` | 接受 Bool、Int、Float、String；Float 向零截断，String 按宿主 C 的十进制解析规则处理。 |
| `float(value: AnyType)` | `Float` | 接受 Bool、Int、Float、String；String 按宿主 C 浮点解析规则处理。 |
| `try_int(value: AnyType)` | `Dictionary` | 按 int 合同尝试转换，失败时返回错误记录。 |
| `try_float(value: AnyType)` | `Dictionary` | 按 float 合同尝试转换，失败时返回错误记录。 |
| `bool(value: AnyType)` | `Bool` | nil、零值和长度为零的复合对象为 false；字符串 true、false 单独识别。 |
| `str(value: AnyType)` | `String` | 完整文本表示。 |
| `list()` / `list(iterable: Iterable)` | `List` | 建立空列表，或消费任意可迭代对象并生成新列表。 |
| `replicate(values: List, count: Int)` | `List` | 将源列表整体浅复制 count 次；count 为非负 Int，零次返回空列表。 |
| `pair(first: AnyType, second: AnyType)` | `Pair` | 建立对。 |
| `iter(start: Int, end: Int)` | `Iterator` | 自动推断步长的半开范围。 |
| `iter(start: Int, step: Int, end: Int)` | `Iterator` | 指定步长的半开范围。 |
| `array(rows: Int, cols: Int, value: AnyType)` | `RealArray \| BoolArray` | Bool 填充 BoolArray；数值填充 RealArray；List 按行优先填充。 |

`int`、`float` 的导出签名接受 AnyType，但实现只处理 Bool、Int、Float、String，其他值报错。
字符串转换使用宿主 C strtol（十进制）和 strtod 的语法及 locale 行为，允许前导空白，
但必须实际消费数字并覆盖完整字节串。空串、纯空白、内嵌 NUL、未消费的尾部字符（包括空白）、
非有限值或 errno 非零都会报错；范围错误包含上溢和下溢。
Float 转 Int 向零截断并在转换前校验范围，拒绝 NaN、无穷和越界值；Int 转 Float 仍按浮点精度舍入。
Bool 转数值时 false 为 0，true 为 1。str 保留完整字节长度，包括内嵌 NUL。

`try_int`、`try_float` 与严格转换共用实现，返回 `ok`、`value`、`code`、`error`、`errno`。
成功时 code/error 为空串；失败时 code 为 invalid_type、invalid_number、out_of_range 或 non_finite，
value 分别为 Int 0 或 Float 0.0；必须先检查 ok，不能把回退值当成成功。errno 始终为 0。
error 是诊断文本，不属于稳定错误码；内存耗尽等资源错误仍报错。
这些结果无需依赖 json 包；与 json::try_parse 一样通过 ok 检查，但失败载荷和附加字段由各接口定义。
`bool` 对 String 的 true、false 文本特殊处理，其他 String 和复合对象按长度判断；例如 bool('0') 为 true。

`array` 的 List 长度必须等于 rows * cols，且元素必须全部为 Bool，或全部为 Int/Float（允许数值混合）。
空 List 配合零元素形状创建 RealArray；要创建空 BoolArray，使用 Bool 标量填充。

数组维度不得为负。Iterator 的步长为零，或步长方向无法从起点到达终点时，范围为空；省略步长时根据端点顺序推断 `1` 或 `-1`。

```tapas
print(int('  +12'), ', ', int(-3.75))
print(bool('false'), ', ', bool('0'))
pprint(replicate([1, 2], 2))
```

<pre class='Tapas-Return'>
12, -3
false, true
[1, 2, 1, 2]
</pre>

### 集合操作

| 签名 | 返回 | 修改与结果 |
|---|---|---|
| `push_front(list: List, value: AnyType)` | `Nil` | 在列表开头插入元素。 |
| `push_back(list: List, value: AnyType)` | `Nil` | 在列表末尾插入元素。 |
| `append(target: Appendable, value: AnyType)` | `Nil` | 追加元素或对（列表、字典等容器）。String 不可变，拼接请使用 `strings::join`。 |
| `insert(list: List, value: AnyType, index: Int)` | `Nil` | 在索引前插入；负数从末尾换算，最终位置允许等于列表长度。 |
| `pop_front(list: List)` | `AnyType` | 删除并返回第一个元素。 |
| `pop_back(list: List)` | `AnyType` | 删除并返回最后一个元素。 |
| `delete(target: Deletable, key: AnyType)` | `Nil` | 删除元素。 |
| `idx(target: Indexable, ...indices: AnyType)` | `AnyType` | 转发一个或多个索引，规则由目标对象决定。 |
| `idx_or(target: Indexable, ...indices: AnyType, default: AnyType)` | `AnyType` | 最后一个参数为默认值；索引不存在时返回它，参数类型错误仍报错。 |
| `keys(value: Dictionary \| Type)` | `List` | 返回 Dictionary 的键，或 Type 可枚举的字段名。 |
| `values(dictionary: Dictionary)` | `List` | 按相应顺序返回值。 |
| `concat(left: Iterable, right: Iterable)` | `List` | 依次消费两侧元素，返回新的浅复制 List。 |
| `fill(target: IndexSettable, value: AnyType, indices: Iterable)` | `Nil` | 按索引序列依次填入同一值；每个元素作为一个完整索引或键。 |
| `count(values: Iterable, value: AnyType)` | `Int` | 使用 identical 规则统计匹配元素的个数。 |
| `sort(values: List)` | `Nil` | 原地排序；数值按大小、String 按字节字典序、List 按递归字典序比较；不保证稳定性。 |

对字典调用`append`时，追加的值必须是`Pair`。
`pop_front`和`pop_back`会转移被删除元素的所有权，不能用于空列表。
`delete`只执行通用删除，仍返回`nil`；Dictionary 缺少指定键时会报错。
`keys`、`values` 返回新的浅层 List；字典顺序来自当前哈希表遍历，不保证插入顺序或排序。
`idx_or`通过非抛错索引能力只查询一次目标；它不先执行`in`测试。
集合中不能保存`nil`。

#### 能力类型与专用操作的边界

根函数的通用性取决于操作所需的能力。Indexable 只保证索引读取，
不保证可写、可迭代、可枚举索引域或数组形状。

| 类别 | 当前接口边界 |
|---|---|
| 通用对象操作 | `len`、`type`、`copy`、`identical`、`str` 使用值或对象自身的协议；数值转换仍校验输入类型。 |
| 可迭代输入 | `list`、`concat`、`count` 接受 Iterable；`concat` 始终返回 List。字符串专用拼接使用 `strings::join`，元素必须为 String。 |
| 通用索引与修改 | `idx`、`idx_or` 转发索引参数；`append`、`delete` 分别要求 Appendable、Deletable；`fill` 要求 IndexSettable 和显式索引序列。能力类型不约束具体键类型。 |
| List 专用操作 | `replicate`、`insert`、首尾 push/pop、`sort` 操作具体的有序 List；仅支持索引读写不足以保证插入、移除或排序语义。 |
| 键和值枚举 | Dictionary 不隐式选择迭代键或值；显式使用 `keys`、`values`。Indexable 本身不能枚举合法键。 |
| 专用构造与反射 | `array`、`iter`、`pair`、Rule 反射和断言保留其对象领域约束。 |

`strings::join`、`concat` 按输入的迭代顺序读取，不修改输入；浅复制会保留复合元素的引用。
String 的迭代单位是字节，因此 `strings::join('abc', '-')` 得到 `a-b-c`，不表示 Unicode 代码点切分。
Array 当前不具备 Iterable 能力；用 `idx`/`idx_or` 访问时必须提供行、列两个索引。

`idx_or` 至少需要一个索引，默认值始终是最后一个实参。List、String、Dictionary、
Array 提供非抛错查询；第三方对象需要实现 `try_indexable` 才能保证索引缺失时返回默认值。
缺少此能力的兼容路径仍调用普通索引，可能抛错。Dictionary 的单键查询保留一次哈希探测的实现。

`copy` 遵循对象的复制协议：List、Dictionary 浅复制元素，Array 复制独立的数据存储，
第三方对象可以定义自己的复制行为。`identical` 不只是比较地址：同类型 List 等对象会进行
结构匹配，`identical(1, 1.0)` 为 false。`sort` 与 heap 默认顺序共用数值、String 和递归 List 的比较实现。
Int/Float 混合比较保留整数精度；String 按完整字节内容比较（包括内嵌 NUL）；
List 从首项起比较，首个不同位置决定顺序，共同前缀相同时短者在前。
数值、String、List 三类之间不定义顺序；NaN、不支持的元素及过深或循环嵌套报错。
`sort` 和默认 `heapify` 检查全部输入值，默认 `push` 检查新增值；List 运算符只比较决定结果所需的位置。
`sort` 不保证稳定性，不接受比较函数；自定义优先级可使用 heap 的比较函数。
数值列表使用内联比较的三路 introsort，小分区采用插入排序，深度耗尽后使用堆排序，
保证最坏 O(n log n) 和 O(log n) 栈空间。单调输入或可用有限次移位整理的输入可在线性时间完成。
String 和嵌套 List 的比较成本更高：大列表先探测已有顺序，其余输入保留平台 qsort。
算法选择基于值类型、规模和已有顺序，不改变排序合同。
大数值分区使用分层扰动取样；同构 Int/Float 在样本无重复时选用双向二路分区，有重复时使用三路分区；混合 Number 保留三路分区。

报错后不保证恢复原排列，但保留所有元素及其引用。

#### 批量填充与计数

`fill` 和 `count` 保持通用能力约束；支持的包对象可以提供同语义的底层批量实现，其他对象继续逐项操作。

`fill` 接受显式索引序列，用 `iter(start, step, end)` 表达半开区间和步长。
也可以传入任意 Iterable 产生的键，例如 `fill(dictionary, 0, ['a', 'b'])`。
它按顺序执行 target[index] = value，重复索引重复写入，不预先复制索引序列；
复合填充值保留同一对象的引用，不为每个位置深复制。List 支持负索引且不会自动扩容。
索引或写入错误会中止调用，此前写入不会回滚。空索引序列仍要求目标支持 IndexSettable。

每个索引元素原样作为一个参数传给写入能力；Pair、List 键不会自动展开成多个坐标。
Array 当前要求行、列两个索引，因此不能直接作为 fill 的目标使用；可使用 `dense::flat(array)` 视图。String 不可变。
这避免了把 IndexSettable 误当成可枚举全部索引的序列，或将字典的复合键误解为坐标。

`count` 使用与 identical、List 成员判断相同的规则，不调用重载的 ==，也不进行 Int/Float 转换。
空输入返回 0。String 按字节迭代，因此 count('ababa', 'a') 为 3，count('ababa', 'aba') 为 0，
不是子串计数。Dictionary 需显式传入 keys 或 values；Array 当前不具备 Iterable 能力；可显式使用 `dense::flat(array)` 获得一维可迭代视图。

```tapas
let stdlib_bulk_mask = replicate([true], 10)
fill(stdlib_bulk_mask, false, iter(2, 2, 10))
print(count(stdlib_bulk_mask, false))
let stdlib_bulk_dictionary = {'a': 1, 'b': 2}
fill(stdlib_bulk_dictionary, 7, ['a', 'b'])
print(count(values(stdlib_bulk_dictionary), 7))
```

<pre class='Tapas-Return'>
4
2
</pre>

```tapas
let stdlib_access_matrix = array(2, 3, [1, 2, 3, 4, 5, 6])
print(idx(stdlib_access_matrix, 1, 2))
print(idx_or(stdlib_access_matrix, 9, 0, -1))
print(strings::join('abc', '-'))
print(len(concat(iter(1, 4), 'xy')))
```

<pre class='Tapas-Return'>
6
-1
a-b-c
5
</pre>

### 会话函数

| 签名 | 返回 | 作用 |
|---|---|---|
| `__ls__([library: Library])` | `List[String]` | 当前根库或指定库中的名称。 |
| `__path__([library: Library])` | `List[String]` | 当前库或指定库的搜索路径。 |
| `__param__(index: Int)` | `AnyType` | 当前变参调用的参数。 |
| `__nparam__()` | `Int` | 当前变参调用的参数数量。 |
| `__binary__([environment: Library \| Function])` | `Nil` | 打印当前或指定值的字节码。 |

以`__`开头的名称由实现保留。`__ls__()`、`__path__()` 的无参形式仅能用于 Library 作用域；
函数中需显式传入 Library。`__param__` 的下标从 0 开始，必须在当前调用的参数范围内；
`__param__`、`__nparam__` 只能在函数调用环境中使用。`__binary__` 的 Function 参数限 Tapas 字节码函数。

## 语言补充包

这些包直接暴露或扩展类型系统、编译器词法设施，以及 Rule 的表示与执行协议。

<a id="package-types"></a>

### `types`：Type 构造与反射包

#### 类型

| 预定义 Type 分组 | 名称 |
|---|---|
| 基础值 | `AnyType`, `Nil`, `Bool`, `Int`, `Float`, `String`, `Time`, `Type` |
| 容器与可调用值 | `List`, `Pair`, `Dictionary`, `Iterator`, `Function`, `Library`, `RealArray`, `BoolArray` |
| 规则 | `Rule`, `RuleInstance`, `RuleIR`, `RuleTerm`, `RuleItem` |
| 通用能力 | `Indexable`, `IndexSettable`, `Appendable`, `Deletable`, `MemberTestable`, `Iterable` |

六种通用能力依次表示索引读取、索引写入、追加、删除、成员判断与迭代。

#### 函数

| 签名 | 返回 | 行为 |
|---|---|---|
| `make_type(...fields: Pair[String, Type])` | `Type` | 从至少一个具名字段构造结构 Type。 |
| `union(...members: Type)` | `Type` | 至少两个 Type 的联合。 |
| `enum(...members: String)` | `Type` | 至少一个 String 成员组成的 Enum Type。 |
| `list(item: Type)` | `Type` | 指定元素 Type 的 List Type。 |
| `iterator(item: Type)` | `Type` | 指定元素 Type 的 Iterator Type。 |
| `optional(field: Pair[String, Type])` | `Type` | 将字段标记为可缺省，用于 make_type。 |
| `pair(first: Type, second: Type)` | `Type` | 指定两侧 Type 的 Pair Type。 |
| `dictionary(key: Type, value: Type)` | `Type` | 指定键和值 Type 的 Dictionary Type。 |
| `rule(...parameters: Type)` | `Type` | 按声明顺序指定参数 Type 的 Rule Type。 |
| `rule_instance(...parameters: Type)` | `Type` | 按声明顺序指定参数 Type 的 RuleInstance Type。 |
| `instance_of(rule: Rule)` | `Type` | 仅匹配指定 Rule 身份的实例 Type；身份只在当前进程内有意义。 |
| `of(value: AnyType)` | `Type` | 值的运行时 Type；普通参数化容器返回基础 Type，Rule 保留参数签名。 |
| `matches(value: AnyType, expected: Type)` | `Bool` | 按结构、联合、Enum、参数化 Type 或能力检查匹配。 |
| `fields(value: Type)` | `Dictionary` | 结构 Type 的字段字典；非结构 Type 返回空字典。 |
| `optional_fields(value: Type)` | `List[String]` | 可缺省字段的名称。 |
| `members(value: Type)` | `List` | Enum 的字符串成员、联合的 Type 成员；其他 Type 返回包含自身的列表。 |
| `base(value: Type)` | `Type` | 返回参数化 Type 的基础 Type。 |
| `parameters(value: Type)` | `Dictionary` | 返回 Type 的参数字典；与根函数 parameters 的 Rule 反射不同。 |
| `definition(value: Type)` | `Dictionary` | 返回 Type 定义的反射字典。 |
| `parameter(name: String)` | `Type` | 声明 Type 参数占位符。 |
| `value_parameter(name: String)` | `Type` | 声明值参数占位符。 |
| `template(type_parameters: List, value_parameters: List, definition: Type)` | `Type` | 从参数列表与定义构造 Type 模板。 |

这些构造形式用于可由编译器识别的 Type 定义与模板声明；不能据此假定任意运行时数据都能动态生成新的用户 Type。模板采用 Type[Types; Values] 参数区，值实参受编译期字面量和约束检查。完整规则见[类型系统](TypeSystem_zh.md)。反射返回新的容器，其中的 Type 值可共享。

完整接口约束见 [`types` README](../src/stdlib/types/README.md)。

<a id="package-syntax"></a>

### `syntax`：词法包

与编译器共用词法视图，供格式化器和其他源码工具使用。

#### 类型

| 名称 | 含义 |
|---|---|
| `Token` | 具有 `kind: String`、`start: Int`、`end: Int` 字段的结构 Type。 |

#### 常量

| 名称 | 类型 | 含义 |
|---|---|---|
| `line_feed` | `String` | 换行字符。 |
| `horizontal_tab` | `String` | 水平制表符。 |

#### 函数

| 签名 | 返回 | 行为 |
|---|---|---|
| `tokens(source: String)` | `List[Token]` | 返回包含 trivia 和 EOF 的有序 token 记录。位置是源文本中的半开字节区间。 |

<a id="package-rules"></a>

### `rules`：Rule 检查、变换与 IR 包

#### 类型

| 名称 | 含义 |
|---|---|
| `PointsOf` | 有限点集定义域模板。 |
| `RangeOf` | 闭整数区间定义域模板。 |
| `Table` | 不可变的有限标量关系表。 |
| `Parameter` | Rule 形参记录。 |
| `Capture` | 闭包捕获记录。 |
| `Condition` | 布尔条件 Item。 |
| `Requirement` | 嵌套 Rule 调用 Item。 |
| `Origin` | 源码位置与来源信息。 |
| `CheckResult` | 规则检查结论、违反项与诊断。 |
| `Violation` | 不满足条件的记录。 |
| `Diagnostic` | 检查诊断记录。 |

#### 函数

| 签名 | 返回 | 行为 |
|---|---|---|
| `item(rule: Rule, target: Int \| String \| RuleItem)` | `RuleItem` | 选择 Rule 的直接 Item。 |
| `drop(rule: Rule, target: Int \| String \| RuleItem)` | `Rule` | 返回删除目标 Item 的派生 Rule。 |
| `violate(rule: Rule, target: Int \| String \| RuleItem)` | `Rule` | 返回要求目标 Item 不成立的派生 Rule。 |
| `drop_if(rule: Rule, target: Int \| String \| RuleItem, premise: Rule)` | `Rule` | premise 成立时允许忽略目标，否则仍要求目标成立。 |
| `violate_if(rule: Rule, target: Int \| String \| RuleItem, premise: Rule)` | `Rule` | 要求 premise 与目标恰有一个成立。 |
| `restrict(rule: Rule, ...restrictions: Pair)` | `Rule` | 按形参名追加固定值或定义域约束。 |
| `membership(value: RuleTerm, domain: RuleTerm)` | `RuleTerm` | 构造成员判断 Term。 |
| `points(element_type: Type, ...values)` | `PointsOf` | 构造指定元素 Type 的有限点集定义域。 |
| `range(start: Int, end: Int)` | `RangeOf[Int]` | 构造包含两端的整数区间；与根函数 iter 的半开区间不同。 |
| `table(columns: List[String], data: Dictionary[String, List])` | `Table` | 按显式列顺序构造关系表；List 行通过 in 匹配。 |
| `term(type: Type)` | `Type` | 构造指定值 Type 的 RuleTerm Type。 |
| `check(rule: RuleInstance \| Rule)` | `CheckResult` | 检查 Condition，返回检查结果。 |
| `inspect(rule: RuleInstance \| Rule)` | `RuleIR` | 返回规则的不可变 IR 视图。 |
| `parameters(ir: RuleIR \| Rule \| RuleInstance)` | `List[Parameter]` | 按声明顺序返回 IR 参数记录。 |
| `items(ir: RuleIR \| Rule \| RuleInstance)` | `List[RuleItem]` | 返回直接 Item 记录。 |
| `terms(ir: RuleIR \| Rule \| RuleInstance)` | `List[RuleTerm]` | 返回 Term 记录。 |
| `origin(value: AnyType)` | `Origin` | 读取来源信息。 |
| `semantic_hash(value: RuleIR \| Rule)` | `Int` | 计算影响语义的内容哈希。 |
| `content_hash(value: RuleIR \| Rule)` | `Int` | 计算包括表示与来源信息的内容哈希。 |
| `serialize(value: RuleIR \| Rule)` | `String` | 序列化支持的 Rule IR。 |
| `deserialize(data: String)` | `Rule` | 从序列化文本恢复 Rule。 |
| `parameter(name: String, type: Type)` | `Parameter` | 构造具名参数记录。 |
| `constant(value: AnyType)` | `RuleTerm` | 构造常量 Term。 |
| `call(function: AnyType, arguments: List[RuleTerm])` | `RuleTerm` | 构造函数调用 Term。 |
| `negation(operand: RuleTerm)` | `RuleTerm[Bool]` | 构造逻辑非 Term。 |
| `conjunction(left: RuleTerm, right: RuleTerm)` | `RuleTerm[Bool]` | 构造逻辑与 Term。 |
| `disjunction(left: RuleTerm, right: RuleTerm)` | `RuleTerm[Bool]` | 构造逻辑或 Term。 |
| `condition(term: RuleTerm, description: String?)` | `Condition` | 构造 Condition，可附说明。 |
| `requirement(rule: Rule \| RuleTerm, arguments: List[RuleTerm])` | `Requirement` | 构造嵌套规则 Requirement。 |
| `implication(antecedent: RuleTerm, consequents: List[RuleTerm], description: String?)` | `RuleItem` | 构造蕴含 Item，可附说明。 |
| `extension(provider: String, kind: String, arguments: List[RuleTerm], payload: AnyType)` | `RuleTerm` | 构造具有 provider、kind 和 payload 的扩展 Term。 |
| `make(display_name: String, parameters: List[Parameter], items: List[RuleItem])` | `Rule` | 从参数与 Item 列表构造 Rule。 |

target 可以是从 0 开始的直接 Item 下标、唯一说明字符串或属于该 Rule 的直接 RuleItem。变换不修改原 Rule；条件变换的 premise 必须具有相同参数签名。哈希不保证无碰撞，不能作为 Rule 身份。序列化仅支持约定的版本、常量与扩展 provider。完整检查和 IR 契约见 [Rule](Rules_zh.md)。

完整接口约束见 [`rules` README](../src/stdlib/rules/README.md)。

<a id="package-evaluators"></a>

### `evaluators`：自定义 Rule evaluator 包

#### 类型

| 名称 | 含义 |
|---|---|
| `Evaluator` | 保存名称、版本和回调的 evaluator Type。 |
| `Context` | 一次 evaluator 调用的绑定与捕获环境。 |
| `Result` | 具有 status、value、violations、diagnostics 字段的结果 Type。 |
| `Diagnostic` | 执行诊断的 Type。 |

#### 函数

| 签名 | 返回 | 行为 |
|---|---|---|
| `make(name: String, version: Int, evaluate: Function, compile: Function?)` | `Evaluator` | 创建 evaluator；compile 回调可省略。 |
| `eval(instance: RuleInstance, evaluator: Evaluator)` | `Result` | 使用 evaluator 处理已绑定实例。 |
| `compile(rule: Rule, evaluator: Evaluator)` | `Result` | 使用 evaluator 编译 Rule；未提供回调时返回 Unsupported。 |
| `binding(context: Context, parameter: rules::Parameter \| Int)` | `AnyType` | 按参数记录或位置读取绑定值。 |
| `capture(context: Context, capture: rules::Capture \| Int)` | `AnyType` | 按 Capture 记录或位置读取捕获值。 |
| `value(context: Context, term: RuleTerm)` | `AnyType` | 在当前 Context 中求取可直接解释的 Term 值。 |
| `requirement(context: Context, requirement: rules::Requirement)` | `RuleInstance` | 解析目标 Rule 和实参，返回 RuleInstance。 |

Result.status 为 Success、Unsupported 或 Failed。回调在所有状态下都必须返回结构完整的 Result，遇到合法但无法解释的结构应显式返回 Unsupported。Context 只在所属调用期间有效，不能跨调用保存或复用。回调协议见 [Rule](Rules_zh.md)。

完整接口约束见 [`evaluators` README](../src/stdlib/evaluators/README.md)。

## 基础工具包

按数据处理、数据结构和系统访问组织，表格与正文使用相同顺序：

- 数据处理：`bytes`、`strings`、`json`。
- 数据结构：`heap`。
- 系统访问：`os`（环境与程序）、`path`（路径文本）、`fs`（文件系统）、`io`（内容与标准流）、`time`（时间）。

<a id="package-bytes"></a>

### `bytes`：字节缓冲区包

#### 类型

| 名称 | 含义 |
|---|---|
| `Buffer` | 固定长度、可变的一维字节缓冲区；每项一个字节，读取为 Int。 |

#### 函数

| 签名 | 返回 | 行为 |
|---|---|---|
| `buffer(length: Int, value: Int)` | `Buffer` | 建立 length 项缓冲区并填入 value；长度非负，value 在 0..255。 |
| `from_string(value: String)` | `Buffer` | 按实际字节长度复制 String，保留 NUL 和非 UTF-8 字节。 |
| `to_string(value: Buffer)` | `String` | 按实际字节长度复制 Buffer，不进行编码转换。 |

Buffer 支持 Int 索引、负索引、`idx_or`、Iterable、MemberTestable、`len`、`copy`、`fill` 和 `count`。
写入只接受 0..255 的 Int，不进行截断或 Bool/Float 转换；不支持追加、删除或扩容。
`copy` 返回独立存储。`count` 与迭代得到的 Int 使用 identical 规则，故 Int 1 不匹配 Float 1.0 或 Bool true。
`fill` 仍接受任意 Iterable 索引；合法 Iterator 范围使用底层批量写入，连续字节填充使用 memset。
String/Buffer 转换使用直接内存复制，不逐项装箱，返回独立存储；修改 Buffer 不会改变已有 String。
此包不执行文本编码转换或 UTF-8 校验；需要文本时使用 strings::is_utf8 显式检查。文件读写仍属于 io 的职责。

```tapas
let stdlib_byte_buffer = bytes::buffer(6, 1)
fill(stdlib_byte_buffer, 255, iter(1, 2, 6))
print(count(stdlib_byte_buffer, 255))
let stdlib_view_array = array(1, 3, true)
let stdlib_flat_view = dense::flat(stdlib_view_array)
fill(stdlib_flat_view, false, iter(1, 3))
print(count(stdlib_flat_view, true))
```

<pre class='Tapas-Return'>
3
1
</pre>

<a id="package-strings"></a>

### `strings`：基础字符串操作包

字符串拼接统一使用 `strings::join`。根作用域的 `join` 保留为历史兼容入口，
共用实现并保持已有字节码槽位；新代码使用包接口。

| 签名 | 返回 | 行为 |
|---|---|---|
| `join(values: Iterable, separator: String)` | `String` | 按迭代顺序拼接 String 元素，分隔符可为空串；空输入返回空串。 |
| `split(text: String, separator: String)` | `List` | 按非空分隔串拆分，保留空项；空输入返回包含一个空串的 List。 |
| `lines(text: String)` | `List` | 按 LF、CRLF、CR 分行并去除换行；空输入返回空 List，尾部换行不增加空项。 |
| `trim(text: String)` | `String` | 移除两端 ASCII 空白：空格、HT、LF、VT、FF、CR。 |
| `find(text: String, needle: String, [start: Int])` | `Int` | 返回首次匹配的绝对字节偏移，未找到为 -1；start 默认 0。 |
| `replace(text: String, old: String, replacement: String)` | `String` | 从左到右替换所有非重叠匹配，old 不得为空。 |
| `starts_with(text: String, prefix: String)` | `Bool` | 检查前缀，空前缀为 true。 |
| `ends_with(text: String, suffix: String)` | `Bool` | 检查后缀，空后缀为 true。 |
| `is_utf8(text: String)` | `Bool` | 严格校验 UTF-8；空串和内嵌 NUL 合法。 |

操作保留完整字节长度，不修改输入，也不执行 Unicode 正规化或大小写折叠。
join 接受任意 Iterable，但每个元素必须为 String；String 按字节迭代，因此 strings::join 并非 Unicode 字符切分。
find 的负 start 从末尾换算，过小从 0 开始，超过长度返回 -1；空 needle 在有效 start 处匹配。
拆分和替换拒绝空模式；lines 保留实际空行。完整合同见 [`strings` README](../src/stdlib/strings/README.md)。

```tapas
print(strings::join(strings::split('a::b::', '::'), '|'))
print(strings::find('ababa', 'aba', 1))
print(strings::replace('你好你好', '你好', '好'))
print(try_int('')::ok)
```

<pre class='Tapas-Return'>
a|b|
2
好好
false
</pre>

<a id="package-json"></a>

### `json`：JSON 编解码包

| 签名 | 返回 | 行为 |
|---|---|---|
| `parse(text: String)` | `AnyType` | 严格解析完整 JSON，错误时产生运行时错误。 |
| `try_parse(text: String)` | `Dictionary` | 返回 `ok`、`value`、`error`、`position` 字段，便于处理无效数据。 |
| `stringify(value: AnyType, [pretty: Bool])` | `String` | 输出紧凑 JSON；pretty 为 true 时缩进输出。 |

`Null` 是包导出的 Type；`json::null` 是可保存的空值，与 `Nil` 不同。
JSON 对象和数组分别映射为 Dictionary 和 List；整数必须在 Int 范围内，不静默转为浮点数。
文本必须是合法 UTF-8，内嵌 NUL 按长度保留。重复键、超出 256 层的嵌套、非有限数值、
非 String 字典键和循环引用报错。字典输出顺序未规定。
`try_parse` 成功时 error 为空串、position 为 -1；失败时 value 为 json::null，
语法错误 position 为零起始字节偏移，映射错误为 -1；参数类型和资源错误仍报错。
完整映射和限制见 [`json` README](../src/stdlib/json/README.md)。

<a id="package-heap"></a>

### `heap`：二叉堆包

`heap` 在普通 List 上原地维护二叉堆，不需要额外的堆对象。

| 函数 | 说明 | 时间复杂度 |
|---|---|---|
| `heapify(items[, less])` | 原地建堆，返回 Nil | O(n) |
| `push(items, value[, less])` | 插入元素，返回 Nil | 均摊 O(log n) |
| `pop(items[, less])` | 移除并返回堆顶 | O(log n) |
| `peek(items)` | 查看堆顶 | O(1) |

默认构建最小堆，支持 Int、非 NaN Float、String，以及由这些值递归组成的 List。
List 使用字典序，共同前缀相同时短者在前；例如 `[1, 3]` 先于 `[1, 9]`。
Int/Float 混合比较保留整数精度，不同类别之间不定义顺序。
`heapify` 和默认 `push` 验证输入，遇到 NaN、不支持的载荷及过深或循环嵌套时会报错；有此类载荷的记录请使用显式比较函数。
验证输入需 O(v)，v 为本次输入递归包含的值数；每次比较成本取决于公共前缀长度。
可选的 `less(a, b)` 必须返回 Bool 并满足严格弱序。同一个堆的修改操作
必须使用一致的比较规则；比较函数不得修改当前堆或元素的排序依据。
相同优先级不保证顺序，空堆的 `pop`、`peek` 报错。

```tapas
let stdlib_heap_items = [4, 1, 3]
heap::heapify(stdlib_heap_items)
heap::push(stdlib_heap_items, 2)
print(heap::pop(stdlib_heap_items))
print(heap::peek(stdlib_heap_items))
```

<pre class='Tapas-Return'>
1
2
</pre>

记录可使用 `(a, b) { return a[0] < b[0] }` 比较优先级，最大堆可使用
`(a, b) { return a > b }`。空堆直接使用 `[]`，长度使用 `len`。
直接修改列表或优先级后，需要重新建堆；接口不做 O(n) 的有效性检查。
完整语义见 [heap 包说明](../src/stdlib/heap/README.md)。

<a id="package-os"></a>

### `os`：运行环境与程序执行包

| 签名 | 返回 | 行为 |
|---|---|---|
| `getenv(name: String)` | `Dictionary` | 返回 found/value，区分未设置与空字符串。 |
| `cwd()` | `String` | 当前工作目录。 |
| `home()` | `String` | 非空 HOME，或当前用户的账户目录。 |
| `temp_dir()` | `String` | 非空 TMPDIR，否则 `/tmp`；只查询，不创建。 |
| `platform()` | `String` | 当前支持 linux、macos。 |
| `run(arguments: List[String], *, ...)` | `Dictionary` | 直接同步执行程序并捕获输入输出，不自动使用 shell。 |

run 的命名选项为 stdin（String 或 bytes::Buffer，默认空串）、cwd（默认继承）、
env（String 键值字典，默认空字典）、inherit_env（默认 true）、timeout（默认 30 秒，必须为有限正数，最大 1e9）、
output_limit（每路默认 1048576 字节，非负 Int）。文件名无 `/` 时按子进程 PATH 搜索，缺省 PATH 为 `/usr/bin:/bin`。
参数和环境不允许 NUL，stdin 允许；未知或重复选项、类型错误在启动前报错。

结果 status 为 exited、signaled、timeout、cancelled、spawn_error、io_error。exit_code 仅 exited 时有效，其他情况为 -1；
signal 为观察到的终止信号，cancel_signal 为触发取消的信号，无信号时为 0。
stdout/stderr 保存原始字节，elapsed 是单调耗时，errno/error 是系统诊断，
output_truncated、stdout_truncated、stderr_truncated 表示捕获是否不完整。非零退出码作为正常程序结果返回。
输出超限后继续排空；超时或取消终止进程组并回收直接子进程。调用线程等待期间处理 SIGINT/SIGTERM，返回后恢复信号掩码。
不修改进程全局 cwd、环境或信号处理器；多线程宿主需协调信号路由。详细限制见 [`os` README](../src/stdlib/os/README.md)。

```tapas
let stdlib_os_run = os::run(['/bin/sh', '-c', 'printf hello; printf log >&2; exit 7'])
print(stdlib_os_run::status)
print(stdlib_os_run::exit_code)
print(stdlib_os_run::stdout)
print(stdlib_os_run::stderr)
```

<pre class='Tapas-Return'>
exited
7
hello
log
</pre>

<a id="package-path"></a>

### `path`：路径文本包

| 签名 | 返回 | 行为 |
|---|---|---|
| `join(...parts: String)` | `String` | 至少一个参数；跳过空项，按需插入 `/`，后续绝对路径丢弃此前部分。 |
| `normalize(value: String)` | `String` | 合并 `/`、移除 `.`、词法消去 `..`；空路径返回 `.`。 |
| `basename(value: String)` | `String` | 忽略尾部 `/` 后取最后一段；根返回 `/`，空输入返回空串。 |
| `dirname(value: String)` | `String` | 忽略尾部 `/` 后取父路径；无父路径返回 `.`，根返回 `/`。 |
| `extension(value: String)` | `String` | 取最后一个扩展名，包含 `.`；只含前导点的文件名没有扩展名。 |
| `is_absolute(value: String)` | `Bool` | 检查是否以 `/` 开头。 |

采用 Linux/macOS 的 POSIX 路径文本约定，拒绝内嵌 NUL；不访问文件系统或解析符号链接，
不展开 `~`、变量或通配符，不识别 Windows 盘符。join 保留 `.`、`..`；normalize 才显式化简。
词法消去 `..` 不保证在存在符号链接时指向同一文件。完整约束见 [`path` README](../src/stdlib/path/README.md)。

```tapas
print(path::join('fixtures', 'orders.json'))
print(path::normalize('/a/../../b//'))
print(path::extension('/a/.env.local'))
```

<pre class='Tapas-Return'>
fixtures/orders.json
/b
.local
</pre>

<a id="package-fs"></a>

### `fs`：文件系统操作包

| 签名 | 返回 | 行为 |
|---|---|---|
| `stat(path: String)` / `lstat(path: String)` | `Dictionary` | 跟随链接/查询末段链接本身；字段 kind、size、permissions、modified。 |
| `exists(path: String)` | `Bool` | 不存在为 false，权限等其他查询错误仍报错。 |
| `list_dir(path: String)` | `List` | 按字节排序的名称，不包含 `.`、`..`。 |
| `mkdir(path: String, [parents: Bool])` | `Nil` | 创建目录；parents 默认 false，为 true 时创建父目录并接受已有目录。 |
| `copy(source: String, target: String)` | `Nil` | 复制普通文件数据，拒绝源链接、特殊文件和已有目标。 |
| `rename(source: String, target: String)` | `Nil` | POSIX 重命名，可替换目标，跨设备报错。 |
| `remove(path: String)` | `Nil` | 删除文件、末段链接或空目录，不递归。 |
| `temp_dir([parent: String])` / `temp_file([parent: String])` | `String` | 创建唯一临时项；默认 TMPDIR 或 `/tmp`，由调用方清理。 |

kind 为 file、directory、symlink 或 other；size 是字节数，modified 是秒精度 Time。
每个函数提供 try_ 版本，参数位置相同，返回 ok/value/code/error/errno。失败时元数据为空字典、
目录枚举为空 List、临时路径为空 String；exists 为 false，其他修改操作成功 true、失败 false。
必须先检查 ok；内存耗尽等资源错误仍报错。详细错误代码和副作用合同见 [`fs` README](../src/stdlib/fs/README.md)。

```tapas
let stdlib_fs_root = fs::temp_dir()
let stdlib_fs_path = path::join(stdlib_fs_root, 'fixture.json')
io::write_text(stdlib_fs_path, '{}')
print(fs::stat(stdlib_fs_path)::kind)
print(len(fs::list_dir(stdlib_fs_root)))
fs::remove(stdlib_fs_path)
fs::remove(stdlib_fs_root)
```

<pre class='Tapas-Return'>
file
1
</pre>

<a id="package-io"></a>

### `io`：文件内容与标准流

| 签名 | 返回 | 行为 |
|---|---|---|
| `read_text(path: String)` | `String` | 一次读取整个文件。 |
| `write_text(path: String, text: String)` | `Nil` | 创建或覆盖文件。 |
| `append_text(path: String, text: String)` | `Nil` | 创建文件或在末尾追加。 |
| `replace_text(path: String, text: String)` | `Nil` | 在同一目录写入临时文件，再原子替换目标文件。 |

这些函数精确保留字符串的字节长度。
文件接口分别使用 String 和 bytes::Buffer；当前不暴露需要调用者管理生命周期的文件句柄或模式字符串。


可恢复接口 `try_read_text(path)`、`try_write_text(path, text)`、`try_append_text(path, text)`、
`try_replace_text(path, text)` 返回 Dictionary；这些接口的参数签名为 AnyType，便于记录类型错误。
记录包含 ok、value、code、error、errno：读取成功时 value 为 String，失败为空串；
写操作成功为 true、失败为 false。读取 value 前必须检查 ok。
code 在成功时为空串，失败时为 invalid_type、invalid_path、not_found、permission_denied、
is_directory、not_directory、no_space、already_exists、not_empty、symlink_loop、cross_device 或 io_error。
io/fs 共用系统错误映射；errno 是平台错误号，参数错误为 0。
严格接口仍报错；两组接口都拒绝空路径或内嵌 NUL，写入前验证文本类型。
普通写入失败可能已产生部分写入；原子替换失败会清理临时文件。资源耗尽仍可能报错。
完整约束见 [`io` README](../src/stdlib/io/README.md)。

#### 二进制文件与标准流

| 签名 | 返回 | 行为 |
|---|---|---|
| `read_bytes(path: String)` | `bytes::Buffer` | 直接读取字节存储，不经过 String 中转。 |
| `write_bytes(path: String, buffer: bytes::Buffer)` | `Nil` | 创建或覆盖二进制文件。 |
| `append_bytes(path: String, buffer: bytes::Buffer)` | `Nil` | 追加字节。 |
| `replace_bytes(path: String, buffer: bytes::Buffer)` | `Nil` | 临时文件写入后原子替换。 |
| `write(text: String)` / `write_error(text: String)` | `Nil` | 向 stdout/stderr 写完整字节，不自动换行。 |
| `flush([stream: String])` | `Nil` | 刷新 stdout（默认）或 stderr。 |
| `is_terminal(stream: String)` | `Bool` | 检查 stdin/stdout/stderr 是否连接交互终端。 |

四个二进制文件函数均有 try_ 版本，使用相同错误记录；失败读取 value 为零长度 Buffer。
标准流写入保留 NUL，不执行编码转换；标准流接口不提供关闭操作，错误仍报错。

<a id="package-time"></a>

### `time`：时间包

| 签名 | 返回 | 行为 |
|---|---|---|
| `from_unix(seconds: Int)` | `Time` | 从 Unix 时间戳创建时间点。时间戳是自 Unix 纪元起的整数秒数。 |
| `unix(value: Time)` | `Int` | 返回时间点对应的 Unix 整数秒数。 |
| `format(value: Time, pattern: String)` | `String` | 使用本地时区和宿主 C `strftime` 格式字符串生成文本。 |

时间戳和时间偏移必须落在宿主平台的`time_t`范围以及 Tapas `Int`范围内。
格式化模式及受支持的转换符由宿主 C 库决定。
Time 本身不保存时区信息；UTC 和严格日期解析接口见下表。

| 新增签名 | 返回 | 行为 |
|---|---|---|
| `monotonic()` | `Float` | 单调秒数，用于耗时差值，不是 Unix 时间。 |
| `monotonic_ns()` | `Int` | 单调纳秒数，检查范围。 |
| `unix_ns()` | `Int` | 当前 Unix 纳秒时间戳，检查范围。 |
| `format_utc(value: Time, pattern: String)` | `String` | 使用 UTC 和宿主 strftime 模式。 |
| `parse_utc(text: String)` | `Time` | 严格解析 YYYY-MM-DDTHH:MM:SSZ，年份 0001..9999；拒绝非法日期、闰秒、偏移和小数秒。 |
| `try_parse_utc(text: AnyType)` | `Dictionary` | 返回 ok/value/code/error/errno，失败 value 为 epoch Time；code 为 invalid_type、invalid_time 或 out_of_range。 |

Time 仍为秒精度；纳秒接口查询调用时刻或单调时钟，不扩大 Time 表示。
两个格式化接口均拒绝模式中的 NUL，格式符仍遵循平台 C 库。完整合同见 [`time` README](../src/stdlib/time/README.md)。

## 数学与概率包

这些包提供标量数学、稠密数组计算、可复现随机数和有限概率分布。

<a id="package-math"></a>

### `math`：标量数学包

下表中的数字参数均为`Int | Float`，除特别说明外返回`Float`。
定义域、溢出、无穷和 NaN 行为遵循宿主 C 数学库。
以下名称都通过`math::`访问。

| 分组 | 签名 |
|---|---|
| 绝对值与根 | `abs(Int \| Float) -> Int \| Float`, `fabs`, `sqrt`, `rsqrt`, `cbrt`: `(Int \| Float) -> Float` |
| 整数平方根 | `isqrt(value: Int) -> Int`：非负 Int 的平方根向下取整，整个 Int 范围内精确；负数报错。 |
| 幂与几何 | `pow(Int \| Float, Int \| Float)`, `hypot(Int \| Float, Int \| Float) -> Float` |
| 三角函数 | `sin`, `cos`, `tan`, `asin`, `acos`, `atan`: `(Int \| Float) -> Float`; `atan2(Int \| Float, Int \| Float)` |
| 双曲函数 | `sinh`, `cosh`, `tanh`, `asinh`, `acosh`, `atanh`: `(Int \| Float) -> Float` |
| 指数 | `exp`, `exp2`, `expm1`: `(Int \| Float) -> Float` |
| 对数 | `log`, `log2`, `log10`, `log1p`, `logb`: `(Int \| Float) -> Float`; `ilogb(Int \| Float) -> Int` |
| 分解 | `frexp(Int \| Float) -> Pair[Float, Int]`, `modf(Int \| Float) -> Pair[Float, Float]` |
| 缩放 | `ldexp`, `scalbn`, `scalbln`: `(Int \| Float, Int) -> Float` |
| 误差与伽马 | `erf`, `erfc`, `lgamma`, `tgamma`: `(Int \| Float) -> Float` |
| 浮点舍入 | `ceil`, `floor`, `nearbyint`, `rint`, `round`, `trunc`: `(Int \| Float) -> Float` |
| 整数舍入 | `lrint`, `llrint`, `lround`, `llround`: `(Int \| Float) -> Int` |
| 余数 | `fmod`, `remainder`: `(Int \| Float, Int \| Float) -> Float`; `remquo -> Pair[Float, Int]` |
| 浮点操作 | `copysign`, `nextafter`, `fdim`, `fmax`, `fmin`: `(Int \| Float, Int \| Float) -> Float`; `fma(Int \| Float, Int \| Float, Int \| Float)` |
| 倒数与 NaN | `eleinv(Int \| Float) -> Float`, `make_nan() -> Float` |
| 分类 | `isfinite`, `isinf`, `isnan`, `isnormal`, `signbit`: `(Int \| Float) -> Bool`; `fpclassify(Int \| Float) -> Int` |
| 有序谓词 | `isgreater`, `isgreaterequal`, `isless`, `islessequal`, `islessgreater`, `isunordered`: `(Int \| Float, Int \| Float) -> Bool` |

缩放函数的整数参数会将浮点数向零截断。

<a id="package-dense"></a>

### `dense`：稠密数组包

#### 类型

| 名称 | 含义 |
|---|---|
| `FlatView` | RealArray 或 BoolArray 的一维行优先可变视图。 |

#### 函数

| 签名 | 返回 | 行为 |
|---|---|---|
| `flat(value: RealArray \| BoolArray)` | `FlatView` | 建立共享存储的一维视图，不复制元素。 |
| `rows(value: RealArray \| BoolArray)` | `Int` | 行数。 |
| `cols(value: RealArray \| BoolArray)` | `Int` | 列数。 |
| `transpose(value: RealArray \| BoolArray)` | `RealArray \| BoolArray` | 新的转置数组。 |
| `identity(size: Int)` | `RealArray` | 建立指定阶数的单位矩阵。 |
| `trace(value: RealArray)` | `Float` | 主对角线之和；允许矩形数组。 |
| `inner(left: RealArray, right: RealArray)` | `Float` | 两个同形数组的 Frobenius 内积。 |
| `outer(RealArray, RealArray)` | `RealArray` | 两个行向量或列向量的外积。 |
| `norm(value: RealArray)` | `Float` | 向量的二范数或矩阵的 Frobenius 范数。 |
| `normalize(RealArray)` | `RealArray` | 返回同形的单位范数副本。 |
| `copy_into(source: RealArray, target: RealArray)` | `Nil` | 将数据复制到同形的既有数组，不分配结果。 |
| `scale_inplace(value: RealArray, factor: Int \| Float)` | `Nil` | 原地执行 `value *= factor`。 |
| `add_scaled_inplace(target: RealArray, source: RealArray, factor: Int \| Float)` | `Nil` | 原地执行 `target += factor * source`。 |
| `gemm(alpha: Int \| Float, left: RealArray, right: RealArray, beta: Int \| Float, output: RealArray)` | `Nil` | 原地执行 `output = alpha * left @ right + beta * output`。 |


`flat` 的视图使用单个 Int 索引并支持负索引、迭代、`fill` 和 `count`。
视图保留源数组的引用，任一侧写入立即对另一侧可见；它不能扩容。
BoolArray 视图读写 Bool；RealArray 视图读取 Float，写入接受 Int 或 Float。
`count` 按读取后的类型匹配，因此 RealArray 视图中的 2.0 不匹配 Int 2。
`copy(view)` 保持 FlatView 类型，但创建独立存储；原二维数组的索引和迭代能力不变。

```tapas
let builtin_dense_matrix = array(2, 3, [1, 2, 3, 4, 5, 6])
print(dense::rows(builtin_dense_matrix), ' x ', dense::cols(builtin_dense_matrix))
pprint(dense::transpose(builtin_dense_matrix))
print(dense::norm(array(1, 2, [3, 4])))
```

<pre class='Tapas-Return'>
2 x 3
[[1, 4],
 [2, 5],
 [3, 6]]
5
</pre>

`inner`要求两侧行数和列数分别相同。
`outer`只接受某一维为 1 的二维数组，但不区分行向量和列向量的方向；结果形状由两侧的元素数量决定。
`normalize`拒绝零范数数组。
`copy_into`、`scale_inplace`、`add_scaled_inplace`和`gemm`都复用调用方提供的存储；`gemm`的输出不能与任一输入是同一数组。

运算符负责方便且无副作用的表达，每次产生新结果，并使用单遍融合循环避免`dcopy`后再计算的额外内存流量；矩阵乘法`@`仍直接使用`dgemm`。
`dense`则提供可复用输出缓冲区的性能接口：`copy_into`、`scale_inplace`、`add_scaled_inplace`、`gemm`分别直接映射到`dcopy`、`dscal`、`daxpy`、`dgemm`。
`inner`和`norm`使用`ddot`与`dnrm2`，`outer`使用`beta = 0`的`dgemm`，避免先清零再累加。
`identity`、`trace`与`transpose`不依赖 BLAS。

线性方程求解、逆矩阵、行列式、秩和特征分解不属于 BLAS 原语。
若将来引入这些接口，应在`dense`中保持同样的高层命名，但通过独立、可选的 LAPACK 后端实现，不能用低效或数值不稳定的手写算法冒充完整线性代数支持。

<a id="package-random"></a>

### `random`：伪随机数与状态包

Source 和 Generator 都是导出的 **Type 值，不是函数**：Source 描述采用哪一种伪随机算法，
Generator 描述保存当前随机状态的生成器对象。创建生成器使用小写的 generator 函数。

#### 类型

| 名称 | 含义 |
|---|---|
| `Source` | 随机源的 Type；其值标识伪随机算法及初始化规则，本身不保存抽样状态。 |
| `Generator` | 随机生成器的 Type；其值保存可变状态，抽样时会推进。 |

#### 随机源常量

| 名称 | 类型 | 含义 |
|---|---|---|
| `pcg32_xsh_rr` | `Source` | 预定义随机源值，采用 PCG-XSH-RR 64/32 算法，固定 stream 为 54；不需要调用。 |

#### 函数

| 签名 | 返回 | 行为 |
|---|---|---|
| `generator(source: Source, seed: Int)` | `Generator` | 按随机源和种子创建独立状态。 |
| `next_int(rng: Generator, bound: Int)` | `Int` | 均匀生成 [0, bound) 内的整数；bound 必须为正。 |
| `next_float(rng: Generator)` | `Float` | 均匀生成 [0, 1) 内的浮点数，使用 53 位精度。 |
| `next_bool(rng: Generator)` | `Bool` | 等概率生成 true 或 false。 |
| `advance(rng: Generator, steps: Int)` | `Nil` | 按 32 位输出位置推进状态；负数表示回退。 |

例如，将随机源常量传给 generator，再使用返回的生成器抽样：

```tapas
let stdlib_random_rng: random::Generator = random::generator(random::pcg32_xsh_rr, 42)
let stdlib_random_value = random::next_int(stdlib_random_rng, 10)
```

相同随机源、种子与调用序列产生相同结果。抽样和 advance 都修改 Generator 状态；该包不提供密码学随机源。

完整接口约束见 [`random` README](../src/stdlib/random/README.md)。

<a id="package-finite"></a>

### `finite`：有限概率分布包

#### 类型

| 名称 | 含义 |
|---|---|
| `Index` | 不可变、非空的非负整数坐标，支持 Indexable。 |
| `Distribution` | 有限索引空间上的概率分布 Type。 |

#### 函数

| 签名 | 返回 | 行为 |
|---|---|---|
| `index(coords: List[Int])` | `Index` | 从坐标列表创建 Index。 |
| `shape(dist: Distribution)` | `Index` | 返回各维长度。 |
| `prob(dist: Distribution, index: Index)` | `Float` | 返回指定索引的概率。 |
| `mass(dist: Distribution, indices: List[Index])` | `Float` | 返回索引集合的总概率；重复索引只计一次。 |
| `samples(dist: Distribution, count: Int, rng: random::Generator)` | `List[Index]` | 独立抽样并推进 rng；结果允许重复。 |
| `uniform(n: Int)` | `Distribution` | 在 n 个索引上均匀分布。 |
| `categorical(weights: List[Float])` | `Distribution` | 按非负权重创建一维分布。 |
| `piecewise_constant(n: Int, steps: List[Pair[Int, Float]])` | `Distribution` | 创建分段常量权重分布；steps 从索引 0 开始。 |
| `piecewise_linear(knots: List[Pair[Int, Float]])` | `Distribution` | 创建分段线性权重分布；首节点为 0，末节点坐标加一决定长度。 |
| `product(dists: List[Distribution])` | `Distribution` | 独立联合分布，拼接各输入形状。 |
| `mixture(dists: List[Distribution], weights: List[Float])` | `Distribution` | 对同形分布加权混合。 |
| `conditional(dist: Distribution, indices: List[Index])` | `Distribution` | 在去重后的索引事件上重新归一化，保留原形状；事件质量必须为正。 |
| `map(dist: Distribution, shape: Index, mapping: Function[Index] -> Index)` | `Distribution` | 将索引映射到目标形状，合并落到同一索引的概率。 |
| `bernoulli(p: Float)` | `Distribution` | 成功概率为 p 的伯努利分布。 |
| `binomial(trials: Int, p: Float)` | `Distribution` | trials 次独立伯努利试验的成功次数分布。 |
| `multinomial(trials: Int, weights: List[Float])` | `Distribution` | 按类别权重进行 trials 次试验的计数分布。 |
| `hypergeometric(population: Int, successes: Int, draws: Int)` | `Distribution` | 有限总体中不放回抽样的成功次数分布。 |
| `zipf(n: Int, exponent: Float)` | `Distribution` | 长度为 n、指定指数的有限 Zipf 分布。 |

各维长度必须为正，坐标必须在形状范围内。权重必须有限、非负且总和为正；概率参数和分布参数按各构造函数的定义校验。Distribution 通过这些构造函数创建，不是无限分布的隐式截断。

完整接口约束见 [`finite` README](../src/stdlib/finite/README.md)。

## 应用包

这些包面向规则求解与采样、源码格式化等具体用途。

<a id="package-solve"></a>

### `solve`：规则求解与采样包

#### 类型

| 名称 | 含义 |
|---|---|
| `HoldResult` | 可满足性状态、查询范围、witness 与诊断。 |
| `SampleResult` | 采样状态、样本、执行统计与诊断。 |

#### 函数

| 签名 | 返回 | 行为 |
|---|---|---|
| `hold(rule: Rule \| RuleInstance)` | `HoldResult` | 在本次配置的整数参数域内查询是否存在满足规则的实例。 |
| `sample(rule: Rule, count: Int, space: Dictionary[String, Indexable], *, ...)` | `SampleResult` | 在有限定义域上产生满足规则、保持条件分布的样本。 |

`hold` 的结果必须先检查 `status`：sat、unsat、unknown、unsupported 或 error。
其字段还包括 scope、reason、witness、integer_min、integer_max、conflicts、bindings。
默认整数参数域为 [-1000000000000, 1000000000000]，可用 TAPAS_SOLVE_INT_MIN 和
TAPAS_SOLVE_INT_MAX 配置；结论只针对配置域，不代表整个 Int 上的全局结论。

`sample` 的 count 是非负样本数。space 按 Rule 形参名指定有限、可索引的定义域；
这些形参是采样变量，其他形参由 solver 补全，不承诺其分布。可选参数必须按名称传入：

| 参数 | 类型 | 默认值与含义 |
|---|---|---|
| `distributions` | `Dictionary[String, finite::Distribution] \| finite::Distribution` | `{}`；按形参指定一维分布，或指定联合分布；形状必须匹配定义域。未指定的变量均匀分布。 |
| `rng` | `random::Generator` | 每次调用新建 PCG-XSH-RR、seed 为 0 的 Generator；传入的状态会推进。 |
| `candidate_limit` | `Int` | `count * 1000`；原始候选总数上限。 |
| `time_limit` | `Float \| Nil` | `nil`；可指定整个调用的墙钟时间上限（秒）。 |
| `core_budget_factor` | `Float` | `1.0`；必须大于零，控制额外求解预算。 |
| `fairness_interval` | `Int` | `4`；必须至少为 2，控制泛化搜索的公平调度间隔。 |
| `generalization` | `String` | `'online_mass_core'`；不可行候选的泛化策略。 |
| `scheduler` | `String` | `'mass_fair'`；额外求解的调度策略。 |
| `trace` | `Bool` | `false`；是否记录逐候选统计。 |
| `cache_limit` | `Int` | `0`；活动缓存区域上限，0 表示无限制。 |
| `cache_policy` | `String` | `'mass'`；超限时按独有质量淘汰，或选择 `'fifo'`、`'binding_count'`。 |
| `exploration` | `String` | `'budget'`；原预算调度，或选择实验性 `'gain'` 门控、`'off'` 禁用泛化。 |
| `core_call_limit` | `Int \| Nil` | `nil`；可增加整个调用的额外求解次数上限，0 禁止额外求解。 |
| `discovery_horizon` | `Int` | `0`；自动估计剩余候选数，正值为门控所用的预计候选总长度。 |
| `events` | `Bool` | `false`；记录查询、缓存替换与探索暂停／恢复事件。 |

SampleResult.status 为 complete、incomplete、unknown、unsupported 或 error。
结果还包括 reason、samples、candidates、solver_calls、core_calls、cache_hits、
cache_regions、trace、elapsed、events、timings。`timings` 区分求解、质量计算和容量选择耗时；容量选择包含质量计算，不能将三者简单相加。探索预算耗尽后仍继续采样；达到候选或时间上限时可能只返回部分样本，
应以 status 判断是否完成，不能仅凭 samples 非空判断成功。

后端依赖 Python 和 OR-Tools，依赖版本见 [requirements.txt](../src/stdlib/solve/requirements.txt)。
默认使用 python3，可用 TAPAS_SOLVE_PYTHON 指定解释器路径。
完整结果字段、支持范围和策略选项见 [solve 包说明](../src/stdlib/solve/README.md)。

<a id="package-format"></a>

### `format`：源码格式化包

这是源码包，入口为 format/__init__.tap，可通过 `tapas -m format` 执行。

#### 类型

| 名称 | 含义 |
|---|---|
| `Edit` | 具有 start: Int、end: Int、text: String 字段的文本编辑。 |

#### 函数

| 签名 | 返回 | 行为 |
|---|---|---|
| `edits(source_text: String)` | `List[Edit]` | 返回按起点递增、不重叠、相对于原文的编辑列表；已格式化时为空列表。 |
| `source(source_text: String)` | `String` | 返回格式化后的新字符串，不修改输入。 |
| `is_formatted(source_text: String)` | `Bool` | 检查文本是否已符合格式。 |
| `file(path: String)` | `Bool` | 格式化并通过原子替换写回；内容改变时返回 true。 |
| `check_file(path: String)` | `Bool` | 只检查文件格式，不写入。 |
| `main(arguments: List[String])` | `Int` | 命令行入口；无检查失败返回 0，否则返回 1。 |

Edit 的 start、end 是原始源码的半开字节偏移。按原文位置消费全部编辑，
或按起点降序应用，结果等于 source 的输出；不能把偏移当成字符位置。
新增换行使用输入中首次出现的换行序列，输入没有换行时使用 LF。
命令行中 --check 之前的路径执行格式化，之后的路径仅检查。
规范布局见[代码风格](Style_zh.md)，编辑接口的完整约束见
[format 包说明](../src/stdlib/format/README.md)。
