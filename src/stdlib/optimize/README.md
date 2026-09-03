# Tapas `optimize` 包设计

简体中文 | [English](README_en.md) | [项目主页](../../../README.md)

> 状态：设计阶段。
> `optimize` 尚未实现，也不属于当前对外公开的标准库 API。
> 本目录只保存设计文档，当前构建和安装不会提供可导入的 `optimize` 包。

`optimize` 基于 Rule 基础设施提供变量传播、可行性搜索和目标优化。
它不增加 Tapas 语法，也不定义另一套公共表达式 IR。

```text
var quantity = optimize::variable(
    "quantity",
    types::Int,
)

var cost = optimize::variable(
    "cost",
    types::Int,
)

let Plan = rule (limit: Int) {
    "数量不能为负数": quantity >= 0
    "数量不能超过限制": quantity <= limit
    "成本关系不成立": cost == quantity * 3
}

let feasible = evaluators::eval(
    Plan(10),
    optimize::find_feasible_assignment,
)

let optimal = evaluators::eval(
    Plan(10),
    optimize::find_optimal(
        cost,
        optimize::minimize,
    ),
)
```

本文描述的是拟议的包设计，所有接口均尚未实现。

## 1. 设计边界

`optimize` 计划作为普通源码包实现。
它依赖：

- `types` 提供 Type、Rule Type 和 Term Type；
- `rules` 提供 Rule、RuleInstance、RuleIR、Term、Condition、Requirement、来源和动态构造；
- `evaluators` 提供 Context、Result、执行入口和编译入口。

核心包不依赖 `optimize`。
安装或移除优化包不应改变 lexer、parser、Rule lowering、VM 或核心 Type 检查器。

优化包不得增加：

- `variable`、`constraint`、`minimize`、`maximize` 或 `solve` 等关键词；
- 优化专用 AST 节点或字节码；
- Constraint、Objective 或 SolverVariable 等 RuleItem；
- 与 RuleIR 并列的公共 Optimization IR；
- 编译器对 `optimize` 包名的特殊判断。

Variable、方向、evaluator 和后端都通过普通包接口提供。

## 2. Variable

### 2.1 创建

```text
var quantity = optimize::variable(
    "quantity",
    types::Int,
)
```

`optimize::variable` 接受显示名称和值 Type，返回不可变 Variable。
每次调用创建新的 Variable 身份；显示名称只用于诊断和展示。

Variable 同时匹配：

```text
optimize::Variable
types::Term
types::term(variable_value_type)
```

因此 Variable 直接使用 Term 运算：

```text
let next = quantity + 1
let positive = quantity > 0
```

`next` 是 Int Term，`positive` 是 Bool Term。
优化包不定义 Expression Type。

### 2.2 IR 表示

Variable 通过优化包的 Term Provider 表示为 Extension Term：

```text
Extension {
    provider: optimize
    version: 1
    kind: Variable
    payload: {
        identity
        display_name
        value_type
        initial_domain?
    }
}
```

Rule 核心只检查 Provider、版本、payload 和 result Type。
Variable 身份、Domain 及后端 lowering 由优化包处理。

同一个 Variable 在多个 Term、Rule 或 evaluator 配置中保持同一身份。
两个显示名称相同的 Variable 仍然相互独立。
Assignment、Domain 和目标引用都按 Variable 身份匹配。

### 2.3 Variable、Rule 参数与 Capture

三者的值来源不同：

| 对象 | 值来源 |
|---|---|
| Rule 参数 | 创建 RuleInstance 时由调用者提供 |
| Capture | evaluator 执行时从 Rule 闭包环境读取 |
| Variable | 由优化 evaluator 传播、搜索或决定 |

```text
let Plan = rule (limit: Int) {
    quantity >= 0
    quantity <= limit
}
```

`Plan(10)` 只绑定 `limit = 10`。
`quantity` 仍是待求 Variable。

Variable 通常通过 `var` 或函数参数作为 Rule 的 Capture 进入 IR。
Evaluator 处理 Capture Term 时读取当前环境值；该值必须仍然是 result Type 相容的 Variable。
`let` 不进入闭包环境，若只希望在局部持有 Variable，应将它作为 Rule 参数显式传入。
绑定已经改变或 Type 不匹配时返回 `Failed`。

## 3. Rule 作为优化模型

RuleIR 是优化包唯一的公共表达式 IR：

| 优化概念 | Rule 基础设施 |
|---|---|
| 参数 | RuleParameter + RuleInstance binding |
| 待求变量 | Variable Extension Term |
| 常量 | Constant Term |
| 表达式 | Intrinsic、Call、Extension Term |
| 硬约束 | Condition |
| 规则组合 | Requirement |
| 顺序 | RuleIR.items |
| 外部环境 | Capture + evaluator Context |
| 说明 | Condition.description |
| 来源 | RuleOrigin |

优化 evaluator 直接遍历 RuleIR，不复制 Condition 或 Term。

### 3.1 Condition

每个 Condition 都解释为必须成立的硬约束：

```text
let Allocation = rule (capacity: Int) {
    quantity >= 0
    quantity <= capacity
}
```

Condition 本身就是约束的规范表示。
优化包不导出内容重复的 Constraint Type。

Evaluator 内部可以建立只读视图：

```text
ConditionView {
    condition: Condition
    context: evaluators::Context
    requirement_path: [Requirement]
    variables: [Variable]
}
```

ConditionView 只在一次 evaluator 执行中存在，不复制 RuleIR，也不成为新的公共 IR。

不引用 Variable 的 Condition 仍然有效。
其结果为 `false` 时，当前 RuleInstance 不存在可行赋值。

### 3.2 Requirement

Requirement 按 RuleIR.items 顺序在当前位置展开：

```text
let NonNegative = rule (value: Int) {
    value >= 0
}

let WithinLimit = rule (value: Int, limit: Int) {
    value <= limit
}

let Plan = rule (limit: Int) {
    require NonNegative(quantity)
    require WithinLimit(quantity, limit)
    cost == quantity * 3
}
```

Evaluator 通过 Context 解析目标 Rule、参数、Capture 和 Requirement 路径。
每个 Requirement 独立展开，不按 Rule 身份去重。
循环依赖返回 `Failed`。

### 3.3 Term 支持

每个 optimizer evaluator 和后端必须声明支持的 Type、Intrinsic 和 Extension Provider。

默认处理规则是：

| Term | 处理方式 |
|---|---|
| Constant | 转换为后端常量 |
| Parameter | 从 RuleInstance 读取具体值 |
| Capture | 从 Context 读取当前值 |
| Variable Extension | 转换为待求变量 |
| Intrinsic | 后端支持时 lowering |
| Call | 默认 `Unsupported` |
| 其他 Extension | Provider 或后端提供 lowering 时处理 |

普通 Call 保留 Tapas 调用语义，但优化器不能从函数实现猜测数学含义。
没有明确 lowering 的 Call 返回 `Unsupported`。

## 4. Evaluator 执行

优化 evaluator 实现统一协议：

```text
evaluate(evaluators::Context) -> evaluators::Result
```

处理过程为：

```text
RuleInstance
    -> 建立 evaluator Context
    -> 按 items 顺序遍历 RuleIR
    -> 绑定参数并读取 Capture
    -> 展开 Requirement
    -> 识别 Variable Extension
    -> 验证后端能力
    -> 建立求解状态或后端模型
    -> 传播或求解
    -> 返回统一 Result
```

`evaluators::Result.status` 使用：

| Status | 含义 |
|---|---|
| `Success` | evaluator 正常完成，包括已证明不可行或无界 |
| `Unsupported` | Rule 使用了 evaluator 不支持的合法 Type、Term 或 Provider |
| `Failed` | 参数、Capture、循环、资源、后端或内部执行错误 |

可行性为 `false`、传播结果不一致或目标无界都是正常优化结论，不属于运行失败。

## 5. 传播

```text
let result = evaluators::eval(
    Plan(10),
    optimize::propagator,
)
```

成功时，`result.value` 匹配 `optimize::Propagation`：

```text
Propagation {
    consistent: Bool
    variable_states
}
```

`consistent` 为 `false` 表示当前 Condition 不能同时成立。
Variable Domain 通过接口查询：

```text
let domain = optimize::domain(
    result.value,
    quantity,
)
```

Domain 的表示由 Variable Type 决定。
Bool Domain 表示允许的布尔值集合；Int Domain 可以表示区间和离散值；其他 Type 由 Provider 或后端扩展。

没有缩小某个 Domain 只表示当前 propagator 没有推导出更强结果。

## 6. 可行赋值

```text
let result = evaluators::eval(
    Plan(10),
    optimize::find_feasible_assignment,
)
```

成功时，`result.value` 匹配 `optimize::Feasibility`：

```text
Feasibility {
    feasible: Bool
    assignment: Assignment?
}
```

`feasible` 为 `true` 时，Assignment 为当前模型中每个需要赋值的 Variable 提供符合其 Type 和 Domain 的具体值，并使全部 Condition 成立。
`feasible` 为 `false` 表示 evaluator 已经证明不存在可行赋值。

Assignment 按 Variable 身份索引：

```text
let value = optimize::value(
    result.value.assignment,
    quantity,
)
```

## 7. 最优赋值

目标保存在 evaluator 配置中，不写入 RuleIR：

```text
let evaluator = optimize::find_optimal(
    cost,
    optimize::minimize,
)

let result = evaluators::eval(
    Plan(10),
    evaluator,
)
```

目标可以是 Variable，也可以是 evaluator 支持的数值 Term：

```text
let evaluator = optimize::find_optimal(
    quantity * 3,
    optimize::minimize,
)
```

目标 Term 中的 Variable 必须出现在本次 RuleInstance 展开的 Condition 中。
目标 result Type 必须支持指定方向和后端比较语义。

成功时，`result.value` 匹配 `optimize::Optimality`：

```text
Optimality {
    feasible: Bool
    bounded: Bool
    assignment: Assignment?
    target_value: AnyType?
}
```

`feasible` 为 `false` 表示不存在可行赋值。
`feasible` 为 `true` 且 `bounded` 为 `false` 表示目标沿指定方向无有限最优值。
两者均为 `true` 时，Assignment 和 target_value 表示已经证明的最优结果。

同一 RuleInstance 可以使用不同目标、方向和后端，不改变 RuleIR。

## 8. 软约束

软约束不是 RuleItem，也不增加语法。
辅助接口可以使用松弛 Variable、惩罚 Term 和普通 Condition 构造新 Rule，再以惩罚 Term 为目标。

变换结果必须通过 `rules::make` 或其他 Rule 构造接口生成新的合法 RuleIR，并保留原 Condition 和 Requirement 的 RuleOrigin。

## 9. 后端

传播器可以直接遍历 RuleIR 并维护 Variable Domain。
外部求解器可以把支持的 Term 编译为私有 Backend Model：

```text
RuleIR
    -> optimizer evaluator
    -> Backend Model
    -> solver
```

Backend Model 可以包含：

- 求解器变量编号；
- 线性矩阵或表达式 DAG；
- 传播队列；
- 搜索树；
- 冲突状态；
- 求解器句柄。

这些都是一次执行或一个缓存中的后端状态，不属于 Tapas 公共 IR，不进入 RuleIR，也不要求跨后端兼容。

后端编译缓存至少考虑：

- RuleIR 语义哈希；
- RuleInstance 参数；
- 当前 Variable 身份和初始 Domain；
- Capture 中影响模型的当前值；
- 目标、方向和优化选项；
- evaluator、Provider 和后端版本；
- 运行时 ABI。

## 10. Type 与接口

`optimize` 至少导出以下 Type：

| Type | 含义 |
|---|---|
| `optimize::Variable` | 任意 Variable Term |
| `optimize::Domain` | Variable 取值范围 |
| `optimize::Assignment` | Variable 赋值映射 |
| `optimize::Propagation` | 传播结果 |
| `optimize::Feasibility` | 可行性结果 |
| `optimize::Optimality` | 最优性结果 |
| `optimize::Direction` | 优化方向 |
| `optimize::Backend` | 后端配置 |

主要接口为：

| 接口 | 作用 |
|---|---|
| `optimize::variable(name, Type)` | 创建 Variable Term |
| `optimize::variable_in(name, Type, Domain)` | 创建带初始 Domain 的 Variable |
| `optimize::domain(Propagation, Variable)` | 查询传播后的 Domain |
| `optimize::value(Assignment, Variable)` | 查询 Variable 赋值 |
| `optimize::propagator` | Domain 传播 evaluator |
| `optimize::find_feasible_assignment` | 可行性 evaluator |
| `optimize::find_optimal(Term, Direction)` | 创建目标优化 evaluator |
| `optimize::minimize` | 最小化方向 |
| `optimize::maximize` | 最大化方向 |

Evaluator 的具体返回值存放在统一 `evaluators::Result.value` 中。

## 11. 身份、来源与保存

Variable Provider 负责稳定版本、Variable 身份、值 Type、初始 Domain、payload 验证和序列化。
同一 Variable 的多次引用在序列化和读取后必须恢复为同一身份。

通过 Capture 取得的 Variable 不固化进 RuleIR。
Evaluator 每次执行时读取当前绑定；缓存处理结果时必须包含实际 Variable 身份及影响模型的当前值。

Condition、Requirement 和 Term 继续使用 RuleOrigin。
优化结果引用 Condition 时必须保留其 RuleOrigin 和 Requirement 路径，使不可行说明和后端诊断能够定位原始 Rule。

求解器变量编号、模型句柄、搜索状态和临时 Domain 不进入 Rule 序列化格式。
需要保存优化结果或后端模型时，由优化包或具体后端定义独立格式。

## 12. 完整性检查

开始执行 optimizer evaluator 前至少检查：

- RuleInstance 参数完整且 Type 正确；
- Capture 可以读取且满足预期 Type；
- Variable Provider、版本、身份和值 Type 有效；
- Condition 引用 Bool Term；
- Requirement 可以解析且不存在运行时循环；
- 目标 Term 和方向兼容；
- 目标 Variable 属于当前展开模型；
- evaluator 和后端支持所有使用到的 Type、Intrinsic 和 Extension；
- Assignment 中的值满足 Variable Type 和 Domain。

任何不支持的合法 Term 返回 `Unsupported`。
任何参数、身份、环境、循环、资源或后端错误返回 `Failed`。
优化包不能忽略 Condition，也不能用语义不同的近似操作替代不支持的节点。

## 13. 实现约束

实现应满足：

```text
lexer / parser
    不包含 optimizer 关键词或语法

compiler
    不引用 optimize 包

RuleIR
    不包含 Variable、Objective 或 SolverConstraint 专用节点

VM
    不增加 optimizer 专用 opcode

optimize
    仅通过 Term Provider、公开 RuleIR 和 evaluator 协议接入
```

优化包的公共语义完全建立在 RuleIR 上；后端模型只是可替换的私有编译产物。
