# bytes：紧凑字节缓冲区

`Buffer` 是包自有的固定长度可变序列，不增加核心值类别。

```tap
bytes::buffer(length: Int, value: Int) -> Buffer
```

每项一字节，读写 Int 0..255，支持负索引、Iterable、MemberTestable、Indexable、IndexSettable。
通用 len/copy/fill/count 可直接使用；copy 独立分配，越界或非字节写入报错，不截断。
不支持扩容、append、delete、文本编码或文件操作。

批量操作通过可选的 vtable bulk_operations 实现。未处理时返回 0 且没有副作用，调用者继续通用逐项路径。
完整契约与示例见 docs/Stdlib_zh.md 和 docs/Stdlib_en.md。

成员判断使用字节查找，命中后提前结束；不改变跨类型不匹配和空缓冲区语义。

## String 转换

```tap
bytes::from_string(value: String) -> Buffer
bytes::to_string(value: Buffer) -> String
```

按完整字节长度直接复制存储，保留 NUL 和任意非 UTF-8 字节，不逐元素装箱。
两种转换都返回独立存储：后续修改 Buffer 不会改变输入或已生成的 String。
不执行编码转换、UTF-8 校验或替换非法字符。需要 UTF-8 文本时使用
`strings::is_utf8(bytes::to_string(buffer))` 显式检查。
零长度 String 和 Buffer 可以互转；`to_string` 只接受本包 Buffer。

二进制文件接口 `io::read_bytes`、`write_bytes`、`append_bytes`、`replace_bytes`
直接使用 Buffer 存储，不需要经由 String 转换；它们均有可恢复的 try_ 版本。
