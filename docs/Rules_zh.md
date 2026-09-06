# Tapas Rule

简体中文 | [English](Rules_en.md) | [项目主页](../README.md)

Rule 是带参数、可组合、可检查的一等值，用于表达函数契约、业务规则、对象不变量，也可以作为测试生成或其他解释器的输入。
本文记录当前已经实现的 Rule 语法、Type、公共 IR 和 evaluator 接口。
通用语法见[语言规范](Syntax_zh.md)，标准库目录见[标准库](Stdlib_zh.md)，Type 规则见[类型系统](TypeSystem_zh.md)。

```tapas
let docs_positive = rule (value: Int) {
    "value must be positive":
        value > 0
}

let docs_bounded = rule (value: Int) {
    docs_positive(value)
    "value must be below ten":
        value < 10
}

assert(docs_bounded(5))

let docs_failed = rules::check(docs_bounded(12))
print(docs_failed["passed"])
print(len(docs_failed["violations"]))
print(docs_failed["violations"][0]["description"])
```
<pre class='Tapas-Return'>
false
1
value must be below ten
</pre>

## 1. 概念与执行时机

| 名称 | 类别 | 作用 |
|---|---|---|
| `rule` | 表达式关键字 | 创建 Rule 字面量 |
| 裸表达式 | Rule 项 | Bool 要求为 true，RuleInstance 要求成立 |
| `and`、`or`、`not` | 逻辑表达式 | 组合或否定 Bool/RuleInstance 的成立性，返回 Bool |
| `implies` | Rule 体语句 | 声明带前件的蕴含约束 |
| `assert` | 根内建函数 | 检查 Rule；失败时产生运行时错误 |
| `Rule` | 值与 Type | 带签名、IR 和捕获环境的不可变规则 |
| `RuleInstance` | 值与 Type | Rule 与一组已绑定实参 |
| `rules` | 标准包 | 检查、查看、构造和序列化 Rule IR |
| `evaluators` | 标准包 | 通过自定义解释器消费 Rule IR |

调用 Rule 只绑定实参并产生`RuleInstance`，不会立即检查。
检查发生在`assert`、`rules::check`或`evaluators::eval`消费实例时。
因此实例可以保存、传递和组合，而不可恢复的断言与可恢复的业务检查仍共享同一份规则。

不存在名为`rule`的包。
小写单数`rule`是关键字，复数`rules`是标准包，首字母大写的`Rule`表示值类别或 Type。

## 2. Rule 字面量

### 2.1 参数、Condition 与局部计算

Rule 参数必须具有 Type 标注。
静态可知的参数数量或 Type 不匹配是编译错误；动态取得的 Rule 在运行时检查签名。

```text
let Between = rule (
    value  : Int,
    minimum: Int,
    maximum: Int,
) {
    value >= minimum
    value <= maximum
}

let instance = Between(5, 0, 10)

let same_instance = Between({
    'maximum': 10,
    'value': 5,
    'minimum': 0
})
```

Rule 也接受一个 Dictionary 作为具名实参表。Dictionary 必须用形参名作为键，完整且仅包含该 Rule 的全部形参；值仍按对应形参的 Type 检查。对于只有一个形参的 Rule，仅当 Dictionary 包含该形参名时采用具名绑定，否则 Dictionary 仍作为普通位置实参。

零参数 Rule 可以省略参数列表。
Rule 体最外层的 Bool 表达式形成 Condition，裸实例形成子规则要求；所有项目都必须成立，空 Rule 恒成立。

说明冒号左侧必须是 String。
右侧可以是单个 Bool/RuleInstance 表达式，也可以是说明块；块中每个表达式分别形成条件或子规则要求，并共享说明。
说明只用于诊断、反射和文档。

```text
let ValidQuantity = rule (quantity: Int, minimum: Int, maximum: Int) {
    "minimum exceeds maximum":
        minimum <= maximum

    "quantity is outside the range": {
        quantity >= minimum
        quantity <= maximum
    }
}
```

Rule 体中的`let`创建局部计算，不形成 Condition。
Rule 项目只允许局部`let`、Bool/RuleInstance 项和 `implies`；赋值、`var`、控制流、导入和直接 IO 不能作为项目。
Condition 内仍可调用普通函数，并保留普通调用、短路和错误语义。

### 2.2 值、身份与捕获

Rule 是普通值，可以保存在容器中、作为参数传递、从函数返回或从模块导出。
直接绑定 Rule 字面量时，绑定名称可作为诊断显示名称，但不参与 Type 相等或身份。
`identical`可以比较 Rule 身份；Rule 不定义按内容比较的`==`。

源码 Rule 会记录它引用的外部绑定。
参数和捕获值在检查时通过 Rule 环境读取，而不是在 RuleInstance 创建时复制成独立快照。

```text
function below(maximum: Int) -> Rule[Int]
{
    return rule (value: Int) {
        value < maximum
    }
}
```

公共 IR 只暴露捕获描述，不暴露环境地址、槽位或引用计数。
动态 Rule 使用显式 Constant 和 Parameter Term，不会隐式捕获构造调用点。

### 2.3 裸实例与子规则组合

Rule 体最外层的表达式是一项要求：Bool 结果必须为 true，RuleInstance 结果必须成立。直接写子实例即可组合规则：

```text
let Positive = rule (value: Int) { value > 0 }
let SmallPositive = rule (value: Int) {
    Positive(value)
    value < 10
}
```

这里的`Positive(value)`保留 Requirement 关系，使用子规则自己的签名、IR 和捕获环境；子规则违规通过`requirement_path`归属于外层规则。重复项目分别检查，循环依赖仍报错。源码 IR 对静态已知的实例项目记录 Requirement；AnyType 等动态表达式在检查得到实例后建立违规路径中的 Requirement 关系。

`require`关键字已移除，旧写法`require R(x)`应改成`R(x)`；`require`现在可作普通标识符。内部 Requirement 节点、`rules::requirement`动态构造接口和违规路径继续保留。

### 2.4 蕴含规则项 `implies`

`implies` 是语言内建的独立 Rule 项（IR kind 为 `Implication`），不是普通 Bool 表达式运算符。它与 Condition 都表达约束，但额外保留前件、后件和触发信息。

```text
let Exchange = rule (approved: Bool, stock: Int, quantity: Int) {
    '批准后必须满足库存要求':
        approved implies {
            quantity > 0
            stock >= quantity
        }
}
```

前件是 Bool 或 RuleInstance 表达式（也支持两者的联合 Type），括号可选；单个 Bool 后件可省略花括号，多个后件必须放在 `{}` 中。推荐仅对单个前件变量省略 `()`；后件仅为单个 Bool 变量时省略 `{}`。调用形式的前件推荐加 `()`；格式化器遵循这一风格。

检查时前件只求值一次。前件为假时后件不求值，逻辑上通过，但未触发；前件为真时按顺序检查全部后件，收集各自的失败及源码位置。多个后件为合取，而非备选结果。求值错误仍是运行时错误，不会被当成未触发。

`rules::check` 的 `implications` 列表记录每次蕴含检查，包含 `item`、`instance`、`triggered`、`passed` 和 `consequents_checked`；也包含通过裸实例项目检查的子规则。未触发时 `passed` 为 true，`consequents_checked` 为 0，这不表示分支已覆盖。

第一版只允许 Rule 体最外层使用 `implies`，不支持链式、嵌套、空后件块，或在后件块中使用裸实例要求、声明和控制流。前后件按现有表达式换行规则解析。

动态构造使用 `rules::implication(antecedent, consequents[, description]) -> RuleItem`，前件 RuleTerm 的 Type 可为 Bool、RuleInstance 或两者的联合；后件仍为非空 Bool RuleTerm List（不是 Condition List）。RuleItem 的 `antecedent` 保留原始 RuleTerm 及其 Type，`consequents` 保留后件。含 RuleInstance 前件的动态 IR 使用版本 3 / `TPIR3`；纯 Bool 蕴含仍使用版本 2，读取兼容 `TPIR1/2`。源码 Rule 序列化的运行时重连限制不变；动态 IR 若只声明实例参数，则仍可跨进程恢复，实例值由调用方重新绑定。自定义 evaluator 必须区分 Bool 值与 RuleInstance 的成立性检查，不能把实例当作普通 Bool。

RuleInstance 前件示例（无需手动调用 checker）：

```tapas
let docs_positive_premise = rule (value: Int) { value > 0 }
let docs_guarded = rule (value: Int) {
    (docs_positive_premise(value)) implies { value < 10 }
}
assert(docs_guarded(-1))
assert(docs_guarded(5))
```

这表示 $Holds(R(p)) \\Rightarrow Q$。前件表达式只计算一次，产生的实例检查一次；不成立时跳过后件，不将前件的 violation 合并进外层结果。这与独立项目 `R(p)` 要求子规则成立不同。前件求值错误仍报告运行时错误；递归前件有深度保护。

原始前件实例仍可通过 IR 来源读取；默认 checker 使用已缓存的本次成立结论，不因生成结果记录再检查一次。显式调用 evaluator 读取源码 Term 可能重新执行源码 checker，并非缓存查询。

裸 Rule 值不是合法前件，即使它没有参数也要写 `R()`。后件仍只接受 Bool；RuleInstance 不获得全局 Bool 转换；Rule 内的逻辑表达式按下节规则检查成立性。实例否定使用下节的`not`；跨表达式共享检查结果时应显式保存 Bool 结论。

### 2.5 逻辑否定 `not`

在 Rule 自身的表达式中，`not`接受 Bool、RuleInstance 或两者的联合 Type，结果为 Bool。普通函数中的`not`仍只接受 Bool；在 Rule 中定义或调用普通函数不会改变该函数的语义。裸 Rule 即使无参数，也必须先写成实例`R()`。

```tapas
let Positive = rule (value: Int) { value > 0 }
let NonPositive = rule (value: Int) { not Positive(value) }
let Contract = rule (value: Int) {
    '非正值的下界': not Positive(value) implies { value >= -10 }
}
assert(NonPositive(-1))
assert(Contract(1))
```

`not R(x)`否定子规则的完整检查结论。如果 R 包含多个合取条件，只要至少一项不成立，否定就成立；不是把每个条件分别取反再合取。空规则成立，所以它的否定不成立。

构造外层 RuleInstance 不会执行检查。只有 checker 执行到`not`时才求值操作数；对实例执行一次完整检查，再取反其`passed`。`not not R(x)`只检查 R 一次，外层取反的是 Bool。短路仍有效：`true or not R(x)`不会构造或检查 R(x)。

求值错误、循环或过深的检查依赖、不支持的 Term/Extension 仍然报错，不会被当作“不成立”。子实例的不通过记录不并入外层 violations；独立否定条件不成立时记录外层 Condition，否定前件为假时蕴含不触发。默认 checker 检查子规则的全部条件，因此其中发生的错误不会被此前的 false 掩盖。

否定结果是 Bool，可以用作普通 Condition、蕴含前件或后件、`and`/`or`的操作数及 Rule 局部`let`的初始化值。重复出现的`not R(x)`分别执行；需要复用一次检查结果时，可在 Rule 内写`let rejected = not R(x)`。普通函数与`if`仍不接受实例的隐式 Bool 转换；Rule 内的`and`、`or`见下节。

IR 用`Not` Term 表示否定，`type`为 Bool，`arguments`恰有一个原始操作数 Term，`payload`为 nil；不把实例操作数预先替换成 Bool。源码表达式中的否定保留该结构，其余源码表达式仍可用`Construct`表示，并通过`arguments`保留包含的否定子表达式。源码局部绑定仍沿用已有 checker/捕获机制，不保证完整的数据流展开。

动态构造写为`rules::negation(operand)`，operand 为 Bool/RuleInstance Term；AnyType Term 允许在检查时验证实际值。结果可交给`rules::condition`或`rules::implication`。例如：

```tapas
let premise = rules::parameter('premise', types::RuleInstance)
let rejected = rules::negation(premise)
let Reject = rules::make('Reject', [premise], [rules::condition(rejected)])
assert(Reject(Positive(-1)))
```

包含`Not`的 IR 使用版本 4 / `TPIR4`，读取兼容`TPIR1/2/3`；不含新节点的 IR 保留原有版本规则。动态 IR 可跨进程恢复并重新绑定实例，源码 Rule 仍依赖创建它的运行时闭包。

自定义 evaluator 必须解释`Not`为 Bool 取反或实例成立性的否定；不能把 RuleInstance 当作 Bool，也不能把 Unsupported/Failed 当作逻辑 false。不支持该语义时应明确返回 Unsupported。`evaluators::value`读取源码 Not 的本次 checker 结果，不额外检查子实例；读取其操作数可得到原始实例。每次调用该 API 仍可能重新执行源码 checker，不承诺跨调用缓存；请求被短路跳过的源码否定子项会报错。


### 2.6 逻辑组合 `and` / `or`

Rule 自身表达式中的`and`、`or`接受 Bool、RuleInstance 及两者的联合类型，结果为 Bool；AnyType 实参在执行时验证。普通函数仍只接受 Bool，即使它由 Rule 调用。`&`、`|`没有获得实例语义。

```tapas
let docs_in_range = rule (x: Int) { x >= 0; x <= 10 }
let docs_override = rule (x: Int) { x == 42 }
let docs_allowed = rule (x: Int) {
    docs_in_range(x) or docs_override(x)
    not (x < 0)
    (docs_in_range(x) and not docs_override(x)) implies { x <= 10 }
}
assert(docs_allowed(5))
assert(docs_allowed(42))
```

操作数从左到右求值。`and`左侧不成立时跳过右侧；`or`左侧成立时跳过右侧，包括跳过实例构造。每个实际执行到的实例检查一次，保留求值错误；错误不会被当作 false 后尝试另一分支。实例只在外层检查执行到该表达式时才被检查。优先级保持`not > and > or`（比较仍比 not 紧）。

两行裸实例是两项独立要求，会分别检查并收集子违规；`R(x) and S(x)`是一个短路 Condition。组合失败记录外层条件，子违规不直接并入外层；成功的`or`不会因为某个分支不成立而留下 violation。逻辑表达式也可用于蕴含前后件、局部`let`及更大的 Bool 表达式；蕴含后件本身仍要求 Bool。

IR 使用`And`、`Or` Term，Bool Type、nil payload、恰好两个有序操作数。不可把它们改写为无序集合或使用会预先计算两个参数的旧 Intrinsic。源码 IR 保留逻辑节点与原始实例操作数；局部绑定仍使用既有 checker 机制。`evaluators::value`读取源码节点的本次结果不重复检查；读取被短路跳过的源码操作数会报错，不强制执行该分支。

动态接口为`rules::conjunction(left, right)`、`rules::disjunction(left, right)`，操作数是 Term。含 And/Or 的 IR 使用版本 5 / TPIR5，并兼容读取 TPIR1–4。不含新节点时保留原有版本规则。自定义 evaluator 必须保留顺序、短路、实例检查和错误传播；不支持时返回 Unsupported。源码字节码需要配套的新运行时。


## 3. 检查接口

### 3.1 `assert`

`assert(rule: RuleInstance | Rule) -> Nil`是根内建函数，不是关键字。
它接受 RuleInstance，或不需要参数的 Rule。
成功时返回`nil`；第一个失败 Condition 产生不可恢复的运行时错误。
`assert`在所有构建模式中执行。

函数契约只使用一次时，默认直接写匿名 Rule：

```text
function withdraw(account, amount)
{
    assert(rule {
        "amount must be positive":
            amount > 0

        "insufficient balance":
            account.balance >= amount
    })

    account.balance = account.balance - amount
}
```

### 3.2 `rules::check`

`rules::check(rule: RuleInstance | Rule) -> CheckResult`执行相同语义，但把失败作为普通值返回，并按顺序收集 violation。

| 字段 | 含义 |
|---|---|
| `status` | 当前完成的检查为 `Success` |
| `passed` | 是否没有 Condition 失败 |
| `violations` | 失败 Condition 的有序列表 |
| `diagnostics` | 求值、结构或 evaluator 诊断 |

Condition 为`false`时，`status`仍为`Success`，`passed`为`false`。
当前`rules::check`遇到参数、环境、Term 求值或不支持的 Extension 时产生普通运行时错误，尚不返回`Unsupported`或`Failed`记录。
程序契约使用`assert`；需要展示、记录或继续处理 Condition failure 时使用`rules::check`。

## 4. Rule Type

Rule 的精确 Type 只记录有序参数 Type，不记录参数名称、Condition 或返回 Type：

| 形式 | 含义 |
|---|---|
| `Rule`、`RuleInstance` | 任意 Rule 或实例 |
| `Rule[T, ...]` | 具有指定参数 Type 的 Rule |
| `RuleInstance[T, ...]` | 已绑定指定参数 Type 的实例 |
| `InstanceOf[R]` | 由同一个运行时规则 R 绑定产生的实例，自动推导参数签名；不保证约束成立 |
| `types::rule(Type...)` | 构造精确 Rule Type 值 |
| `types::rule_instance(Type...)` | 构造精确实例 Type 值 |

零参数精确 Type 写成`Rule[]`和`RuleInstance[]`。
参数 Type 按不变规则比较；名称和说明不参与 Type 相等。
每个精确 Rule Type 都可赋给宽泛`Rule`，实例同理。

```text
let IntRule = types::rule(types::Int)
let Positive: IntRule = rule (value: Int) {
    value > 0
}
let instance: RuleInstance[Int] = Positive(1)
```

## 5. `rules` 包

### 5.1 查看公共 IR

`rules::inspect(rule: Rule | RuleInstance) -> RuleIR`返回不可变 IR。
读取函数返回独立 List，修改这些 List 不会改变原始 IR。

| Type | 作用 |
|---|---|
| `RuleIR` | 完整规则表示 |
| `Parameter`、`Capture` | 参数和捕获描述 |
| `Item`、`Condition`、`Requirement` | 有序 Rule 项目 |
| `Term`、`term(Type)` | 无类型或精确结果 Type 的 Term |
| `Origin` | 源码范围 |
| `CheckResult`、`Violation`、`Diagnostic` | 检查结果 |

以上名称均通过`rules::`访问。
常用读取函数为：

| 接口 | 结果 |
|---|---|
| `parameters(ir)` | 按签名顺序返回 Parameter |
| `items(ir)` | 按执行顺序返回 Item |
| `item(rule, target)` | 按整数位置、说明文字或 Item 身份取得直接 Item |
| `terms(ir)` | 返回 Term 表 |
| `origin(value)` | 返回 IR 值的来源 |
| `semantic_hash(value)` | 忽略显示信息的语义哈希 |
| `content_hash(value)` | 包含说明和来源的内容哈希 |

RuleIR 可读取`display_name`、`source`、`version`、`parameters`、`captures`、`terms`、`items`和`origins`。
Term 可读取`id`、`kind`、`type`、`arguments`、`payload`、`provider`和`version`。
Item 可读取`kind`、`term`、`rule`、`arguments`和`description`。

基础 Term kind 为`Constant`、`Parameter`、`Capture`、`Intrinsic`、`Call`、`Construct`、`Convert`、`Extension`、`Not`、`And`和`Or`。
工具应依赖这些公开字段，而不是内部指针。

### 5.2 动态构造与序列化

| 接口 | 作用 |
|---|---|
| `parameter(name, type)` | 创建 Parameter |
| `constant(value)` | 创建 Constant Term |
| `call(function, arguments)` | 创建 Call Term |
| `conjunction(left, right)` | 创建从左到右短路的 And Term |
| `disjunction(left, right)` | 创建从左到右短路的 Or Term |
| `negation(operand)` | 创建 Bool/RuleInstance 成立性否定的 Not Term |
| `condition(term[, description])` | 创建 Condition |
| `implication(antecedent, consequents[, description])` | Bool/RuleInstance 前件 Term 与非空 Bool 后件 Term 列表构成蕴含 |
| `requirement(rule, arguments)` | 创建 Requirement |
| `extension(provider, kind, arguments, payload)` | 创建扩展 Term |
| `make(display_name, parameters, items)` | 验证并创建 Rule |
| `serialize(rule_or_ir)` | 生成规范 String |
| `deserialize(data)` | 验证并恢复 Rule |

```text
let value = rules::parameter("value", types::Int)
let lower = rules::constant(0)
let upper = rules::constant(10)

let InRange = rules::make("InRange", [value], [
    rules::condition(value >= lower, "value must be non-negative"),
    rules::condition(value <= upper, "value must be at most ten"),
])
```

每次`parameter`调用都产生不同身份。
`make`验证参数列表、Bool Condition、Requirement 签名、Term 引用和 Term 图循环。
源码 Rule 和动态 Rule 使用同一种 RuleIR，可由同一 checker 和 evaluator 消费。

动态 Rule 的序列化是自包含的。
源码 Rule 依赖 checker 和捕获环境，因此序列化包含运行时重连信息；离开创建它的运行时或无法恢复闭包时，`deserialize`会失败，不会返回语义不完整的 Rule。
无法稳定编码的调用目标或 payload 也会被拒绝。

## 6. `evaluators` 包

Evaluator 为同一 RuleIR 提供其他解释方式，例如文档提取、符号验证、测试生成或优化。
它不能改变项目顺序或基础 Tapas 运算语义。

| Type | 作用 |
|---|---|
| `Evaluator` | evaluator 值 |
| `Context` | 一次求值的只读上下文 |
| `Result` | 统一结果结构 |
| `Diagnostic` | evaluator 诊断 |

以上名称均通过`evaluators::`访问。

| 接口 | 作用 |
|---|---|
| `make(name, version, evaluate[, compile])` | 创建 evaluator |
| `eval(instance, evaluator)` | 建立 Context 并调用 evaluate |
| `compile(rule, evaluator)` | 调用可选 compile 处理器 |
| `binding(context, parameter_or_index)` | 读取参数绑定 |
| `capture(context, capture_or_index)` | 读取捕获值 |
| `value(context, term)` | 按普通 Tapas 语义求值 Term |
| `requirement(context, requirement)` | 创建子 RuleInstance |

evaluate 处理器接收 Context，返回具有`status`、`value`、`violations`和`diagnostics`字段的结果。
compile 处理器可省略；未提供时调用`compile`返回`Unsupported`。

```text
let observer = evaluators::make("observer", 1, (context)
{
    return {
        "status": "Success",
        "value": evaluators::binding(
            context, context["ir"]["parameters"][0]),
        "violations": [],
        "diagnostics": [],
    }
})
```

Evaluator 遇到合法但无法解释的 Term 或 provider 时应明确返回`Unsupported`，不能静默改变含义。
Context 只读，不能修改 IR、参数绑定或捕获环境。

## 7. 执行模型

直接读取声明与绑定值时，不需要创建 evaluator：

```tapas
let ReflectionExample = rule (amount: Int) { amount > 0 }
let reflection_instance = ReflectionExample(-1)
let reflection_parameters = parameters(ReflectionExample)
let reflection_arguments = arguments(reflection_instance)
```

`reflection_parameters` 是由参数名与 Type 组成的 Pair 列表，例如
`[pair('amount', types::Int)]`；`reflection_arguments` 是 `[-1]`。
这些查询不会运行规则，因此也能读取不满足约束的实例。`parameters` 接受 Function、Rule、
RuleInstance 或 RuleIR；`arguments` 只接受已绑定的 RuleInstance。无参数时返回空列表，
不支持的对象报错，不以空列表伪装成功。普通函数、匿名函数、闭包与函数副本保留参数名和 Type；未标注参数使用可获得的上下文 Type，否则为 AnyType。标准库函数读取已登记的签名。旧字节码函数需要重新编译；没有参数元信息的第三方原生函数明确报错。

`parameters` 查询的是声明，不是完整的调用规则：`(...)` 没有具名形参，返回空列表，但它仍是变参函数；返回的 Pair 不表示可选参数、默认值或变参标志。

返回列表是新容器；修改它不会替换原绑定或声明。绑定值中的可变对象仍共享引用，
不是深快照。`parameters` 返回的 Type 是 Type 对象而非字符串。
它不同于 `rules::parameters(ir)`：后者保留原有的 `List[Parameter]` IR 接口，供分析工具使用。

```text
Rule literal
    -> immutable RuleIR + checker
    -> bind capture environment
    -> Rule
    -> bind arguments
    -> RuleInstance
    -> assert / check / evaluator
```

源码 Rule 编译为共享 IR 和 checker，运行时再绑定捕获环境。
动态 Rule 没有源码专用 checker，使用同一 IR 的通用路径。
公共语义不规定 checker 是解释执行、提前编译还是缓存；优化不得改变项目顺序、闭包读取、错误位置或循环检测。

## 8. 文法

```ebnf
rule-literal = "rule",
               [ "(", [ rule-parameters ], ")" ],
               rule-block ;

rule-parameters = rule-parameter,
                  { ",", rule-parameter }, [ "," ] ;

rule-parameter = IDENTIFIER, ":", type-expression ;

rule-block = "{", separators, [ rule-item-list ], separators, "}" ;

rule-item-list = rule-item, { separator-run, rule-item } ;

rule-item = let-declaration
          | condition-statement
          | described-condition-statement
          | implication-statement ;

condition-statement = expression ;
implication-statement = [ STRING, ":", separators ],
                        expression, "implies",
                        ( expression | condition-block ) ;

described-condition-statement = STRING, ":", separators,
                                ( expression | condition-block ) ;

condition-block = "{", separators, condition-statement,
                  { separator-run, condition-statement },
                  separators, "}" ;

rule-application = expression ;
```

独立表达式必须产生 Bool 或 RuleInstance；裸 Rule 不会自动实例化。
`rule`是保留字；`assert`、`rules::check`和 evaluator API 使用普通函数调用文法。

## 9. 当前限制

- `assert` 失败后不可恢复；需要普通结果时使用 `rules::check`。
- Requirement 循环在执行路径上检测，不保证都能在编译期发现。
- Extension Term 需要理解对应 provider 和版本的 checker 或 evaluator。
- 源码 Rule 的跨进程序列化受 checker 与捕获环境重连能力限制。
- evaluator 的 compile 处理器可选，不承诺自动生成专用程序。
- Rule IR 是不可变视图；修改规则必须构造新的 Rule。

## 10. 离散点集、整数区间与成员判断

`rules::points(element_type, ...values)` 创建 `rules::PointsOf[T]`，其中 T 是传入的 Type。允许空点集，逐项检查成员类型，并按 `identical` 语义去除重复值。T 可以是任意现有 Type，包括 Enum、结构 Type、容器、函数和规则类型；没有仅限标量的限制。Type 值在表达式中使用 `types::Int` 等现有写法，类型标注使用包限定形式 `rules::PointsOf[Int]`。

`rules::range(start, end)` 创建 `rules::RangeOf[Int]`，表示包含两端的闭整数区间。第一版只接受 Int，拒绝 Float、混合端点及 `start > end`；相同端点表示单点区间。区间不会展开为列表，因此可以表示完整的大整数边界。

```tapas
let domain_color = types::enum('red', 'green', 'blue')
let domain_choices: rules::PointsOf[domain_color] = rules::points(domain_color, 'red', 'blue')
let domain_limits: rules::RangeOf[Int] = rules::range(1, 20)
let domain_rule = rule (color: domain_color, quantity: Int) {
    color in domain_choices
    quantity in domain_limits
}
assert(domain_rule('red', 4))
print(rules::check(domain_rule('green', 4))::passed)
```
<pre class='Tapas-Return'>
false
</pre>

`in` 在 Rule 内外均支持点集和区间；类型不兼容的候选值不属于该集合，返回 false。点集本身不提供修改接口，但其中的复合值沿用 Tapas 的引用语义，不进行深复制：对外部对象的修改仍可影响成员的内容。成员比较沿用 `identical`，不会额外引入结构相等规则。点集列表按构造顺序保留，用于展示，不表示采样偏好。

可读字段：点集有 `type`、`values`，区间有 `type`、`start`、`end`。`values` 返回新列表，但列表成员仍共享引用。`len(points)` 返回去重后的存储项数；`len(range)` 不受支持。`types::of` 保留 `PointsOf[T]` / `RangeOf[Int]`，`types::parameters` 的 `item` 给出元素类型。动态来源的类型标注在运行时检查。

RuleIR 使用 Bool 类型的 `In` Term，两个有序参数分别是待检查值和集合。源码中的 `in` 保留原始操作数及来源；检查按现有成员运算的顺序先求右侧再求左侧，每个操作数求值一次。外层 `and`、`or` 的短路不会求值被跳过的成员表达式。`evaluators::value` 读取源码节点或操作数会运行一次 checker 并读取本次记录，不会再单独求值操作数；读取被短路跳过的节点报错。

动态构造可以使用 `rules::membership(value_term, domain_term)`，或让现有 `in` 的任一操作数为 Term，例如 `parameter in rules::constant(points)`；这时结果是 In Term，交给 `rules::condition` 使用。

含 In、点集/区间常量或其参数类型的 IR 使用 TPIR6，兼容读取 TPIR1–5。整数区间与包含可传输标量/Type 的点集常量支持跨进程序列化。任意对象图、闭包或规则身份等成员继续受现有序列化边界约束，无法传输时明确报错；运行时 points 接受这些值不意味着可以跨进程恢复它们。源码 Rule 仍需在原进程重连 checker 和捕获环境。

## 11. 限制已有 Rule

`rules::restrict(base, ...restrictions) -> Rule` 接受一个 Rule 和零个或多个 `Pair[String : AnyType]`。Pair 的键是参数名；值为具体值时生成 `==` 条件，为 `RangeOf[Int]` 或 `PointsOf[T]` 时生成 `in` 条件。

```tapas
let restrict_base = rule (quantity: Int, approved: Bool) {
    quantity >= 0
    approved
}
let restrict_trial = rules::restrict(restrict_base,
    'quantity': rules::range(1, 10),
    'approved': true,
)
assert(restrict_trial(4, true))
```

返回值是具有新身份的普通 Rule，保留原参数名、类型及顺序，调用时仍需提供全部参数。原 Rule 不变；新 Rule 通过 Requirement 引用它，保留其 checker、闭包和诊断。附加条件以参数名作为描述。零个限制得到等价的新 Rule；重复参数限制取交集，冲突或空点集可以构造，但没有满足这些限制的实例。该函数不会调用求解器检查可行性。

未知参数名、非字符串键及不匹配的具体值或点集成员会报错。第一版区间限制用于 Int 或 AnyType 参数；空点集表示空域。仅支持顶层参数名，不把点号解释为字段路径。返回规则包含原 Rule 引用，因此跨进程序列化仍受 Rule 常量的现有限制。

## 12. 选择与变换 Rule Item

`rules::item(rule, target)`、`rules::drop(rule, target)`、`rules::violate(rule, target)`、`rules::drop_if(rule, target, premise)`和`rules::violate_if(rule, target, premise)`使用相同的 target：

- Int 是`rules::items(rule)`中从 0 开始的位置；
- String 与直接 Item 的`description`完全匹配，找不到或匹配多项时明确报错；
- Item 必须直接属于输入 Rule，以对象身份匹配。

```tapas
let transform_bounds = rule (value: Int) {
    '下界': value >= 0
    '上界': value <= 10
}

let lower_item = rules::item(transform_bounds, '下界')
let without_lower = rules::drop(transform_bounds, lower_item)
let below_lower = rules::violate(transform_bounds, 0)

assert(without_lower(-1))
assert(below_lower(-1))
assert(rule { not below_lower(0); not below_lower(11) })
```

`drop`删除所选 Item；`violate`保留其他直接 Item，并把目标 Item 替换为其成立性的否定。Condition `C`变为`not C`；Requirement `Child(...)`变为`not Child(...)`；`A implies B`的违反等价于`A and not B`。多个 implication 后件作为一个合取整体取反。

`drop_if`和`violate_if`的第三个参数是与原 Rule 参数签名相同的 premise Rule，并自动使用同一组参数调用。设 premise 的成立性为 `P`，目标 Item 的成立性为 `t`：

- `drop_if`以`P or t`替换目标；`P`成立时不再限制`t`，`P`不成立时`t`必须成立；
- `violate_if`以`(P or t) and (not P or not t)`替换目标；`P`成立时`t`必须不成立，`P`不成立时`t`必须成立。

```tapas
let negative = rule (value: Int) {
    value < 0
}
let conditional_bounds = rules::violate_if(
    transform_bounds,
    '下界',
    negative,
)

assert(conditional_bounds(-1))
assert(conditional_bounds(0))
```

premise 的参数数量或对应类型不同会在构造时明确报错。premise 保留自己的闭包；派生 Rule 每次检查或求解时读取其当前捕获值。

返回值是参数签名相同、身份不同的普通 Rule，原 Rule 不变。源码 Rule 的 checker 与闭包由派生 Rule 保留，因此调用和求解都在使用时读取捕获值。变换只作用于直接 Item，不递归搜索 Requirement；要精准违反子 Rule 的某项，应直接变换该子 Rule。说明文字不是稳定标识，容易变化或重复的代码应先通过整数位置取得 Item，再传递 Item 对象。

## 13. 源码表达式的结构化 IR

源码 Rule 在生成 checker 的同时，递归生成表达式图。参数引用复用 Parameter 身份，外部变量引用关联闭包槽位的 Capture；不会在定义 Rule 时读取或固定捕获值。

```tapas
var ir_minimum = 3
let ir_example = rule(x: Int) {
    let y = x + 1
    y >= ir_minimum
}
let ir_comparison = rules::inspect(ir_example)::items[0]::term
print(ir_comparison::kind)
print(ir_comparison::payload)
assert(ir_example(2))
ir_minimum = 5
assert(rule { not ir_example(2) })
```
<pre class='Tapas-Return'>
Intrinsic
>=
</pre>

此处条件是 `Intrinsic(">=", Local(y), Capture(ir_minimum))`；Local 的初始化表达式是 `Intrinsic("+", Parameter(x), Constant(1))`。`Local` 使用 `Construct` 节点表示，payload 为 `local:<绑定编号>:<名字>`，唯一操作数为初始化表达式。多个使用点共享初始化图，而不反复展开或执行初始化代码。

| 表达式 | 节点与操作数 |
|---|---|
| 标量字面量 | Constant，payload 是实际的 Int、Float、Bool、String 或 Nil 值 |
| 参数 / 外部变量 | Parameter / Capture，使用绑定身份而非运行时名字查找 |
| 算术、比较、Pair | Intrinsic，payload 是运算符；操作数按源码左右顺序保存 |
| 正负号 | Intrinsic，payload 为 `pos` / `neg` |
| `and` / `or` / `not` / `in` | 对应的 And / Or / Not / In |
| `value::field` | Intrinsic `member`，操作数为接收者和字段名 Constant |
| `values[index]` | Intrinsic `index`，操作数为接收者和索引表达式 |
| 列表 / 字典 | Construct `list` / `dictionary`；字典操作数为交替的键和值 |
| 切片 | Construct `slice:...`，标明端点是否省略，保留存在的端点表达式 |
| 普通函数或 Rule 调用 | Call，payload 是调用目标 Term，arguments 是实参表达式 |
| 隧道调用 `x.f(...)` | Call 的目标为 Construct `tunnel:f`，包含实际函数绑定和接收者，随后按隧道语义应用实参 |
| `to` 转换 | Convert，保留左右两个操作数 |

函数和 Rule 字面量引入新的词法作用域，当前保留为显式 Extension `opaque:...`；`this` 等未解析的特殊引用保留为 Extension `reference:...`。这些节点明确标识分析边界，不应由 solver 猜测其含义。调用已捕获的 Rule 则会完整保留其目标绑定和实参图，可以在查询准备阶段读取该 Rule 并递归分析。结构化 IR 并不意味着任意函数已经能被求解器翻译。

`evaluators::capture(context, capture)` 和 `evaluators::value(context, capture)` 直接读取当前 Rule 闭包中的值，不执行 checker。C 接口 `trule_read_capture` 提供相同的受身份校验的读取能力，供后续查询准备阶段使用；它不建立快照。

对有运行时记录的源码操作表达式，`evaluators::value` 运行一次 checker 后读取该表达式的记录，不再单独执行表达式，因此保持既有求值顺序和函数调用次数。短路跳过的表达式没有记录，读取时报错。每次 `evaluators::value` 调用是独立的求值请求；不会跨请求缓存 checker 的结果。参数和捕获节点直接读取其绑定，合成的字段名 Constant 直接返回其值；切片描述、隧道目标等合成节点没有独立求值记录。

新源码表达式图使用 TPIR7，兼容读取 TPIR1–6。源码 Rule 的序列化仍遵循既有的同进程闭包重连限制。这层提供可分析结构及上下文读取接口；`solve` 包负责查询准备、后端翻译和求解。

## 14. 可行性查询

`solve::hold(Rule | RuleInstance)` 已提供可运行的 CP-SAT 后端。它在调用时读取上下文，返回状态和通过 checker 复核的见证实例。solve 默认把输入 Rule 的 Int 参数限制在闭区间 ±10¹²，可通过 `TAPAS_SOLVE_INT_MIN/MAX` 配置。该域在 presolve 前生效；结果 `scope` 为 `configured`，`unsat` 仅表示所配置域内无解，不改变语言 Int 的语义。接口、依赖安装、支持范围及可运行示例见 [solve 示例](../examples/solve/README.md)。
