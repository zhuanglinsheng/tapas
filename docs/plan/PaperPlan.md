# Tapas 论文写作计划

> 状态：研究规划草案
>
> 更新日期：2026-09-05
>
> 研究主线：以可执行关系契约实现抗漂移的状态测试



## 1. 论文定位

本文研究的是一个软件测试问题：

复杂业务状态测试中的 generator、precondition、next-state model、oracle 和 shrinker 往往分别维护相同的领域规则，因而产生重复定义和语义漂移；常规 shrink 又容易破坏跨步骤数据依赖，使缩减后的轨迹非法或不可重放。

本文的方法是把状态空间、动作适用性以及状态、动作、真实输出与后状态之间的预期关系写入同一个可执行契约模型 $R$，其核心转移关系为 $C(s,a,o,s')$；测试系统再从 $R$ 获得相互一致的适用性判定、约束感知生成、真实执行 oracle 和轨迹修复。

单一事实来源是设计原则；关系契约的测试解释一致性、依赖修复型缩减及其对政策演化漂移的影响才是候选研究贡献。语言、RuleIR 和求解器只是实现并检验这些贡献的载体。

建议的英文工作标题：

> **Drift-Resistant Stateful Testing from Executable Relational Contracts**

建议的中文标题：

> **基于可执行关系契约的抗漂移状态测试**

核心研究命题是：

> 能否从同一个关系式状态契约模型中派生语义一致的动作适用性判定、约束感知生成、真实执行 oracle 和依赖修复型轨迹缩减？与分别维护测试工件相比，这种方法能否减少政策演化造成的语义漂移，并在稀疏合法动作空间中提高测试与反例缩减质量？



## 2. 愿景与创新边界

本文希望提出一个规则系统测试平台，核心闭环是：

1. 用贴近业务概念的类型和规则描述领域；
2. 判断给定对象或状态是否满足规则；
3. 求解规则是否可满足，并构造有效见证；
4. 生成有效状态和动作序列；
5. 执行真实系统并判定状态转移；
6. 搜索违反性质的轨迹；
7. 在保持有效、可重放和仍然失败的前提下缩减反例。

以下元素单独来看都不是充分的新颖性来源：

- `Enum`、`Range` 和 refinement/domain 类型；
- precondition、postcondition、invariant 与 contract；
- `satisfy`/SAT 检查；
- 状态机、模型检查和 property-based testing；
- API schema 驱动的测试生成；
- 一门声明式 DSL；
- 把关系 $C(s,a,o,s')$ 的部分变量留空并交给 CP/SAT/SMT 求解；
- 把 property 重化为 IR，再交给不同 evaluator 或 runner 解释。

CP/SAT/SMT 在本文中是实现机制，不是算法创新。关系的多向求解是系统基础，也不单独构成论文贡献。

业务规则驱动的 Property-Based Testing (PBT) 本身也不是新颖性来源。Aichernig 和 Schumi 已从工业 Web Service 的业务规则模型派生 EFSM、请求序列、输入生成器和 FsCheck 测试，并连接真实 SUT。因此，“从一份业务规则模型生成状态测试”不能作为本文的贡献。

真正需要验证的创新主张是：

> 一个关系式状态契约模型可以被赋予相互一致的测试解释，并用于真实黑盒轨迹的生成、判定和依赖修复；这种方法能够减少政策演化时的测试语义漂移，并在存在跨步骤依赖的状态测试中产生更短、合法且可重放的反例。

“同一 Rule 被多个 API 调用”还不够。生成、判定和修复必须实际消费同一 RuleIR，而不是在 evaluator 内重新复制业务条件；同时必须说明这些解释之间的语义关系。若实现仍要求分别编写 generator、precondition、postcondition、abstract next-state function 和领域专用 shrinker，论文的中心主张就不成立。

第一篇论文必须实现并评价**依赖修复型轨迹缩减**：修改或删除早期步骤后，runner 重放真实系统，根据新的实际状态重新求解后续 action 参数，再判断是否仍触发同类违反。它应作为一项可独立比较的测试算法，而不是可选功能，也不能被夸大为新的通用求解理论。

Tapas 不消除所有手写测试代码。真实系统调用、动作到 API 的绑定、状态与输出的观测、环境重置、搜索预算以及复杂度度量仍可由用户提供。论文所主张消除的是这些组件对**领域合法性和预期状态转移谓词**的重复编码。



## 3. 数学模型

把被测系统抽象为：

$$
\mathcal{M} = (S, I, \{P_k\}, \{D_k\}, \{O_k\}, \{C_k\})
$$

其中：

- $S$ 是合法状态空间；
- $I \subseteq S$ 是初始状态集合；
- $P_k$ 是第 $k$ 类动作的参数空间；
- $D_k(s,p)$ 判断动作参数 $p \in P_k$ 在状态 $s$ 下是否可用；
- $O_k$ 是动作输出空间；
- $C_k(s,p,o,s')$ 是期望的状态转移关系。

具体动作实例为：

$$
a = k(p)
$$

真实实现的执行结果为：

$$
E(s, k(p)) = (o, s')
$$

一次有效测试需要满足：

$$
s \in S \land D_k(s,p)
$$

发现错误意味着真实执行结果违反领域契约：

$$
E(s,k(p))=(o,s') \land \neg C_k(s,p,o,s')
$$

状态序列测试把单步关系扩展为轨迹：

$$
\tau = s_0, a_0, o_0, s_1, \ldots, a_{n-1}, o_{n-1}, s_n
$$

其中 $s_0 \in I$，每一步均满足动作可用条件，而测试目标是搜索使某个 invariant 或 transition contract 失效的最短或最简单轨迹。

传统状态测试通常为同一领域政策 $R$ 分别实现：

$$
G_R,\ P_R,\ N_R,\ O_R,\ H_R
$$

其中 $G_R$ 是 generator，$P_R$ 是 precondition，$N_R$ 是 abstract next-state function，$O_R$ 是 oracle，$H_R$ 是 shrinker。它们由开发者分别维护，因此政策从 $R$ 演化为 $R'$ 时，任一工件未同步更新都可能产生语义漂移。

令第 $k$ 类动作的契约单元为 $R_k=(P_k,D_k,C_k)$。Tapas 不保存五份领域政策的编码，而是为 $R_k$ 提供领域无关的测试解释：

$$
\operatorname{Enabled}_{R_k},\
\operatorname{Generate}_{R_k},\
\operatorname{Oracle}_{R_k},\
\operatorname{Repair}_{R_k}
$$

对第 $k$ 类动作，至少要求：

$$
\operatorname{Enabled}_{R_k}(s,p)
\iff
p \in P_k \land D_k(s,p)
$$

$$
p \in \operatorname{Generate}_{R_k}(s,\phi)
\Rightarrow
\operatorname{Enabled}_{R_k}(s,p)
\land
\exists o,s'.\ C_k(s,p,o,s') \land \phi(s,p,o,s')
$$

其中 $\phi$ 是可选的测试目标；未指定时为 `true`。$D_k$ 表示动作是否允许被测试调用，$C_k$ 则描述允许调用后可能出现的成功、拒绝或其他预期结果，不能把“业务拒绝”误写成“动作不可调用”。

真实系统执行后，oracle 检查真实观察值，而不是求解器预测值：

$$
E(s,k(p))=(o_{\mathrm{real}},s'_{\mathrm{real}})
$$

$$
\operatorname{Oracle}_{R_k}(s,p,o_{\mathrm{real}},s'_{\mathrm{real}})
\iff
C_k(s,p,o_{\mathrm{real}},s'_{\mathrm{real}})
$$

这里的“一致”不是几个 API 恰好接收同一个对象，而是它们共享同一关系语义并满足上述性质。对不支持存在量化或完备求解的规则片段，必须弱化为单向可靠性并显式返回 `Unknown`/`Unsupported`。

政策演化写为 $R \rightarrow R'$。若测试工件 $X$ 在某个输入上的行为不再等于新契约在该解释下的含义，就发生语义漂移：

$$
\exists x.\ X(x) \ne \llbracket R' \rrbracket_X(x)
$$

任意程序的语义等价通常不可直接计算。实验应预先定义政策原子、变更脚本和有界见证域，在这一范围内测量不一致，而不能声称解决一般程序等价问题。

反例缩减可表为：

$$
\arg\min_{\tau'} \operatorname{size}(\tau')
$$

约束为：

$$
\operatorname{Valid}(\tau') \land
\operatorname{Replayable}(\tau') \land
\operatorname{Violates}(\tau')
$$

依赖修复型 shrink 只能重新求解后续测试输入，不能伪造真实输出或后状态。每个候选轨迹都必须重置或恢复被测系统、真实执行并重新观察：

$$
\operatorname{Replay}_{E}(\tau')
\land
\operatorname{SameViolation}_{R}(\tau')
$$



## 4. 预期论文贡献

第一篇论文应控制在三项主贡献，避免把每个语法特性或后端列为贡献：

1. **一致的关系契约测试语义。** 定义关系式状态契约及其适用性、生成、真实执行 oracle 和修复解释，给出核心规则片段上的一致性性质、不完备边界与实现。
2. **依赖修复型状态轨迹缩减。** 给出一种面向真实黑盒系统的 shrink 算法：删除或简化早期步骤后，根据真实重放状态重新求解后续动作输入，同时保持轨迹合法、可重放并触发同类违反。
3. **规则演化与测试效果的实证研究。** 通过预先定义的政策演化、多个领域、真实或历史缺陷、系统化 mutation 与消融实验，比较语义漂移、有效动作比例、缺陷发现能力和反例质量。

单一事实来源和 Tapas 系统本身是贯穿三项贡献的设计与工程载体，不单列为 novelty。如果系统尚不能兑现前两项，就不宜急着写完整研究论文；可以先写 tool/demo paper，但其影响力和说服力会明显较弱。

### 非核心加分项：统一语言与系统设计

将检查、可满足性判断、动作生成、真实执行 oracle 和轨迹 repair 集成进同一门语言，是重要的系统与可用性贡献，但不应替代上述三项核心贡献。仅仅把已有能力包装成几个语法构造，容易被视为 engineering integration 或 syntactic sugar；真正有价值的是语言让关系契约成为共享的语义对象，并使一致性尽可能由构造保证。

Tapas 的语言与 RuleIR 应具体承担以下职责：

- 让领域合法性和预期状态转移只定义一次；
- 用 Enum、Range、结构 Type 和 RuleInstance 表达业务概念，而不要求用户把领域对象手工编码为整数；
- 对状态、动作参数、输出和后状态进行静态检查，并在调用处提供统一的类型与引用体验；
- 让 runtime checker、solver、test runner 和 repair/shrink evaluator 消费同一中间表示；
- 对不能求解、只能运行时检查或可能返回 `Unknown` 的构造提供明确诊断；
- 允许增加新的领域无关 evaluator，而不修改已有领域模型。

论文不能仅以示例“看起来更简洁”证明语言设计优秀。若将其作为次级贡献，应至少报告：

- 相同业务模型的代码量、辅助代码量和重复领域谓词数量；
- 需要逃逸到宿主语言或直接求解器编码的规则数量；
- 编译器在测试运行前发现的类型、引用和解释能力错误；
- 非作者建模者的完成时间、修改时间和错误数量；
- 新增 evaluator 或更换求解后端时领域模型需要修改的范围。

因此，第一篇论文可以把 Tapas 描述为承载核心方法的统一语言与系统，并把良好语法、业务可读性和工具体验作为增强证据；不能把“首次将这些功能放进一门语言”列为独立 novelty。若未来发现 `Type[Types; Values]`、Rule/RuleInstance 或多解释器语义具有可独立成立的一般性语言洞见，再将其发展为第二篇语言与语义论文。



## 5. 必须建立的语义保证

测试系统至少应明确以下性质及其适用边界：

- **解释一致性：** `check`、适用性、生成、oracle 与 repair 对同一核心规则不得给出相互矛盾的结论。
- **检查一致性：** 求解器给出的见证必须通过同一规则的运行时检查。
- **单一来源：** evaluator 消费同一 RuleIR，不在实现内部复制领域谓词；规则变更必须自动影响所有派生操作。
- **生成有效性：** 生成的状态和动作必须满足对应 domain 与前置条件。
- **反例真实性：** 报告的失败必须来自真实执行结果，而非求解器内部的伪轨迹。
- **缩减保持性：** 缩减后的轨迹仍合法、可重放并触发同一类违反；修复只能改变测试输入。
- **真实观察边界：** shrink 和求解器只能选择或修复测试输入；被测实现的输出与后状态必须来自真实执行。
- **不完备性显式化：** 对尚不支持或无法判定的规则返回 `Unsupported` 或 `Unknown`，不能误报为 `Unsat`。
- **Unsat 的可信范围：** 只有在后端对相应规则片段完备时，才允许把不可满足结论解释为不存在见证。

论文必须明确定义核心规则片段 $\mathcal{L}_{core}$。第一版可限制为 Bool、Enum、有界 Range、结构 Type、等式、受限算术、`require`、`implies` 与可展开的 RuleInstance；外部函数、副作用、无限域和无法编码的高阶构造只允许 `check`，或返回 `Unknown`/`Unsupported`。

这些保证比语法是否使用 `Range[Int; 0, 100]` 更接近论文的理论核心。



## 6. 与已有工作的关系

相关工作不能只列“相似语言”，应按它们解决的问题分类：

| 方向 | 代表系统 | 主要能力 | Tapas 需要证明的差异 |
| --- | --- | --- | --- |
| 程序契约与验证 | JML、Dafny、SPARK | 函数/程序的前后置条件、不变量、静态或动态验证 | 是否能以同一领域模型驱动黑盒状态测试和反例缩减 |
| 交互/API 契约 | Pact、OpenAPI | 服务交互或接口 schema 的一致性 | 是否能表示跨步骤业务状态与动作可用条件 |
| 约束建模与求解 | MiniZinc、CUE | 描述约束、配置或优化问题 | 是否提供面向测试执行、oracle、轨迹和 shrink 的完整闭环 |
| 关系建模/模型寻找 | Alloy | 有界关系模型与反例寻找 | 是否能直接连接真实实现并持续生成可执行测试 |
| 形式模型驱动测试 | ProB、B/Event-B、Quint、TLA+ 周边工具 | 从状态模型进行动画、约束求解、模型检查和测试路径生成 | 不声称“一份状态模型多用途”是首次提出；比较真实黑盒执行、业务规则演化和 shrink 的差异 |
| 业务规则驱动 PBT | Aichernig–Schumi | 从工业业务规则模型派生 EFSM、输入生成和 FsCheck 状态测试，并执行真实 Web Service | 这是最直接的先行工作；差异必须落在关系契约解释一致性、依赖修复 shrink、跨领域性和政策漂移实证，而不是“规则生成测试” |
| Stateful property-based testing | Hypothesis、QuickCheck `eqc_statem` | 动作序列、precondition、postcondition、真实执行和自动 shrink | 是否无需分别维护 generator、precondition、next-state、postcondition 与 shrinker |
| 谓词驱动生成 | Luck | 从逻辑谓词生成满足约束的数据，并建立生成语义 | 状态转移、真实系统 oracle、规则演化和轨迹缩减是否带来额外方法贡献 |
| 约束生成器综合 | Palamedes | 从值谓词综合正确的受约束随机生成器 | 不声称从谓词自动生成合法数据是新颖的；只评价它作为关系契约测试解释的一部分所带来的增量 |
| 可编程 PBT | Programmable Property-Based Testing | 将 property 重化并与 runner 解耦，允许用户定义不同 runner | 不声称“IR + 多 evaluator”本身新颖；证明固定测试解释间的一致性以及状态轨迹 repair 的额外价值 |
| 生成式反例缩减 | Hypothesis reducer | 通过生成决策的表示和重写缩减测试输入 | 依赖修复必须针对真实状态轨迹、跨步骤数据依赖和同类违反保持给出额外算法与比较 |
| 约束式 MBT | OCL/SMT/search-based test generation | 从模型约束求解测试数据和路径 | 求解器只是后端；贡献应落在测试工件去重、真实执行判定和反例质量 |
| Schema/API fuzzing | Schemathesis | 从 OpenAPI/GraphQL 生成 API 测试 | 是否从 API 形状推进到业务规则和跨调用状态转移 |
| Policy engines | Cedar、OPA | 授权或策略判定 | 是否把规则扩展为可求解、可生成和可执行的测试模型 |

论文需要承认这些系统的成熟贡献，避免使用“首个 contract DSL”“首个 SAT 测试系统”“首个多解释 property”“首个从业务规则生成 PBT”或“首个从关系生成状态测试”等宽泛表述。可接受的 claim 应精确到**关系契约的哪些测试解释满足何种一致性、依赖修复型 shrink 如何不同于既有缩减，以及政策演化产生了什么可测量的语义收益**。



### 集约化文献矩阵

下表不是最终论文的引用清单，而是定题、设计实现和实验前需要核查的工作集。共 46 项，其中“精读”表示必须逐节阅读并填写第 13 节的差分矩阵；“对照”表示需要在 related work 中准确定位；“实验”表示主要用于设计基线、指标或统计方法；“背景”表示用于建立概念谱系。预印本、工具文档和版本资料均明确标注，不能在论文中伪装成同行评议成果。

| # | 研究簇 | 文献与正式出处 | 与 Tapas 的关系 | 用法 |
| ---: | --- | --- | --- | --- |
| 1 | MBT 基础 | Chow, 1978, [*Testing Software Design Modeled by Finite-State Machines*](https://doi.org/10.1109/TSE.1978.231496), IEEE TSE | FSM 测试生成的经典起点；界定状态、转移与覆盖的基本谱系 | 背景 |
| 2 | MBT 基础 | Harel, 1987, [*Statecharts: A Visual Formalism for Complex Systems*](https://doi.org/10.1016/0167-6423%2887%2990035-9), SCP | 复杂状态建模的代表；提醒论文区分表示法创新与测试方法创新 | 背景 |
| 3 | MBT 基础 | Cheng & Krishnakumar, 1993, [*Automatic Functional Test Generation Using the Extended Finite State Machine Model*](https://doi.org/10.1145/157485.164585), DAC | EFSM 的参数、守卫与自动测试生成，与动作参数模型直接相关 | 对照 |
| 4 | MBT 基础 | Kalaji, Hierons & Swift, 2009, [*Generating Feasible Transition Paths for Testing from an EFSM*](https://doi.org/10.1109/ICST.2009.29), ICST | 约束下可行路径生成；可作为轨迹生成与求解代价的先行工作 | 对照 |
| 5 | MBT 综述 | Utting, Pretschner & Legeard, 2012, [*A Taxonomy of Model-Based Testing Approaches*](https://doi.org/10.1002/stvr.456), STVR | 用统一术语定位 Tapas 的模型、选择准则、执行方式和 oracle | 精读 |
| 6 | MBT 方法 | Utting & Legeard, 2007, [*Practical Model-Based Testing: A Tools Approach*](https://www.sciencedirect.com/book/9780123725011/practical-model-based-testing), Morgan Kaufmann | MBT 工程闭环和工具比较的基础读物 | 背景 |
| 7 | 形式 MBT | Leuschel & Butler, 2008, [*ProB: An Automated Analysis Toolset for the B Method*](https://doi.org/10.1007/s10009-007-0063-9), STTT | 同一形式模型已可用于动画、模型检查和约束求解；限制宽泛 novelty claim | 精读 |
| 8 | 形式 MBT | Satpathy, Butler, Leuschel & Ramesh, 2007, [*Automatic Testing from Formal Specifications*](https://doi.org/10.1007/978-3-540-73770-4_6), TAP | 从非确定形式模型符号执行并连接真实 Java 实现 | 精读 |
| 9 | 形式建模 | Abrial et al., 2010, [*Rodin: An Open Toolset for Modelling and Reasoning in Event-B*](https://doi.org/10.1007/s10009-010-0145-y), STTT | Event-B 的建模、精化和证明工具链；用于区分验证系统与测试系统 | 对照 |
| 10 | 关系建模 | Jackson, 2002, [*Alloy: A Lightweight Object Modelling Notation*](https://doi.org/10.1145/505145.505149), TOSEM | 有界关系模型寻找的代表；Tapas 不能把关系 + SAT 本身作为贡献 | 精读 |
| 11 | 约束式 MBT | Ali, Iqbal, Khalid & Arcuri, 2014, [*Generating Test Data from OCL Constraints with Search Techniques*](https://doi.org/10.1109/TSE.2013.17), IEEE TSE | 从业务友好的模型约束生成数据；比较可表达性、有效率与性能 | 精读 |
| 12 | PBT 基础 | Claessen & Hughes, 2000, [*QuickCheck: A Lightweight Tool for Random Testing of Haskell Programs*](https://doi.org/10.1145/351240.351266), ICFP | PBT 的 generator、property 与 shrink 基础 | 精读 |
| 13 | Stateful PBT | Claessen & Hughes, 2002, [*Testing Monadic Code with QuickCheck*](https://doi.org/10.1145/636517.636527), Haskell Workshop / SIGPLAN Notices | 有状态与副作用程序的 PBT 基础 | 精读 |
| 14 | Stateful PBT | Arts, Hughes, Johansson & Wiger, 2006, [*Testing Telecoms Software with Quviq QuickCheck*](https://doi.org/10.1145/1159789.1159792), Erlang Workshop | 工业状态机 PBT 和真实系统执行经验 | 对照 |
| 15 | Stateful PBT | Hughes, 2007, [*QuickCheck Testing for Fun and Profit*](https://doi.org/10.1007/978-3-540-69611-7_1), PADL | QuickCheck 的模型、生成和缩减方法总览 | 背景 |
| 16 | Stateful PBT | Hughes, Pierce, Arts & Norell, 2016, [*Mysteries of DropBox: Property-Based Testing of a Distributed Synchronization Service*](https://doi.org/10.1109/ICST.2016.37), ICST | 真实分布式状态系统、命令模型与长序列错误，是强经验基线 | 精读 |
| 17 | PBT 工具 | MacIver et al., 2019, [*Hypothesis: A New Approach to Property-Based Testing*](https://doi.org/10.21105/joss.01891), JOSS | 统一生成表示和自动 shrink 的成熟系统 | 精读 |
| 18 | PBT 实证 | Goldstein et al., 2024, [*Property-Based Testing in Practice*](https://doi.org/10.1145/3597503.3639581), ICSE | 开发者如何编写 property/generator 以及现实痛点；支撑可用性实验 | 精读 |
| 19 | 业务规则 PBT | Aichernig & Schumi, 2016, [*Property-Based Testing with FsCheck by Deriving Properties from Business Rule Models*](https://doi.org/10.1109/ICSTW.2016.24), A-MOST/ICSTW | Tapas 最直接的先行工作：业务规则到 EFSM、生成器和状态测试 | 精读 |
| 20 | 业务规则 PBT | Aichernig & Schumi, 2019, [*Property-Based Testing of Web Services by Deriving Properties from Business-Rule Models*](https://doi.org/10.1007/s10270-017-0647-0), SoSyM | 工业扩展版；必须逐项确认生成、oracle、状态更新和 shrink 是否重复 | 精读 |
| 21 | Web Service PBT | Lampropoulos & Sagonas, 2012, [*Automatic WSDL-Guided Test Case Generation for PropEr Testing of Web Services*](https://doi.org/10.4204/EPTCS.98.3), WWV | Schema/WSDL 驱动生成；用于区分结构合法性与业务状态合法性 | 对照 |
| 22 | Web Service PBT | Francisco et al., 2013, [*Turning Web Services Descriptions into QuickCheck Models for Automatic Testing*](https://doi.org/10.1145/2505305.2505306), Erlang Workshop | 从接口描述派生 QuickCheck 模型，与 Tapas 的系统边界相邻 | 对照 |
| 23 | Web Service PBT | Earle et al., 2014, [*Jsongen: A QuickCheck Based Library for Testing JSON Web Services*](https://doi.org/10.1145/2633448.2633454), Erlang Workshop | JSON schema 到生成器；是 Range/Enum 域生成的邻近基线 | 对照 |
| 24 | Web Service PBT | Fredlund et al., 2014, [*Property-Based Testing of JSON Based Web Services*](https://doi.org/10.1109/ICWS.2014.110), ICWS | JSON 服务 PBT 的接口、生成和执行经验 | 对照 |
| 25 | 规格演化 | Li, Thompson, Lamela Seijas & Francisco, 2014, [*Automating Property-Based Testing of Evolving Web Services*](https://doi.org/10.1145/2543728.2543741), PEPM | 已研究接口演化下测试代码同步；Tapas 要聚焦领域政策语义漂移 | 精读 |
| 26 | 谓词生成 | Lampropoulos et al., 2017, [*Beginner's Luck: A Language for Property-Based Generators*](https://doi.org/10.1145/3009837.3009868), POPL | 从谓词与分布要求生成满足数据，直接限制 `satisfy`/generate 的 novelty | 精读 |
| 27 | 生成器推导 | Lampropoulos, Paraskevopoulou & Pierce, 2018, [*Generating Good Generators for Inductive Relations*](https://doi.org/10.1145/3158133), POPL/PACMPL | 从归纳关系编译可靠且完备的生成器；支撑“解释一致性”的比较 | 精读 |
| 28 | 搜索式 PBT | Löscher & Sagonas, 2017, [*Targeted Property-Based Testing*](https://doi.org/10.1145/3092703.3092711), ISSTA | 用搜索策略引导生成；是 Tapas 反馈导向搜索的直接基线 | 精读 |
| 29 | 覆盖导向 PBT | Lampropoulos, Hicks & Pierce, 2019, [*Coverage Guided, Property Based Testing*](https://doi.org/10.1145/3360607), OOPSLA/PACMPL | 在稀疏前置条件下结合结构生成与覆盖反馈 | 精读 |
| 30 | 谓词生成 | Boyapati, Khurshid & Marinov, 2002, [*Korat: Automated Testing Based on Java Predicates*](https://doi.org/10.1145/566172.566191), ISSTA | 从可执行谓词有界生成非同构对象，并以 postcondition 判定 | 精读 |
| 31 | SAT 规格测试 | Khurshid & Marinov, 2004, [*TestEra: Specification-Based Testing of Java Programs Using SAT*](https://doi.org/10.1023/B%3AAUSE.0000038938.10589.b9), ASE Journal | Alloy/SAT 规格生成输入和检查输出；直接反驳“SAT + contract 即创新” | 精读 |
| 32 | 语义 Fuzzing | Padhye et al., 2019, [*Semantic Fuzzing with Zest*](https://doi.org/10.1145/3293882.3330576), ISSTA | 在结构生成器之上以有效性和覆盖引导稀疏语义空间搜索 | 实验 |
| 33 | 约束生成器综合 | Goldstein et al., 2026, [*The Search for Constrained Random Generators*](https://doi.org/10.1145/3808329), PLDI | Palamedes 从值谓词综合正确生成器；Tapas 必须证明状态闭环的增量 | 精读 |
| 34 | 可编程 PBT | Keles et al., 2026, [*Programmable Property-Based Testing*](https://doi.org/10.1145/3828685), ICFP | Property 重化与 runner 解耦；限制“RuleIR + 多 evaluator”的 novelty | 精读 |
| 35 | 反例缩减 | Zeller & Hildebrandt, 2002, [*Simplifying and Isolating Failure-Inducing Input*](https://doi.org/10.1109/32.988498), IEEE TSE | Delta debugging 基础；定义删除式缩减基线 | 精读 |
| 36 | 结构化缩减 | Misherghi & Su, 2006, [*HDD: Hierarchical Delta Debugging*](https://doi.org/10.1145/1134285.1134307), ICSE | 对结构化输入进行层次化删除；比较结构保持与关系修复 | 对照 |
| 37 | 反例缩减 | MacIver & Donaldson, 2020, [*Test-Case Reduction via Test-Case Generation: Insights from the Hypothesis Reducer*](https://doi.org/10.4230/LIPIcs.ECOOP.2020.13), ECOOP | Hypothesis 生成决策表示与通用重写；Tapas shrink 的最关键基线之一 | 精读 |
| 38 | PBT 缩减 | de Vries, 2023, [*falsify: Internal Shrinking Reimagined for Haskell*](https://doi.org/10.1145/3609026.3609733), Haskell Symposium | 让生成与缩减共享表示、避免单独 shrinker；直接邻近单一来源主张 | 精读 |
| 39 | 编译器缩减 | Regehr et al., 2012, [*Test-Case Reduction for C Compiler Bugs*](https://doi.org/10.1145/2345156.2254104), PLDI | 实际 failure preservation、有效性和缩减成本的实验范式 | 实验 |
| 40 | 实验统计 | Arcuri & Briand, 2011, [*A Practical Guide for Using Statistical Tests to Assess Randomized Algorithms in Software Engineering*](https://doi.org/10.1145/1985793.1985795), ICSE | 指导随机化测试算法的重复次数、显著性与效应量报告 | 实验 |
| 41 | Mutation | Jia & Harman, 2011, [*An Analysis and Survey of the Development of Mutation Testing*](https://doi.org/10.1109/TSE.2010.62), IEEE TSE | 设计 mutation operators、等价 mutant 处理和 mutation score | 实验 |
| 42 | Mutation | Papadakis et al., 2018, [*Mutation Testing Advances: An Analysis and Survey*](https://doi.org/10.1016/bs.adcom.2018.03.015), Advances in Computers | 更新 mutation 实验方法与威胁分析 | 实验 |
| 43 | Agent benchmark | Yao et al., 2025, [*$\tau$-bench: A Benchmark for Tool-Agent-User Interaction in Real-World Domains*](https://openreview.net/forum?id=roNSXZpUDN), ICLR | Retail/airline 的政策、工具、任务和最终状态评价；只作为实验对象而非方法来源 | 精读 |
| 44 | Agent benchmark | Barres et al., 2025, [*$\tau^2$-Bench: Evaluating Conversational Agents in a Dual-Control Environment*](https://arxiv.org/abs/2506.07982), arXiv 预印本 | 双方可操作共享环境和组合式任务生成；可启发多主体状态依赖实验 | 对照 |
| 45 | Agent benchmark | Sierra, 2026, [$\tau^3$-Bench repository and release](https://github.com/sierra-research/tau2-bench), 系统/数据版本资料 | 当前 retail 适配的数据来源；引用时必须锁定仓库提交、数据版本和评测协议 | 实验 |
| 46 | 有状态生成器综合 | Zhou, Desai, Delaware & Jagannathan, 2026, [*Trace-Guided Synthesis of Effectful Test Generators*](https://doi.org/10.1145/3808264), PLDI | 从 effectful 操作的符号轨迹综合序列生成器并保留数据/控制依赖；与动作序列生成及 repair 极为接近 | 精读 |

表格应作为活的研究账本维护。每篇精读完成后，至少记录：模型输入是什么、是否连接真实 SUT、哪些测试工件由模型派生、是否生成状态序列、如何定义 oracle、如何 shrink、能否处理规格演化、证据所在页码。第 19、20、27、31、33、34、37、38、46 项最可能直接改变 Tapas 的贡献边界，优先级高于继续扩展语法。

工具能力还需要结合正式文档核查：[ProB Test Case Generation](https://prob.hhu.de/w/index.php?title=Test_Case_Generation)、[Event-B Model-Based Testing](https://wiki.event-b.org/index.php/D32_Model-based_testing)、[QuickCheck `eqc_statem`](https://quviq.com/documentation/eqc/eqc_statem.html) 和 [Hypothesis Stateful Testing](https://hypothesis.readthedocs.io/en/latest/stateful.html)。这些资料用于确认当前工具行为，但不替代相应论文引用。



## 7. 研究问题与实验设计

### RQ1：关系契约的解释一致性与支持边界

同一关系契约的 `check`、适用性、生成、真实执行 oracle 和 repair 解释是否一致？这种一致性在哪些规则片段上成立？

验证内容：

- 对 $\mathcal{L}_{core}$ 给出形式定义和至少单向可靠性结论；
- 求解见证通过运行时 `check` 的比例；
- 生成动作违反适用性条件的数量；
- oracle 与直接执行契约所得结论的不一致数量；
- differential/property/metamorphic tests 发现的解释器差异；
- 各语言构造的 `Supported`、`Unknown` 与 `Unsupported` 比例。

### RQ2：反例缩减与依赖修复

规则派生的 shrink 是否能产生更短、更稳定且仍然合法的失败轨迹？依赖修复是否优于仅删除动作或遇到非法候选就放弃？

主要指标：

- 缩减前后轨迹长度和数据复杂度；
- shrink 时间、求解次数和真实重放次数；
- 重放成功率与同类违反保持率；
- 缩减过程中产生的非法候选比例；
- 因后续数据依赖而无法继续缩减的比例；
- 开发者定位缺陷所需时间（若能开展用户研究）。

### RQ3：规则重复与政策演化

分别维护测试工件会产生多少领域规则重复？政策演化时，Tapas 是否能减少同步修改和语义漂移？

主要指标：

- 可表达的规则比例和需要逃逸到宿主语言的规则数量；
- 模型代码量、重复领域谓词数量和重复比例；
- 每次政策变更需要修改的位置、代码量和时间；
- generator、precondition、next-state、oracle 与 shrinker 之间出现的不一致数量；
- 未同步更新导致的无效测试、漏报和误报数量。

政策演化实验应在实现比较前固定一组政策原子和变更脚本，例如认证、显式确认、同商品限制、支付能力和拒绝时 frame condition，并在有界见证域上检查漂移。不能在看到结果后选择对 Tapas 最有利的变更，也不能把任意程序等价包装成已经解决的问题。

“Enum 比整数更直观”应作为假设，而不是事实。若要主张可理解性或可维护性，需要独立参与者实验或至少由非作者开发者完成建模复现。

### RQ4：测试有效性与求解增量

关系契约驱动的测试是否比随机生成、拒绝采样或手写生成器发现更多、更深的状态错误？其中 `Range`、过滤、约束求解与反馈导向搜索分别贡献多少？

主要指标：

- 有效输入/有效动作比例；
- mutation score 与发现的独立缺陷数量；
- 首次发现缺陷的执行次数和墙钟时间；
- 达到的状态深度、转移覆盖与业务规则覆盖；
- 无效请求消耗的测试预算。

消融配置至少包括：

1. 无约束随机生成；
2. `Range`/`Enum` 域生成；
3. 生成后过滤；
4. 约束求解生成；
5. 求解加反馈导向搜索。

### RQ5：一般性、代价与边界

关系契约测试与依赖修复的收益能否跨越 agent 与非 AI 系统？统一解释是否带来不可接受的性能成本？哪些语言构造会导致 `Unknown`、组合爆炸或语义不一致？

需要报告：

- 模型加载、编译和求解耗时；
- 随状态数、动作参数和轨迹深度增长的扩展性；
- 支持、部分支持和不支持的规则片段；
- 超时与 `Unknown` 的比例；
- tau3 agent 与非 AI 系统上的效果差异。



## 8. 评估对象与基线

仅使用 `tau3-retail` 不足以支持“通用规则系统测试”的论断。建议至少包含：

1. **Retail/agent workflow：** 以换货、退款、身份验证、支付能力等规则形成完整纵向示例；
2. **非 Agent 的状态系统：** 例如账户/账本、订单履约或审批流，证明方法不依赖 LLM agent；
3. **另一个不同结构的领域：** 例如授权策略、预订或库存系统，用于检验模型迁移成本。

缺陷集应明确区分：

- 真实或历史缺陷；
- 人工植入但由领域专家设计的缺陷；
- 由系统 mutation operator 批量产生的 mutants。

基线按适用范围选择，不能要求某个工具完成其设计目标之外的任务：

- 随机测试和拒绝采样；
- 手写 fixture/generator/oracle；
- Hypothesis/QuickCheck 风格的 stateful property testing；
- Aichernig–Schumi 风格的“业务规则模型 → EFSM/PBT”流程，至少进行功能级复现或逐能力对照；
- Hypothesis reducer/普通状态序列删除式 shrink；
- Schemathesis（仅在具有 OpenAPI/GraphQL 接口的实验对象上）；
- ProB、Quint 或其他 model-based testing 工具（在状态模型可公平对应时）；
- Luck/Palamedes 风格的谓词约束生成（用于隔离“自动生成合法输入”的增量）；
- 直接 SMT/约束编码（用于衡量 Tapas 抽象带来的成本与可维护性）。

比较 generator、precondition、next-state、oracle 和 shrinker 的重复时，必须给予基线与 Tapas 等价的领域信息。不能让 Tapas 使用完整 policy，而只让 Hypothesis 或 QuickCheck 使用字段类型。对无法直接表达相同规则的基线，应如实记录适配代码和逃逸逻辑，而不是把它们算作工具缺陷。

政策演化实验应区分：

- **表达成本：** 初次建模需要写多少领域逻辑；
- **演化成本：** policy 变化后需要同步修改多少位置；
- **漂移结果：** 故意漏改一个组件时会产生何种漏报、误报或无效测试；
- **测试效果：** 统一契约解释是否在相同时间预算下保持或提高缺陷发现能力。

随机化实验需要固定预算、公开种子、重复运行，并报告置信区间和效应量，而不只报告最佳结果。



## 9. 论文结构

建议正文结构如下：

1. Introduction：问题、核心洞见、贡献和结果摘要；
2. Motivation：以 retail 的跨步骤依赖反例展示普通 shrink 的失效，再展示一次政策演化导致的测试工件漂移；
3. Problem and model：状态、动作、真实执行、关系契约、解释一致性和语义漂移的形式化；
4. Contract interpretations：从同一关系契约派生适用性、生成、oracle 和 repair，并给出核心规则片段上的性质；
5. Testing algorithm：约束感知生成、真实执行、搜索和依赖修复型 shrink；
6. Implementation：Rule IR、多解释器、求解后端和系统边界；
7. Evaluation：RQ、对象、基线、结果、统计方法；
8. Threats to validity；
9. Related work；
10. Conclusion。

语言教程、完整语法和工程接口放入附录或 artifact 文档，避免正文被语法细节淹没。



## 10. 实施里程碑与投稿门槛

### 阶段 A：可运行纵向闭环

- retail 示例清楚地区分 model、真实执行适配器和测试 runner；
- 同一 Rule 可执行 `check` 与 `satisfy`；
- 至少一个 action 可生成参数、执行真实实现并检查 transition；
- 失败轨迹可重放；
- 自动生成和 oracle 路径中不复制 retail 业务条件。

### 阶段 B：研究原型

- `Range`、有限域与求解语义稳定；
- $\mathcal{L}_{core}$ 及每种解释的支持边界冻结；
- 序列生成和至少一种反馈策略可用；
- 依赖修复型 shrink 可用；
- repair/shrink 路径中不复制领域条件；
- 至少实现一个依赖后续 action 参数的多步反例，并验证删除式与修复式 shrink 的差别；
- `Unknown`/`Unsupported` 语义完整；
- 解释器一致性有自动化测试。

### 阶段 C：实证准备

- 至少三个实验对象，其中至少一个非 Agent 系统；
- 建立真实/人工/mutation 缺陷集；
- 固定政策演化序列并建立语义漂移测量工具；
- 完成 Aichernig–Schumi 方案的功能级对照，确认论文不再依赖“业务规则生成 PBT”这一已知贡献；
- 所有基线可重复运行；
- 预先固定 RQ、预算、指标和统计方案；
- artifact 能从干净环境复现实验表格。

### 阶段 D：可以投稿

只有同时满足以下条件才建议投完整研究论文：

- 不依赖语言“看起来新”也能说清研究贡献；
- 不把 CP/SAT/SMT 或“多 evaluator”本身列为 novelty；
- 不把“业务规则生成状态测试”或“一个模型同时生成输入和 oracle”列为 novelty；
- 有关系契约解释一致性的形式化结论和实现证据；
- 有直接证据表明依赖修复优于普通轨迹缩减；
- 有直接证据表明统一契约减少政策演化中的语义漂移；
- 至少一项主要结果相对强基线具有明确效应；
- 对求解正确性和不完备边界有清晰陈述；
- 语言与工具体验有可测量证据，而不只依赖语法示例或作者主观评价；
- 结论覆盖范围与实验对象数量一致；
- 开源 artifact、模型、缺陷集、种子和结果分析脚本齐全。



## 11. 投稿路线与期刊分析

### 如何理解“level”

期刊没有跨机构统一的单一等级。“level”至少包含四个不同维度：领域认可度、选稿严格度、与本文的主题匹配度，以及所在单位采用的 CCF/中科院/学校目录。后者会随年份和单位规则变化，正式投稿前必须按当年考核口径另行核验。

下面的“难度”是根据期刊定位和本文需要达到的证据强度作出的研究规划判断，不等同于官方录用概率。公开指标截取于 2026-09-05；“首轮决定”通常混合 desk rejection 与外审决定，不能当作录用所需时间。

| 期刊 | 相对定位与难度判断 | 与第一篇的匹配度 | 官方当前公开周期/指标 | 对 Tapas 的主要要求 |
| --- | --- | --- | --- | --- |
| **Software Testing, Verification and Reliability (STVR)** | 成熟的测试与验证专刊；主题最集中。难度为中高，官网当前显示 4% acceptance rate，不能视为“容易的专业刊” | **最高，建议首选** | CiteScore 3.9、JIF 1.7、接受率 4%、投稿到首次决定中位数 26 天 | 关系契约语义、修复型 shrink、漂移实证、强测试基线、mutation/真实缺陷与可复现 artifact |
| **Automated Software Engineering (ASE Journal)** | 自动化软件工程的重要专业期刊；相对要求最高，系统新颖性和实证规模都要更强 | 很高，适合结果足够强时冲刺 | 2025 JIF 3.9；投稿到首次决定中位数 16 天 | 证明自动化方法不只是 DSL 包装；多个系统、大规模实验、明确增益与工程可用性 |
| **Software and Systems Modeling (SoSyM)** | 建模领域的成熟专刊；难度中高，强调建模方法的理论基础和真实应用 | 高，但叙事必须从“测试工具”转向“可执行领域模型与多解释” | 2025 JIF 3.0；投稿到首次决定中位数 4 天 | 严格定义元模型/语义、模型关系、建模收益和真实建模经验；仅有形式化符号不够 |
| **Science of Computer Programming (SCP)** | 传统的程序设计语言与软件方法期刊；选题范围较宽，难度中等至中高 | 中高，适合语言实现与实验软件技术叙事 | 官方范围页可确认期刊覆盖语言设计、验证、测试和 experimental software technology；未取得可可靠引用的当前官方审稿周期 | 需要把 Rule IR、多解释器或类型/求解设计提升为技术贡献，并有实现与实验，不宜只有产品愿景 |
| **The Art, Science, and Engineering of Programming (Programming Journal)** | 专注 programming 的同行评议开放期刊；机制透明但并不等于低门槛 | 第一篇中等；未来语言论文更合适 | 每年至少三批审稿，官方目标为每个审稿周期约 4 个月；无开放获取费用 | 需要突出编程模型、语义或语言工程的一般性洞见；纯 benchmark/test-tool 论文不占优势 |

期刊官方信息来源：

- [STVR 主页与实时指标](https://onlinelibrary.wiley.com/journal/10991689)
- [Automated Software Engineering 主页与实时指标](https://link.springer.com/journal/10515)
- [Automated Software Engineering aims and scope](https://link.springer.com/journal/10515/aims-and-scope)
- [Software and Systems Modeling 主页与实时指标](https://link.springer.com/journal/10270)
- [Software and Systems Modeling aims and scope](https://link.springer.com/journal/10270/aims-and-scope)
- [Science of Computer Programming 官方范围说明](https://shop.elsevier.com/journals/science-of-computer-programming/0167-6423)
- [Programming Journal purpose and review cycle](https://programming-journal.org/purpose/)



### 推荐顺序

如果第一篇完成的是“关系契约解释一致性 + 依赖修复型 shrink + 政策漂移实证”，推荐顺序为：

1. **STVR**：主题最准确，最容易让评审按测试贡献评价 Tapas，而不是纠缠“是不是一门全新的语言”；
2. **ASE Journal**：若实验证据非常强，可作为更激进的首投或 STVR 之外的高目标；
3. **SoSyM**：若最终最强贡献落在统一状态建模、语义和模型解释，而非缺陷发现效果；
4. **SCP**：若最终最强贡献落在 Rule IR、多解释器和语言/系统实现；
5. **Programming Journal**：更适合作为后续独立语言论文的候选。

不建议仅因为示例涉及 agent 就投 Machine Learning。除非论文真正提出并验证了新的学习问题、算法或统计方法，否则 benchmark 中出现 LLM agent 并不会把核心贡献变成机器学习研究。



### 时间规划

官方“首次决定”数据很短时，往往包含快速编辑筛选，不代表完整同行评议或最终录用。项目管理上应采用更保守的假设：

- 投稿前补实验、artifact 和预审：3--6 个月；
- 投稿后到收到一次实质性外审意见：预留 2--5 个月；
- 一轮大修：2--4 个月；
- 从首次投稿到最终录用：按 8--18 个月安排较稳妥。

这组区间是项目排期假设，不是任何期刊的承诺。Programming Journal 采用批次制，每一轮目标约四个月；如果错过批次或需要进入下一轮，日历时间会相应增加。



## 12. 两篇论文而不是一篇论文承载全部愿景

较稳妥的长期路线是：

### 论文一：测试系统论文

- 中心：以关系式状态契约驱动抗漂移的真实黑盒状态测试；
- 贡献：关系契约的测试解释一致性、依赖修复型轨迹 shrink，以及政策演化下语义漂移的实证研究；
- 首选：STVR；强结果可考虑 ASE Journal；
- Tapas 语言：作为维持统一语义、静态诊断和业务可读性的次级系统贡献；
- CP/SAT/SMT：作为方法实现机制，不作为核心 novelty。

### 论文二：语言与语义论文

- 中心：`Type[Types; Values]`、Rule/RuleInstance、范围类型、规则可满足性和多解释器的一致语义；
- 贡献：语言设计、形式语义、类型/值索引、解释器一致性或可扩展求解架构；
- 候选：SCP、Programming Journal、SoSyM，或与成果体量匹配的 PL/SE 会议；
- 前提：必须存在可独立于测试应用成立的一般性语言洞见。

拆分的目的不是降低标准，而是避免一篇论文同时承担语言、求解器、状态模型、测试框架、agent benchmark 和产品系统六条叙事，导致任何一项都讲不深。



## 13. 当前最重要的下一步

在继续扩展语法之前，先冻结论文问题和实验协议，再实现能力。优先交付三份表。

第一份是“现有工作—Tapas 差分矩阵”，用于阻止已经被覆盖的能力重新进入 novelty：

| 能力 | Aichernig–Schumi | ProB/Event-B | Luck/Palamedes | QuickCheck/Hypothesis | Programmable PBT | Tapas 必须增加的差分 |
| --- | --- | --- | --- | --- | --- | --- |
| 业务规则/模型生成测试 |  |  |  |  |  |  |
| 关系契约多向解释 |  |  |  |  |  |  |
| 真实 SUT oracle |  |  |  |  |  |  |
| 跨步骤依赖 shrink |  |  |  |  |  |  |
| 删除后重放并修复输入 |  |  |  |  |  |  |
| 政策演化漂移评估 |  |  |  |  |  |  |

任何一行只有在精读原论文并记录证据位置后才能填写；“未提及”不能自动解释为“不支持”。

第二份是“领域规则—测试工件矩阵”，用于证明当前方法到底消除了哪些重复：

| 领域条件 | Rule | generator | precondition | oracle | next-state | shrinker |
| --- | --- | --- | --- | --- | --- | --- |
| 用户认证 | 唯一定义位置 | 派生/复制 | 派生/复制 | 派生/复制 | 派生/复制 | 派生/复制 |
| 目标确认 |  |  |  |  |  |  |
| 同商品限制 |  |  |  |  |  |  |
| 支付能力 |  |  |  |  |  |  |
| 拒绝时状态保持 |  |  |  |  |  |  |

Tapas 列必须能够证明除 Rule 外没有第二份业务谓词；基线则如实记录分别维护的逻辑。

第三份是“语义—能力矩阵”：

| Rule 构造 | `check` | `satisfy` | `generate` | transition oracle | shrink | 完备性说明 |
| --- | --- | --- | --- | --- | --- | --- |
| Enum/有限集合 |  |  |  |  |  |  |
| Range |  |  |  |  |  |  |
| 结构 Type |  |  |  |  |  |  |
| `require` |  |  |  |  |  |  |
| `implies` |  |  |  |  |  |  |
| RuleInstance 条件 |  |  |  |  |  |  |
| 外部/不透明函数 |  |  |  |  |  |  |

三张表分别回答论文的三个根本问题：与先行工作究竟差在哪里，领域谓词是否真的只有一份，以及关系契约在哪些解释器中能够被可靠消费。它们会直接转化为贡献声明、实现边界、政策演化实验、缩减实验、消融实验和威胁分析。
