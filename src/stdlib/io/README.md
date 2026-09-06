# io：文本文件输入与输出

`io` 提供以 String 路径读取、写入、追加和替换文本文件的操作。文本按原始字节读写，不执行字符编码转换或换行规范化。

## 支持范围

### 文本操作

```tap
io::read_text(path: String) -> String
io::write_text(path: String, text: String) -> Nil
io::append_text(path: String, text: String) -> Nil
io::replace_text(path: String, text: String) -> Nil
```

`read_text` 读取 path 的全部内容并返回 String。

`write_text` 创建或截断 path，然后写入 text。`append_text` 创建文件或在现有文件末尾追加 text。

`replace_text` 在目标路径旁创建临时文件，完整写入并同步 text 后通过重命名替换目标；目标已存在时保留其权限位。与直接截断写入相比，该操作避免向目标暴露部分写入的内容。

### 限制

path 不得包含空字节。`replace_text` 要求目标目录允许创建临时文件和执行重命名；其原子性遵循所在文件系统的重命名语义，不提供跨文件系统替换。

所有函数只接受签名及约束明确允许的输入；路径或文本类型不合法，以及打开、读取、写入、同步、关闭或重命名失败，一律返回错误。
