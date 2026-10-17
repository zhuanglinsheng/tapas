# json：JSON 编解码

使用随仓库提供的 yyjson 0.13.0 实现严格 JSON 解析和序列化。无需额外安装依赖，
不执行文件或网络 I/O；读取文件可组合 `io::read_text`。

## 接口

```tap
json::Null: Type
json::null: Null
json::parse(text: String) -> AnyType
json::try_parse(text: String) -> Dictionary
json::stringify(value: AnyType, [pretty: Bool]) -> String
```

`parse` 消费整个输入，只允许 JSON 标准语法；允许 JSON 空白，不接受注释、尾逗号、
BOM、NaN、Infinity 或非法 UTF-8。重复对象键报错，包括解码后相同的转义键。

| JSON | Tapas |
| --- | --- |
| null | `json::null` |
| boolean | Bool |
| 无小数点或指数的整数 | Int；超出 Tapas Int 范围报错，不转成 Float |
| 带小数点或指数的数字 | 有限 Float；溢出报错 |
| string | String，按长度保留内嵌 NUL；解码 Unicode 转义和代理对 |
| array | List |
| object | String 键的 Dictionary |

`json::null` 是不可变的包对象，可以保存在变量、List、Dictionary 中；与返回值 `Nil`
不同。`types::matches(value, json::Null)` 检查类型；`identical` 或 `==` 比较空值。
`copy(json::null)` 仍表示同一个逻辑空值。字段不存在与字段值为 `json::null` 不同，
应使用 `key in dictionary` 区分。不要通过 `bool` 判断 JSON null。

`try_parse` 对格式、数字范围、重复键及深度错误返回以下完整记录，调用方必须先检查 `ok`：

| 字段 | 含义 |
| --- | --- |
| `ok: Bool` | 是否成功 |
| `value: AnyType` | 成功时的解析值；失败时为 `json::null` |
| `error: String` | 成功时为空串；失败时为诊断文本，文本不属于稳定错误码 |
| `position: Int` | 语法错误的零起始字节偏移；成功或映射错误为 -1 |

错误参数类型和运行时资源耗尽仍按普通运行时错误处理。`parse` 直接产生运行时错误，
语法诊断包含字节偏移。

`stringify` 默认输出紧凑 JSON；第二个位置参数为 true 时使用缩进格式。
只接受上表中的值。非 String 字典键、非有限浮点数、非法 UTF-8、循环容器及其他对象
（如 Pair、Array、Rule、Time）报错，不隐式调用 `str`。共享但无循环的容器允许重复编码。
字典键顺序未规定；不提供规范化或签名用的 canonical JSON。浮点输出可以改变十进制
拼写，不能作为保留原始金额文本或任意精度十进制的手段。

解析与输出最多允许 256 层 List/Dictionary。返回值独立于输入 JSON 文本和 yyjson 文档，
后续修改解析出的容器不会修改源字符串。序列化不修改输入。

```tapas
let document = json::parse('{"name":"Tapas","value":null}')
print(document::name)
print(types::matches(document::value, json::Null))
print(json::stringify([document::value, 42]))
let checked = json::try_parse('[1,]')
print(checked::ok)
```

<pre class='Tapas-Return'>
Tapas
true
[null,42]
false
</pre>

依赖来源见 `src/third_party/yyjson/VERSION`，MIT 许可证随发行版安装。
