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
    require docs_positive(value)
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
| `require` | Rule 体语句 | 组合另一个 Rule 应用 |
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
```

零参数 Rule 可以省略参数列表。
Rule 体最外层的 Bool 表达式形成 Condition；所有 Condition 都必须成立，空 Rule 恒成立。

说明冒号左侧必须是 String。
右侧可以是单个 Bool 表达式，也可以是说明块；块中每个表达式分别形成 Condition，并共享说明。
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
Rule 项目只允许局部`let`、Condition 和`require`；赋值、`var`、控制流、导入和直接 IO 不能作为项目。
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

### 2.3 `require`

`require`只能出现在 Rule 体最外层，后面必须是 Rule 应用：

```text
let Positive = rule (value: Int) {
    value > 0
}

let SmallPositive = rule (value: Int) {
    require Positive(value)
    value < 10
}
```

项目严格按源码顺序执行。
Requirement 使用目标 Rule 自己的签名、IR 和捕获环境。
同一 Rule 在不同位置被引用时分别执行。
执行器维护当前 Requirement 路径；同一 RuleInstance 再次出现在当前路径中时报告循环依赖，而不是无限递归。

## 3. 检查接口

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
| `terms(ir)` | 返回 Term 表 |
| `origin(value)` | 返回 IR 值的来源 |
| `semantic_hash(value)` | 忽略显示信息的语义哈希 |
| `content_hash(value)` | 包含说明和来源的内容哈希 |

RuleIR 可读取`display_name`、`source`、`version`、`parameters`、`captures`、`terms`、`items`和`origins`。
Term 可读取`id`、`kind`、`type`、`arguments`、`payload`、`provider`和`version`。
Item 可读取`kind`、`term`、`rule`、`arguments`和`description`。

基础 Term kind 为`Constant`、`Parameter`、`Capture`、`Intrinsic`、`Call`、`Construct`、`Convert`和`Extension`。
工具应依赖这些公开字段，而不是内部指针。

### 5.2 动态构造与序列化

| 接口 | 作用 |
|---|---|
| `parameter(name, type)` | 创建 Parameter |
| `constant(value)` | 创建 Constant Term |
| `call(function, arguments)` | 创建 Call Term |
| `condition(term[, description])` | 创建 Condition |
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
          | require-statement ;

condition-statement = expression ;

described-condition-statement = STRING, ":", separators,
                                ( expression | condition-block ) ;

condition-block = "{", separators, condition-statement,
                  { separator-run, condition-statement },
                  separators, "}" ;

require-statement = "require", rule-application ;
rule-application = expression ;
```

`require`的表达式必须产生 RuleInstance。
`rule`与`require`是保留字；`assert`、`rules::check`和 evaluator API 使用普通函数调用文法。

## 9. 当前限制

- `assert` 失败后不可恢复；需要普通结果时使用 `rules::check`。
- Requirement 循环在执行路径上检测，不保证都能在编译期发现。
- Extension Term 需要理解对应 provider 和版本的 checker 或 evaluator。
- 源码 Rule 的跨进程序列化受 checker 与捕获环境重连能力限制。
- evaluator 的 compile 处理器可选，不承诺自动生成专用程序。
- Rule IR 是不可变视图；修改规则必须构造新的 Rule。
