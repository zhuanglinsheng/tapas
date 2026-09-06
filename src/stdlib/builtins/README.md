# builtins：根作用域内建操作

`builtins` 目录实现无需包名前缀即可调用的根作用域操作，包括集合构造与修改、类型转换、输入输出、反射、计时和 Rule 断言。它不是可通过 `builtins::` 访问的普通包；下列名称直接存在于程序根作用域。

## 支持范围

### 集合与数组

```tap
array(rows: Int, cols: Int, value: AnyType) -> RealArray | BoolArray
list(...values: AnyType) -> List
pair(first: AnyType, second: AnyType) -> Pair
keys(value: Dictionary | Type) -> List
values(dictionary: Dictionary) -> List
concat(left: List, right: List) -> List
sort(values: List) -> Nil
```

`array` 创建指定行列数的二维数组。`value` 为 Bool 时创建 BoolArray，为 Int 或 Float 时创建 RealArray；为 List 时，列表长度必须等于 `rows * cols`，并且全部元素必须同为 Bool 或同为数值。数组按行优先顺序填充。

`list` 和 `pair` 分别使用传入值创建 List 与 Pair。`keys` 返回 Dictionary 的键，或 Type 可迭代的字段名；`values` 返回 Dictionary 的值。`concat` 返回两个列表内容依次连接的新 List，不修改输入列表。`sort` 原地排列 List；数值之间按数值大小比较，其他值只提供运行时类型顺序，不承诺业务语义上的全序。

### 可变集合操作

```tap
idx(target: Indexable, key: AnyType) -> AnyType
append(target: Appendable, value: AnyType) -> Nil
delete(target: Deletable, key: AnyType) -> Nil
push_front(list: List, value: AnyType) -> Nil
push_back(list: List, value: AnyType) -> Nil
pop_front(list: List) -> AnyType
pop_back(list: List) -> AnyType
insert(list: List, value: AnyType, index: Int) -> Nil
```

`idx` 执行目标对象的索引读取。`append` 和 `delete` 分别调用目标对象的追加与删除能力。

`push_front`、`push_back` 和 `insert` 原地修改 List；负的插入位置从列表末尾换算，最终位置可以等于列表长度。`pop_front` 与 `pop_back` 移除并返回首尾元素，空列表返回错误。

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

`len` 返回对象长度；Nil 的长度为 `0`，没有复合长度的标量长度为 `1`。`type` 返回运行时类型名称。`copy` 按对象自身的复制语义创建副本，标量直接复制。`identical` 比较两个值是否具有相同身份；它不同于普通值相等。

`int` 接受 Int、Float、Bool 或完整表示十进制整数的 String。`float` 接受 Int、Float、Bool 或完整表示浮点数的 String。`bool` 对 Nil、数值、String 和其他复合对象执行布尔转换；String `true` 与 `false` 被特别识别，其他复合对象按长度判断。`str` 返回值的完整字符串表示。

### 迭代与反射

```tap
iter(start: Int, end: Int) -> Iterator
iter(start: Int, step: Int, end: Int) -> Iterator
parameters(value: Function | Rule | RuleInstance | RuleIR) -> List[Pair[String, Type]]
arguments(instance: RuleInstance) -> List[AnyType]
```

`iter` 创建从 `start` 开始、但不包含 `end` 的整数迭代器。省略 `step` 时，根据端点顺序使用 `1` 或 `-1`；`step` 为零或方向无法到达 end 时，迭代器为空。`parameters` 按声明顺序返回函数或规则的参数名称与 Type；函数必须带有可用的参数元数据。`arguments` 按参数顺序返回 RuleInstance 当前绑定值的浅层容器副本。

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

`print` 使用简略表示依次写出值并追加换行，`pprint` 使用完整表示。`input` 可先写出提示，然后读取一行并移除行尾换行；输入结束且没有内容时返回 Nil。

`assert` 检查 Rule 或 RuleInstance，不满足时返回错误。`clock` 与 `clock_ns` 返回当前进程消耗的 CPU 时间，单位分别为秒和纳秒；`now` 返回当前系统时间的 Time 值。

### 限制

数组维数必须是非负整数且乘积不能溢出。集合索引、可变操作和转换必须满足目标对象的能力及具体类型约束。I/O、内存、时间源或 Rule 检查失败均返回错误。

名称以双下划线开头的 session 操作属于编译与运行时内部协议，不是应用程序公共接口，因此不列入支持范围。
