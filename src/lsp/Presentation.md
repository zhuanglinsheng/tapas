# 符号显示规则

hover 和补全的 detail 统一由 `presentation.c` 生成。解析器只负责提供符号类别、名称、静态类型和定义；显示层负责前缀、分隔符以及展开规则，不查询运行时对象，不执行用户代码。

## 类别与格式

| 类别 | 示例 | 名称来源 |
|---|---|---|
| 只读变量 | `let count: Int` | 词法绑定名 |
| 可变变量 | `var count: Int` | 词法绑定名 |
| 参数 | `parameter quantity: Int` | 参数声明名 |
| 循环变量 | `iteration variable item: Int` | 迭代绑定名；前缀跟随语义符号类别 |
| 函数声明／包导出的函数 | `Function solve::hold[Union[Rule, RuleInstance]] -> solve::HoldResult` | 声明名／包限定名 |
| 保存函数的变量 | `let query: Function[Union[Rule, RuleInstance]] -> solve::HoldResult` | 变量名，不冒充函数声明名 |
| 保存 Rule 的变量 | `let R: Rule[Int]` | 变量名，不给匿名 Rule 命名 |
| Type 值／类型定义 | `Type State {count: Int}`；`Type Number = Union[Int, Float]` | 类型绑定名／包限定名 |
| 包／导入的命名空间 | `package solve` | 包名／导入别名 |
| 结构字段 | `reason: String` | 字段名 |
| 标准库普通值 | `math::pi: Float` | 包限定名 |
| 类型信息缺失 | `let value: ?`；`Function callback[?] -> ?` | 仍保留已知类别和名称 |

函数签名中的参数按类型、按声明顺序展示；可变参数使用 `...`。参数名不从旧的 detail 字符串反向解析。已声明的返回值跟随实参类型关系统一显示为 `T`，例如 `Function copy[T] -> T`，依据 result_relation 元数据而非函数名称。若要展示默认值、可选参数或泛型关系等更多信息，应扩展结构化元数据与统一格式化器，而非让包自行生成签名。

## 结构与别名

- 顶层有名称的结构类型变量／字段采用 `let bounds: solve::HoldResult {status: String, ...}`，类型名不再重复，定义不另起一行。
- 匿名结构直接显示 `{field: Type}`。
- 函数签名、List 等容器内的类型别名保持名称，不自动展开。嵌套的匿名结构按原有类型格式显示。
- 直接查看 Type 值时展开其定义；记录类型前不加等号，其余定义使用 `=`。
- `?` 表示静态类型未知，不代表可选字段。可选字段仍采用 `field?: Type` 的类型表示。
- 循环类型沿用静态类型格式化器的递归占位表示，不递归展开对象。

## 入口一致性

- 根级标准符号、包成员、包别名成员和接收者调用使用同一个标准符号适配器。
- 本地符号与导入符号使用同一个源码符号适配器。导入引用保留实际声明身份，位置范围仍对应当前引用。
- 结构字段的 hover 和补全使用同一个字段适配器。
- 补全 label、文档大纲和工作区符号名称只承担名称／导航职责，不拼入上述类型详情。
- shadowing 与别名解析在格式化之前完成；格式化器不按拼写猜测符号来源。

标准库 `type` 和已解析的 Type IR 是类型依据。手写 `detail` 属于旧的说明元数据，不再充当 hover 或补全签名；它不被拼接到签名，也不被解析成类型。未来的说明段落应有独立的文档字段。

运行时 `print/pprint` 是另一层：它显示实际对象的内存地址、Rule identity、绑定和 IR 内容。编辑器没有这些运行时值，因此不会伪造地址或编号。两层共享类型表示习惯，但不能混用对象身份与词法变量名。

## 扩展原则

新增包、结构类型或函数不应修改 hover handler。先补全符号元数据；只有引入新语义类别时，才扩展 presentation 层与类别测试。任何新显示规则同时验证 hover 和补全，并覆盖本地／导入或直接／别名引用。
