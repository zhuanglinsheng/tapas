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

当前提供 12 个原生包和 1 个源码包 `format`，按与语言核心的关系和用途分为三类：

| 分类 | 包 |
|---|---|
| 语言补充包 | [`types`](../src/stdlib/types/README.md), [`syntax`](../src/stdlib/syntax/README.md), [`rules`](../src/stdlib/rules/README.md), [`evaluators`](../src/stdlib/evaluators/README.md) |
| 基础工具包 | [`io`](../src/stdlib/io/README.md), [`time`](../src/stdlib/time/README.md), [`random`](../src/stdlib/random/README.md), [`heap`](../src/stdlib/heap/README.md), [`math`](../src/stdlib/math/README.md), [`dense`](../src/stdlib/dense/README.md) |
| 应用包 | [`finite`](../src/stdlib/finite/README.md), [`solve`](../src/stdlib/solve/README.md), [`format`](../src/stdlib/format/README.md) |

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
| `bool(value: AnyType)` | `Bool` | nil、零值和长度为零的复合对象为 false；字符串 true、false 单独识别。 |
| `str(value: AnyType)` | `String` | 完整文本表示。 |
| `list()` / `list(iterable: Iterable)` | `List` | 建立空列表，或消费任意可迭代对象并生成新列表。 |
| `replicate(values: List, count: Int)` | `List` | 将源列表整体浅复制 count 次；count 为非负 Int，零次返回空列表。 |
| `pair(first: AnyType, second: AnyType)` | `Pair` | 建立对。 |
| `iter(start: Int, end: Int)` | `Iterator` | 自动推断步长的半开范围。 |
| `iter(start: Int, step: Int, end: Int)` | `Iterator` | 指定步长的半开范围。 |
| `array(rows: Int, cols: Int, value: AnyType)` | `RealArray \| BoolArray` | Bool 填充 BoolArray；数值填充 RealArray；List 按行优先填充。 |

`int`、`float` 的导出签名接受 AnyType，但实现只处理 Bool、Int、Float、String，其他值报错。
字符串转换调用宿主 C 的 strtol（十进制）和 strtod，允许前导空白；未消费的尾部字符（包括空白）或 errno 非零会报错。
它们以首个 NUL 作为文本结尾，并非按 String 字节长度验证完整输入；当前 float('') 返回 0。
Float 转 Int 使用宿主 C 转换，当前未单独检查非有限值或越界。Bool 转数值时 false 为 0，true 为 1。
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
| `join(values: Iterable, separator: String)` | `String` | 依次拼接可迭代对象产生的 String 元素，分隔符可为空串。 |
| `append(target: Appendable, value: AnyType)` | `Nil` | 追加元素或对（列表、字典等容器）。String 不可变，拼接请使用 `join`。 |
| `insert(list: List, value: AnyType, index: Int)` | `Nil` | 在索引前插入；负数从末尾换算，最终位置允许等于列表长度。 |
| `pop_front(list: List)` | `AnyType` | 删除并返回第一个元素。 |
| `pop_back(list: List)` | `AnyType` | 删除并返回最后一个元素。 |
| `delete(target: Deletable, key: AnyType)` | `Nil` | 删除元素。 |
| `idx(target: Indexable, ...indices: AnyType)` | `AnyType` | 转发一个或多个索引，规则由目标对象决定。 |
| `idx_or(target: Indexable, ...indices: AnyType, default: AnyType)` | `AnyType` | 最后一个参数为默认值；索引不存在时返回它，参数类型错误仍报错。 |
| `keys(value: Dictionary \| Type)` | `List` | 返回 Dictionary 的键，或 Type 可枚举的字段名。 |
| `values(dictionary: Dictionary)` | `List` | 按相应顺序返回值。 |
| `concat(left: Iterable, right: Iterable)` | `List` | 依次消费两侧元素，返回新的浅复制 List。 |
| `sort(values: List)` | `Nil` | 原地排序；数值按大小比较，其他值只按运行时存储类别排列。 |

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
| 可迭代输入 | `list`、`join`、`concat` 接受 Iterable；`join` 仍要求每个元素为 String，空输入返回空串；`concat` 始终返回 List。 |
| 通用索引与修改 | `idx`、`idx_or` 转发索引参数；`append`、`delete` 分别要求 Appendable、Deletable。能力类型不约束具体键类型。 |
| List 专用操作 | `replicate`、`insert`、首尾 push/pop、`sort` 操作具体的有序 List；仅支持索引读写不足以保证插入、移除或排序语义。 |
| 键和值枚举 | Dictionary 不隐式选择迭代键或值；显式使用 `keys`、`values`。Indexable 本身不能枚举合法键。 |
| 专用构造与反射 | `array`、`iter`、`pair`、Rule 反射和断言保留其对象领域约束。 |

`join`、`concat` 按输入的迭代顺序读取，不修改输入；浅复制会保留复合元素的引用。
String 的迭代单位是字节，因此 `join('abc', '-')` 得到 `a-b-c`，不表示 Unicode 代码点切分。
Array 当前不具备 Iterable 能力；用 `idx`/`idx_or` 访问时必须提供行、列两个索引。

`idx_or` 至少需要一个索引，默认值始终是最后一个实参。List、String、Dictionary、
Array 提供非抛错查询；第三方对象需要实现 `try_indexable` 才能保证索引缺失时返回默认值。
缺少此能力的兼容路径仍调用普通索引，可能抛错。Dictionary 的单键查询保留一次哈希探测的实现。

`copy` 遵循对象的复制协议：List、Dictionary 浅复制元素，Array 复制独立的数据存储，
第三方对象可以定义自己的复制行为。`identical` 不只是比较地址：同类型 List 等对象会进行
结构匹配，`identical(1, 1.0)` 为 false。`sort` 目前不是通用比较器接口，不保证字符串的
字典序、复合值的内容顺序、NaN 顺序或稳定排序；Int 与 Float 混合比较会转换为 double，大整数可能丢失精度；需要可控优先级时可使用 heap 的比较函数。

```tapas
let stdlib_access_matrix = array(2, 3, [1, 2, 3, 4, 5, 6])
print(idx(stdlib_access_matrix, 1, 2))
print(idx_or(stdlib_access_matrix, 9, 0, -1))
print(join('abc', '-'))
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

### `rules`：Rule 检查、变换与 IR 包

#### 类型

| 名称 | 含义 |
|---|---|
| `PointsOf` | 有限点集定义域模板。 |
| `RangeOf` | 闭整数区间定义域模板。 |
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

这些包提供通用运算、系统访问、数据结构和数组计算。

### `io`：文本文件包

| 签名 | 返回 | 行为 |
|---|---|---|
| `read_text(path: String)` | `String` | 一次读取整个文件。 |
| `write_text(path: String, text: String)` | `Nil` | 创建或覆盖文件。 |
| `append_text(path: String, text: String)` | `Nil` | 创建文件或在末尾追加。 |
| `replace_text(path: String, text: String)` | `Nil` | 在同一目录写入临时文件，再原子替换目标文件。 |

这些函数精确保留字符串的字节长度。
当前接口不引入文件句柄、模式字符串或二进制值；它们需要独立的生命周期和类型设计。

### `time`：时间包

| 签名 | 返回 | 行为 |
|---|---|---|
| `from_unix(seconds: Int)` | `Time` | 从 Unix 时间戳创建时间点。时间戳是自 Unix 纪元起的整数秒数。 |
| `unix(value: Time)` | `Int` | 返回时间点对应的 Unix 整数秒数。 |
| `format(value: Time, pattern: String)` | `String` | 使用本地时区和宿主 C `strftime` 格式字符串生成文本。 |

时间戳和时间偏移必须落在宿主平台的`time_t`范围以及 Tapas `Int`范围内。
格式化模式及受支持的转换符由宿主 C 库决定。
当前版本没有保存时区信息，也没有提供 UTC 格式化或日期解析。

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

### `heap`：二叉堆包

`heap` 在普通 List 上原地维护二叉堆，不需要额外的堆对象。

| 函数 | 说明 | 时间复杂度 |
|---|---|---|
| `heapify(items[, less])` | 原地建堆，返回 Nil | O(n) |
| `push(items, value[, less])` | 插入元素，返回 Nil | 均摊 O(log n) |
| `pop(items[, less])` | 移除并返回堆顶 | O(log n) |
| `peek(items)` | 查看堆顶 | O(1) |

默认按 Int、非 NaN Float 的数值大小构建最小堆；混合比较保留整数精度。
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

### `math`：标量数学包

下表中的数字参数均为`Int | Float`，除特别说明外返回`Float`。
定义域、溢出、无穷和 NaN 行为遵循宿主 C 数学库。
以下名称都通过`math::`访问。

| 分组 | 签名 |
|---|---|
| 绝对值与根 | `abs(Int \| Float) -> Int \| Float`, `fabs`, `sqrt`, `rsqrt`, `cbrt`: `(Int \| Float) -> Float` |
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

### `dense`：稠密数组包

| 签名 | 返回 | 行为 |
|---|---|---|
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

## 应用包

这些包面向有限概率建模、规则求解与采样、源码格式化等具体用途。

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

SampleResult.status 为 complete、incomplete、unknown、unsupported 或 error。
结果还包括 reason、samples、candidates、solver_calls、core_calls、cache_hits、
cache_regions、trace、elapsed。样本允许重复；达到预算上限时可能只返回部分样本，
应以 status 判断是否完成，不能仅凭 samples 非空判断成功。

后端依赖 Python 和 OR-Tools，依赖版本见 [requirements.txt](../src/stdlib/solve/requirements.txt)。
默认使用 python3，可用 TAPAS_SOLVE_PYTHON 指定解释器路径。
完整结果字段、支持范围和策略选项见 [solve 包说明](../src/stdlib/solve/README.md)。

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
