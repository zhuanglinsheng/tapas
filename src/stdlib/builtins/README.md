# builtins：根作用域内建操作

`builtins` 目录实现无需包名前缀即可调用的根作用域操作，包括集合构造与修改、类型转换、输入输出、反射、计时和 Rule 断言。它不是可通过 `builtins::` 访问的普通包；下列名称直接存在于程序根作用域。

## 支持范围

### 集合与数组

```tap
array(rows: Int, cols: Int, value: AnyType) -> RealArray | BoolArray
list([iterable: Iterable]) -> List
replicate(values: List, count: Int) -> List
pair(first: AnyType, second: AnyType) -> Pair
keys(value: Dictionary | Type) -> List
values(dictionary: Dictionary) -> List
concat(left: Iterable, right: Iterable) -> List
sort(values: List) -> Nil
```

`array` 创建指定行列数的二维数组。`value` 为 Bool 时创建 BoolArray，为 Int 或 Float 时创建 RealArray；为 List 时，列表长度必须等于 `rows * cols`，并且全部元素必须同为 Bool 或同为数值。数组按行优先顺序填充。零元素形状配合空 List 创建 RealArray；创建空 BoolArray 应传 Bool 标量。

`replicate` 将整个 List 浅复制 count 次，count 必须是非负 Int，零次返回空 List；复合元素保留引用。

`list()` 创建空 List，`list(iterable)` 消费 Iterable 创建新 List；`pair` 使用两个传入值创建 Pair。`keys` 返回 Dictionary 的键，或 Type 可迭代的字段名；`values` 返回 Dictionary 的值。`concat` 按两个 Iterable 的迭代顺序创建新 List，不修改输入。对 Dictionary，`keys` 与 `values` 采用当前哈希表遍历顺序，不承诺插入顺序或排序。`sort` 原地排列 List；数值之间按数值大小比较，其他值只提供运行时类型顺序，不承诺业务语义上的全序；Int/Float 混合比较转为 double，大整数可能丢失精度，不保证 NaN 顺序或稳定排序。

### 可变集合操作

```tap
idx(target: Indexable, ...indices: AnyType) -> AnyType
idx_or(target: Indexable, ...indices: AnyType, default: AnyType) -> AnyType
append(target: Appendable, value: AnyType) -> Nil
delete(target: Deletable, key: AnyType) -> Nil
push_front(list: List, value: AnyType) -> Nil
push_back(list: List, value: AnyType) -> Nil
pop_front(list: List) -> AnyType
pop_back(list: List) -> AnyType
insert(list: List, value: AnyType, index: Int) -> Nil
```

`idx_or`在索引不存在时返回`default`，但参数 Type 错误仍然报错；Dictionary
实现只执行一次哈希查询。第三方对象需要实现 `try_indexable` 才保证缺失时返回默认值；兼容路径仍可能通过普通索引抛错。

`idx` 转发一个或多个索引；`idx_or` 将最后一个实参作为默认值，其余实参作为索引。Array 需要行、列两个索引。`append` 和 `delete` 分别调用目标对象的追加与删除能力。Dictionary 删除不存在的键会报错。

`push_front`、`push_back` 和 `insert` 原地修改 List；负的插入位置从列表末尾换算，最终位置可以等于列表长度。`pop_front` 与 `pop_back` 移除并返回首尾元素，空列表返回错误。

### 文本拼接

```tap
join(values: Iterable, separator: String) -> String
```

`join` 要求迭代产生的每个元素都是 String；空 Iterable 返回空串。String 自身
按字节迭代，因此 `join('abc', '-')` 为 `a-b-c`。保留 List 的直接存储路径，
其他对象通过 Iterable 能力读取。

### 对象与类型转换

```tap
len(value: AnyType) -> Int
type(value: AnyType) -> String
copy(value: T) -> T
identical(left: AnyType, right: AnyType) -> Bool
int(value: AnyType) -> Int
float(value: AnyType) -> Float
bool(value: AnyType) -> Bool
str(value: AnyType) -> String
```

`len` 返回对象长度；Nil 的长度为 `0`，没有复合长度的标量长度为 `1`。`type` 返回运行时类型名称。`copy` 按对象自身的复制语义创建副本，标量直接复制。`identical` 按运行时类型及对象自身的结构匹配规则比较，不只是地址比较，Int 与 Float 不会隐式转换。

`int` 接受 Int、Float、Bool 或 String（通过宿主 C strtol 按十进制解析）。`float` 接受 Int、Float、Bool 或 String（通过宿主 C strtod 解析）。`bool` 对 Nil、数值、String 和其他复合对象执行布尔转换；String `true` 与 `false` 被特别识别，其他复合对象按长度判断。`str` 返回值的完整字符串表示。

字符串数值解析允许前导空白；尾部未消费字符或 errno 非零时报错，首个 NUL 视为文本结尾。当前 float('') 返回 0。Float 转 Int 向零截断，未单独校验非有限值或越界。

### 迭代与反射

```tap
iter(start: Int, end: Int) -> Iterator
iter(start: Int, step: Int, end: Int) -> Iterator
parameters(value: Rule | RuleInstance | RuleIR) -> List[Pair[String, Type]]
arguments(instance: RuleInstance) -> List[AnyType]
```

`iter` 创建从 `start` 开始、但不包含 `end` 的整数迭代器。省略 `step` 时，根据端点顺序使用 `1` 或 `-1`；`step` 为零或方向无法到达 end 时，迭代器为空。`parameters` 按声明顺序返回 Rule IR 的参数名称与描述性 Type；不接受 Function。`arguments` 按参数顺序返回 RuleInstance 当前绑定值的浅层容器副本。

### 控制台、断言与时间

```tap
print(...values: AnyType) -> Nil
pprint(...values: AnyType) -> Nil
input(prompt: String = '') -> String | Nil
assert(rule: RuleInstance | Rule) -> Nil
clock() -> Float
clock_ns() -> Int
now() -> Time
```

`print` 使用简略表示依次写出值并追加换行，`pprint` 使用完整表示；它们都不自动插入分隔符。`input` 可先写出提示，然后读取一行并移除行尾换行；输入结束且没有内容时返回 Nil。

`assert` 检查 Rule 或 RuleInstance，不满足时返回错误。`clock` 与 `clock_ns` 返回当前进程消耗的 CPU 时间，单位分别为秒和纳秒；`now` 返回当前系统时间的 Time 值。

### 限制

数组维数必须是非负整数且乘积不能溢出。集合索引、可变操作和转换必须满足目标对象的能力及具体类型约束。I/O、内存、时间源或 Rule 检查失败均返回错误。

名称以双下划线开头的 session 操作属于编译与运行时内部协议，不是应用程序公共接口，因此不列入支持范围。

公共接口的能力范围及专用限制见 [标准库文档](../../../docs/Stdlib_zh.md#集合操作)。
