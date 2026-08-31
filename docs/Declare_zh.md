# Tapas `declare` 与声明中间语言设计

本文提出 Tapas 的一个语言发展方向：增加具名 `declare` 语句，把声明体中的普通 Tapas 表达式转换为带类型、已完成名称解析、可以在运行时作为普通值操作的声明中间语言（Declaration IR）。

本文是设计提案，不是当前语言规范。
示例中的 `declare`、`compile`、`eval` 和 `declarations` 包尚未实现，具体名称和接口需要经过原型验证后才能进入正式规范。

## 1. 目标

`declare` 的核心目标不是增加一套优化建模语法，而是为 Tapas 增加一种受控的元编程能力：

> 普通代码用于计算值；`declare` 用于把代码提升为可检查、可组合、可改写、可翻译和可执行的声明值。

同一份声明以后可以被不同解释器用于：

- 检查具体对象是否满足声明；
- 生成满足声明的测试状态；
- 传播未知值的取值范围；
- 搜索边界场景和反例；
- 编译为普通检查函数或其他目标表示；
- 生成说明文档；
- 记录失败原因并缩减失败案例。

语言表面应优先帮助读者理解程序表达的领域意图。
名称解析、符号 ID、IR 节点、求解器变量和后端能力等实现细节不应进入普通声明代码。

## 2. 非目标

第一阶段不追求：

- 对任意 Tapas 程序进行自动反向求值；
- 把有副作用的普通代码自动变成可传播约束；
- 用字符串形式的源码实现不受限制的 `eval`；
- 在核心语法中加入 `variable`、`constraint`、`minimize` 等优化专用概念；
- 让普通用户直接操作解析器产生的原始 AST；
- 一开始就支持循环、赋值、异常和任意控制流的声明化；
- 将某个特定求解器的数据结构规定为 Tapas 的语言语义。

优化、验证、测试生成和文档输出都是 Declaration IR 的潜在解释方式，不是 `declare` 本身的含义。

## 3. 核心表面语法

具名声明采用以下格式：

```tapas
declare InRange(x: Int) {
    "x 的范围在 0 到 10 之间":
        x >= 0 and x <= 10
}
```

形式语法可以表示为：

```ebnf
declare-statement = "declare", IDENTIFIER,
                    "(", [ declare-parameters ], ")",
                    declare-block ;

declare-parameters = declare-parameter,
                     { ",", declare-parameter }, [ "," ] ;

declare-parameter  = IDENTIFIER, ":", type-expression ;

declare-block      = "{", separators,
                     [ declare-item-list ],
                     separators, "}" ;

declare-item-list  = declare-item,
                     { separator-run, declare-item } ;

declare-item       = let-declaration
                   | proposition-expression
                   | annotated-proposition
                   | declaration-application ;

annotated-proposition = STRING, ":", separators,
                        proposition-expression ;
```

在 `annotated-proposition` 中，冒号后可以直接写命题，也可以换行并缩进后再写。
换行只改善阅读，不引入基于缩进的块结构。

`declare Name(...) { ... }` 在当前词法作用域建立一个只读名称。
读取 `Name` 得到第一类 `Declaration` 值；它可以保存到容器、作为参数传递、从模块导出或交给元编程接口。

第一阶段要求公开参数具有显式 Type。
声明 IR 需要稳定、完整的参数签名，不能依赖某个 evaluator 猜测参数类型。
以后可以在类型推断足够稳定时放宽这一要求。

名称是否使用大写开头只是风格约定，不影响语义。

## 4. 声明体的含义

`declare` 与普通代码共享 Tapas 的字面量、名称、调用、索引、成员、容器、算术、比较和逻辑表达式。
它不建立另一套表达式语言。
不同之处在于：声明体不会立即生成普通求值字节码，而会被降低为 Declaration IR。

### 4.1 顶层布尔表达式声明其结果成立

```tapas
declare Positive(x: Int) {
    x > 0
}
```

`x > 0` 的表面 Type 是 `Bool`。
它位于声明体顶层，因此被记录为一项应当成立的命题，而不是立即计算并丢弃的布尔值。

同一声明体中的多项顶层命题共同成立，逻辑上相当于合取：

```tapas
declare InRange(x: Int) {
    x >= 0
    x <= 10
}
```

等价于：

```tapas
declare InRange(x: Int) {
    x >= 0 and x <= 10
}
```

### 4.2 字符串为命题提供读者说明

```tapas
declare ValidQuantity(quantity: Int) {
    "数量必须为正数":
        quantity > 0

    "单笔订单不能超过 100 件":
        quantity <= 100
}
```

`"说明": proposition` 复用 Tapas 现有的 Pair 表达式形状，但在声明体顶层具有明确的上下文含义：为命题附加人类可读说明。
字符串不是机器 key，不参与命题真假，也不要求全局唯一。

编译器为命题维护内部节点 ID 和源码位置。
实现不能迫使用户为了工具需求编写 `range_x:` 一类低信息量名称。

第一阶段一条说明对应一个命题表达式。
需要同时说明多个条件时，可以使用普通逻辑：

```tapas
"x 的范围在 0 到 10 之间":
    x >= 0 and x <= 10
```

### 4.3 `let` 为声明表达式命名

```tapas
declare CorrectTotal(
    price: Float,
    quantity: Int,
    total: Float,
) {
    let subtotal = price * quantity

    "总价等于单价乘以数量":
        total == subtotal
}
```

在表面类型系统中，`subtotal` 的 Type 仍然是 `Float`，而不是用户可见的 `Expr<Float>`。
在 Declaration IR 中，它引用 `price * quantity` 对应的带类型 Term 节点。

`let` 只建立局部名称和共享表达式，不自动增加命题：

```tapas
declare Example(x: Int) {
    let positive = x > 0
}
```

上例没有断言 `positive` 成立，未使用的局部 Term 可以被消除。
要记录命题，应写：

```tapas
declare Example(x: Int) {
    let positive = x > 0
    positive
}
```

`let subtotal = price * quantity` 也不会创建一个能够独立取值的逻辑变量。
如果 `subtotal` 是需要从外部传入、观察或反向推导的领域对象，应将其写入声明参数：

```tapas
declare CorrectSubtotal(
    price: Float,
    quantity: Int,
    subtotal: Float,
) {
    subtotal == price * quantity
}
```

### 4.4 声明可以组合

```tapas
declare PositiveQuantity(order: Order) {
    order.quantity > 0
}

declare EnoughInventory(order: Order, inventory: Inventory) {
    inventory.quantity >= order.quantity
}

declare ValidOrder(order: Order, inventory: Inventory) {
    PositiveQuantity(order)
    EnoughInventory(order, inventory)
}
```

在普通执行环境中，应用一个 `Declaration` 得到 `DeclarationInstance`。
在另一个声明体的顶层应用声明，则表示把该实例包含的命题纳入当前声明，并保留来源映射。

### 4.5 第一阶段不允许的声明项

第一阶段的声明体不支持：

- `var`；
- 赋值；
- `return`；
- `break` 和 `continue`；
- 依靠可变状态的 `for` 和 `while`；
- 导入语句；
- 具有外部副作用的调用。

条件关系使用布尔逻辑表达，而不是把执行控制流暗中改写成逻辑。
例如，“加急订单必须有足够库存”应表示为一个蕴含 Term。
Tapas 是否增加 `implies` 运算符属于独立语法决策；在此之前可以由声明库提供等价的纯逻辑构造。

禁止这些项目不是因为 Declaration IR 永远不能表达它们，而是为了保证读者能够判断哪些代码在构造声明、哪些代码会产生运行时副作用。

## 5. `declare` 是降低边界，不是原始 AST 引用

编译流程建议分为：

```text
源码
  -> lossless tokens
  -> 解析 AST
  -> 词法作用域与名称解析
  -> Type 检查与调用解析
  -> effect / evaluator capability 检查
  -> Typed Declaration IR
```

Declaration IR 不应直接保存原始 AST，原因包括：

- 原始 AST 中的名称仍然是文本，组合后容易发生捕获；
- 语法上不同的表达式可能具有相同语义；
- 运算符重载和函数调用尚未确定目标；
- Type、隐式转换和错误位置尚未固定；
- 解析 AST 会随着表面语法演化，不适合作为长期运行时协议。

因此，`declare` 保存的是 AST 经名称解析和 Type 检查后的语义表示。
它可以保留源码形状和注释元数据用于展示，但 evaluator 不应重新解析名称或猜测 Type。

## 6. Declaration IR

IR 应是不可变、可共享、可序列化并带版本号的图结构。
以下结构只描述逻辑模型，不限定 C 实现中的具体内存布局。

### 6.1 核心标识

```text
DeclarationId   声明定义的稳定内部标识
SymbolId        参数、局部绑定、捕获值和被调名称的解析结果
TypeId          对应一个规范化 Tapas Type
TermId          Declaration 内一个带类型表达式节点
PropositionId   一个应当成立的 Bool Term
SourceId        文件、源码范围和展开来源
```

IR 中的引用使用 ID，不使用名称字符串。
名称字符串只用于展示、反射和诊断。

### 6.2 Term

每一个 Term 都必须携带确定的 `TypeId`：

```text
Term {
    id: TermId
    type: TypeId
    kind: TermKind
    source: SourceId
}
```

第一阶段至少需要以下 Term：

```text
Constant(value)
Parameter(symbol)
Capture(symbol, value)
Unary(operator, operand)
Binary(operator, left, right)
Call(callee, arguments, effect, capabilities)
Index(receiver, indices)
Member(receiver, resolved_member)
List(items)
Pair(first, second)
Dictionary(entries)
Convert(value, target_type)
```

逻辑与比较仍然是带 `Bool` Type 的 Term：

```text
Equal(left, right)       : Bool
LessThan(left, right)    : Bool
And(left, right)         : Bool
Or(left, right)          : Bool
Not(value)               : Bool
```

这样不必再为优化建模发明一套 `ConstraintExpression` 类型层级。
一个顶层 `Term<Bool>` 被声明为成立时，才形成 Proposition。

### 6.3 Proposition 与说明

```text
Proposition {
    id: PropositionId
    condition: TermId       // 必须是 Bool
    description: String?    // 面向读者
    source: SourceId
    origin: OriginChain     // 组合、展开和改写来源
}
```

`description` 不充当内部 ID。
修改说明文字不应改变声明的逻辑含义。

### 6.4 Declaration

```text
Declaration {
    id: DeclarationId
    name: String
    parameters: [Parameter]
    captures: [Capture]
    terms: TermArena
    propositions: [Proposition]
    dependencies: [DeclarationId]
    signature: DeclarationType
    source: SourceId
    ir_version: Int
}
```

局部 `let` 可以作为 Term 图中的共享节点和调试名称保存，不必保留为具有独立运行时身份的变量。

### 6.5 DeclarationInstance

应用声明时不直接执行其命题，而是产生参数替换：

```text
DeclarationInstance {
    declaration: Declaration
    bindings: Dictionary[SymbolId, Value | Term]
    residual_propositions: [Proposition]
}
```

所有参数绑定到具体值时，实例可以交给布尔 evaluator 检查。
参数仍包含未知 Term 或领域对象时，实例可以交给传播、搜索或其他支持残留表达式的 evaluator。

## 7. IR Type 的完备性

这里的“完备”不是指能够证明任意数学命题，而是要求：

> 每一种允许出现在 `declare` 中的表面构造，都能被无歧义地表示、检查、遍历、序列化，并由 evaluator 明确接受或拒绝。

IR 必须满足以下不变量：

1. 每个 Term 都有确定的 Tapas Type；
2. 每个名称引用都已经解析到 `SymbolId`；
3. 每个运算符都已经解析到具体内建操作或可调用目标；
4. 每个调用都带有参数签名、结果 Type 和 effect 信息；
5. 每个 Proposition 的结果 Type 必须是 `Bool`；
6. 每个节点都能追溯到源码或生成它的元程序；
7. 每个 IR 文件都有格式版本和语言版本；
8. evaluator 对不支持的节点返回带来源的能力错误，不能静默改变语义。

Declaration IR 需要覆盖 Tapas 的规范化 Type，包括：

```text
Nil
Bool
Int
Float
String
Time
List<T>
Pair<A, B>
Dictionary<K, V>
Array<Element, Shape?>
Structure<Fields>
Union<Members>
Function<Parameters, Result, Effects>
Declaration<Parameters>
DeclarationInstance
Type
AnyType
```

其中 `Function<...>` 和 `Declaration<...>` 要求类型系统能够表示完整签名，而不只是当前的原始 `types::Function` 类别。
这是实现 `compile()`、安全组合和良好诊断的前置工作。

普通声明代码中的 `x: Int` 和 `x + 1: Int` 不应变成用户可见的 `Term<Int>`。
`Term`、`Proposition` 和 `Symbol` 是反射与高级元编程接口使用的 IR Type；普通代码仍然使用领域 Type。

## 8. 第一类声明与元编程

`declare` 的结果必须是第一类值，否则它只能是一种静态配置语法，不能实现预期的元编程能力。

至少应支持：

```text
inspect(declaration)              读取签名、命题、说明和来源
instantiate(declaration, values)  建立 DeclarationInstance
compose(declarations)             合并声明并保留来源
substitute(declaration, bindings) 进行卫生的符号替换
transform(declaration, visitor)   产生新的不可变声明
serialize(declaration)            保存版本化 IR
```

所有组合和改写必须基于 `SymbolId`，不能通过拼接名称字符串实现。
元程序生成的新节点需要记录生成来源，以便诊断同时指出“生成代码的位置”和“最终声明的位置”。

### 8.1 通过语法动态创建

具名 `declare` 可以出现在允许局部声明的词法作用域中，并捕获外部只读值：

```tapas
let BoundedBy = (minimum, maximum) {
    declare Bound(x: Int) {
        x >= minimum
        x <= maximum
    }

    return Bound
}

let Range0To10 = BoundedBy(0, 10)
```

调用 `BoundedBy` 时会得到带捕获常量的新 `Declaration`。
第一阶段只允许捕获可安全冻结和序列化的值；对可变容器、外部资源或本地 C 对象的捕获必须拒绝，除非相应 evaluator 明确支持。

第一阶段只要求具名形式 `declare Name(...) { ... }`。
是否增加匿名 `declare(...) { ... }` 可以等具名声明、捕获和调试信息稳定后再决定。

### 8.2 通过构造 API 动态创建

高级工具、AI、可视化编辑器和外部导入器需要不依赖源码文本地创建同一种 IR：

```tapas
let x = declarations::parameter('x', types::Int)

let InRange = declarations::make(
    'InRange',
    [x],
    [
        declarations::proposition(x >= 0),
        declarations::proposition(x <= 10),
    ],
)
```

构造 API 可以比普通语法更显式、更繁琐，因为它只面向真正需要操作 IR 的元程序。
两条路径必须汇合到同一种规范化表示：

```text
declare Name(...) { ... } ----+
                               +--> Typed Declaration IR
declarations::make(...) -------+
```

不能维护一套“语法声明”和另一套“动态声明”；否则 evaluator、序列化和诊断会逐渐产生不一致。

## 9. `eval()`

`eval()` 只接收已经完成名称解析和 Type 检查的 `Declaration` 或 `DeclarationInstance`，不接收源码字符串：

```tapas
let result = eval(InRange(5), evaluators::boolean)
```

布尔 evaluator 要求所有参数最终绑定到具体值，并返回声明中所有 Proposition 是否成立。
缺少参数、调用包含副作用或遇到不支持的节点时，应返回带源码位置的错误。

不同 evaluator 可以产生不同结果：

```text
eval(instance, evaluators::boolean)     -> Bool
eval(instance, evaluators::explain)     -> Explanation
eval(instance, propagation_evaluator)   -> DomainState
eval(instance, document_evaluator)      -> Document
```

`eval()` 不应根据参数是否未知而悄悄从布尔检查切换成求解。
解释方式由显式 evaluator 决定，结果 Type 由 evaluator 的签名决定。

## 10. `compile()`

`compile()` 把 Declaration IR 针对一个明确目标彻底降低：

```tapas
let check_range = compile(InRange, evaluators::boolean)
let valid = check_range(5)
```

对于布尔目标，结果可以是普通可执行函数；对于其他目标，结果可以是后端程序、查询、求解器模型或序列化制品。

```text
compile(declaration, boolean_evaluator) -> Function
compile(declaration, sql_evaluator)     -> SqlProgram
compile(declaration, solver_evaluator)  -> SolverProgram
```

`compile()` 必须：

- 在编译前检查目标 evaluator 的节点和 Type 能力；
- 固定参数签名、捕获值和结果 Type；
- 对纯 Term 进行常量折叠和公共子表达式共享；
- 保留 Proposition 到源码的映射；
- 使用 Declaration IR、evaluator 版本和捕获值生成缓存 key；
- 对不支持的语义产生错误，不能退化为含义不同的近似实现。

“彻底编译”不意味着存在一个能够执行所有 Declaration 的唯一后端。
每个 evaluator 只需要编译自己声明支持的 IR 子集。

## 11. 调用、纯度与 evaluator 能力

Declaration IR 中的调用需要分为：

```text
PureIntrinsic     已知且可由多个 evaluator 解释的内建纯操作
PureFunction      可执行但不一定可反向传播的纯函数
DeclarationCall   对另一个 Declaration 的应用
OpaqueCheck       只能在参数具体时执行的确定性检查
EffectfulCall     有外部副作用，声明体中禁止
```

例如，布尔 evaluator 可以执行 `PureFunction` 和 `OpaqueCheck`；传播 evaluator 可能只支持一部分 `PureIntrinsic` 和 `DeclarationCall`。
一个调用不能因为某个后端无法传播就被错误地拒绝为非法声明，但相应后端必须清楚报告能力边界。

长期可以为函数增加 effect 和 evaluator 能力签名，例如：

```text
effects: []
interpreters: [execute, constant_fold]
```

本文不规定具体表面语法，但 Declaration IR 从第一版起应保留这些字段，避免以后只能依靠函数名称猜测纯度。

## 12. 来源、说明与诊断

声明将被组合、实例化和改写，因此单个源码范围不足以解释错误。
每个 Proposition 需要维护来源链：

```text
当前实例
  -> 组合它的声明项
  -> 原始 Declaration
  -> 原始表达式源码范围
  -> 可选的元程序生成位置
```

当声明冲突或 evaluator 不支持某个节点时，诊断应优先展示：

1. 人类说明字符串；
2. 原始表达式；
3. 参数的实际绑定；
4. 引入该命题的声明组合路径；
5. evaluator 缺失的能力。

说明字符串属于文档元数据，应被序列化并由编辑器、测试报告和文档 evaluator 使用。

## 13. 与当前 Tapas 编译器的结合

当前编译器已经具有可复用前端、AST、词法作用域、引用到符号的解析结果、Type 信息和字节码后端。
`declare` 应在这些阶段之上增加独立降低路径，而不是在 VM 运算符中临时重载普通值。

建议的实现分层是：

```text
lexer / parser
    增加 declare token、具名声明 AST 节点和声明体语法

semantic
    建立 Declaration 名称、参数作用域、局部 let 和捕获关系

type analysis
    产生完整参数签名、每个 Term 的 Type、调用结果和 effect 信息

declaration lowering
    把已分析 AST 转换为 Typed Declaration IR

runtime
    增加不可变 Declaration 与 DeclarationInstance 值及引用计数

evaluator protocol
    遍历 IR，并支持 eval、compile、能力检查和来源诊断

bytecode / serialization
    保存声明模板、捕获值、IR 版本和编译缓存
```

解析 AST 继续服务于编辑器和源码工具；Declaration IR 服务于运行时元编程和多后端解释。
二者不能合并成一个同时承担所有职责的数据结构。

## 14. 分阶段实现

### 阶段一：语法与只读 IR

- 支持 `declare Name(parameters) { ... }`；
- 支持顶层 Bool、`"说明": Bool`、局部 `let` 和声明组合；
- 完成参数 Type、名称解析、Term Type 和来源映射；
- 提供只读 IR 打印和快照测试；
- 不提供传播或优化。

### 阶段二：第一类运行时值与布尔 `eval()`

- `Declaration` 和 `DeclarationInstance` 成为运行时值；
- 支持保存、传递、导出、实例化和组合；
- 提供具体参数上的布尔 evaluator；
- 明确纯调用、OpaqueCheck 和副作用错误。

### 阶段三：动态构造与 `compile()`

- 提供 `declarations` 构造、检查、替换和变换 API；
- 支持卫生的动态 IR 生成；
- 将布尔声明编译为普通可执行检查函数；
- 提供版本化序列化和缓存。

### 阶段四：测试解释器

- 合法状态生成；
- 边界场景探索；
- 失败解释和缩减；
- 确定性回放；
- 与结构 Type、Agent 工具和状态转换接口结合。

### 阶段五：传播与外部后端

- 为可传播 Term 定义明确子集；
- 接入传播、搜索、SQL、SMT 或其他后端；
- 通过 capability 检查区分可传播命题和仅可检查命题；
- 不改变 `declare` 的核心表面语义。

## 15. 必须覆盖的测试

实现进入正式语言前，至少需要验证：

- 具名声明的解析、作用域、引用、重命名和模块导出；
- 参数、局部 `let`、捕获值和遮蔽的 Symbol ID 正确性；
- 每种允许 Term 的 Type 正确性和错误诊断；
- `"说明": proposition` 与普通 Pair 在不同上下文中的区分；
- `let` 不自动产生 Proposition；
- 声明组合后的来源链和说明保留；
- 动态构造 API 与表面语法产生规范等价的 IR；
- IR 序列化、反序列化和版本拒绝；
- evaluator 能力不足时的确定性错误；
- `eval()` 不接受源码字符串或未解析 AST；
- `compile()` 结果与相同 evaluator 的 `eval()` 结果一致；
- 不允许副作用、可变捕获和未经声明的后端近似。

## 16. 当前建议与待定问题

本文建议先确定以下原则：

1. 正式表面格式为 `declare Name(...) { ... }`；
2. 公开参数第一阶段要求显式 Type；
3. 顶层 Bool 表示命题，多个命题共同成立；
4. `let` 绑定 Term，但不自动断言；
5. `"说明": proposition` 是推荐的读者说明形式；
6. Declaration 是第一类、不可变、可组合的运行时值；
7. 表面语法和动态构造 API 必须产生同一种 Typed Declaration IR；
8. `eval()` 和 `compile()` 只处理已解析、已检查的 IR，不处理源码字符串；
9. evaluator 必须显式声明能力，不能静默改变声明含义；
10. 优化和求解属于后端，不属于 `declare` 核心语法。

仍需通过原型决定：

- 是否以及何时支持匿名 `declare(...) { ... }`；
- 是否加入原生 `implies`、量词和集合推导；
- 捕获复合值时采用冻结、复制还是显式参数；
- Declaration Type 的参数名是否参与类型相等；
- IR 的公共反射粒度和稳定性承诺；
- `compile()`、`eval()` 是根函数、包函数还是 evaluator 协议上的隧道调用；
- 哪些普通纯函数能够自动降低，哪些只能成为 OpaqueCheck；
- `.tapc` 是否直接携带 Declaration IR，或保存可重建 IR 的模板。

## 17. 结论

`declare` 最重要的能力不是延迟执行一个代码块，而是把普通 Tapas 表达式提升为一种第一类、带类型、已解析名称并保留来源的语义制品。

```text
普通 Tapas 代码负责动态构造声明
declare 负责产生 Typed Declaration IR
eval 负责按照指定 evaluator 解释声明
compile 负责将声明彻底降低到指定目标
```

只要表面语言继续直接表达领域意图，而所有符号、Type、来源、能力和后端细节由 Declaration IR 承担，Tapas 就可以在不演变成求解器专用语法的前提下获得真正的元编程能力，并以此支撑复杂系统测试及后续建模用途。
