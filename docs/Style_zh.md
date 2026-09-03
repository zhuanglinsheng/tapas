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

续行参数相对函数声明缩进八个空格。
这条规则以函数参数列表是否跨越物理行判断，而不是以签名长度判断。
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

只用于一次断言的 Rule 默认直接写在`assert`内，不引入无意义的局部名称：

```text
assert(rule {
    value is Int | Float
})
```

带参数的 Rule 在关键字与参数列表之间保留一个空格：

```text
let Positive = rule (value: Int) {
    value > 0
}
```

函数标签、参数断言和函数实现之间各留一个空行，使契约与实现边界清晰。

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
