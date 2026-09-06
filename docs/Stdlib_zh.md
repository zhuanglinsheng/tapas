# Tapas 标准库

简体中文 | [English](Stdlib_en.md) | [项目主页](../README.md)

本文记录随 Tapas 提供的根内建函数、原生包和源码包。
语言语法与运算符语义见[语言规范](Syntax_zh.md)，Type 与`types`包见[类型系统](TypeSystem_zh.md)，Rule 与 evaluator 接口见[Rule](Rules_zh.md)。

根函数直接按名称调用；包成员使用`package::name`。
根函数的第一个参数可以作为隧道调用的接收者。
标准库 API 会随语言版本演进，不属于词法或语法定义。

## 1. 签名说明

`A | B`表示两种运行时 Type 之一，`AnyType`表示任意 Tapas 值，`...AnyType`表示零个或多个参数。
返回 Type 为`Nil`表示过程，其结果不能保存。
根函数可以在第一个参数为接收者时使用隧道调用，如`append(items, value)`等于`items.append(value)`。

## 2. 控制台、文本文件、检查与时间

| 签名 | 返回 | 行为 |
|---|---|---|
| `print(...AnyType)` | `Nil` | 打印简略表示并换行。 |
| `pprint(...AnyType)` | `Nil` | 打印完整表示并换行。 |
| `input()` | `String \| Nil` | 从标准输入读取一行；EOF 返回 `nil`。 |
| `input(prompt: String)` | `String \| Nil` | 不换行打印并刷新提示后读取一行；EOF 返回 `nil`。 |
| `len(value: AnyType)` | `Int` | 复合值长度；`nil` 为 0，其他标量为 1。 |
| `type(value: AnyType)` | `String` | 返回运行时类型名。 |
| `copy(value: T)` | `T` | 复制标量或浅复制复合值。 |
| `identical(left: AnyType, right: AnyType)` | `Bool` | 检查运行时同一性。 |
| `parameters(value: Function \| Rule \| RuleInstance \| RuleIR)` | `List[Pair[String, Type]]` | 按声明顺序读取参数名与实际 Type 对象，不执行规则。 |
| `arguments(instance: RuleInstance)` | `List[AnyType]` | 按声明顺序读取已绑定实参，不执行或检查规则。 |
| `assert(rule: RuleInstance \| Rule)` | `Nil` | 检查 Rule；Condition 不满足时产生运行时错误。完整语义见 [Rule](Rules_zh.md#3-检查接口)。 |
| `clock()` | `Float` | 进程 CPU 时间（秒）。 |
| `clock_ns()` | `Int` | 进程 CPU 时间（纳秒），适合测量较短的代码。 |
| `now()` | `Time` | 返回调用时刻对应的绝对时间点。默认显示使用本地时区。 |

`pprint`的名称表示结构化的完整打印；生成字符串使用`str`，不再把通常表示“格式化为字符串”的`sprint`用作打印函数。
`io`包提供最小的完整文本文件接口：

| 签名 | 返回 | 行为 |
|---|---|---|
| `io::read_text(path: String)` | `String` | 一次读取整个文件。 |
| `io::write_text(path: String, text: String)` | `Nil` | 创建或覆盖文件。 |
| `io::append_text(path: String, text: String)` | `Nil` | 创建文件或在末尾追加。 |
| `io::replace_text(path: String, text: String)` | `Nil` | 在同一目录写入临时文件，再原子替换目标文件。 |

这些函数精确保留字符串的字节长度。
当前接口不引入文件句柄、模式字符串或二进制值；它们需要独立的生命周期和类型设计。

`syntax`包提供与编译器共用的词法视图，供格式化器和其他源码工具使用：

| 名称 | 类型 | 行为 |
|---|---|---|
| `syntax::Token` | `Type` | 具有 `kind: String`、`start: Int`、`end: Int` 字段的结构 Type。 |
| `syntax::tokens(source: String)` | `List` | 返回包含 trivia 和 EOF 的有序 token 记录。位置是源文本中的半开字节区间。 |
| `syntax::line_feed` | `String` | 换行字符。 |
| `syntax::horizontal_tab` | `String` | 水平制表符。 |

`format`是源码标准包，其入口仍为`format/__init__.tap`。
它导出`source`、`is_formatted`、`file`、`check_file`和`main`，并可通过`tapas -m format`执行。
代码布局的规范定义见[代码风格](Style_zh.md)。

## 3. 转换与构造

| 签名 | 返回 | 行为 |
|---|---|---|
| `int(value: Bool \| Int \| Float \| String)` | `Int` | 转换为整数；字符串必须是完整十进制输入。 |
| `float(value: Bool \| Int \| Float \| String)` | `Float` | 转换为浮点数。 |
| `bool(value: AnyType)` | `Bool` | 显式真值转换。 |
| `str(value: AnyType)` | `String` | 完整文本表示。 |
| `list(...AnyType)` | `List` | 用参数建立新列表。 |
| `pair(first: AnyType, second: AnyType)` | `Pair` | 建立对。 |
| `iter(start: Int, end: Int)` | `Iterator` | 自动推断步长的半开范围。 |
| `iter(start: Int, step: Int, end: Int)` | `Iterator` | 指定非零步长的半开范围。 |
| `array(rows: Int, cols: Int, fill: Bool \| Int \| Float \| List)` | `RealArray \| BoolArray` | 建立稠密数组。 |

数组维度不得为负，显式迭代步长不得为零。

## 4. 集合操作

| 签名 | 返回 | 修改与结果 |
|---|---|---|
| `push_front(list: List, value: AnyType)` | `Nil` | 在列表开头插入元素。 |
| `push_back(list: List, value: AnyType)` | `Nil` | 在列表末尾插入元素。 |
| `append(target: Appendable, value: AnyType)` | `Nil` | 追加文本、元素或对。 |
| `insert(list: List, value: AnyType, index: Int)` | `Nil` | 在索引前插入。 |
| `pop_front(List[T])` | `T` | 删除并返回第一个元素。 |
| `pop_back(List[T])` | `T` | 删除并返回最后一个元素。 |
| `delete(target: Deletable, key: AnyType)` | `Nil` | 删除元素。 |
| `idx(target: Indexable, key: AnyType)` | `AnyType` | 单参数索引。 |
| `keys(Dictionary)` | `List` | 按未规定顺序返回键。 |
| `values(Dictionary)` | `List` | 按相应顺序返回值。 |
| `concat(List, List)` | `List` | 返回浅复制拼接结果。 |
| `sort(values: List)` | `Nil` | 按运行时全序原地排序。 |

对字典调用`append`时，追加的值必须是`Pair`。
`pop_front`和`pop_back`会转移被删除元素的所有权，不能用于空列表。
`delete`只执行通用删除，仍返回`nil`。
集合中不能保存`nil`。

## 5. 会话函数

| 签名 | 返回 | 作用 |
|---|---|---|
| `__ls__([Library])` | `List` | 当前根库或指定库中的名称。 |
| `__path__([Library])` | `List` | 当前库或指定库的搜索路径。 |
| `__param__(index: Int)` | `AnyType` | 当前变参调用的参数。 |
| `__nparam__()` | `Int` | 当前变参调用的参数数量。 |
| `__binary__([Library \| Function])` | `Nil` | 打印当前或指定值的字节码。 |

以`__`开头的名称由实现保留。

## 6. 时间包 `time`

| 签名 | 返回 | 行为 |
|---|---|---|
| `time::from_unix(seconds: Int)` | `Time` | 从 Unix 时间戳创建时间点。时间戳是自 Unix 纪元起的整数秒数。 |
| `time::unix(value: Time)` | `Int` | 返回时间点对应的 Unix 整数秒数。 |
| `time::format(value: Time, pattern: String)` | `String` | 使用本地时区和宿主 C `strftime` 格式字符串生成文本。 |

时间戳和时间偏移必须落在宿主平台的`time_t`范围以及 Tapas `Int`范围内。
格式化模式及受支持的转换符由宿主 C 库决定。
当前版本没有保存时区信息，也没有提供 UTC 格式化或日期解析。

## 7. 标量数学包 `math`

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

## 8. 稠密数组包 `dense`

| 签名 | 返回 | 行为 |
|---|---|---|
| `dense::rows(value: RealArray \| BoolArray)` | `Int` | 行数。 |
| `dense::cols(value: RealArray \| BoolArray)` | `Int` | 列数。 |
| `dense::transpose(value: RealArray \| BoolArray)` | `RealArray \| BoolArray` | 新的转置数组。 |
| `dense::identity(size: Int)` | `RealArray` | 建立指定阶数的单位矩阵。 |
| `dense::trace(value: RealArray)` | `Float` | 主对角线之和；允许矩形数组。 |
| `dense::inner(left: RealArray, right: RealArray)` | `Float` | 两个同形数组的 Frobenius 内积。 |
| `dense::outer(RealArray, RealArray)` | `RealArray` | 两个行向量或列向量的外积。 |
| `dense::norm(value: RealArray)` | `Float` | 向量的二范数或矩阵的 Frobenius 范数。 |
| `dense::normalize(RealArray)` | `RealArray` | 返回同形的单位范数副本。 |
| `dense::copy_into(source: RealArray, target: RealArray)` | `Nil` | 将数据复制到同形的既有数组，不分配结果。 |
| `dense::scale_inplace(value: RealArray, factor: Int \| Float)` | `Nil` | 原地执行 `value *= factor`。 |
| `dense::add_scaled_inplace(target: RealArray, source: RealArray, factor: Int \| Float)` | `Nil` | 原地执行 `target += factor * source`。 |
| `dense::gemm(alpha: Int \| Float, left: RealArray, right: RealArray, beta: Int \| Float, output: RealArray)` | `Nil` | 原地执行 `output = alpha * left @ right + beta * output`。 |

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
