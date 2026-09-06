# Tapas 代码风格

简体中文 | [English](Style_en.md) | [项目主页](../README.md)

本文定义 Tapas 源码的推荐格式。
语言语法决定代码是否有效；本规范只统一有效代码的写法。
随 Tapas 发布的`format`包实现这些机械规则。

## 缩进与空白

- 每层使用四个空格，不使用制表符。
- 删除行尾空白，保留文件原有的换行符形式。
- 函数调用的名称与 `(` 之间不留空格。
- 关键字与紧随其后的 `(` 或 `{` 之间留一个空格，例如 `if (`、`rule (`、`rule {` 和 `else {`。
- 右括号与代码块的 `{` 之间留一个空格。

`rule`作为关键字时写成`rule (`；`types::rule`是普通函数名，因此调用时仍写成`types::rule(`。

## 函数

单行函数签名之后的`{`单独成行。
具名函数与函数字面量使用同一规则：

```text
function add(left: Int, right: Int) -> Int
{
    return left + right
}
```

参数列表已经跨行时，不再为`{`单独增加一行；它跟在签名最后一个记号之后：

```text
function structural_edits(
        source: String,
        tokens: List,
        clean_before: List[Bool],
        indents: List[String],
        line_break: String,
) -> Dictionary {
    return {}
}
```

函数和 Rule 的续行参数均相对声明所在行缩进八个空格，单独成行的右括号与声明行对齐。
所有参数列表默认采用`name: Type`形式：`:`前不留空格，后保留一个空格，
单行和多行参数列表均不要求纵向对齐。手工对齐`:`是可选的排版方式，
不是格式检查要求；格式化器不自动添加对齐填充，也不移除已有的手工对齐。
返回类型跨行书写时也视为多行签名。

## 控制流

控制流采用紧凑的 C 风格布局。
`elif`和`else`紧跟在前一个代码块的`}`后：

```text
if (condition) {
    handle_true()
} elif (alternative) {
    handle_alternative()
} else {
    handle_false()
}

while (condition) {
    advance()
}

for (let value in values) {
    consume(value)
}
```

`}`与`elif`或`else`之间不写分号。
需要跨行的复杂条件可以在括号内自然换行，代码块的`{`仍放在右括号所在行。

## Rule 字面量

`implies` 前件仅为单个 Bool 或 RuleInstance 变量时省略 `()`；后件仅为单个 Bool 变量时省略 `{}`，例如 `a implies b`。调用前件如 `(Allowed(state, parameters))` 推荐保留 `()`，复杂后件使用 `{}`，多个后件必须使用块。格式化器会应用这个推荐风格，但保留不便安全移动的注释。

只用于一次断言的 Rule 默认直接写在`assert`内，不引入无意义的局部名称：

```text
assert(rule { value is Int | Float })
```

带参数的 Rule 在关键字与参数列表之间保留一个空格：

```text
let Positive = rule (value: Int) {
    value > 0
}
```

Rule 的参数列表跨行时，使用与函数参数相同的缩进和默认间距：

```text
let WithinRange = rule (
        value: Int,
        minimum: Int,
        maximum: Int,
) {
    value >= minimum
    value <= maximum
}
```

Rule 中带字符串说明的 Condition 必须将命题写在下一行，并相对说明再缩进一级。
同一个 Rule 中相邻且带字符串说明的 Condition 之间保留一个空行；不带字符串
说明的 Condition 不要求空行：

```text
let Transferable = rule (balance: Int, amount: Int) {
    "amount must be positive":
        amount > 0

    "balance is insufficient":
        balance >= amount
}
```

字符串说明右侧是共享说明的 Condition 块时，`{`跟在冒号后，不单独换行。
块内的 Condition 相对说明缩进一级并连续书写，不强制插入空行：

```text
"quantity is outside the range": {
    quantity >= minimum
    quantity <= maximum
}
```

函数内部通过`assert`直接声明的匿名 Rule 可以保持紧凑：当 Rule 只有一个且
不带字符串说明的 Condition 时，整个断言可以写在一行。带字符串说明或包含多个
Condition 时不使用这个例外。

`assert`前后不强制保留空行。函数体内建议用空行分隔不同的逻辑代码块，
例如将参数断言、局部准备、主要计算和结果处理分别成组；同一组内的连续语句
可以紧凑书写。

## 格式化

格式化文件：

```sh
tapas -m format examples/a.tap examples/b.tap
```

只检查而不写入：

```sh
tapas -m format --check examples/a.tap examples/b.tap
```

`--check`直接作为模块参数传入，不需要额外的空`--`。
检查模式在所有文件均已格式化时返回 0；否则打印需要格式化的路径并返回 1。
