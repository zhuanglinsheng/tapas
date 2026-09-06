# rules：不可变 Rule IR 的构造、检查与变换

`rules` 提供 Rule 与 RuleInstance 的检查和反射、不可变 RuleIR 的动态构造、Rule Item 变换，以及有限点集与整数区间。RuleIR 保存参数、Capture、Condition、Requirement、Term 和来源信息；构造与变换操作返回新对象，不修改输入 Rule。

## 支持范围

### 类型

```tap
types::RuleIR: Type
types::RuleTerm: Type
types::RuleItem: Type
rules::Parameter: Type
rules::Capture: Type
rules::Condition: Type
rules::Requirement: Type
rules::Origin: Type
rules::CheckResult: Type
rules::Violation: Type
rules::Diagnostic: Type
rules::PointsOf: Type
rules::RangeOf: Type
```

`RuleIR`、`RuleTerm` 与 `RuleItem` 是 core 对象 Type，由 `types` 暴露；rules
包只提供这些对象的领域操作。Parameter 与 Capture 分别表示调用参数和闭包捕获；
Condition 与 Requirement 是 RuleItem 的具体种类，RuleTerm 表示规则表达式，
Origin 表示来源位置。

`CheckResult` 保存规则检查结论，Violation 与 Diagnostic 描述不满足条件及执行诊断。`PointsOf[T]` 表示有限值集合，`RangeOf[Int]` 表示包含两个端点的整数闭区间。

### 检查与反射

```tap
rules::check(rule: RuleInstance | Rule) -> rules::CheckResult
rules::inspect(rule: RuleInstance | Rule) -> types::RuleIR
rules::parameters(value: RuleIR | Rule | RuleInstance) -> List[rules::Parameter]
rules::items(value: RuleIR | Rule | RuleInstance) -> List[types::RuleItem]
rules::terms(value: RuleIR | Rule | RuleInstance) -> List[types::RuleTerm]
rules::origin(value: AnyType) -> rules::Origin
```

`check` 执行普通 Rule checker，并返回统一检查结果。`inspect` 返回输入规则使用的 RuleIR；对 RuleInstance 的检查与反射保留其参数绑定背景。

`parameters`、`items` 与 `terms` 按 RuleIR 中的规范顺序返回对应对象。`origin` 返回支持来源信息的 Rule、Item 或 Term 的 Origin。

### 哈希与序列化

```tap
rules::semantic_hash(value: RuleIR | Rule) -> Int
rules::content_hash(value: RuleIR | Rule) -> Int
rules::serialize(value: RuleIR | Rule) -> String
rules::deserialize(data: String) -> Rule
```

`semantic_hash` 对影响规则语义的内容计算哈希，`content_hash` 还包含用于表示和来源区分的内容。哈希用于快速比较和缓存键，不应当作无碰撞身份。

`serialize` 返回 RuleIR 的版本化文本表示，`deserialize` 验证并恢复为 Rule。只有序列化协议支持的 Term、常量、Provider 和版本能够往返保存。

### Rule Item 选择与变换

```tap
rules::item(rule: Rule, target: Int | String | RuleItem) -> types::RuleItem
rules::drop(rule: Rule, target: Int | String | RuleItem) -> Rule
rules::violate(rule: Rule, target: Int | String | RuleItem) -> Rule
rules::drop_if(rule: Rule, target: Int | String | RuleItem, premise: Rule) -> Rule
rules::violate_if(rule: Rule, target: Int | String | RuleItem, premise: Rule) -> Rule
rules::restrict(rule: Rule, ...restrictions: Pair) -> Rule
```

target 为 Int 时按直接 Item 的零基位置选择，为 String 时按唯一的直接 Item 描述选择，为 Item 时要求对象直接属于该 Rule。描述不存在或不唯一时返回错误。

`drop` 返回删除目标 Item 的派生 Rule。`violate` 将目标 Item 替换为其不成立条件。`drop_if` 在 premise 成立时允许忽略目标，在 premise 不成立时仍要求目标成立；`violate_if` 要求 premise 与目标成立性恰有一个为真。premise 必须与原 Rule 具有相同参数签名。

`restrict` 接受形如 `name: value` 或 `name: domain` 的 Pair，为具名参数添加等值或成员约束，并保留原 Rule 为 Requirement。参数名称必须存在，值或 Domain 必须与参数 Type 相容。

### Domain

```tap
rules::points(element_type: Type, ...values: AnyType) -> rules::PointsOf
rules::range(start: Int, end: Int) -> rules::RangeOf
rules::membership(value: RuleTerm, domain: RuleTerm) -> types::RuleTerm
```

`points` 构造元素 Type 明确的有限点集；每个值必须匹配 `element_type`。`range` 构造包含 start 与 end 的整数闭区间。`membership` 构造表示 value 属于 domain 的 Bool Term。

### Term 与 Rule 构造

```tap
rules::term(type: Type) -> Type
rules::parameter(name: String, type: Type) -> rules::Parameter
rules::constant(value: AnyType) -> types::RuleTerm
rules::call(function: AnyType, arguments: List[RuleTerm]) -> types::RuleTerm
rules::negation(operand: RuleTerm) -> types::RuleTerm
rules::conjunction(left: RuleTerm, right: RuleTerm) -> types::RuleTerm
rules::disjunction(left: RuleTerm, right: RuleTerm) -> types::RuleTerm
rules::condition(term: RuleTerm, description: String?) -> rules::Condition
rules::requirement(rule: Rule | RuleTerm, arguments: List[RuleTerm]) -> rules::Requirement
rules::implication(antecedent: RuleTerm, consequents: List[RuleTerm], description: String?) -> types::RuleItem
rules::extension(provider: String, kind: String, arguments: List[RuleTerm], payload: AnyType) -> types::RuleTerm
rules::make(display_name: String, parameters: List[rules::Parameter], items: List[RuleItem]) -> Rule
```

`term` 返回指定结果 Type 的 Term Type。`parameter` 与 `constant` 创建 Parameter Term 和常量 Term。`call`、`negation`、`conjunction`、`disjunction` 与 `membership` 构造对应表达式；逻辑操作要求 Bool Term。

`condition` 使用 Bool Term 创建硬条件。`requirement` 使用目标 Rule Term 和实参 Term 创建规则依赖。`implication` 表示 antecedent 成立时全部 consequents 必须成立。`extension` 创建由 provider 与 kind 标识的扩展 Term，其解释与 payload 验证由对应 Provider 负责。

`make` 从参数和 Item 列表创建动态 Rule。每个 Parameter 必须唯一并属于参数列表，Condition 与 implication antecedent 必须具有合法 Bool Type，Requirement 的参数数量和 Type 必须与已知目标 Rule 相容；Term 图不得循环。

### 限制

Rule 变换只选择输入 Rule 的直接 Item，不递归搜索嵌套 Requirement。序列化、哈希和 Extension 的稳定性依赖相应协议版本与 Provider；不要使用显示名称、描述或哈希替代对象身份。

所有函数只接受签名及约束明确允许的输入；类型、索引、参数关系、Rule 签名、Term 图或序列化数据不合法，以及无法识别的输入，一律返回错误。
