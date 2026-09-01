# Tapas Rule 设计

Rule 用于定义必须成立的条件。它是带参数、可组合、可检查的一等值，可用于参数校验、业务规则、对象不变量、测试生成和优化模型。

```text
let Positive = rule(value: Int) {
    "值必须大于零": value > 0
}

let ValidAmount = rule(amount: Int) {
    require Positive(amount)
    "金额不能超过单笔限额": amount <= 10000
}

assert(ValidAmount(amount))
```

`rule` 和 `require` 是语言关键字，但属于不同的语法类别：`rule` 引入 Rule 字面量表达式，`require` 引入 Rule 体语句。`assert` 是语言核心提供的 built-in 函数，不是关键字。`types`、`rules` 和 `evaluators` 是包名，依次提供类型描述、公共 IR 与扩展解释能力。

本文描述语言设计，相关语法和运行时接口尚未实现。

## 1. 语言核心

### 1.1 名称与语法类别

本文使用以下固定分类：

| 名称 | 类别 | 含义 |
|---|---|---|
| `rule` | 表达式关键字 | 引入 Rule 字面量表达式，求值后产生 Rule 值 |
| `require` | Rule 体语句关键字 | 在 Rule 体中创建 Requirement，引用另一个 Rule 应用 |
| `assert` | built-in 函数 | 检查 RuleInstance 或零参数 Rule；成功返回 `nil`，失败时产生不可恢复的运行时错误 |
| `Rule` | 运行时值类别 | 带参数、IR 和词法环境的不可变闭包值 |
| `RuleInstance` | 运行时值类别 | Rule 与一组已绑定实参组成的检查对象 |
| `types` | 标准包 | 提供 Rule 和 RuleInstance 的核心 Type |
| `rules` | 标准包 | 提供 `check`、公共 Rule IR、动态构造和序列化接口 |
| `evaluators` | 标准包 | 提供可选的自定义 Rule 解释协议 |

不存在名为 `rule` 的包。单数小写 `rule` 始终指表达式关键字；首字母大写的 Rule 指语言概念或运行时值；复数小写 `rules` 指标准包。

`require` 不是函数，只能构成 Rule 体语句。`assert` 使用普通函数调用语法，可以像其他 built-in 函数一样被引用和调用；实现可以优化对该 built-in 的直接调用。

### 1.2 Rule 值

`rule` 是表达式关键字。由它引入的 Rule 字面量是表达式，通常使用 `let` 命名：

```text
let NonNegative = rule(value: Int) {
    value >= 0
}
```

零参数 Rule 可以省略参数列表：

```text
let Ready = rule {
    database.connected
    cache.available
}
```

Rule 可以保存在容器中、作为参数传递、从函数返回或从模块导出。直接绑定 Rule 字面量时，编译器使用绑定名称作为诊断显示名称；显示名称不参与 Rule 身份和 Type 相等。

每次求值 Rule 字面量都会创建新的 Rule 闭包。别名仍引用同一个 Rule：

```text
let Alias = Ready
```

Rule 支持 `identical` 检查运行时身份，不定义基于内容的 `==`。

### 1.3 参数与 RuleInstance

Rule 参数必须标注 Type：

```text
let Between = rule(
    value: Int,
    minimum: Int,
    maximum: Int,
) {
    value >= minimum
    value <= maximum
}
```

调用 Rule 只绑定参数并产生 RuleInstance，不执行检查：

```text
let instance = Between(value, 0, 10)
```

静态可知的参数数量或 Type 不匹配属于编译错误。运行时取得的 Rule 在调用时重新检查签名，不匹配属于普通运行时错误。

### 1.4 Condition

Rule 体最外层的 Bool 表达式语句创建 Condition。所有 Condition 都必须成立；空 Rule 没有 Condition，因此恒成立。

```text
let ValidRange = rule(value: Int) {
    value >= 0
    value <= 10
}
```

Condition 可以携带说明文字：

```text
let ValidQuantity = rule(
    quantity: Int,
    minimum: Int,
    maximum: Int,
) {
    "最小值不能超过最大值":
        minimum <= maximum

    "数量必须在有效范围内": {
        quantity >= minimum
        quantity <= maximum
    }
}
```

冒号左侧必须是 String。右侧可以是一个 Bool 表达式，也可以是包含一个或多个 Bool 表达式的说明块。说明块中的每个表达式分别创建 Condition，并共享说明文字。

说明文字只用于诊断和文档，不参与逻辑判断和 Rule Type。

### 1.5 局部名称与 Term

Rule 体中的 `let` 为表达式建立局部名称，不创建 Condition：

```text
let CorrectTotal = rule(
    price: Float,
    quantity: Int,
    total: Float,
) {
    let subtotal = price * quantity
    "总价不正确": total == subtotal
}
```

Rule 参数和 Rule 体中的表达式被记录为带有结果 Type 的延迟计算。它们在执行检查时求值，不要求在创建 Rule 时求值。

Rule 体只允许局部 `let`、Condition 和 `require`，不允许赋值、可变声明、控制流、导入或直接的 IO 语句。Condition 中可以调用普通 Tapas 函数：

```text
let ValidCode = rule(code: String) {
    normalize(code) == code
}
```

调用保持普通 Tapas 语义，包括参数求值、闭包读取、原生调用、短路行为和运行时错误。Rule 不引入额外的纯函数制度。

### 1.6 闭包

Rule 与函数使用相同的闭包环境规则。它可以捕获外层的 `var` 和函数参数；这些名称
被记录为对环境的引用，执行检查时才读取当前值：

```text
let make_limit = (maximum) {
    return rule(value: Int) {
        "数值超过限制": value <= maximum
    }
}

let UnderTen = make_limit(10)
```

`maximum` 是函数参数，因此属于环境捕获，不会因为 Rule 创建而固化为常量。
`let` 使用临时存储，不能被函数或 Rule 闭包捕获。复合对象只有存放在可捕获的
`var` 或函数参数中时，才能通过环境引用读取。Rule 不建立环境快照。

只有源码字面量、Rule 体内可确定的常量表达式，以及动态构造接口显式创建的常量，才作为 Constant 保存。

### 1.7 `require` 语句

`require` 是只能出现在 Rule 体最外层的语句关键字。该语句在当前 Rule 的执行顺序中引用另一个 Rule：

```text
let PositiveQuantity = rule(order: Order) {
    "数量必须为正数": order.quantity > 0
}

let EnoughInventory = rule(order: Order, inventory: Inventory) {
    "库存不足": inventory.quantity >= order.quantity
}

let ValidOrder = rule(order: Order, inventory: Inventory) {
    require PositiveQuantity(order)
    require EnoughInventory(order, inventory)
}
```

`require` 后必须是 Rule 应用。它不复制目标 Rule；执行到 Requirement 时求值目标 Rule 与参数，并使用目标 Rule 自己的闭包继续检查。

Rule 项目严格按照源码顺序执行。同一 Rule 在不同位置被多次引用时，每个 Requirement 都独立执行，不按身份去重。

执行器维护当前 Requirement 路径。再次进入路径中已有的 Rule 时发生循环依赖错误；编译器可以提前报告静态可证明的循环。

### 1.8 `assert` built-in 函数

`assert` 是语言核心提供的 built-in 函数，不是关键字、专用语句或包接口。其函数签名接受 RuleInstance 或零参数 Rule：

```text
assert(ValidAmount(amount))
assert(Ready)
```

带有未绑定参数的 Rule 不能直接传给 `assert`，普通 Bool 也不能作为 `assert` 的参数。检查成功时返回 `nil`。

`assert` 按顺序执行 Condition，在第一个结果为 `false` 的 Condition 处产生普通 Tapas 运行时错误并终止当前执行。该错误不可恢复，函数不承担业务错误返回。错误信息至少包含：

- Rule 显示名称或源码位置；
- Condition 的说明；
- 相关参数值；
- Requirement 路径；
- Condition 和 assertion 的源码位置。

`assert` 在所有构建模式中执行，不得被发布构建或优化选项删除。Term 求值发生错误时，继续遵守 Tapas 的普通运行时错误规则。

局部匿名 Rule 可以直接用于 assertion：

```text
let withdraw = (account, amount) {
    assert(rule {
        "金额必须为正数": amount > 0
        "余额不足": account.balance >= amount
    })

    account.balance = account.balance - amount
}
```

### 1.9 编译模型

Rule IR 是编译器生成检查代码的规范输入，不要求运行时逐节点解释。

```text
源码 Rule 字面量
    -> RuleTemplate {
           ir
           assertion_checker
       }
    -> 运行时绑定词法环境
    -> Rule
```

编译器默认为每个源码 Rule 字面量生成一个 assertion checker，就像为每个函数字面量生成函数程序。多个 Rule 闭包可以共享同一 RuleTemplate，并分别持有自己的词法环境。未使用的 Rule 及其 checker 可以由死代码消除移除。

`assert` 的 VM 实现从 RuleInstance 取得 Rule、实参和 assertion checker，然后调用该 checker。checker 按项目顺序执行表达式、调用和条件跳转；Requirement 可以调用目标 Rule 的 checker，也可以被普通内联优化展开。

编译器不需要识别 `assert(...)` 调用点才能保证正确性。直接调用识别只用于消除临时 RuleInstance、内联小型匿名 Rule 或直接调用已知 checker。通过别名调用 built-in 仍具有相同语义：

```text
let verify = assert
verify(ValidAmount(amount))
```

`rules::check` 和自定义 evaluator 的编译产物不默认生成。它们在首次需要时解释 Rule IR，或按需编译并缓存。对于编译期可确定的直接调用，实现也可以提前生成相应 checker。

运行时动态构造的 Rule 没有源码阶段生成的专用 assertion checker。首次执行时可以使用通用 RuleIR checker，也可以延迟编译并缓存；这不涉及重新解析源码。

`assert` 的实现不依赖 `rules` 包或公共 Evaluator 协议。未实现后两者时，Rule 字面量、`require` 和 `assert` 仍然完整可用。

## 2. `types` 包

Rule 和 RuleInstance 是语言核心值。`types` 包提供原始 Type 和静态 Type 构造器，
类型标注使用参数化 Type 应用：

| 形式 | 含义 |
|---|---|
| `types::Rule` | 任意 Rule |
| `Rule[Type, ...]` | 具有指定参数 Type 列表的 Rule 标注 |
| `types::rule(Type...)` | 构造具体 Rule Type 值 |
| `types::RuleInstance` | 任意 RuleInstance |
| `RuleInstance[Type, ...]` | 具有指定签名的 RuleInstance 标注 |
| `types::rule_instance(Type...)` | 构造具体 RuleInstance Type 值 |

Rule Type 只包含有序参数 Type 列表，不包含参数名称和返回 Type。Rule 的结果固定为一组必须成立的 Condition。

参数 Type 按不变规则比较。参数名称只用于源码、反射和诊断，不参与 Type 相等。零参数 Rule 的精确标注是 `Rule[]`，对应 Type 值由 `types::rule()` 构造。

每个精确 Rule Type 都匹配 `types::Rule`；每个精确 RuleInstance Type 都匹配 `types::RuleInstance`：

```text
let IntRule = types::rule(types::Int)

let Positive: IntRule = rule(value: Int) {
    value > 0
}

let NonNegative: Rule[Int] = rule(value: Int) {
    value >= 0
}
```

`types` 包只定义语言核心的 Rule Type，不收纳公共 IR、检查结果或 Evaluator 类型。包定义的值由对应包提供 Type：

```text
types::Rule              核心 Rule Type
rules::RuleIR            rules 包的 IR Type
rules::CheckResult       rules 包的检查结果 Type
evaluators::Evaluator    evaluators 包的扩展 Type
```

类型系统能够描述这些值，不表示它们必须定义在 `types` 包中。

## 3. `rules` 包与公共 IR

`rules` 是可选的标准包，提供可恢复检查、公共 IR、动态构造和序列化接口。它不定义 `rule`、`require` 或 `assert` 的核心语义，也不引入关键字。

### 3.1 `rules::check`

`rules::check` 是 `rules` 包中的普通函数，不是关键字或语句。它将检查失败作为普通结果返回：

```text
let result = rules::check(ValidOrder(order, inventory))

if(result.passed == false) {
    print(result.violations)
}
```

```text
rules::CheckResult {
    status: Success | Unsupported | Failed
    passed: Bool
    violations: [rules::Violation]
    diagnostics: [rules::Diagnostic]
}

rules::Violation {
    condition: rules::Condition
    description: String?
    arguments: Dictionary
    requirement_path: [rules::Requirement]
    source: rules::Origin
}
```

Condition 为 `false` 仍属于 `Success`。`Unsupported` 表示 checker 无法处理某个合法的扩展 Term；`Failed` 表示参数、环境、Term 求值或资源错误。

`rules::check` 按顺序处理全部项目并收集 violation。Term 求值失败时返回 `Failed` 并停止本次检查。

check checker 不随每个 Rule 默认生成。`rules::check` 优先调用该 Rule 已缓存的 check checker；缓存不存在时可以解释 IR，或者按需编译并缓存。对于编译期可确定的直接调用，编译器可以提前生成 check checker，但这只是优化。

### 3.2 公共 IR 的归属

编译器内部始终维护 Rule IR。`rules` 包将同一份规范表示暴露为不可变、可读取、可验证的公共视图，不复制或重新解析 Rule。

```text
let ir: rules::RuleIR = rules::inspect(ValidAmount)
```

公共 IR Type 全部定义在 `rules` 包：

| Type | 含义 |
|---|---|
| `rules::RuleIR` | Rule 的规范语义表示 |
| `rules::Parameter` | 参数描述 |
| `rules::Capture` | 可捕获环境绑定的引用描述 |
| `rules::Item` | Condition 或 Requirement |
| `rules::Condition` | Bool Term 与说明 |
| `rules::Requirement` | Rule 引用与参数 Term |
| `rules::Term` | 任意 Term |
| `rules::term(Type)` | 具有指定结果 Type 的 Term |
| `rules::Origin` | 来源关系 |

内部 arena、指针、环境槽位、引用计数和编译缓存不属于公共 IR，也不能通过反射取得。

### 3.3 IR 结构

源码 Rule 的编译模板保存规范 IR 和默认 assertion checker；Rule 运行时值绑定该模板与闭包环境：

```text
RuleTemplate {
    type: RuleType
    ir: rules::RuleIR
    assertion_checker: Callable
}

Rule {
    identity: RuleIdentity
    template: RuleTemplate
    environment: EnvironmentRef
    capture_addresses: [EnvironmentAddress]
    display_name: String?
    checker_cache: CheckerCache
}

RuleInstance {
    rule: Rule
    bindings: [Value]
}
```

动态构造的 Rule 使用经过完整性验证的 RuleIR 创建等价模板。其 assertion checker 初始可以指向通用 RuleIR checker，并在实现支持时替换或缓存为专用编译程序。`checker_cache` 保存 check checker 和 evaluator 专用程序，不保存捕获环境的求值结果。

公共 RuleIR 不持有运行时环境地址：

```text
rules::RuleIR {
    parameters: [rules::Parameter]
    captures: [rules::Capture]
    terms: [rules::Term]
    items: [rules::Item]
    origins: [rules::Origin]
    source: SourceId?
    version: Int
}

rules::Parameter {
    id: ParameterId
    name: String
    type: Type
    origin: OriginId
}

rules::Capture {
    id: CaptureId
    expected_type: Type?
    origin: OriginId
}
```

Item 保留源码顺序：

```text
rules::Item = rules::Condition | rules::Requirement

rules::Condition {
    term: TermId
    description: String?
    origin: OriginId
}

rules::Requirement {
    rule: TermId
    arguments: [TermId]
    origin: OriginId
}
```

Requirement 的 Rule Term 必须产生与参数兼容的 `types::rule(...)` 值。它可以引用常量 Rule、闭包捕获的 Rule，或其他合法计算得到的 Rule。

Term 的公共逻辑结构为：

```text
rules::Term {
    id: TermId
    type: Type
    kind: TermKind
    arguments: [TermId]
    origin: OriginId
    payload: Any?
}
```

基础 TermKind 包括：

```text
Constant
Parameter
Capture
Intrinsic
Call
Construct
Convert
Extension(provider, version, kind)
```

Intrinsic 表示已解析的 Tapas 内建操作。Call 保持普通 Tapas 调用语义。Extension 由对应 Term Provider 验证，并由理解该 Provider 的 checker 或 evaluator 处理。

Origin 保存源码位置及变换来源。Requirement 展开、参数替换和 IR 变换必须保留来源路径，以便诊断能够回到原始 Condition。

### 3.4 IR 读取

`rules` 包至少提供以下只读接口：

| 接口 | 作用 |
|---|---|
| `rules::inspect(Rule)` | 返回 RuleIR |
| `rules::parameters(RuleIR)` | 按签名顺序返回参数 |
| `rules::items(RuleIR)` | 按源码顺序返回 Item |
| `rules::terms(RuleIR)` | 返回 Term 表的只读视图 |
| `rules::origin(IRValue)` | 返回来源信息 |

用户和工具可以遍历 IR，但不能修改节点、编号、环境地址或引用计数。需要改变 Rule 时，必须通过构造接口创建新的 Rule。

### 3.5 动态构造

`rules` 包提供不可变构造接口：

| 接口 | 结果 |
|---|---|
| `rules::parameter(name, Type)` | 创建 Parameter Term |
| `rules::constant(value)` | 创建 Constant Term |
| `rules::call(function, arguments)` | 创建普通 Call Term |
| `rules::condition(term, description)` | 创建 Condition |
| `rules::requirement(rule, arguments)` | 创建 Requirement |
| `rules::extension(provider, kind, arguments, payload)` | 创建 Extension Term |
| `rules::make(display_name, parameters, items)` | 创建并验证 Rule |

```text
let value = rules::parameter("value", types::Int)
let lower = rules::constant(0)
let upper = rules::constant(10)

let InRange = rules::make(
    "InRange",
    [value],
    [
        rules::condition(value >= lower, "值不能小于零"),
        rules::condition(value <= upper, "值不能大于十"),
    ],
)
```

不同的 `rules::parameter` 调用创建不同 ParameterId，即使显示名称相同也不合并。`rules::make` 保持项目顺序，并验证：

- Parameter Term 属于参数列表；
- Condition Term 的结果 Type 是 Bool；
- Requirement 的签名相容；
- Term 引用、Provider、payload 和来源有效；
- IR 不包含非法内部循环。

动态构造 API 只有通过 `rules::constant` 才按值保存普通对象，不捕获调用方的词法绑定。需要实时环境引用时应使用源码 Rule 字面量；动态 Rule 引用的现有 Rule 仍保留自己的闭包环境。

动态构造和源码字面量产生相同的规范 IR，并由相同的 checker 和 evaluator 处理。

### 3.6 完整性、哈希与序列化

Rule IR 的完整性检查至少确认：

- ParameterId、CaptureId、TermId 和 OriginId 有效；
- Term 参数及结果 Type 正确；
- Condition 引用 Bool Term；
- Requirement 的 Rule Type 和参数签名正确；
- Term Provider、版本和 payload 有效；
- Term 与 Origin 引用没有非法循环；
- Rule 闭包为每个 CaptureId 提供有效环境地址。

Rule IR 提供语义哈希和内容哈希。语义哈希包含参数 Type、Term、项目顺序、调用目标和 Provider 版本，不包含显示名称、说明文字、源码位置和捕获环境的当前值。内容哈希额外包含说明文字和来源，用于诊断制品与序列化。

`rules::serialize` 必须保存 RuleIR，并保存或重新连接闭包环境。无法稳定定位的局部环境不能跨进程恢复，此时返回 `Unsupported`。反序列化后必须重新执行完整性检查。

## 4. `evaluators` 包

`evaluators` 是建立在公共 Rule IR 上的可选扩展包，不属于语言核心，也不提供新的语言语句。未实现该包时，Rule、`require`、`assert` 和 `rules::check` 的核心用途不受影响。

Evaluator 用于定义 Rule 的其他解释方式，例如符号验证、测试生成、文档提取和优化。它不能改变 Rule 的项目顺序或基础 Tapas 语义。

### 4.1 类型

`evaluators` 包定义：

| Type | 含义 |
|---|---|
| `evaluators::Evaluator` | evaluator 值 |
| `evaluators::Context` | 一次执行的只读上下文 |
| `evaluators::Result` | 统一结果 |
| `evaluators::Diagnostic` | evaluator 诊断 |

```text
evaluators::Result {
    status: Success | Unsupported | Failed
    value: Any
    violations: [rules::Violation]
    diagnostics: [evaluators::Diagnostic]
}
```

`Unsupported` 表示 evaluator 不支持某个合法 Type、Term、Call 或 Extension；`Failed` 表示参数、环境、求值、后端或资源错误。

### 4.2 Context

```text
evaluators::Context {
    rule: types::Rule
    instance: types::RuleInstance
    ir: rules::RuleIR
    binding(ParameterId) -> Value
    capture(CaptureId) -> Value
    value(TermId) -> Value
    requirement(rules::Requirement) -> types::RuleInstance
}
```

`binding` 返回当前 Rule 参数。`capture` 每次调用时从 Rule 闭包环境读取当前值。`value` 按普通 Tapas 语义执行 Term。`requirement` 求值目标 Rule 和参数、检查签名并创建子 RuleInstance。

符号 evaluator 可以直接读取 Term，不必调用 `value`。Context 不允许修改 Rule IR、参数绑定或闭包环境。

### 4.3 协议与入口

```text
evaluators::Evaluator {
    name: String
    version: Int
    evaluate: Function(evaluators::Context) -> evaluators::Result
    compile: Function(types::Rule) -> evaluators::Result?
}
```

`evaluate` 是必需处理器。`compile` 是可选处理器；没有编译能力时，`evaluators::compile` 返回 `Unsupported`。

`evaluators::eval`、`evaluators::compile` 和 `evaluators::make` 都是普通包函数。主要执行入口为：

```text
let result = evaluators::eval(instance, evaluator)
let compiled = evaluators::compile(rule_value, evaluator)
```

`evaluators::eval` 建立 Context 并调用 `evaluate`。`evaluators::compile` 成功时在 Result.value 中返回普通可调用值；其参数签名与 Rule Type 相同，并持有 Rule 的词法环境。

自定义 evaluator 使用普通包接口构造：

```text
let custom = evaluators::make(
    "custom",
    1,
    evaluate_function,
    compile_function,
)
```

最后一个参数可以是 `nil`。Evaluator 必须按照 items 顺序观察 Rule；它可以明确拒绝不支持的节点，但不能静默改变节点含义。

`assert` 和 `rules::check` 不通过该公共协议才能成立。实现可以在 `evaluators` 包中提供与 assertion checker 和 check checker 等价的标准适配器，供动态工具统一调用。

### 4.4 编译与缓存

Evaluator 可以直接遍历 RuleIR，也可以将其编译为 evaluator 专用程序：

```text
rules::RuleIR
  ├── evaluator.evaluate -> 运行时解释
  └── evaluator.compile  -> evaluator 专用程序
```

编译缓存至少考虑：

- Rule IR 语义哈希；
- evaluator 名称和版本；
- Term Provider 版本；
- 编译选项；
- 运行时 ABI。

编译程序在执行时读取实时闭包环境，因此捕获值的当前内容不属于代码缓存键。只有缓存求值结果时，才需要考虑环境版本或捕获值。

## 5. 文法

```ebnf
rule-literal = "rule",
               [ "(", [ rule-parameters ], ")" ],
               rule-block ;

rule-parameters = rule-parameter,
                  { ",", rule-parameter }, [ "," ] ;

rule-parameter = IDENTIFIER, ":", type-reference ;

rule-block = "{", separators,
             [ rule-item-list ],
             separators, "}" ;

rule-item-list = rule-item,
                 { separator-run, rule-item } ;

rule-item = let-declaration
          | condition-statement
          | described-condition-statement
          | require-statement ;

condition-statement = expression ;

described-condition-statement = STRING, ":", separators,
                                ( expression | condition-block ) ;

condition-block = "{", separators,
                  condition-statement,
                  { separator-run, condition-statement },
                  separators, "}" ;

require-statement = "require", rule-application ;

rule-application = expression ;
```

`require` 的 expression 必须产生 RuleInstance 形状的应用，并在 Rule IR 中分解为目标 Rule Term 和参数 Term。

`rule` 是表达式关键字，`require` 是只在 Rule 体内有效的语句关键字；二者加入 Tapas 保留字表。

`assert` 不是关键字，不增加专用文法。它是语言核心预置的可调用值，使用普通 call-expression 文法；运行时检查参数必须是 RuleInstance 或零参数 Rule。`rules::check` 和 Evaluator API 同样是普通包函数，不增加文法。
