# Tapas 产品设计

[项目主页](README.md) | [Rule](docs/Rules_zh.md) | [使用说明](docs/Usage_zh.md)

> 状态：设计阶段。
> 本文描述 Tapas 第二阶段的产品设计。除明确标记为当前实现的部分外，文中的产品对象和接口名称都尚未成为稳定 API。

本文使用`Rule`、`RuleIR`、`Evaluator`和`Result`表示当前已经存在的值或 Type；使用`Model`、`Action`、`Trace`等形式表示尚在设计的产品对象；checker、生成器、运行器和后端表示职责，不预设最终包名或 Type 名称。

Tapas 是一个面向复杂系统的规则驱动测试系统，以小型 DSL 组织领域规则和测试流程。
通用编程能力用于组织数据、函数和模块，不是产品继续扩展的主要目标。

Tapas 的核心目标是让用户只写一次领域规则，再将同一份规则用于校验、合法测试数据生成、反例搜索，并在适合时用于求解器驱动的状态空间探索。

## 1. 产品目标

### 1.1 要解决的问题

复杂系统测试通常同时面对以下问题：

1. 同一条业务规则分别出现在校验器、fixture、生成器、测试断言和评测器中，修改时容易产生语义漂移；
2. 字段、对象和状态之间存在关联约束，普通随机数据大多不是合法测试状态；
3. 有状态系统、Agent 和仿真环境中的操作组合很多，手写样例难以覆盖边界和异常路径；
4. 测试失败后只留下一个大输入或一段长轨迹，难以说明哪条规则被违反，也难以稳定重放。

Tapas 使用`Rule`保存领域事实，并让不同 evaluator 在不改变规则含义的前提下检查、生成或搜索这些事实。

### 1.2 适用对象

| 使用者 | 主要任务 |
|---|---|
| 研究者 | 描述约束、复现实验、控制变量、搜索边界与反例 |
| 测试工程师 | 构造合法数据、覆盖边界、执行状态测试、缩减失败样例 |
| Agent 和仿真系统开发者 | 定义环境合法状态、动作前提、安全不变量和终止条件 |
| 复杂业务系统团队 | 测试支付、订单、权限、供应链、调度和工作流 |
| 开发者工具用户 | 在 CLI、CI、脚本、服务或现有测试框架中使用规则能力 |

这些使用方式共享同一条测试主线：

1. 定义什么输入或状态是合法的；
2. 定义当前状态允许执行哪些操作；
3. 检查操作结果和全局不变量；
4. 自动产生合法输入、状态和操作参数；
5. 搜索违反规则的输入或操作序列；
6. 缩减、解释和重放失败；
7. 在规则边界和状态空间中继续探索。

### 1.3 贯穿示例

下面的 Rule 使用当前已经实现的语法描述一条简化的退货规则：

```tapas
let Refundable = rule (
    status        : String,
    delivered_days: Int,
    confirmed     : Bool,
) {
    "order has not been delivered":
        status == "Delivered"

    "delivery time is invalid":
        delivered_days >= 0

    "return window has expired":
        delivered_days <= 30

    "return has not been confirmed":
        confirmed
}

let checked = rules::check(
    Refundable("Delivered", 14, true),
)
print(checked["passed"])
```

<pre class='Tapas-Return'>
true
</pre>

同一份`Refundable`计划用于以下测试活动：

| 用途 | 使用方式 | 状态 |
|---|---|---|
| 校验已有订单 | 通过`rules::check`执行 | 当前实现 |
| 生成合法订单 | 生成满足全部 Condition 的参数 | 设计阶段 |
| 生成边界 | 生成`delivered_days`为 29、30 和 31 的相邻样例 | 设计阶段 |
| 检查 Action | 作为退货操作的前置条件 | 设计阶段 |
| 缩减反例 | 缩减时继续保证退货请求本身合法 | 设计阶段 |

后续章节中的生成器、Action 和反例都以这种复用方式为准，不要求用户复制 Rule 中的条件。

## 2. 设计边界

### 2.1 Rule 是唯一的规则表示

`Rule`和`RuleIR`是领域规则的公共表示。
checker、生成器、状态测试、反例搜索和 solver backend 不得分别建立内容相同但语义独立的 Constraint 模型。

同一条规则可以由多个 evaluator 使用，但这不表示所有 evaluator 都能处理任意 Tapas 表达式。
checker 按普通 Tapas 语义执行函数调用；生成器或 solver 只有在理解相应 Term 时才能分析它。

### 2.2 支持范围和运行准备必须提前检查

如果用户只能在运行生成器之后才发现大量`Unsupported`，那么“规则只写一次”的产品承诺实际上没有成立。
因此，每个分析型 evaluator 都需要在正式执行前完成两类检查。

| 检查 | 输入 | 需要回答的问题 |
|---|---|---|
| 结构支持检查 | `Rule`或`RuleIR` | Evaluator 是否理解全部 Term、Type、Requirement 和 Extension |
| 实例准备检查 | `RuleInstance`和运行环境 | 参数、Capture、Domain、Provider 和外部资源是否已经准备好 |

结构支持只由 Rule 内容和 evaluator 能力决定，适合缓存，也可以由 LSP 提前展示。
实例准备还依赖本次绑定和运行环境，需要在每次执行前检查。

下面的字段只说明支持报告需要表达的信息，不规定最终字段名称或具体 Type：

| 字段 | 含义 |
|---|---|
| `supported` | 当前 Rule 是否能由该 evaluator 完整分析 |
| `unsupported` | 不支持的 Term、Type、Requirement 或 Extension |
| `reason` | 不支持的原因 |
| `origin` | 对应源码位置 |
| `requirement_path` | 该节点所在的规则组合路径 |
| `required_provider` | 需要哪个 Provider 或 lowering 才能处理 |

现有`evaluators::compile`可以作为结构支持检查的入口，但具体返回结构仍需设计。
实例准备检查是否使用新的 evaluator 接口，仍属于开放问题。

LSP 展示结构支持；CLI 在执行前同时展示结构支持和实例准备问题。
两类问题都不能等到搜索运行一段时间后才报告。

### 2.3 Rule 的三种使用方式

为了让能力边界清楚，产品文档需要区分三种 Rule：

| 形式 | checker | 生成器或 solver |
|---|---|---|
| 只使用公共 Intrinsic 和已支持 Term | 可以执行 | 可以分析 |
| 包含普通 Tapas `Call` | 可以执行 | 默认`Unsupported` |
| 使用 Extension Term 或具有显式 lowering 的 Call | 可以执行 | 对应 evaluator 安装 Provider 或 lowering 后可以分析 |

这种区分不产生新的 Rule Type，也不产生第二套语法。
它只说明某个 Rule 对某个 evaluator 是否可移植。

### 2.4 产品分层

Tapas 不把所有测试能力都加入 Core。

| 层次 | 内容 | 依赖方向 |
|---|---|---|
| Core | `Rule`、`RuleIR`、checker、evaluator 协议和公共结果语义 | 不依赖具体测试策略或外部系统 |
| 测试包 | 生成、边界、状态运行、Trace、缩减和探索 | 依赖 Core，通过普通包提供 |
| 适配层 | 外部数据、Agent harness、τ-bench 格式、Python、Gym 和 solver backend | 依赖 Core 或测试包，不改变其语义 |

`Model`、`Action`和`Trace`首先作为测试包中的产品概念验证；`Task`留在应用层。
只有纵向示例证明普通结构和协议不足时，才考虑增加新的核心引用 Type。

Benchmark 生成建立在测试包之上，不属于 Core。
RAG、LLM、语音和分布式运行器属于适配层，也不进入 Core。

### 2.5 不追求通用语言完整性

新能力优先通过以下方式实现：

1. 使用现有`Rule`、`RuleIR`和 Type；
2. 增加普通标准包和函数；
3. 增加 evaluator 或 Term Provider；
4. 增加外部 adapter 或可选 backend。

只有当某个改动显著改善 Rule 的表达、组合或诊断，且无法通过库设计清晰实现时，才考虑增加语法。

语法提案至少需要说明：

1. 它改善了哪一种 Rule 表达或测试工作流；
2. 现有 Rule、函数、Type、标准包和 evaluator 为什么无法合理实现；
3. 它是否产生只能由部分 evaluator 理解的新语义；
4. 它对 RuleIR、序列化、LSP、formatter、checker 和后端的长期成本；
5. 是否有两个以上不同领域证明它不是单一场景特例。

## 3. 当前基础

### 3.1 当前实现

| 能力 | 状态 | 说明 |
|---|---|---|
| `Rule`和`RuleInstance` | 当前实现 | 带参数、可捕获环境、可保存、传递、组合和延迟检查 |
| `Condition`和`Requirement` | 当前实现 | 表示规则条件和规则组合 |
| `RuleIR` | 当前实现 | 包含 Parameter、Capture、Term、Item、Origin 和版本信息 |
| `assert`和`rules::check` | 当前实现 | 按普通 Tapas 语义检查 Rule |
| Rule 构造、反射和序列化 | 当前实现 | 提供动态构造、检查、哈希和序列化接口 |
| `evaluators` | 当前实现 | 通过 Context 消费 RuleIR，并返回统一 Result |
| CLI、C API、LSP 和 VS Code 扩展 | 当前实现 | 提供运行、嵌入和编辑支持 |
| `optimize` | 设计阶段 | 已有设计文档，尚未实现，也不是公开标准库 API |

### 3.2 当前限制

- `rules::check`能返回普通 Condition violation，但部分参数、环境、Term 求值和 Extension 问题仍直接产生运行时错误；
- evaluator 协议已经存在，但标准生成器、shrinker、状态运行器和 explorer 尚未实现；
- RuleIR 可以记录普通 Tapas 调用，但分析型 evaluator 不能自动理解任意函数实现；
- 源码 Rule 的跨进程序列化受闭包环境和 checker 重连能力限制；
- 当前没有统一的`Model`、`Action`、`Task`和`Trace`公共语义；
- 当前示例主要展示通用编程能力，尚未跑通 Rule 驱动测试的完整流程。

## 4. 外部数据与建模成本

Tapas 不能要求用户为了测试而重新建立一份完整业务模型。
用户通常已经拥有 JSON 数据、数据库 fixture、API schema、C 对象或 Agent Tool schema。

### 4.1 数据进入 Tapas 的原则

1. 外部 adapter 将已有数据转换为普通 Tapas 值或只读视图；
2. Rule 参数直接接收这些值，不要求再定义一套专用 State Type；
3. 字段名称、Type、可选状态和外部身份只在 adapter 中映射一次；
4. Trace 保存足以重放的外部身份、输入和 observation；
5. Adapter 不能在转换时悄悄修复非法数据，否则 checker 无法观察真实问题。

### 4.2 Schema 与 Type

从 OpenAPI、JSON Schema、数据库或 Tool schema 导入 Type 属于适配能力。
导入结果应尽量使用现有结构 Type 和容器 Type，不增加外部 schema 专用 Rule 节点。

Schema 只能描述结构时，领域约束继续写在 Rule 中。
Schema 已经包含稳定约束时，adapter 可以生成对应 RuleIR，但必须保留来源并允许用户检查生成结果。

### 4.3 外部系统状态

外部系统可以提供三种状态能力：

| 能力 | 用途 |
|---|---|
| Snapshot | 保存完整状态，适合确定性重放和结果比较 |
| Diff | 记录操作前后的结构化变化，适合大型状态 |
| Observation | 保存无法直接快照的外部响应和事件 |

Runner 必须记录本次运行实际具备哪种能力。
如果外部系统不能恢复状态，结果可以保存为诊断 Trace，但不能宣称能够完全重放。

### 4.4 建模成本检查

每个纵向示例都要检查以下问题：

1. 同一个领域字段是否在 adapter、State、Task 和 evaluator 中重复定义；
2. 同一条业务条件是否在 Rule、Generator 和 Action 前置条件中重复表达；
3. 接入已有 fixture 是否需要大量与测试目标无关的转换代码；
4. 用户增加一条 Rule 后，是否需要同步修改多个 evaluator；
5. 外部数据发生兼容变更时，是否只需修改 adapter。

如果示例无法满足这些要求，应优先缩小对象模型，而不是继续增加抽象。

## 5. 产品对象

### 5.1 对象及其职责

| 对象 | 所在层 | 职责 | 状态 |
|---|---|---|---|
| `Rule` | Core | 描述输入、状态、操作或结果必须满足的事实 | 当前实现 |
| `RuleIR` | Core | 为 checker 和 evaluator 提供统一规则表示 | 当前实现 |
| `Evaluator` | Core | 以某种方式消费 RuleIR | 当前实现 |
| `Result` | Core | 表示执行状态、领域结论、violation 和 diagnostic | 当前实现，语义待统一 |
| `State` | 普通值 | 表示某个时刻的领域数据 | 不增加专用 Type |
| `Domain` | 测试包 | 描述待生成值的有限范围或结构信息 | 设计阶段 |
| `Model` | 测试包 | 组织初始状态 Rule、全局不变量和 Action | 设计阶段 |
| `Action` | 测试包 | 描述参数、前置条件、执行和后置条件 | 设计阶段 |
| `Trace` | 测试包 | 有序记录消息、调用、观察和状态变化 | 设计阶段 |
| `Counterexample` | 测试包 | 保存原始失败、最小失败和重放信息 | 设计阶段 |
| `BoundaryCase` | 测试包 | 保存边界两侧样例、最小差异和规则来源 | 研究项 |
| `Task` | 应用层 | 描述初始条件、目标、允许的参与者和评测依据 | 设计阶段 |
| Backend | 适配层 | 提供传播、搜索、solver 或外部执行实现 | 可替换 |

`State`保持普通值，可以是结构、容器或 adapter 提供的视图。
`Task`和 benchmark 数据属于应用层，不能反向要求 Core 增加专用语法或 IR。

### 5.2 Model 与 Action

`Model`至少组织：

- 初始状态 Rule；
- 全局不变量 Rule；
- 可用 Action；
- 状态观察或快照方式；
- 终止条件和默认预算。

`Action`至少组织：

- 名称和参数 Type；
- 前置条件 Rule；
- 执行函数或外部 adapter；
- 后置条件 Rule；
- 读写分类和可观测结果。

Action 描述领域状态转移，Tool 描述 Agent 或外部系统调用该能力的接口。
一个 Tool 可以映射到一个 Action，也可以只执行查询；产品设计不强制两者一一对应。

### 5.3 Trace

`Trace`按顺序记录以下事件：

- Agent 或用户消息；
- Agent 或用户 ToolCall；
- Action 执行结果；
- 环境自动事件；
- State Snapshot、Diff 或 Observation；
- violation 和 diagnostic。

每个事件需要记录 Actor、顺序、来源和版本。
未知事件不能被重放器或 evaluator 静默忽略。

### 5.4 Result

产品结果需要分开表示执行状态和领域结论。
下面的字段描述公共结果需要承载的信息，不规定最终字段名称或具体 Type。

| 字段 | 含义 |
|---|---|
| `status` | Evaluator 是否正常完成 |
| `value` | `passed`、`feasible`、`found`等具体领域结论 |
| `violations` | Rule Condition failure |
| `diagnostics` | 不支持、执行失败、预算和后端信息 |
| `artifacts` | Trace、Counterexample、BoundaryCase 或 Assignment |
| `statistics` | 尝试数、接受率、搜索节点、覆盖和耗时 |

执行状态至少区分：

| 状态 | 含义 |
|---|---|
| `Success` | Evaluator 正常得出结论，包括 Rule 不满足或模型不可行 |
| `Unsupported` | 输入合法，但包含 evaluator 不支持的语义 |
| `Failed` | 参数、环境、循环、后端或内部执行错误 |
| `Incomplete` | 设计项；预算耗尽、取消或中断，尚未得出完整结论 |

`feasible = false`不等于`Failed`。
没有在预算内找到反例也不等于反例不存在。

## 6. 核心工作流

### 6.1 校验已有对象或状态

1. 用户用实际对象或状态创建`RuleInstance`；
2. checker 按 Rule 项目顺序执行 Condition 和 Requirement；
3. checker 返回是否通过、violation、Requirement 路径和源码位置；
4. 调用者决定中止程序、记录结果或继续测试。

这条工作流是其他 evaluator 的语义基线。
生成、搜索和 solver 给出的具体结果最后都要回到同一个 checker。

### 6.2 生成合法数据和规则边界

1. 用户提供 Type、可选 Domain、Rule、seed 和预算；
2. 生成器检查 Rule 的结构支持情况；
3. 生成器检查参数、Capture、Domain 和 Provider 是否已经准备好；
4. 生成器从 Type、Domain 和 RuleIR 中提取候选值与边界；
5. 生成器使用直接构造、传播、搜索或拒绝采样得到候选值；
6. checker 再次验证每个候选值；
7. 生成器返回合法值、采用的策略、来源、预算和统计信息；
8. 用户需要边界时，生成器同时给出边界两侧和边界本身的最小差异样例。

规则边界与合法数据生成同时设计。
它直接服务普通测试，不依赖`Model`、benchmark 或 solver 成熟后才可使用。

### 6.3 执行有状态测试

1. Runner 生成或载入满足初始 Rule 的 State；
2. Runner 根据当前 State 筛选前置条件成立的 Action；
3. Runner 生成 Action 参数并再次检查前置条件；
4. Runner 执行纯模型函数或外部系统调用；
5. Runner 记录 observation 和 State 变化；
6. Runner 检查 Action 后置条件和全局不变量；
7. Runner 将消息、调用、结果和检查记录追加到 Trace；
8. Runner 在目标完成、发现失败或预算耗尽时返回 Result。

Runner 支持纯模型、外部系统和影子比较三种模式。
外部调用失败、Rule violation 和预算耗尽分别报告。

### 6.4 搜索并缩减反例

1. 用户指定目标 Condition、禁止 Action、Goal failure 或实现差异；
2. Explorer 生成初始 State 和合法 Action 序列；
3. Runner 执行序列并记录首次目标失败；
4. Shrinker 依次缩小输入、State、Action 参数和 Trace；
5. 每个缩减候选重新检查初始合法性和 Action 前置条件；
6. 只有仍然触发同一目标失败的候选才能替换当前反例；
7. 最终 Result 保存原始反例、最小反例、缩减过程和重放信息。

缩减不能通过破坏前置条件制造伪反例。

### 6.5 生成并运行 benchmark

1. 应用层用领域 Rule、Action 和原子场景构造 Task；
2. 生成器产生合法 fixture，并构造或搜索一条参考 Trace；
3. 任务质量检查确认 Task 可解，评分能够接受正例并拒绝受控负例；
4. 外部 Agent 或系统运行 Task，evaluator 保存最终结果、过程 Policy 和可重放 Trace。

Benchmark 生成建立在前四条工作流之上，不是最先实现的 Core 能力。
具体生成过程和质量检查见第 9 节。

## 7. 校验、生成与探索

### 7.1 校验与诊断

checker 继续保留完整 Tapas 运行语义，并形成所有测试能力共享的诊断格式。

诊断至少包括：

- Condition 说明和源码位置；
- 完整 Requirement 路径；
- 相关输入、State Diff 或 Action；
- 首次违反规则的 Trace 位置；
- 不支持的 Term 和所需 Provider；
- 外部 adapter 或 backend 诊断。

`assert`继续用于不可恢复的程序契约。
需要展示、记录或继续处理的测试检查使用普通 Result。

### 7.2 约束感知生成

第一批生成范围包括：

- Bool、有限范围 Int、枚举式值和常用 String 约束；
- List、Dictionary 和结构化领域对象；
- Condition 和嵌套 Requirement；
- 等于、不等于、范围、集合成员、长度和基础逻辑组合；
- 用户扩展的 Domain、Generator 和 Term Provider；
- 数量、seed、大小、深度、时间和尝试次数预算。

生成器按以下顺序选择策略：

1. 从 Type 和显式 Domain 直接构造；
2. 从 RuleIR 提取常量、边界和候选值；
3. 传播局部约束，缩小 Domain；
4. 在受控预算内搜索或拒绝采样；
5. 对支持的 Rule 子集调用可选 solver backend。

生成器必须报告实际使用的策略。
标记为成功生成的值必须全部通过原 Rule。
不可满足、暂未找到、不支持和执行失败使用不同结论。

### 7.3 边界发现

边界发现分为四种问题。

| 类别 | 需要回答的问题 | 典型结果 |
|---|---|---|
| Rule 决策边界 | 哪个最小字段变化会改变 Rule 结论 | 边界两侧、边界本身和 governing Condition |
| 状态可达性边界 | 到达目标或非法 State 的最短合法序列是什么 | 最短 Trace 和首次越界位置 |
| Task 可解性边界 | 哪个最小变化使 Task 从可解变为不可解 | 变化字段、缺失 Tool 或冲突 Rule |
| Agent 能力边界 | Agent 在哪个难度区间开始明显失败 | 带 trial 和置信区间的经验结果 |

Rule 决策边界优先实现。
它只依赖 Rule 和生成器，能够较早证明 Tapas 相比普通随机生成的价值。

状态可达性边界依赖`Model`和`Trace`。
Task 可解性边界依赖 benchmark 应用层。
Agent 能力边界属于实验结果，不是逻辑证明，需要记录模型、prompt、Tool schema、采样参数、trial、seed、环境版本和成本。

### 7.4 反例和缩减

反例搜索目标可以是：

- 某条 Condition 首次为假；
- 某个禁止 Action 被执行；
- 最终 Goal Rule 不满足；
- 两个实现或模型结果不一致；
- 系统进入指定高风险或不可恢复 State。

Shrinker 可以缩小数值、String、容器、结构字段、初始 State、Action 参数和 Trace。
领域 Type 可以注册自定义 shrinker。

`Counterexample`至少保存：

| 字段 | 含义 |
|---|---|
| `original` | 第一次发现的输入或 Trace |
| `minimal` | 缩减后的输入或 Trace |
| `target` | 需要保持的目标 failure |
| `violations` | 对应 Rule violation |
| `steps` | 缩减过程和预算 |
| `replay` | seed、fixture、版本和配置 |

### 7.5 覆盖与探索

Explorer 可以使用随机、BFS、DFS、best-first、覆盖引导或混合策略。
具体算法不进入公共对象语义。

覆盖信息至少包括：

- Condition 和 Requirement；
- Action 和状态转移；
- Rule 边界；
- Goal、拒绝、转人工和错误路径；
- 用户定义的状态分区或风险标签。

Explorer 可以优先选择未覆盖规则、稀有 State 或高风险路径，但不能跳过 Rule 检查换取覆盖率。
预算耗尽时返回部分覆盖、已经发现的 Trace 和未完成原因。

### 7.6 Solver-backed 能力

Solver 用于增强适合符号处理的 Rule 子集，不是使用 Tapas 的前置依赖。

设计包括：

- Variable、Domain、Assignment 和传播结果；
- RuleIR 到私有 backend model 的明确 lowering；
- 可行赋值、边界赋值和不可满足判断；
- 后端允许时的冲突 Rule 或不可满足核心；
- 超时、取消、资源限制和编译缓存；
- 由普通 checker 二次验证具体结果。

Solver 不得增加专用关键字、AST、RuleItem、VM opcode 或第二套公共 Constraint IR。
普通`Call`没有显式 lowering 时返回`Unsupported`。

通用数学优化不是产品核心。
只有目标优化直接服务测试数据选择、边界发现或探索优先级时，才进入测试包。

## 8. Model、Trace 与外部系统

### 8.1 Runner 模式

| 模式 | 执行方式 | 适用场景 |
|---|---|---|
| 纯模型 | Tapas 函数计算下一 State | 研究、仿真和快速搜索 |
| 外部系统 | Adapter 调用服务、C API 或测试夹具 | 集成测试和真实业务系统 |
| 影子比较 | 模型预期和真实系统同时运行 | 差分测试和实现验证 |

### 8.2 Trace 与重放

`Trace`需要同时服务 runner、evaluator、shrinker 和用户工具。

1. Runner 以稳定顺序写入事件；
2. Evaluator 读取消息、调用、State 变化和 Rule 结果；
3. Shrinker 删除或修改事件后重新执行；
4. 重放器恢复 fixture，并按顺序重放 observation 或 Action；
5. 用户可以查看原始 Trace、最小 Trace 和两者差异。

保存的反例不能依赖原始随机运行仍在内存中。
Trace 格式需要版本；未知事件必须产生明确诊断。

### 8.3 外部错误

Runner 分别处理：

- Adapter 无法连接或反序列化；
- ToolCall 参数不合法；
- Action 前置条件不成立；
- 外部系统执行失败；
- 执行成功但后置条件或不变量失败；
- 预算耗尽或用户取消。

这些情况不能合并成普通 Rule violation，也不能都作为一个模糊的运行时错误返回。

## 9. Benchmark 生成

Benchmark 生成是测试系统的应用能力。
它证明同一份 Rule 可以用于初始状态生成、Action 合法性、结果评分、负例构造和 Task 质量检查，但不会改变 Core。

### 9.1 Domain 与原子场景

一个测试领域由以下内容组成：

- State Type 和 adapter；
- State Rule 和 Policy Rule；
- Action 和 Tool；
- 原子场景；
- Task 模板；
- 难度维度；
- 评测 Rule。

每个原子场景至少说明：

- 适用前提 Rule；
- 对初始 State 的增量约束；
- 用户可见信息和隐藏信息；
- Agent 或用户需要完成的状态变化；
- Goal Rule；
- 一条可执行参考 Trace，或足以搜索解的结构化目标；
- 与其他场景的组合条件和冲突条件；
- intent、必要 Tool、最少步骤和风险标签。

### 9.2 生成过程

1. 选择测试领域、原子场景和难度配置；
2. 检查场景组合条件，并合并初始 Rule、Policy Rule 和 Goal Rule；
3. 使用普通生成器产生合法 fixture；
4. 构造或搜索一条参考 Trace；
5. 重放参考 Trace，检查每一步前置条件和最终 Goal；
6. 生成近边界样例和受控负例；
7. 检查评分条件是否能够接受正例并拒绝负例；
8. 按 Rule 语义、场景结构和 Trace 特征去重；
9. 按难度分层采样并导出数据集。

### 9.3 任务质量检查

- 初始 fixture 满足全部 State Rule；
- 参考 Trace 的每一步都合法，最终满足 Goal 和不变量；
- Task 不是意外无需动作即可通过的 no-op；
- 至少一个合理 mutation 会被 evaluator 拒绝；
- reward basis 与实际 criteria 双向一致；
- 已知信息足以开始任务，未知信息可以通过合法交互获得；
- 多解 Task 按最终结果和过程 Policy 判定，不强制唯一 Action 序列；
- 相同版本、配置和 seed 可以重新生成相同 Task；
- 数据集 split 按场景结构和语义去重，不只按自然语言文本去重。

### 9.4 难度维度

- 原子场景数量；
- 必需读写 Tool 数量；
- 最短参考 Trace 长度；
- Rule 和 Requirement 深度；
- 耦合字段数量；
- Action 分支和干扰 Tool 数量；
- 用户侧操作数量；
- 信息不对称程度；
- 确认、拒绝和转人工节点；
- 合法解数量与最短错误 Trace；
- 初始 State 到 Policy 边界的距离。

### 9.5 输出

| 输出 | 内容 |
|---|---|
| Tapas native | Rule、来源、fixture、参考 Trace、质量检查和重放信息 |
| Benchmark adapter | 特定版本的 τ-bench 或其他 benchmark 格式 |
| Runner adapter | Python、Gym、CI 或自定义 Agent harness 使用的数据 |

自然语言描述可以由模板或 LLM 生成，但可执行 Task 语义来自结构化场景和 Rule。
生成文本还要接受实体一致性、信息泄漏和目标一致性检查。

## 10. τ-bench 系列示例

### 10.1 参考范围

原始[τ-bench](https://github.com/sierra-research/tau-bench)提供零售和航空单控制场景。
[τ²-bench](https://arxiv.org/abs/2506.07982)增加 dual-control、用户侧 Tool 和组合式 Task 生成。
当前官方[τ³-bench 仓库](https://github.com/sierra-research/tau2-bench)继续加入电信、知识密集型银行、语音模态和任务修正。

Tapas 不在 Core 中复制 τ-bench 数据结构。
这些例子只用于验证一份 Rule 能否同时定义合法 State、Action 前置条件、不变量、Task 生成约束、评分、反例和边界。

官方数据和评分规则会变化。
兼容测试需要固定仓库版本、Task split、领域 fixture、reward basis 和 adapter 版本，不能只标记“兼容 τ³-bench”。

### 10.2 示例目录

```text
examples/
  general/                 # 通用编程能力
  rules/                   # 最小 Rule、支持范围和诊断
  tau/
    common/                # 共享 Model、Task、Trace 和结果工具
    retail/                # 零售单控制流程
    airline/               # 航空多约束流程
    telecom/               # dual-control 电信流程
    banking/               # 知识检索后的 Policy 检查
    generation/            # Benchmark 生成与任务质量检查
    boundaries/            # Rule、Task 和 Agent 边界
```

`general`只展示通用编程基础。
`rules`解释产品核心。
`tau`中的例子必须跑通完整测试流程，不能只演示孤立 API。

### 10.3 第一条产品闭环

| 编号 | 示例 | 内容 |
|---|---|---|
| T0 | `rules/retail_policy.tap` | 定义退货期限、订单状态、商品资格、退款方式和身份 Rule |
| T1 | `rules/retail_support.tap` | 分别检查 evaluator 对同一 Rule 的结构支持和实例准备情况 |
| T2 | `tau/retail/valid_order.tap` | 从外部 fixture 建立普通 Tapas 值，并生成合法订单和边界订单 |
| T3 | `tau/retail/single_tool.tap` | 执行一次退货或换货，检查前置、后置和全局不变量 |
| T4 | `tau/retail/alternative_traces.tap` | 验证不同合法 Trace 可以到达等价目标状态 |
| T5 | `tau/retail/counterexample.tap` | 对带有已知缺陷的实现搜索并缩减非法写操作 |

T0 中的 Rule 必须直接用于：

1. 检查已有订单；
2. 生成合法订单；
3. 产生退货期限等规则边界；
4. 筛选 Action 和生成 Action 参数；
5. 检查 Trace 中的 Policy；
6. 保持缩减后反例合法。

T5 不能只演示一个必然失败的断言。
示例需要提供一个外观正常但包含已知缺陷的退货或换货实现，例如在恢复最终余额后仍执行过未经确认的写操作，或者特定操作顺序造成重复退款。
Tapas 应找到该缺陷，并返回比原始失败更短的 Trace。

### 10.4 后续示例

| 编号 | 示例 | 内容 |
|---|---|---|
| T6 | `tau/airline/multi_constraint.tap` | 舱位、航线、时间、价格和改签 Policy 的多约束生成 |
| T7 | `tau/telecom/dual_control.tap` | Agent 和用户分别使用 Tool 改变共享环境 |
| T8 | `tau/telecom/composed_task.tap` | 组合多个原子故障并控制 Task 难度 |
| T9 | `tau/banking/policy_grounding.tap` | 将检索事实交给 Rule 决定回答或 Tool 操作 |
| T10 | `tau/end_to_end.tap` | 生成 Task、运行外部 Agent、评分、保存 Trace 和重放失败 |

T9 只验证检索结果之后的确定性 Policy 层，不把 RAG 加入 Tapas Core。

### 10.5 评分语义

τ-bench 风格例子区分：

| 评分 | 含义 |
|---|---|
| 最终结果 | 最终 State、环境断言和必要沟通是否满足 Task |
| 过程 Policy | 是否发生未经认证、未经确认或禁止的 Action |
| 参考相似度 | 实际 Trace 与一条参考 Trace 的相似程度，只用于诊断 |

参考 Trace 用于证明 Task 至少存在一种解、生成目标状态和提供调试基线，不默认定义唯一正确路径。

拒绝、转人工和只读 Task 需要显式沟通或环境 Rule，不能只用“数据库没有变化”判定成功。
Reward basis 引用空 criteria 时任务质量检查失败；criteria 存在但未进入 reward basis 时至少产生警告。

## 11. 产品成立条件

本节用于判断产品设计是否真正解决了问题，而不只判断接口是否实现。

### 11.1 规则复用

- T0 中的领域条件只在 Rule 中出现一次；
- 生成器、Action、evaluator 和 shrinker 不复制同一条件；
- 用户增加一条 Rule 后，不需要同步修改所有 evaluator；
- Rule 的结构支持和实例准备问题在执行前可见。

### 11.2 生成和搜索正确性

- 标记为成功生成的值再次通过原 Rule 的比例为 100%；
- Solver 返回的具体 Assignment 再次通过原 Rule 的比例为 100%；
- 缩减后的反例稳定触发同一个目标 failure；
- 搜索预算耗尽不会被解释为不可满足或不存在反例；
- 不支持的结构不会被当作通过或不可行。

### 11.3 重放和诊断

- 相同 Rule、版本、fixture、配置和 seed 产生相同的确定性输入序列；
- 保存的失败工件可以在兼容环境中独立重放；
- Violation 指向 Condition、Requirement 路径和 Origin；
- `Unsupported`指出具体节点、原因和需要的 Provider；
- 外部不可控 observation 被保存，或者明确标记为不能完整重放。

### 11.4 接入成本

- 外部 fixture 的字段和 Type 只映射一次；
- 纵向示例不重新实现一套业务数据库；
- Agent Tool schema 通过 adapter 使用，不要求在 Core 中复制；
- 外部接入代码保持集中，不能分散到 Rule、Action 和 evaluator 中；
- 数据格式变化主要影响 adapter，不要求重写 Rule。

### 11.5 缺陷发现

T0 至 T5 必须自动发现一个非显然的已知缺陷，并给出可理解的最小反例。
如果只能发现直接写在断言中的显然错误，产品闭环尚未成立。

以下任一情况出现时，应回到产品对象设计，而不是继续增加能力：

- 同一规则仍需在多个位置重复表达；
- 常见 Rule 大量落入`Unsupported`；
- 外部数据接入需要重建完整领域模型；
- 找到的反例无法稳定重放或解释；
- Benchmark 生成早于普通 Rule 生成和边界能力成熟；
- 为适配某个 benchmark 而修改 Core 语义。

## 12. 工具、版本与质量

### 12.1 工具接口

- CLI 提供校验、生成、边界、运行、探索和重放入口；
- C API 允许注册外部 Type、Action 和 adapter；
- LSP 展示 Rule 签名、组合关系、来源和 evaluator 支持范围；
- CI 保存 Counterexample、Trace、覆盖和重放命令；
- 外部 SDK 只封装公共语义，不建立第二套 Result。

### 12.2 版本与来源

跨运行保存的工件至少关联：

- Rule 语义哈希和必要的内容哈希；
- Model、Task、Evaluator 和 Backend 版本；
- Parameter、Capture、Variable 和 Provider 身份；
- Condition、Requirement、Action 和 Trace Event 来源；
- fixture、seed、预算和运行时 ABI；
- 外部 adapter 的 schema 或环境版本。

### 12.3 资源和取消

- Generator、Shrinker、Explorer 和 Solver 接受显式预算；
- 长时间操作可以取消，并返回已经完成的安全部分结果；
- 统计信息包括尝试数、接受率、搜索节点、缩减次数、覆盖增量和耗时；
- 缓存键包含影响语义的 Rule、绑定、Domain、配置、Provider、Backend 和 ABI 版本。

### 12.4 测试要求

- 每项能力同时提供 C 单元测试、Rule 回归测试和端到端领域示例；
- 随机测试自身固定 seed，并输出失败工件；
- 性能优化不改变 Condition 顺序、短路、Capture 读取、来源和 Requirement 路径；
- 未知 Provider、版本和 Trace Event 不会被静默接受；
- 公共 API 在稳定前明确标记设计阶段或实验状态。

## 13. 开放问题

以下问题由 T0 至 T5 和后续纵向示例决定，不在本文中提前固化 API：

1. `Model`和`Action`使用普通结构、标准库引用 Type，还是协议对象；
2. Action 前置、后置和全局不变量如何共享参数；
3. Tool 与 Action 使用一对一映射、组合关系，还是两种视图；
4. Trace 保存 Snapshot、Diff、可重放 Action，还是三者组合；
5. 外部系统不能完整恢复时，怎样定义有限重放；
6. Generator 的待生成变量是否与`optimize::Variable`共用概念；
7. `Incomplete`是否进入通用 Result status；
8. 结构支持检查和实例准备检查使用同一个 Result，还是两个独立接口；
9. Task Goal 始终使用 Rule，还是也允许目标 State 和目标 Term；
10. Counterexample 按 Condition 身份、Requirement 路径还是用户目标判断“同一个失败”；
11. `BoundaryCase`是否需要成为公共 Type；
12. Coverage 使用 Rule Item、Origin、语义哈希还是组合键作为稳定身份；
13. τ-bench 兼容只包含数据格式，还是同时包含评分和 runner 协议。

## 14. 明确不做

- 为了语言完整性增加类、宏、异步、并发或其他通用语法；
- 与 Python、JavaScript、Rust 等语言比较通用表达力；
- 使用 Tapas 重写大型应用、Agent 平台或业务服务；
- 为 Generator、Model、Benchmark 或 Solver 建立另一套约束 DSL；
- 承诺任意 Tapas 函数都能被 Generator 或 Solver 分析；
- 将某个商业或开源 Solver 变为 Core 的硬依赖；
- 将 RAG、LLM、语音、分布式执行或大型可视化平台加入 Core；
- 把匹配唯一参考 Trace 当作所有 Agent Task 的正确性定义；
- 在没有证据时把“预算内未找到”解释为“不存在”。

## 15. 实现顺序建议

先统一 checker 的 Result、结构支持检查和实例准备诊断，再用零售 Rule 跑通外部 fixture、合法生成和 Rule 边界。随后增加最小`Action`、`Trace`和 runner，完成终态评分、替代合法路径和已知缺陷的最小反例。只有这条闭环证明规则没有重复、常见 Rule 可被分析且外部接入成本可接受后，再扩展 dual-control、组合式 benchmark、覆盖探索和 solver backend。
