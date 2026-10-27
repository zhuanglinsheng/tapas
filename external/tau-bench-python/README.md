# 论文抽样实验：独立 Python / OR-Tools 实现

这是《基于约束编程的分布保持测试采样方法》的独立实验实现。**不依赖 Tapas 编译器、Rule 语法、运行时、可执行文件或 Python 适配层。** τ-bench Retail 是复杂业务实验对象，另有可穷举合成实例用于校验概率语义和机制。

目标是对先验输入 X 判断是否存在 Y 满足 R(X,Y)，生成条件分布 pi(X | exists Y: R(X,Y))。CP-SAT 返回的 Y 只是见证，不根据见证数量调整 X 的概率。所有策略保持同一条 iid 候选流；只有已证明 UNSAT 的绑定区域才能提前拒绝候选。业务查询或泛化查询遇到 UNKNOWN 时终止并标记未完成，绝不跳过后继续采样。

## 运行

从论文根目录（本目录的上一级，当前名为 `external`，可任意改名或移动）执行：

```sh
python3 -m pip install -r tau-bench-python/requirements.txt
python3 tau-bench-python/fetch_upstream.py
python3 -m unittest discover -s tau-bench-python -p 'test_*.py'
python3 tau-bench-python/run_experiments.py --output /tmp/cp-sampling-new-run
```

默认参数为 5 个 seed、接受样本数 32/128、订单数 8/32、请求商品数 1/2，以及 10 个策略。每次必须指定新的或空的输出目录，避免混合历史结果。绘图可用 `--no-plot` 关闭，此时运行实验只需 OR-Tools 与 Python 标准库。

可选的 C 加速器只加速先验抽样，不依赖 Tapas，不更换随机数生成器。使用当前 Python 的头文件和本机 C 编译器构建（目前构建脚本面向 Unix/macOS）：

```sh
python3 tau-bench-python/build_native.py
python3 -m unittest discover -s tau-bench-python -p 'test_*.py'
SAMPLING_NATIVE=0 python3 -m unittest discover -s tau-bench-python -p 'test_*.py'
```

未构建或显式设 `SAMPLING_NATIVE=0` 时使用纯 Python；导入时不会编译或下载。Python 与 C 路径均预计算累计权重，每个坐标消耗一次原有 Python RNG 调用，保留 `random.choices` 的累计加法与二分边界。实验元数据记录实际后端和已加载二进制的 SHA-256，源码快照包含 `.c` 文件；移动目录后应重新构建，不能假定二进制跨平台兼容。

小规模完整机制实验：

```sh
python3 tau-bench-python/run_experiments.py --suite synthetic --seeds 11 29 47 --samples 32 128 --output /tmp/cp-sampling-synthetic
```

包含全部七种 Retail 修改操作的快速实验：

```sh
python3 tau-bench-python/run_experiments.py --orders 8 --slots 1 --seeds 11 29 --samples 16 --strategies rejection raw mass_b4 --output /tmp/cp-sampling-smoke
```

多商品与规模扩展可分别选择：

```sh
python3 tau-bench-python/run_experiments.py --suite retail --operations exchange modify_items return --orders 8 32 128 --slots 1 2 3 --seeds 11 29 47 71 97 --samples 32 128 --output /tmp/cp-sampling-retail-scale
```

`--source` 指定原始 τ-bench 仓库；`--candidate-limit` 是每轮候选上限，未获得指定接受数量时报告未完成，不能当作成功实验。`--exact-limit` 控制离线目标分布的最大穷举规模，默认 4096。大实例不伪造全联合精确分布或 TV 值。

目录定位以脚本自身为基准，不查找外层项目。默认数据源为论文根目录内的 `tau-bench/`；数据与工具通过随包的 `UPSTREAM_SOURCE.json` 校验 SHA-256，运行时无需原始仓库的 `.git`，也不读取任何父目录 Git 信息。`fetch_upstream.py` 在已有有效快照时不调用 Git；只有下载缺失数据时需要 Git 和网络。

可在论文根目录执行 `python3 verify_standalone.py` 做隔离验证：复制到任意新目录、去除上游 Git 元数据并移除 `tau-bench-tapas/`，禁止访问原 Tapas 源码，然后执行纯 Python 测试、全部业务操作的配对小实验、绘图与论文编译。另加 `--native` 会在新目录从 C 源码重新构建扩展并验证两条路径，需要本机编译器；验证器不会复制现有二进制。

## 方法与对照

- `rejection`：每个候选都交给同一 CP-SAT oracle；作为未加速的拒绝采样基线。
- `full`：缓存完整失败赋值。
- `raw`：直接缓存 CP-SAT assumptions 返回的充分不可行绑定核。
- `mus`：在预算内做删除式核收缩；预算结束时不保证已得到 MUS。
- `minimum`：在预算内按绑定数递增搜索；只有完成相应搜索才有最小基数含义。
- `mass_b1`、`mass_b4`：按新增覆盖概率质量上界调度区间搜索，逻辑搜索步数预算为 floor(beta * sqrt(t))；复用已知查询结果也消耗一步，因此实际额外求解次数不超过该预算。
- `fifo_b4`、`width_b4`：改变搜索调度，保留相同预算。
- `bounded_mass`、`bounded_fifo`、`bounded_width`：容量 4，比较新增覆盖/独有质量交换、FIFO 与绑定数淘汰。
- `adaptive_b4`：先观察 32 个候选；近期 32 个候选没有新的业务 UNSAT 时暂停探索。根据已观测业务求解耗时积累 10% 的学习额度，预计至少能支付两次平均查询才启动下一步；扣除实际选择、质量计算及探索成本。另以当前接受率估计剩余候选量，连乐观覆盖上界也不能摊薄下一步成本时暂停。参数是公开启发式，不是最优决策或回本保证；一次昂贵探测可能超支，之后等待额度恢复。
- `gain_b4`：另加公开的剩余候选视界 1000，只有“视界×覆盖上界 > 1”才支付下一次泛化查询成本；不是多步回本保证。

MassCore 搜索使用固定失败输入的绑定子集区间 [I, I union J]：最大绑定仍 SAT 可剪枝；最小绑定 UNSAT 可准入；否则细分，并按 pi(G(I) minus cache) 上界选择下一次查询。每第四个逻辑搜索步选择最早任务以避免纯质量排序饥饿。该实现遵循论文/C 版的机制设计，是独立实现，不保证不同语言、OR-Tools 版本下产生相同原始核或逐条查询轨迹。

独立先验的区域并集质量用精确的值分区递归计算，不枚举整个笛卡尔积；显式相关先验按联合概率表求和。浮点仅用于概率质量评分与汇总，不作为 UNSAT 证明。当前缓存状态下重复的质量计算有记忆化，缓存改变即失效。独立先验还以有界 LRU（32768 项）复用残余区域并集的递归结果，可跨候选区域及缓存版本复用；每轮开始清空，避免策略间热缓存优势。部分查询的确定 SAT/UNSAT 结果可复用，发现查询返回的原始核也用于缓存，而非仅缓存整个查询赋值。关闭的搜索节点会及时移除。所有复用只依赖此前的证明，不读取未来候选。部分赋值 SAT 只用于证明存在补全；完整 X 的有效见证可直接复用来接受同一个候选，仍保留输出中的重复样本。

## 实验模型

六个合成问题包括四组偏斜/重叠/基数机制实例、显式相关先验，以及具有不同见证数量的投影实例。它们全部可穷举，使用独立 Python 谓词与 CP-SAT 逐点对照。见证数量实例中，正确条件概率与按解数量加权的概率不同，用于检查 X/Y 边界。

Retail 从固定提交 `59a200c6d575d595120f1cb70fea53cef0632f6b` 的原始数据建立直接 CP 模型，包含取消、订单地址修改、支付修改、商品修改、退货、换货、默认地址修改。使用关系表、整数金额、条件约束、求和和商品实例位置的 AllDifferent；不把合法重复商品 ID 一概禁止。

实验边界必须保留：

1. 这是结构化操作合法性及部分后状态见证模型，不是完整客服对话系统或原始 benchmark 的端到端 pass^k 评测。认证、确认、状态已查询、全部商品提醒是明确的抽象输入，不冒充真实语言证据。
2. 数据库原有 7 个含重复 item_id 的订单未进入当前选择池，以避免原工具“总取第一个同 ID 商品”的顺序修改歧义。其余订单按状态分层选取，并优先选含多个支付账户的用户，保证支付实验有可行输入。选取规则与实际 ID 均记录在 metadata.json。
3. 金额采用原始十进制数据的整数分；不声称与任意 Python 浮点边界等价。接受样本额外执行原工具，核对成功、状态、选定礼品卡余额、换货差价及地址等已建模输出。该外部核对不是完整 JSON 状态等价证明。
4. 先验由实验公开指定，不是从真实流量估计。概率坐标包括订单/用户索引、认证关系、确认、支付账户局部编号、商品实例位置、替换规格局部编号等。局部编号解码会使实际工具参数产生关联，这是明确的先验定义。替换规格在同商品中排序，当前不覆盖跨商品请求。非相关操作不引入无用的商品槽。
5. 候选输入的全部坐标属于 X；difference、post_balance、post_status 等属于 Y。返回的样本允许重复，不做去重重采样；只在离线原工具验证时去重节省审计成本。

## 证据与统计

每个问题/seed/接受批量下随机打乱策略运行顺序，同 seed 使用相同候选流。逐项核对接受输入和候选位置；不同批量核对接受前缀。策略不读取未来候选，发现查询全部计费。

报告业务查询、发现查询、总调用、缓存命中、缓存概率质量、总运行时间及建模/求解/核提取/匹配/质量/选择时间。墙钟时间含候选处理、模型克隆、绑定、求解和全部在线学习成本；不含共享的数据库读取、基础模型构造、离线验证、统计与绘图。selection 内包含部分 mass 计时，不能把所有计时分项直接相加。

当前在线计时还排除最终缓存覆盖率报表，将其 CPU/墙钟成本单列于 `diagnostics`。历史结果可能含该成本，须以各轮源码与元数据为准，不跨协议比较。可选 `ProductPrior.mass_node_limit` 为每次评分设置递归状态预算，默认 0 保持精确；回退只提供未覆盖质量上界，报告 `coverage` 和 `cache_mass_kind`，不作为可行性证明。

同 seed 对拒绝采样计算调用差和时间差，再对 seed 做 2000 次 percentile bootstrap 区间。小 seed 数的区间只作 pilot 描述，不当作充分统计证据。TV 只汇总各 seed 最大接受批量的 rejection 样本，绝不把同流策略副本或批量前缀当作独立样本；并报告同样样本数下 iid 目标抽样的参考 TV 分位数。有限样本 TV 检查不是分布保持的证明，安全缓存与相同接受流才是机制验证的主要证据。

CP-backed rejection 使用相同通用求解 oracle，不代表每一种专用业务检查器。不得仅凭调用次数减少声称端到端更快；额外学习在短批量、强原始核或高质量计算成本下可能亏损，原样保留这些结果。

输出包含：

- `report.html`：独立可打开的结果表与图。
- `summary.json`：逐问题、策略、批量的均值/区间和分布检查。
- `rows.jsonl.gz`：逐次运行的全部样本、见证、接受位置、查询与证书事件。
- `metadata.json`：Python/OR-Tools 版本、原始文件哈希、精确先验、模型哈希、协议与实现哈希。
- `implementation/`：本轮使用的 Python 源码、源数据校验清单与依赖文件快照，防止后续代码更新使旧结果不可复核。旧版本快照保留原始字节，其中可能仍有当时的目录假设；它们不由当前实验自动导入。
- `comparison`、`distribution` 的 PNG/PDF：查询与实际耗时，以及可穷举实例的分布检查。

当前结果是研究 pilot。仍需扩大 seed、接受批量、先验与规模范围，加入更多业务域/外部方法对照后，才可形成论文的完整实验结论。

## 实现优化的配对消融

`benchmark_optimization.py` 只有显式传入 `--baseline` 才加载指定的旧源码，不在正常采样或测试时自动运行历史快照。例如在论文根目录执行：

```sh
python3 tau-bench-python/benchmark_optimization.py --baseline tau-bench-python/results/optimization_20261021/baseline --suite synthetic --samples 32 128 --output /tmp/masscore-ablation
```

对照包括旧原始核缓存、当前原始核缓存、旧 MassCore、只替换概率计算的 MassCore、完整优化版，以及自适应版本。`mass_reinvest` 是可选消融：重复查询的节省继续投入探索，保留原先按实际求解数限制预算的行为；完整优化版则同时限制逻辑搜索步数。报告墙钟/进程 CPU 时间、实际求解次数、质量计算耗时、配对差值区间，并保存原版和当前实现快照。各版本逐项核对接受序列与位置、UNSAT 证书及 Retail 原工具输出。首次阶段的 `synthetic_v2` / `retail_v1` 使用调整逻辑步数预算之前的实现；应以各目录的快照为准。

优化记录及退步分析见 [OptimizationFindings.md](OptimizationFindings.md)。历史结果不会被覆盖或与新版本混合汇总。自适应版作为实验对照保留，尚无证据支持它普遍优于直接缓存原始核。

调用次数、探索收益和算法局限的深入分析见 [AlgorithmAnalysis.md](AlgorithmAnalysis.md)；`analyze_query_counts.py` 只重放已有事件，不调用求解器或运行历史源码。

## 统一证明复用层

`proofs.py` 为同一个静态 CP 模型维护两类证明：活跃 nogood 的子集索引，以及已证实 SAT 完整补全的文字位图索引。单绑定 nogood 查表，二绑定查邻接表，较长 nogood 使用两个监视的否定等值文字进行冲突匹配。监视位置可跨独立查询保留，赋值每次独立重置；该层不做单位传播闭包，也不修改候选值。

SAT 查询返回完整 X 补全和 Y 见证。补全可以证明其任意部分赋值有解；只有候选 X 与完整补全相同时，才复用见证直接接受。不会把求解器找到的补全主动加入输出，也不会去重重采样。原始核基线及其他缓存策略均使用相同能力，`rejection` 保持逐候选调用 CP 的参照。

默认 `Config` 开启 `proof_reuse`、`sat_memo`、`eager_proofs`、`defer_scores`，`proof_index='watched'`；可用 `dataclasses.replace` 分别关闭或选择 `'linear'`。先推进已有证明，再对需要新查询的区间计算概率评分；推断仍支付一个逻辑搜索步骤，实际 CP 调用单独计数。自适应门控对这些实际学习耗时照常计费。`charge_memo=False` 的消融维持按实际调用计费，不启用提前推进；它不提供相同的逻辑步骤上限。

nogood 容量只约束活跃不可行区域；删除、包含替换和淘汰后重建索引。SAT 证明另存，当前无容量上限，报告 `proof_store.sat_entries`，不能据此声称整个系统内存有界。已建立的搜索节点端点仍保留其有效证明状态。当前也不跨模型或数据库版本复用证明。

新增 `sat_cache_hits`（业务输入的完整见证命中）、`memo_hits`（搜索中的证明/精确查询复用）、`inference_hits`（SAT/UNSAT/exact）、`proof_store`（条目数及监视操作）和证明查找/维护耗时。实际查询事件保存补全、见证和构造/求解/提取时间；推断事件保存证据来源。审计器逐条检查蕴含方向、见证及实际调用/推断计费；候选恒等式变为 `business_calls + cache_hits + sat_cache_hits = candidates`。

`memoize_misses=True` 另外记住当前证明库中未找到证据的部分赋值；新增 SAT 证明或活跃 nogood 改变时全部失效，最多 32768 项。这不是 SAT/UNSAT 证明，也不消费逻辑搜索预算；`proof_miss_hits` 单独计数。可设为 False 或使用消融策略 `mass_no_miss_cache`。Oracle 复用模型的 Python 包装对象，每次查询先用基础模型完全重置 proto 和常量映射；不保留旧 assumptions，也不复用 CP-SAT 学习子句，单个 Oracle 供串行调用。

`benchmark_optimization.py` 提供 `legacy_raw`、`legacy_mass`、`raw`、`mass_optimized`、`raw_linear`、`mass_linear`、`mass_no_witness`、`mass_exact`、`mass_lazy`、`mass_eager_scored`、`adaptive` 等对照。若显式基线目录含 `oracle.py`，旧版使用自己的 oracle，避免将新补全提取开销加到旧基线。结果和局限见 [ProofReuseFindings.md](ProofReuseFindings.md)。

后续本地性能优化及负结果见 [LocalPerformanceFindings.md](LocalPerformanceFindings.md)。新增 `raw_python` / `mass_python`（仅关闭 C 抽样）、`mass_no_miss_cache` 和 `legacy_adaptive` 消融。旧基线同时使用自己的 `proofs.py` 和抽样函数。`--candidate-limit` 默认 100000；大批量、低接受率任务应显式设置更高上限，未完成状态记录于 `failure.json`。

## 主动探索的独立机制实验

`retail_trajectories.py` 新增有限目录、多步骤的真实 Retail CP 状态模型；`benchmark_retail_trajectories.py` 对照固定操作、独立操作混合与状态传递，分别计算总求解器调用和时间。每步余额、订单状态与支付历史均和原始工具离线核对。范围、原工具语义细节和结果见 [RetailTrajectoryFindings.md](RetailTrajectoryFindings.md)，诊断入口为 `analyze_retail_trajectories.py`。

三类算法的统一性能比较见 [ThreeAlgorithmComparison.md](ThreeAlgorithmComparison.md)。`compare_sampling_algorithms.py` 从事件分别统计业务/探索 SAT、UNSAT、UNKNOWN，报告总调用、在线 CPU/墙钟、求解/构造/提取时间、证明复用与完成率，并生成 CSV、JSON、可筛选 HTML 和调用—时间图。主表固定一个主动实现，其他版本放消融；`benchmark_exploration.py` 支持 `rejection`，可用 `--wall-limit` 对各方法统一设置软额度，保留未完成前缀。

[ActiveExplorationAnalysis.md](ActiveExplorationAnalysis.md) 分析探索相对持续学习 raw 的净收益、优势条件与损失边界。`exploration_workloads.py` 提供条件图资源分配、等价强核编码、可行容量负面对照，以及包含负载见证的装箱。使用默认原生核，不填充或弱化求解器证据。

`exploration.py` 另设 `core_fifo`、`core_mass`、`core_scout`，从实际核出发做定向删除探测，与默认 MassCore 区间搜索独立对照；没有替换默认策略。单文字核不探测，空任务前沿不进入调度；侦察版连续 3 次付费探测没有新增不可行覆盖后停探，此规则不保证一般净收益。`core_scout_unskipped` 只用于诊断空前沿开销。

`core_timed` 另设 10 毫秒已观测学习墙钟额度，在步骤之间检查，单步可能超支；`core_delayed` 是等待 32 候选的时机对照。`mass_safe` 保留原区间搜索并提供相同的辅助 UNKNOWN 放弃能力，用于公平比较。侦察的 `positive_score` 在预算回退模式下只表示上界为正，不证明实际新增覆盖。

`context_exploration.py` 将原生业务核的公共条件与上下文多样性纳入在线决策：`context_gate` 仅控制原核收缩的启动，`context_direct` 直接向 CP 查询公共条件。信号结合先验质量、剩余样本、上下文到达率及实测成本，不作为不可行证明。实现、五组独立对照和限制见 [ContextAdaptationFindings.md](ContextAdaptationFindings.md)；审计入口为 `analyze_context_adaptation.py`。

此独立策略允许放弃辅助 UNKNOWN 查询的任务，因为该查询不承担当前业务候选的可行性判断。业务 UNKNOWN 仍停止采样；默认 MassCore 原有的保守终止规则不变。所有未知结果均不作为可行或不可行证明。

```sh
python3 tau-bench-python/benchmark_exploration.py --preset grid \
  --samples 8 128 --seeds 101 113 127 149 163 \
  --strategies raw mass_b4 fifo_b4 width_b4 core_fifo core_mass core_scout \
  --output /tmp/active-exploration-new
```

还可用 `--preset packing`、`retail`、`hardness`、`multiguard`，`--capacity 4`、`--aux-timeout 0.02`、`--mass-nodes 32`。超时规则对所有算法相同，只限制部分赋值的辅助调用，完整业务查询仍用默认 5 秒；概率节点预算默认关闭，发生回退会标记覆盖率界限。未完成轮次保留并单列完成率，不能把较少的未完成工作算作节省。每轮保存源码、CP 模型和先验哈希，核对接受流及前缀，并在机制实例中独立检查每个部分查询与核的逻辑有效性。分析工具 `analyze_exploration.py` 只读取显式选定的结果目录，输出净业务失败/成功调用收益、探索支出和科学图表。

`certify_exploration_gap.py` 生成局部核独占反例，`--verify-native` 可穷举小模型核对原生接口的完整输入核词汇；`profile_exploration_queries.py` 离线复解记录的代表性绑定，报告 CP 冲突/分支/确定性工作量。诊断求解与在线算法调用严格分计。
