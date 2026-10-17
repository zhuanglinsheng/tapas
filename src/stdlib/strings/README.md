# strings：基础字符串操作

所有操作按 String 的实际字节长度处理，保留内嵌 NUL。偏移和模式匹配以字节为单位，
不改变现有 String 索引语义，也不进行 Unicode 大小写折叠或正规化。

| 接口 | 行为 |
| --- | --- |
| `join(values: Iterable, separator: String) -> String` | 按迭代顺序拼接 String 元素，分隔符可为空串；空输入返回空串。 |
| `split(text: String, separator: String) -> List` | 按非空分隔串拆分，保留首尾及连续分隔符产生的空项；空输入返回 `['']`。 |
| `lines(text: String) -> List` | 按 LF、CRLF 或 CR 分行，移除换行符；末尾换行不增加额外空项，空输入返回 `[]`。 |
| `trim(text: String) -> String` | 移除两端 ASCII 空白：空格、HT、LF、VT、FF、CR。 |
| `find(text: String, needle: String, [start: Int]) -> Int` | 返回首个匹配的绝对字节偏移，未找到为 -1；start 默认 0。 |
| `replace(text: String, old: String, replacement: String) -> String` | 从左至右替换全部非重叠匹配，old 不得为空；不再次匹配新增文本。 |
| `starts_with(text: String, prefix: String) -> Bool` | 是否具有指定前缀；空前缀为 true。 |
| `ends_with(text: String, suffix: String) -> Bool` | 是否具有指定后缀；空后缀为 true。 |
| `is_utf8(text: String) -> Bool` | 检查严格 UTF-8：拒绝过长编码、代理项、截断序列及大于 U+10FFFF 的码点。空串和 NUL 合法。 |

`join` 接受任意 Iterable，但每项必须是 String，不隐式调用 str；String 自身按字节迭代，
因此 `strings::join('abc', '-')` 返回 `a-b-c`，不表示 Unicode 码点切分。
List 使用直接存储路径，其他输入使用迭代能力；保留元素与分隔符中的内嵌 NUL，不修改输入。
根作用域的 join 仅保留为历史兼容入口，与本包使用同一个实现；新代码使用 strings::join。

`find` 的负 start 从字节串末尾换算，过小则从 0 开始，超过长度则返回 -1。
空 needle 在有效 start 处匹配，包括等于长度的位置。返回值始终相对于完整源串。
`split` 和 `replace` 拒绝空模式，避免隐含字节边界插入或 Unicode 切分语义。
查找使用 KMP，扫描最坏 O(n + m)，模式辅助空间 O(m)。

所有函数不修改输入；拆分产生新的 List。参数类型错误或不合法的空模式产生运行时错误。
`is_utf8` 只校验编码，不保证可显示、无控制字符或符合某一语言的文本规范。

```tapas
print(strings::join(strings::split('a::b::', '::'), '|'))
print(strings::replace('你好你好', '你好', '好'))
print(strings::find('ababa', 'aba', 1))
```

<pre class='Tapas-Return'>
a|b|
好好
2
</pre>
