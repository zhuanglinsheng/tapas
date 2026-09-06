# solve：规则求解与概率采样

`solve` 提供 Rule 与 RuleInstance 的可满足性查询，以及有限定义域上保持输入分布的规则采样。`hold` 在本次查询配置的整数参数域内搜索满足规则的实例，并返回状态、witness、查询范围和诊断信息；返回结论只针对该配置域，不表示整个 Int 类型上的全局结论。`sample` 从用户指定的采样变量中产生候选输入，由 solver 判断是否存在满足 Rule 的补全，并使接受的采样变量保持可行域上的条件分布。

## 支持范围

### 类型

```tap
solve::HoldResult: Type
solve::SampleResult: Type
```

`HoldResult` 是具有以下字段的结构值：

| 字段 | 类型 | 含义 |
|---|---|---|
| `status` | `String` | `sat`、`unsat`、`unknown`、`unsupported` 或 `error` |
| `scope` | `String` | 当前固定为 `configured`，表示结论针对原 Rule 与本次配置的整数参数域 |
| `reason` | `String` | 不支持、错误、未决、绑定越界或诊断不可用的原因；普通冲突列表可用时为空字符串 |
| `witness` | `Unknown` | `sat` 时为输入 Rule 的实例，其他状态为 Nil |
| `integer_min` | `Int` | 本次查询采用的整数参数域下界；presolve 可以进一步收紧变量范围 |
| `integer_max` | `Int` | 本次查询采用的整数参数域上界；presolve 可以进一步收紧变量范围 |
| `conflicts` | `List[{rule: Rule, condition: String}]` | 在配置域与绑定背景下不能同时成立的条件集合，不保证最小 |
| `bindings` | `Dictionary` | 已绑定实例的参数字典；未绑定 Rule 查询为空字典 |

`SampleResult` 是具有以下字段的结构值：

| 字段 | 类型 | 含义 |
|---|---|---|
| `status` | `String` | `complete`、`incomplete`、`unknown`、`unsupported` 或 `error` |
| `reason` | `String` | 未完成、未决、不支持或错误的原因；完成时为空字符串 |
| `samples` | `List[Dictionary]` | 按接受顺序返回的完整参数赋值，键为 Rule 形参名，允许重复 |
| `candidates` | `Int` | 已产生的原始候选数 |
| `solver_calls` | `Int` | 候选可行性查询次数 |
| `core_calls` | `Int` | 用于 core 泛化的额外 solver 查询次数 |
| `cache_hits` | `Int` | 命中安全不可行区域、未提交 solver 的候选数 |
| `cache_regions` | `Int` | 返回时缓存中未被其他区域包含的区域数 |
| `trace` | `List[Dictionary]` | 启用 trace 时按候选记录的实验数据；默认是空列表 |
| `elapsed` | `Float` | 本次调用的墙钟运行时间，单位为秒 |

### 通用操作

```tap
solve::hold(rule: Rule | RuleInstance) -> solve::HoldResult
```

`hold` 查询是否存在满足 `rule` 的实例。传入 Rule 时，它搜索未绑定参数；传入 RuleInstance 时，它保留已有绑定并检查其余约束。`sat` 表示找到了配置域内且通过 VM checker 复核的实例，`unsat` 表示配置域内无解，`unknown` 表示超时等原因导致结论未决。

`unsupported` 表示规则使用了当前后端无法翻译的合法结构，`error` 表示配置、依赖、通信、模型或 witness 复核失败。业务代码必须先检查 `status`，不能根据 `witness` 或 `conflicts` 字段是否存在推断结果。

### 概率采样

```tap
solve::sample(
    rule: Rule,
    count: Int,
    space: Dictionary[String, Indexable],
    *,
    distributions: Dictionary[String, finite::Distribution]
        | finite::Distribution = {},
    rng: random::Generator =
        random::generator(random::pcg32_xsh_rr, 0),
    candidate_limit: Int = count * 1000,
    time_limit: Float | Nil = nil,
    core_budget_factor: Float = 1.0,
    fairness_interval: Int = 4,
    generalization: String = 'online_mass_core',
    scheduler: String = 'mass_fair',
    trace: Bool = false
) -> solve::SampleResult
```

`rule`、`count` 和 `space` 是必填位置参数。`*` 之后的参数均可省略，传入时必须使用参数名。`count` 是需要返回的样本数，必须为非负整数。

`space` 将 Rule 形参名映射到完整的有限定义域。键必须精确匹配 Rule 形参名，值必须支持 `index -> value` 读取并提供有限长度。出现在 `space` 中的形参是采样变量；其他形参是由 solver 补全、不承诺概率分布的 existential witness。

`distributions` 可以按形参名为采样变量指定一维有限分布。未出现的采样变量在其定义域上使用均匀分布；指定的分布必须满足 `finite::shape(distribution) == finite::index([len(domain)])`。这些一维分布按 Rule 形参顺序组成独立联合分布。

需要相关先验时，`distributions` 也可以直接传入一个联合 `finite::Distribution`。其 rank 必须等于采样变量数，第 `i` 个 shape 分量必须等于第 `i` 个采样变量的定义域长度，维度顺序采用 Rule 形参顺序。独立先验的缓存并集质量使用结构化划分计算；一般联合先验使用有限索引空间枚举精确计算，其时间与联合索引空间大小成正比，适用于能够完整枚举的小域。从分布得到坐标后，`sample` 使用每个 `domain[i]` 取得传给 Rule 的真实值。

`rng` 提供可重现的随机状态，采样会推进传入 Generator 的状态。省略时，每次调用使用由 `random::pcg32_xsh_rr` 和 seed `0` 新建的 Generator。

`candidate_limit` 是本次调用最多产生的原始候选数。被接受、被 solver 拒绝和命中已缓存不可行区域的候选都计入该上限。达到上限但尚未得到 `count` 个样本时，返回已产生的前缀并将结果标记为未完成。

`time_limit` 是整个 `sample` 调用的墙钟时间上限，单位为秒。`nil` 表示不增加总时间限制，但单次 solver 查询仍遵守求解后端的超时。达到总时限时返回已产生的前缀并标记为未完成。

`core_budget_factor` 是 core 泛化的额外求解预算系数 `β`。处理 `t` 个原始候选后，累计最多执行 `floor(β * sqrt(t))` 次额外 solver 查询。这些查询只用于将已证明不可行的具体输入泛化成更大的安全不可行区域，不包括对原始候选的可行性查询。`core_budget_factor` 必须大于零。

`fairness_interval` 控制多个未完成 core 泛化节点之间的调度。节点 `(I, J)` 表示所有满足 `I ⊆ C ⊆ I ∪ J` 的候选绑定集，其优先级是 `π(G(I) \ U)`：最小端区域相对于当前安全缓存并集 `U` 的未覆盖质量。该质量按照所选的独立或相关联合分布精确计算。一般查询选择优先级最大的节点；每第 `fairness_interval` 次额外查询改为推进创建时间最早的未完成节点。`fairness_interval` 必须大于等于 `2`。

`generalization` 选择失败候选的处理方式：

- `none` 不建立缓存，是纯 rejection 基线；
- `full` 只缓存完整失败赋值；
- `raw_core` 将采样绑定作为 CP-SAT assumptions，在同一次业务求解中缓存求解器直接返回的充分绑定 core；不支持 assumption core 的查询按安全契约退化为完整赋值，trace 分别记为 `unsat_raw_core` 和 `unsat_raw_fallback`；
- `deletion_mus` 按形参顺序逐项尝试删除绑定，完成后得到包含关系最小的 core；
- `minimum_core` 按绑定数从少到多枚举子集，第一个经证明 UNSAT 的子集具有最小基数；该模式具有指数级最坏查询数，只适合小维度实验；
- `online_mass_core` 使用可暂停的 `(I, J)` 分支定界搜索，也是默认生产模式。

`scheduler` 选择额外查询在未完成搜索之间的顺序：`mass_fair` 采用未覆盖质量优先并按 `fairness_interval` 公平轮转，`mass` 只按未覆盖质量，`fifo` 按创建顺序，`binding_count` 优先推进当前候选绑定较少的搜索。它不改变缓存安全性或输出分布，只改变有限预算投向哪里。

`trace` 为 `true` 时，`SampleResult.trace` 按候选顺序记录 `outcome`、累计调用数、缓存命中数、缓存区域数、活动搜索数和精确 `cache_mass`。一般联合分布的质量需要枚举整个有限索引空间，因此 trace 会增加运行时间；性能实验应同时报告关闭 trace 的端到端时间。

`SampleResult` 除样本和原有计数外，还返回 `cache_hits`、最终 `cache_regions` 与 `trace`。总 solver 调用数为 `solver_calls + core_calls`；`solver_calls` 只计原始候选查询，缓存命中不计入其中。

`sample` 只将经过完整求解并确认存在合法补全的候选加入结果。只有可靠的 UNSAT 结论才能产生缓存区域；UNKNOWN、超时和不支持的规则不得当作 UNSAT 并跳过，而应结束本次调用并在结果中报告原因。失败候选创建可暂停、恢复的 `(I, J)` 分支搜索；每个已经查询并证明 UNSAT 的端点立即加入安全缓存，较大的安全区域会删除被其包含的旧缓存区域。分支、继承已知端点、零质量剪枝和缓存整理不消耗 solver 预算。第一版固定串行处理候选且 solver 使用单 worker，不提供 `workers` 参数。

例如：

```tap
let R = rule (size: String, quantity: Int, discount: Int) {
    discount <= quantity * 10
}

let result = solve::sample(
    R,
    100,
    {
        'size': ['small', 'medium', 'large'],
        'quantity': [1, 2, 3, 4]
    },
    distributions = {
        'size': finite::categorical([1.0, 2.0, 7.0])
    },
    candidate_limit = 10000,
    time_limit = 30.0
)
```

`size` 和 `quantity` 是采样变量；`discount` 由 solver 补全。`size` 使用指定权重，`quantity` 使用均匀分布。每个接受的 assignment 都是包含三个形参完整赋值的 Dictionary；概率保证只适用于 `size` 和 `quantity`。

### 支持的规则

`hold` 支持 Int、Bool、有限字符串 Enum 参数及具体值绑定，支持整数线性算术、标量比较、布尔组合、整数区间与点集，以及 `restrict`、动态 Rule 和非递归嵌套 Rule。已绑定的固定字段结构参数，以及由标量参数构造并传给子 Rule 的固定字段记录，也可以参与受支持的字段读取、算术和条件。

### 限制

`hold` 不执行任意用户函数来探测约束，不支持浮点、变量乘变量、除法、未绑定结构参数搜索、符号索引、递归 Rule、匿名词法作用域或未知 Extension。规则包含当前源码 IR 未记录的完全未使用局部初始化表达式时，查询返回 `unsupported`。

默认整数参数域为闭区间 `[-10¹², 10¹²]`。该域约束输入 Rule 的未绑定 Int 参数和 RuleInstance 已绑定的 Int 参数，但不改变 Tapas Int、普通 checker、捕获常量或中间算术结果的语义。所有函数只接受签名及约束明确允许的输入；配置或模型不合法时返回 `error`，不会静默回退。

## 使用说明

入门程序见[求解示例](../../../examples/solve/README.md)。以下命令从仓库根目录执行。

### 依赖

Tapas 构建不依赖 Python 或 OR-Tools；调用求解需要 Python 3 和本目录的依赖：

```sh
python3 -m venv .venv-solve
.venv-solve/bin/python -m pip install -r src/stdlib/solve/requirements.txt
TAPAS_SOLVE_PYTHON="$PWD/.venv-solve/bin/python" build/bin/tapas examples/solve/feasibility.tap
```

若默认 `python3` 已安装依赖，可省略 `TAPAS_SOLVE_PYTHON`。它接受解释器路径，不接受带参数的 shell 命令。

### 基本查询

```tap
let R = rule (x: Int, y: Int) {
    x in rules::range(0, 10)
    y in rules::range(0, 10)
    x + y == 12
}
let S = rules::restrict(R, 'x': 4)
let result = solve::hold(S)
if (result::status == 'sat') {
    let values = arguments(result::witness)
    print(values[0], values[1]) // 4, 8
    assert(result::witness)
}
```

## 配置整数参数域

默认域为闭区间 **[-10¹², 10¹²]**。它是查询的明确约束，在 presolve 前加入，不再是分析失败后的临时搜索截断。

通过环境变量配置，每次 `hold` 调用都会读取：

```sh
TAPAS_SOLVE_INT_MIN=0 TAPAS_SOLVE_INT_MAX=1000 build/bin/tapas examples/solve/feasibility.tap
```

- 未设置的一端使用默认值；上下界都包含，允许负数和单点区间。
- 空字符串、非整数、颠倒区间及超出支持整数端点的配置返回 `error`，不会静默回退。端点须严格位于有符号 64 位整数的最小值与最大值之间；CP-SAT 仍会验证具体模型的数值限制。
- `sat` 表示找到配置域内且通过 checker 的实例；`unsat` 表示配置域内无解；`unknown` 表示求解超时等未决情况。
- 域同时约束输入 Rule 的未绑定 Int 参数和 RuleInstance 已绑定的 Int 参数。范围外的绑定实例返回 `unsat`。
- 此设置不改变 Tapas Int 类型、普通 checker、捕获常量或中间算术结果。嵌套规则按实参表达式展开，不额外截断中间值。
- 不自动扩大配置域。例如 `x == 1000000000001` 在默认配置下返回 `unsat / configured`；把上界调高后才可能返回 `sat`。

后端从配置域出发传播线性界限，保留 `points` 的空洞，按逻辑分支处理约束，检测差分约束负环并消除固定变量。预算耗尽只影响收紧程度，求解仍保留完整约束。

模块接口及算法见下文的 presolve 架构说明。

编译流程为 RuleIR 与调用时捕获值 → 内存中的 JSON 请求 → 通用模型 IR → presolve → CP-SAT 模型 → witness → VM 复核。每个 VM 首次查询时按需启动 Python 后端，后续查询复用该进程和 OR-Tools 导入；每次查询仍独立构建模型，不缓存捕获值或求解结果。请求和响应通过带长度前缀的本地 socket 传输，不生成中间 Python、JSON 或结果文件；Python 使用 `-B` 禁止写入字节码缓存。VM 清理时回收后端进程，通信失败后下一次查询重新启动后端。

单次 CP-SAT 求解限时 5 秒；每次通信使用整体 15 秒截止时间（包含发送、编译、求解和接收）。请求上限 16 MiB，响应上限 4 MiB。缺少解释器、需要进入 CP-SAT 阶段但 OR-Tools 不可用、模型无效和 witness 复核失败均返回 `error`，不会伪装成无解。

## 已支持的直接翻译

- Int、Bool、有限字符串 Enum 参数；具体值绑定。
- 已绑定的固定字段结构参数（含嵌套结构）；从无条件参数等式中取得全部固定值的 Rule，包括 `restrict` 构造的 Rule。
- 整数加减、正负号、常量乘法，标量比较。
- `and`、`or`、`not`、`implies`，包括完整的双向布尔等价编码。
- Int / Bool / Enum 点集和整数区间；规则内直接调用 `rules::points` / `rules::range`，以及捕获预先构造的域。构造时的区间端点和点集成员须已知，暂不支持依赖求解变量的域构造。
- `restrict`、动态 Rule、非递归的嵌套 Rule、嵌套实参表达式。
- 当前上下文中的标量、域、列表和字典快照；已知索引及字段读取。
- 所有声明均在 IR 中有引用的纯局部初始化表达式；同一查询内复用其编译结果。

第一版不执行任意用户函数来探测约束，不支持浮点、变量乘变量、除法、结构参数搜索、符号索引、递归 Rule、匿名词法作用域及未知 Extension。明确跳过的常量短路分支不要求后端理解其内容。

当前源码 IR 不包含完全未使用的局部初始化表达式。为避免漏掉初始化过程中的行为，包含此类声明的 Rule 暂时返回 `unsupported`。这是后续查询准备阶段的工作，不影响普通 checker。

调用时导出的数据是本次查询的独立快照。当前后端只允许已知纯操作，Tapas VM 在求解期间同步等待，随后在同次调用中复核原规则。返回的 witness 仍遵循普通 RuleInstance 语义；调用结束后若改变原规则的捕获环境，再次检查它可能得到不同结果。

## 无解与失败诊断

`HoldResult.conflicts` 是列表，每一项包含实际的 Rule 对象和可读的条件字符串。直接使用通用打印即可：

```tap
pprint(result::conflicts)
```

完整表示显示 Rule 名称与类型签名，例如 `Rule #12[Int]`；条件保留在 `condition` 字段中，无需给 Rule 手工映射名称。

- 列表表示一组共同冲突的条件，不表示每条条件单独有错，也不保证最小核心。它始终以 `integer_min/max`、参数类型和 `bindings` 为背景。
- `rule` 是可继续交给 `rules::inspect` 或 `solve::hold` 的对象，不是名称字符串。直接属于输入 Rule 的条目保留其身份；`restrict` 内部使用原 Rule 的快照，所以快照与原变量不保证 `identical`，可通过共享的 RuleIR 判断来源。不要从变量别名猜测唯一 Rule 名称。
- 直接正向包含的嵌套 Rule 可以展开；or/not/implies 内的调用保留在外层条件中，不能把分支条件错误地独立列出。
- 条件文本用于展示，不是重新解析执行的接口；尚未提供源码行号。列表不会按旧版的四条文本上限截断；超出诊断预算或单条文本传输限制时返回空列表，并用 `reason` 说明详情不可用。
- `sat`、`unsupported`、`error`、`unknown` 的冲突列表为空。绑定值越界也返回空列表和明确 `reason`，不伪造 Rule 条件。
- 空列表本身不能用于判断是否可行，业务代码应先看 `status`。普通 `unsat` 且列表有效时，`reason` 为空。
- presolve 使用有预算的删除证明，CP-SAT 使用原配置域上的独立 assumptions 模型；方法细节不会再混入普通冲突展示。诊断失败不改变已经证明的无解结论。

## 分析与编译架构

`hold` 是存在性查询，不拥有 presolve 算法。当前共用管线为：

```text
C：Rule / RuleInstance + 调用时捕获快照
  → lowering.lower(request) → Query
  → presolve.presolve(query, options) → PresolveResult
  → cp_sat.compile_model(query, analysis, cp_model) → CompiledModel
  → cp_sat.hold(compiled) → 候选结果
  → C / VM：恢复原 RuleInstance，运行 checker 复核
```

这些是 solve 包内部的 Python 接口，本次未新增 Tapas 层的 `solve::presolve` 语法。`lower`、`presolve` 无 OR-Tools 依赖，可以供模型检查、未来的范围查询或其他后端复用。`Query.integer_domain` 保存明确的查询整数域，presolve 将其作为初始约束；纯分析调用也可保留无穷域，但 CP-SAT 编译拒绝未证明有限且未配置的域。`compile_model` 不执行搜索；未来的最小化、最大化、枚举查询可使用 `CompiledModel.model` 和 `variables`。不要跨捕获快照复用分析结果。

## 模块边界

| 文件 | 责任 |
|---|---|
| `solve.c` | 导出受身份检查的捕获快照、管理 VM 所属进程、验证通信及恢复结果 |
| `model_ir.py` | 不可变的整数仿射表达式、布尔公式、变量和 Query |
| `lowering.py` | RuleIR、局部值、嵌套规则、参数替换、类型检查和纯函数白名单 |
| `presolve.py` | 数学整数域传播、差分约束分析、域内等价简化；不截断未知范围 |
| `cp_sat.py` | 已声明域的 CP-SAT 建模、存在性查询 |
| `diagnostics.py` | 基于原始条件的充分冲突集合及文本诊断，不复用收紧域 |
| `backend.py` | 协议入口与阶段调度、错误转译 |

构建时按依赖顺序将模块嵌入可执行文件，启动时只在内存中加载模块。仍然使用 VM 内复用的后端进程，不生成临时 Python、请求或响应文件。

## 返回的分析信息

`PresolveResult` 包含 `domains`、`predicate`、`infeasible`、`steps`、`rounds`、`converged`。

- `domains[i]` 与 `Query.variables[i]` 对齐，是若干整数闭区间的并；无穷端点表示尚未证明有限界限。有限端点始终使用 Python 整数计算。
- 所有符合查询声明域的原解都在返回域内。返回域是**必要条件的近似**，不是变量可取值的精确投影。
- 在这些域内，`predicate` 与原查询等价；可移除已由域保证的约束，并替换固定值。
- `infeasible` 表示仅根据原始约束证明了矛盾，可对所声明的查询域返回 `unsat`。
- `converged` 只表示当前抽象传播达到不动点，不表示得到了最优 bound。

`CompiledModel` 包含收紧后的 `domains`、`analysis`、变量映射及模型。`Query.integer_domain` 保留最初声明的范围；收紧过程不能扩大它。配置域属于查询语义，不是原 Rule 在整个 Int 类型上的已证明范围。

## 算法

### 1. 规范化

把嵌套规则按实际参数展开为共享变量上的公式；捕获值作为本次查询的常量。线性表达式规范化为 `Σ aᵢxᵢ + c`，合并重复变量、删除零系数。布尔结构转成 `and` / `or`，否定下推到原子谓词，`implies(a,b)` 表示 `not a or b`。展平同类连接、去重和折叠布尔常量。

比较最终使用 `<= 0`、`== 0`、`!= 0`。整数严格不等式平移 1；用系数 gcd 归一化，等式常数不能被 gcd 整除时立即矛盾。不等式除法采用向上/向下取整，不能截向零或使用浮点近似。

### 2. 必须成立的差分约束

从顶层合取中提取 `x - y <= k`，建边 `y → x`，权重 `k`，加入代表常量 0 的锚点。等式产生双向边。Bellman–Ford 检测负环；负环可证明无界初始域上的矛盾，如 `x < y and y <= x`。从 0 出发的最短路径给上界，反向图从 0 出发的路径给下界。

绝不从 `or` 分支提取全局边。复杂线性约束交给下一阶段。该阶段最坏为 O(VE)，受共享工作预算限制；未完成的路径分析仍只给出安全的界限。

### 3. 域传播

对 `Σ aᵢxᵢ + c <= 0`，以其他变量贡献的最小值 `m` 推导：

- `aᵢ > 0`：`xᵢ <= floor(-m / aᵢ)`；
- `aᵢ < 0`：`xᵢ >= ceil(-m / aᵢ)`。

其他项没有有限最小贡献时，不推导该端点。每条线性约束使用一次贡献快照，单轮 O(项数)，后续轮次传播新增范围。等式按两个方向处理；单变量 `!=` 删除对应整数点；`points` 的析取保留不连续域。

`and` 顺序收缩域，外层迭代至不动点或预算耗尽。`or` 从同一个输入域独立分析各分支，丢弃已证明矛盾的分支，再对每个变量取域的并。一个分支没有约束某变量，就不能从另一分支推导该变量的全局 bound。分支间关系可能被近似掉，但保留原布尔谓词保证正确性。

默认最多 32 轮、100000 个工作单位，分支合并超过 256 个区间时退化到区间包络。预算停止只损失精度，不引入新的矛盾结论。当前使用全量轮次而非增量依赖队列；这是后续大模型性能优化点。

### 4. 建模简化与后端

根据证明域替换 singleton 变量、合并线性项，删除被域蕴含的谓词。CP-SAT 直接使用区间并定义域；固定变量用整数常量，不额外建变量。顶层合取及线性约束直接提交；嵌套逻辑采用完整双向重化，同一谓词复用布尔变量。

运行时默认声明整数参数域为 [-10¹², 10¹²]，支持通过 `TAPAS_SOLVE_INT_MIN` 和 `TAPAS_SOLVE_INT_MAX` 配置。每次调用独立读取配置并随协议版本 3 的请求发送；在 presolve 前应用，而不是建模结束后补界限。范围外的已绑定 Int 参数同样不满足查询。限制针对输入 Rule 参数，嵌套实参表达式、捕获常量和中间算术值不额外截断。

默认不再存在自动扩展或临时截断策略。CP-SAT `INFEASIBLE` 返回 `unsat`，超时未判定返回 `unknown`。结果 `scope` 固定为 `configured`，明确结论针对所声明的域；`integer_min/max` 记录此次有效配置。普通 Tapas Int 和 checker 语义不变。配置语法、顺序和数值范围在进入后端前检查，后端再次校验；模型溢出仍返回 `error`。

## “最佳 bound” 的边界与后续工作

一般整数约束下，精确求一个变量最小值/最大值本身就是优化问题，不能承诺轻量 presolve 总能求出。当前实现安全、预算可控的界限推导，没有实现逐变量最优化。

后续可在此架构增加：

1. 依赖队列与增量传播，减少大型稀疏模型的重复扫描；
2. 仿射变量消元、同余域传播及更强的关系域；
3. 可选的逐变量 min/max 精化，共享整体时间预算。最优端点的结论必须同时携带查询的声明域，不能宣称整个 Int 类型上的全局最优；
4. Tapas 层的范围分析结果接口，明确区分声明域、必要范围和实际可达值。

## 验证

`test_presolve.py` 包含取整、gcd、离散空洞、分支保护、无界负环、超默认边界和建模消元测试，并用固定种子的随机公式穷举验证：域收缩不丢解，域与残余公式组合等价于原式。低预算和区间包络退化也执行同样的等价检查。CP-SAT 结果另外与穷举解集比较。

参考：[OR-Tools 整数建模与区间并定义域](https://github.com/google/or-tools/blob/stable/ortools/sat/docs/integer_arithmetic.md)。

## 冲突解释接口

`Query.constraints` 保存谓词、来源 Rule 索引和展示表达式。直接作为必要条件调用的 Rule 展开其来源条件；在 or/not/implies 内部的调用保留外层来源，不能拆成独立必要条件。类型与参数替换仍遵循原降级流程。

`presolve_conflict(query)` 对已证明无解的集合执行有预算的删除检查，仅在候选子集再次被证明无解时接受删除。`solver_conflict(query, cp)` 在原配置域上建立带 assumptions 的独立模型，不能使用 `analysis.domains` 中依赖于被撤销条件的界限。

`explain_unsat` 返回 `(reason, rows)`。普通冲突 `reason` 为空，`rows` 中每项是 `{rule: index, condition: text}`；域外绑定、诊断异常或超过预算时给出 reason 和空列表。最多 256 项、每项 UTF-8 文本最多 4000 字节，超限不能悄悄裁掉部分核心。诊断不承诺最小核心，不改变已有 UNSAT 判定。

协议版本 3 在 witness 参数之后增加冲突数及逐项的 Rule 索引、十六进制编码文本。C 层校验数量、状态、索引范围与文本类型，恢复为 `{rule: Rule, condition: String}`，并通过引用计数保留 Rule 对象。`HoldResult` 增加 `conflicts` 和 `bindings`，后二者在所有结果中都存在。配置域、输入实例绑定是冲突成立的背景，不伪装成普通 Rule 条件。

注意 `rules::restrict` 的原规则常量是快照。冲突指向模型实际使用的 Rule，快照与原变量可能有不同对象身份但共享 RuleIR。不得为了展示名称而把两个闭包环境不同的 Rule 强行合并。

验证包括冲突充分性、正向嵌套来源、否定分支保护、列表完整性、类型结构、绑定背景、Rule 生命周期，以及源码和字节码查询。

## 静态类型与编辑器支持

`hold` 的静态签名为 `Function[Rule | RuleInstance] -> solve::HoldResult`。
标准包索引同时识别原生函数和以函数值导出的可调用对象；包内 Type 可以在符号的 `type` 中声明结构定义，签名通过通用名称解析引用它。旧式 `type: "Type"` 继续使用已有的内建类型规则，无需修改扩展 ABI 或 VSCode 插件。

HoldResult 的 status、scope、reason 为 String；integer_min/max 为 Int；conflicts 为 List[{rule: Rule, condition: String}]；bindings 为 Dictionary。静态 witness 使用 Unknown，运行时仍由 AnyType 承载：它可能是实例或 Nil，而当前尚未实现根据 status 自动收窄类型。这保持既有的 checker/witness 调用方式，不能仅凭字段存在就假定求解成功。

语言服务器对结果变量显示名称及结构定义，支持字段 hover、字段补全以及 conflict 条目的字段推导。新增检查覆盖源码、字节码和实际 LSP 请求。


## 已固定结构参数与 retail

`solve::hold(rules::restrict(Model, 'state': state, ...))` 在所有参数都有明确常量等式时，将这些值用于查询准备，保留所有原始约束和重复限制，不执行用户函数或 checker 来替代求解。只识别无条件 `Parameter == Constant`，以及保持相同参数身份的直接 Requirement；不会从否定、析取或蕴含分支推断固定值。扫描有深度和工作量上限；未完全固定时回到普通编译路径。

结构类型按字段导出并递归验证，固定字段访问可以参与普通条件、算术和嵌套 Rule。配置整数域也约束根结构参数中声明为 Int 的字段；捕获常量和嵌套算术中间值不因此被截断。未绑定结构参数仍返回 `unsupported`，不自动猜测字符串域或搜索结构字段。

字典快照保留查询内对象身份，符合运行时字典相等按身份判断的语义，不使用 Python 的结构相等替代。`restrict` 保留值快照；修改原始数据不会重写已构造的限制。规则捕获仍在每次查询时重新读取。witness 绑定到输入 Rule 并复用对应固定值，不通过重建字典破坏身份；仍由 VM 复核。

模块导出数据以快照参与固定键查表，不调用模块中的用户函数。零未知变量的查询也通过通用 lowering、presolve 和 CP-SAT 路径处理；无解可由 presolve 提前证明。retail 使用这条路径比较完整实现和缺陷实现，均返回普通 `HoldResult`。

验证覆盖 `test/solve/records.tap` 的源码与字节码，以及 `retail_contract_detection` 中两份实现的全部查询。

### 构造记录中的符号字段

源码 Rule 可以用标量参数构造固定字段记录，再将该记录传给结构参数子 Rule。lowering 保留字段中的 Int、Bool、Enum 符号变量，递归检查参数类型，字段读取直接引用原变量。键必须是已知标量；构造记录没有捕获的运行时身份，其相等比较仍不支持，不会擅自改成结构相等。

这支持 retail 中未知支付额度的输入生成，返回的 witness 仍绑定原标量参数。未绑定的顶层结构参数、符号目录索引与任意用户函数调用仍不支持。相关实验见 `examples/retail/test_generate_valid_exchange.tap` 和 `test_generate_rejected_exchange.tap`。

有限字符串 Enum 符号可传入 String 参数或记录字段；保留原 Enum 变量及编码用于求解与 witness 恢复，不扩大为无限字符串域。retail 的 `test_generate_joint_inputs.tap` 联合搜索额度、认证与确认，并覆盖错误身份下要求允许的无解查询。
