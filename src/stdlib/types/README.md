# types：运行时 Type 构造与反射

`types` 提供 Tapas 内建 Type 值、结构与组合 Type 构造器，以及运行时值和 Type 的反射操作。Type 是不可变值；构造操作返回规范化 Type，不修改传入 Type。

## 支持范围

### 内建类型

```tap
types::AnyType: Type
types::Nil: Type
types::Bool: Type
types::Int: Type
types::Float: Type
types::String: Type
types::List: Type
types::Pair: Type
types::Dictionary: Type
types::Iterator: Type
types::Function: Type
types::Library: Type
types::RealArray: Type
types::BoolArray: Type
types::Time: Type
types::Type: Type
types::Rule: Type
types::RuleInstance: Type
types::RuleIR: Type
types::RuleTerm: Type
types::RuleItem: Type
```

这些值表示对应运行时对象的基础 Type。`RuleIR`、`RuleTerm` 与 `RuleItem`
是语言核心 Rule 表示的对象 Type；具体规则包可以在其上提供构造、检查、序列化等操作。
`AnyType` 匹配任意值，其他基础 Type 只匹配各自允许的运行时值。带参数的
List、Pair、Dictionary、Iterator、Rule 与 RuleInstance Type 通过下列构造器创建。

### 能力类型

```tap
types::Indexable: Type
types::Appendable: Type
types::IndexSettable: Type
types::Deletable: Type
types::Contains: Type
types::Iterable: Type
```

能力 Type 按对象支持的运行时操作匹配，而不是按单一具体对象种类匹配。它们分别表示索引读取、追加、索引写入、删除、成员判断与迭代能力。

### 结构与组合 Type

```tap
types::make_type(...fields: Pair[String, Type]) -> Type
types::optional(field: Pair[String, Type]) -> Type
types::union(...members: Type) -> Type
types::enum(...members: String) -> Type
types::list(item: Type) -> Type
types::iterator(item: Type) -> Type
types::pair(first: Type, second: Type) -> Type
types::dictionary(key: Type, value: Type) -> Type
types::rule(...parameters: Type) -> Type
types::rule_instance(...parameters: Type) -> Type
```

`make_type` 从至少一个具名字段创建结构 Type。普通字段写作 `'name': FieldType`；将字段 Pair 传给 `optional` 后，该字段在结构匹配中可以缺省。字段名称必须是 String，字段值必须是 Type。

`union` 从至少两个 Type 创建联合 Type。`enum` 从至少一个 String 创建有限字符串 Enum Type。

`list`、`iterator`、`pair` 与 `dictionary` 创建带元素、两侧或键值参数的容器 Type。`rule` 与 `rule_instance` 按参数顺序创建 Rule 和 RuleInstance Type；允许零个参数。

### 用户定义 Type 模板

```tap
types::parameter(name: String) -> Type
types::value_parameter(name: String) -> Type
types::template(type_parameters: List, value_parameters: List, definition: Type) -> Type
```

`parameter` 和 `value_parameter` 分别创建类型参数和值参数占位符。`template`
的前两个参数固定为参数列表，第三个参数是使用这些占位符构造的 Type：

```tap
let T = types::parameter('T')
let N = types::value_parameter('N')

let Box = types::template(
    [T],
    [],
    types::make_type('value': T),
)

let Sized = types::template(
    [T],
    [N : types::Int],
    types::make_type('value': T, 'size': N),
)

let boxed: Box[String] = {'value': 'text'}
let sized: Sized[String; 4] = {'value': 'text', 'size': 4}
```

类型参数列表直接包含 `parameter` 占位符。值参数列表可以直接包含
`value_parameter` 占位符（约束默认为 `AnyType`），也可以包含
`value_parameter : ConstraintType` Pair；显式约束必须是静态 Type。应用模板时，值参数
会成为精确值 Type，因此上例 `size` 字段只接受整数 `4`，而不只是任意 Int。

标注中的参数区遵循 `Type[Types; Values]`：模板同时含两类参数时必须保留分号；
只有类型参数时写作 `Box[String]`，只有值参数时可写作 `Exact[4]`。当前值实参是
编译期可识别的 Bool、Int、Float 或 String 字面量；模板参数数量、类别和约束
均在编译期检查。

### 值反射

```tap
types::of(value: AnyType) -> Type
types::matches(value: AnyType, expected: Type) -> Bool
```

`of` 返回值的运行时 Type。对于 Rule 与 RuleInstance，返回值包含按声明顺序取得的参数 Type；对于参数化容器的普通运行时值，当前返回其基础 Type。

`matches` 判断 value 是否符合 expected，包括结构字段、联合、Enum、参数化 Type 与能力 Type 的匹配规则。

### Type 反射

```tap
types::fields(value: Type) -> Dictionary
types::optional_fields(value: Type) -> List[String]
types::members(value: Type) -> List
types::base(value: Type) -> Type
types::parameters(value: Type) -> Dictionary
types::definition(value: Type) -> Dictionary
```

`fields` 返回结构 Type 的字段名到字段 Type 的 Dictionary；非结构 Type 返回空 Dictionary。`optional_fields` 返回其中可缺省的字段名。

`members` 对 Enum 返回成员 String，对联合 Type 返回成员 Type；其他 Type 返回只包含自身的 List。`base` 返回参数化或派生 Type 的基础 Type。

`parameters` 返回已知参数化 Type 的参数 Dictionary：点集、区间、List 和 Iterator 使用 `item`，Pair 使用 `first` 与 `second`，Dictionary 使用 `key` 与 `value`；其他 Type 返回空 Dictionary。`definition` 返回 Type 保存的定义元数据副本。

### 限制

Type 反射结果是新的 List 或 Dictionary 容器，但其中的 Type 值仍是不可变共享值。`types::of` 只支持运行时已注册的值种类；未知扩展对象必须由其 Provider 提供 Type 集成。

所有函数只接受签名及约束明确允许的输入；字段、成员、Type 或参数不合法，以及无法识别的输入，一律返回错误。
