# evaluators：自定义 Rule evaluator 适配

`evaluators` 提供自定义 Rule evaluator 的创建、执行和编译入口，以及 evaluator 回调读取 RuleInstance 参数、Capture、Term 与 Requirement 的 Context 操作。Evaluator 通过统一 Result 返回成功、不支持或失败状态及诊断信息。

## 支持范围

### 类型

```tap
evaluators::Evaluator: Type
evaluators::Context: Type
evaluators::Result: Type
evaluators::Diagnostic: Type
```

`Evaluator` 保存名称、版本、evaluate 回调和可选 compile 回调。`Context` 表示一次 Rule evaluator 执行中的 RuleInstance、参数绑定与 Capture 环境。`Result` 表示 evaluator 的统一返回值，`Diagnostic` 表示随结果返回的诊断。

Result 使用 `status`、`value`、`violations` 和 `diagnostics` 字段。`status` 为 `Success`、`Unsupported` 或 `Failed`；`value` 保存 evaluator 的业务结果，后两个字段分别保存规则违反项和执行诊断。回调应在所有状态下返回结构完整的 Result。

### 创建与执行

```tap
evaluators::make(name: String, version: Int, evaluate: Function, compile: Function?) -> Evaluator
evaluators::eval(instance: RuleInstance, evaluator: Evaluator) -> evaluators::Result
evaluators::compile(rule: Rule, evaluator: Evaluator) -> evaluators::Result
```

`make` 创建不可变 Evaluator。`evaluate` 必须是 Function；`compile` 可以省略或为 Nil，否则必须是 Function。`name` 和 `version` 用于识别 evaluator 实现及其协议版本。

`eval` 对已经绑定的 RuleInstance 执行 evaluator 的 evaluate 回调。`compile` 对 Rule 执行 compile 回调；Evaluator 没有提供 compile 回调时返回不支持结果。具体业务返回值存放在 Result 中。

### Context 操作

```tap
evaluators::binding(context: Context, parameter: rules::Parameter | Int) -> AnyType
evaluators::capture(context: Context, capture: rules::Capture | Int) -> AnyType
evaluators::value(context: Context, term: RuleTerm) -> AnyType
evaluators::requirement(context: Context, requirement: rules::Requirement) -> RuleInstance
```

`binding` 使用 Parameter 或参数位置读取当前 RuleInstance 的绑定值。`capture` 使用 Capture 或捕获位置读取本次执行的闭包值。`value` 在当前 Context 中求取可直接解释的 Term 值。`requirement` 解析 Requirement 的目标 Rule 与实参，并返回对应 RuleInstance。

### 限制

Context 只在所属 evaluator 调用期间有效，不应跨调用保存或复用。回调必须显式处理其支持的 Type、Term 和 Extension；无法解释但合法的结构应返回不支持结果，而不是忽略 Rule 内容。

所有函数只接受签名及约束明确允许的输入；参数、索引、回调类型或 Context 不合法，以及无法识别的输入，一律返回错误。
