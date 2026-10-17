# path：POSIX 路径文本操作

本包面向 Tapas 当前 Linux/macOS 平台，使用 `/` 分隔符，确定性地处理路径文本。
不访问文件系统、不转换为绝对路径、不检查存在性、不解析符号链接，也不展开 `~`、
环境变量或通配符。反斜杠和 Windows 盘符不具有特殊含义。

| 接口 | 行为 |
| --- | --- |
| `join(...parts: String) -> String` | 至少一个参数；跳过空项，按需插入 `/`；后续绝对路径丢弃此前部分。保留各项内部的分隔符和 `.`、`..`。全部为空时返回空串。 |
| `normalize(value: String) -> String` | 合并重复 `/`，移除 `.`，按词法消去可抵消的 `..`；相对路径保留不可消去的 `..`，绝对路径不会越过根。 |
| `basename(value: String) -> String` | 忽略尾部 `/` 后返回最后一段；根路径返回 `/`，空路径返回空串。 |
| `dirname(value: String) -> String` | 忽略尾部 `/` 后移除最后一段及多余的尾部分隔符；没有父路径时返回 `.`，根路径返回 `/`。 |
| `extension(value: String) -> String` | 文件名的最后一个扩展名，包含 `.`；只有前导点的文件名没有扩展名，`.env.local` 返回 `.local`。 |
| `is_absolute(value: String) -> Bool` | 是否以 `/` 开始；空路径为 false。 |

所有接口要求 String 且拒绝内嵌 NUL，但不要求 UTF-8。除 `join` 和 `normalize`
明确规定的处理外，不自动重写路径。`normalize` 将任何数量的前导 `/` 折叠为一个 `/`，
移除尾部 `/`；空路径或抵消后的相对路径返回 `.`。

**词法规范化不保证文件系统等价。** 例如 `link/../file` 中的 link 如果是符号链接，
消去 `link/..` 可能改变实际指向。因此 `join` 不隐式调用 `normalize`。
这些接口也不构成访问目录的安全边界。

```tapas
print(path::join('fixtures', 'orders.json'))
print(path::normalize('/a/../../b//'))
print(path::extension('/a/.env.local'))
```

<pre class='Tapas-Return'>
fixtures/orders.json
/b
.local
</pre>
