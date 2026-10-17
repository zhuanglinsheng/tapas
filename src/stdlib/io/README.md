# io：文件内容与标准流

`io` 提供文本和二进制文件的读取、写入、追加与替换，以及标准输出、标准错误和终端查询。文件路径使用 String。文本按原始字节读写，不执行字符编码转换或换行规范化。

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

path 必须非空且不得包含空字节。`replace_text` 要求目标目录允许创建临时文件和执行重命名；其原子性遵循所在文件系统的重命名语义，不提供跨文件系统替换。

上述严格接口只接受签名及约束明确允许的输入；路径或文本类型不合法，以及打开、读取、写入、同步、关闭或重命名失败，一律返回错误。

## 可恢复操作

```tap
io::try_read_text(path: AnyType) -> Dictionary
io::try_write_text(path: AnyType, text: AnyType) -> Dictionary
io::try_append_text(path: AnyType, text: AnyType) -> Dictionary
io::try_replace_text(path: AnyType, text: AnyType) -> Dictionary
```

与对应严格接口共用实际文件操作；路径无效、参数类型错误和操作系统 I/O 失败返回记录，
不会因为这些错误中止后续案例。内存耗尽等运行时资源错误仍报错。

| 字段 | 含义 |
| --- | --- |
| `ok: Bool` | 是否成功，读取 value 前必须检查 |
| `value` | 读取成功时为完整 String，失败时为空串；写操作成功为 true，失败为 false |
| `code: String` | 成功为空串；失败为稳定错误标识 |
| `error: String` | 成功为空串；失败为诊断文本，内容可能因平台和语言环境变化 |
| `errno: Int` | 操作系统错误号；成功或参数校验失败为 0，具体数字依平台而异 |

错误标识包括 `invalid_type`、`invalid_path`、`not_found`、`permission_denied`、
`is_directory`、`not_directory`、`no_space`、`already_exists`、`not_empty`、`symlink_loop`、`cross_device` 和兜底 `io_error`。
路径校验与系统错误代码映射由 io/fs 共用，具体能出现哪些错误取决于操作和平台。
路径必须非空且不含 NUL。文本参数在打开文件前验证，错误参数不会先截断目标文件。

不提供自动重试、错误回滚或目录创建。`try_write_text` 和 `try_append_text` 可能在失败前
写入部分内容；需要替换语义时使用 `try_replace_text`。后者在失败时清理临时文件，
与严格 `replace_text` 一样不提供父目录 fsync 或完整的断电持久性保证。

## 二进制文件

`read_bytes(path: String) -> bytes::Buffer` 直接构造独立的 Buffer，不经 String 中转。
`write_bytes(path, buffer)`、`append_bytes(path, buffer)`、`replace_bytes(path, buffer)`
接受 String 路径和 bytes::Buffer，返回 Nil；与文本版本具有相同覆盖、追加和原子替换合同。
四个接口均提供 `try_` 版本，参数接受 AnyType，返回相同的结果记录。
失败读取的 value 为零长度 Buffer，失败写入为 false。先检查 ok 再消费 value。

## 标准流

| 接口 | 行为 |
| --- | --- |
| `write(text: String) -> Nil` | 按完整字节长度写 stdout，不自动换行。 |
| `write_error(text: String) -> Nil` | 按完整字节长度写 stderr，不自动换行。 |
| `flush([stream: String]) -> Nil` | 刷新 stdout（默认）或 stderr；不接受 stdin。 |
| `is_terminal(stream: String) -> Bool` | stream 为 stdin、stdout、stderr；终端为 true，文件或管道为 false，无效描述符报错。 |

标准流接口不引入可关闭的文件句柄；调用方不会通过这些接口关闭标准流。
写入保留 NUL，不执行编码转换；类型、流名或 I/O 错误仍报错。
