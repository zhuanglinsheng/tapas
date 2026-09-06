# syntax：Tapas 源码词法切分

`syntax` 提供 Tapas 源码的 tokenizer，并公开格式化等源码工具需要的换行与水平制表符常量。Token 使用源码 String 的字节偏移表示范围。

## 支持范围

### 类型

```tap
syntax::Token: Type
```

`Token` 是包含 `kind: String`、`start: Int` 和 `end: Int` 字段的结构值。`kind` 是 tokenizer 的规范 token 名称；`start` 是包含端点，`end` 是不包含端点，因此原始文本可用 `source[start:end]` 取得。

### 字符常量

```tap
syntax::line_feed: String
syntax::horizontal_tab: String
```

`line_feed` 是单字符 String `"\n"`，`horizontal_tab` 是单字符 String `"\t"`。

### 通用操作

```tap
syntax::tokens(source: String) -> List[syntax::Token]
```

`tokens` 按源码顺序返回全部 Token，包括 tokenizer 产生的空白、换行、注释和 EOF Token。每个范围都引用传入 source，不复制 token 文本。

### 限制

`tokens` 只执行词法切分，不构造 AST，也不验证完整语法或名称解析。偏移量是底层源码文档使用的字节位置；包含多字节字符时不能把它当作 Unicode 字符序号。

所有函数只接受签名明确允许的 String；类型不合法或词法分析失败，以及无法识别的输入，一律返回错误。
