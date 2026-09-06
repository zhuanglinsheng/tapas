# format：Tapas 源码格式化

`format` 提供 Tapas 源码文本格式化、格式检查和文件原地更新。格式化器基于 `syntax::tokens` 处理空白、缩进、函数与控制结构的大括号，以及 implication 表达式；注释内容保持不变，并避免跨注释移动表达式。

## 支持范围

### 类型

```tap
format::Edit: Type

Edit {
    start: Int
    end: Int
    text: String
}
```

`Edit` 是包含 `start: Int`、`end: Int` 与 `text: String` 字段的结构 Type。它表示将原始源码的半开区间 `[start, end)` 替换为 text。`start` 与 `end` 是 `syntax::tokens` 使用的源码字节偏移。

### 文本操作

```tap
format::edits(source_text: String) -> List[format::Edit]
format::source(source_text: String) -> String
format::is_formatted(source_text: String) -> Bool
```

`edits` 返回将 source_text 转换为标准格式所需的 Edit。结果按 start 递增排列，各 Edit 互不重叠，并且全部位置都相对于原始 source_text；按原文位置消费全部 Edit，或按 start 降序原地应用，结果必须等于 `source(source_text)`。已经符合格式时返回空 List。调用方可以只应用位于目标范围内的 Edit，用于编辑器的局部格式化、修改预览或增量更新。

`source` 返回格式化后的新 String，不修改输入。它将行首缩进中的水平制表符展开为四个空格，删除行尾空白，统一受支持结构周围的空格和换行，并使用输入中首次出现的换行序列生成新增换行；输入没有换行时使用 `\n`。

`is_formatted` 在 `source(source_text)` 与原文本相同时返回 `true`。

### 文件操作

```tap
format::file(path: String) -> Bool
format::check_file(path: String) -> Bool
format::main(arguments: List[String]) -> Int
```

`file` 读取并格式化 path；内容发生变化时以 `io::replace_text` 原地替换文件并返回 `true`，无需修改时返回 `false`。`check_file` 只检查文件，不写入内容。

`main` 提供命令行入口。`--check` 之前的路径会原地格式化并在发生修改时打印，`--check` 之后的路径只检查格式并在不符合格式时打印。没有检查失败时返回 `0`，否则返回 `1`。

### 限制

格式化仅基于当前 tokenizer 能识别的 Tapas 源码结构，不构造完整 AST，也不负责修复或完整验证语法错误。为了避免改变注释归属，注释位于 implication 的前件或后件边界时，相关结构可能保持原样。输入混用多种换行序列时，现有换行不会被统一替换。

筛选 Edit 不等同于重新以某个范围为上下文运行格式化。调用方只应应用完全位于目标范围内的 Edit；与范围边界相交的 Edit 应整体跳过或扩大目标范围，不能截断 Edit 的 text。

文件操作使用文本 I/O；路径、读取、临时替换或写入失败时返回错误。所有函数只接受签名及约束明确允许的输入，类型或参数不合法，以及无法识别的输入，一律返回错误。
