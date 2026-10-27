# 基于约束编程的分布保持测试采样方法

这里是可单独复制、改名和运行的论文工作目录。主实验、测试、绘图、参考数据校验和论文编译均不依赖外层 Tapas 仓库。**唯一例外是 `tau-bench-tapas/`**，其中集中保存 Tapas 原型及早期 Tapas 实验。

版本控制保留源码、测试、论文资料和整理后的 Markdown 分析报告。已有实验运行目录、验证输出、逐次事件、性能分析文件及中间图表已移至仓库根目录的 `tmp/external/`，保留原来的相对路径，不提交。下文与历史分析中的 `results/`、`verification/` 路径对应这份本地归档；旧实现对照需要另行提供基线快照。

从仓库根目录运行时，新产物默认写入 `tmp/external/`；独立复制后，Python 实验和论文编译使用当前运行目录下的 `tmp/external/`，不依赖父仓库。也可以通过 `--output` 或 `--report` 显式指定临时输出位置。

## 安装与执行

以下命令均从本目录执行；本目录不必叫 `external`，父目录也不必是 Git 或 Tapas 项目。

```sh
python3 -m pip install -r requirements.txt
python3 tau-bench-python/fetch_upstream.py
python3 -m unittest discover -s tau-bench-python -p 'test_*.py'
python3 -m unittest discover -s tests -p 'test_*.py'
python3 tau-bench-python/run_experiments.py --suite synthetic --output /tmp/cp-sampling-new-run
```

Retail 运行使用同级 `tau-bench/` 内的数据与工具，不访问父目录。数据来源固定为提交 `59a200c6d575d595120f1cb70fea53cef0632f6b`，以包内 SHA-256 清单核验，不要求保留其 `.git`。仅首次下载缺失数据需要 Git/网络；已有有效快照可以离线运行。

完整隔离验证：

```sh
python3 verify_standalone.py
```

验证器将目录复制到任意名称和层级的临时位置，移除 `tau-bench-tapas/` 与 Git 元数据，清除 Tapas 相关环境变量，并通过 Python 审计钩子禁止访问原源码目录；然后从无关的空工作目录执行单元测试、39 次合成/Retail 配对小实验（含自适应 MassCore）、原始工具核对、绘图和 PDF 编译。结果写入调用目录的 `tmp/external/verification/standalone.json`。没有 TeX 时可以用 `--skip-paper` 只验证 Python 部分，此时报告不会声称已验证论文编译。

这里的自包含指不依赖 Tapas 项目；Python、OR-Tools、绘图库和 TeX Live 仍是声明的系统/第三方依赖，没有把它们冒充为已打包的离线运行环境。

## 写作与研究入口

| 资料 | 位置与状态 |
|---|---|
| 当前论文源稿 | [paper/PaperDraft.tex](paper/PaperDraft.tex)，仍是草稿 |
| 已有论文 PDF | [paper/PaperDraft.pdf](paper/PaperDraft.pdf)，保留已有版本 |
| 独立编译说明 | [paper/README.md](paper/README.md)；`python3 paper/build.py` 默认输出到调用目录的 `tmp/external/paper/build/` |
| 理论与相关工作记录 | [research/SamplingResearch.md](research/SamplingResearch.md)，候选机制和推导写入正文前须核对假设 |
| 早期研究方向 | [research/archive/PaperPlan.md](research/archive/PaperPlan.md)，历史状态测试写作计划，不是当前论文计划 |
| 本地参考文献 | [references/](references/)，含 Scenic 论文 PDF 和索引 |
| 首次迁移记录 | [migration_manifest.json](migration_manifest.json)，历史路径和哈希保留原样 |
| 本次依赖收拢记录 | [self_containment_manifest.json](self_containment_manifest.json)，记录早期实验移入例外目录的对应关系 |

论文的 TeX、TikZ 图形和编译脚本均位于 `paper/`，不需要外层项目文件，也不要求使用桌面编辑器。

## 实验与案例

| 目录 | 用途 |
|---|---|
| [tau-bench-python/](tau-bench-python/README.md) | **论文主实验**：独立 Python/OR-Tools 模型、采样、对照、消融、验证、统计与绘图 |
| [tau-bench/](tau-bench/) | 固定版本的原始数据与工具；不修改上游源码 |
| [tests/](tests/) | 数据快照和目录可移植性回归 |
| [tau-bench-tapas/](tau-bench-tapas/README.md) | **允许依赖外层 Tapas 的例外**：原型、审阅及 `experiments/sampling/` 下的早期实验 |

独立 Python 实验不导入例外目录中的代码；常规采样和测试不执行历史源码快照。优化消融工具仅在显式提供 `--baseline` 时加载指定旧实现，基线由使用者另行提供。Tapas 可执行文件、`src/`、`include/`、产品文档、父仓库 Git 状态或工作目录都不是其输入。

## 已有结果索引

| 阶段 | 结果入口 | 说明 |
|---|---|---|
| Python 合成 pilot | [报告](tau-bench-python/results/synthetic_pilot/report.html) | 324 次运行 |
| Python 综合 smoke | [报告](tau-bench-python/results/smoke/report.html) | 78 次运行，含七种 Retail 修改操作 |
| Python 多商品 pilot | [报告](tau-bench-python/results/retail_multislot_pilot/report.html) | 18 次双商品运行 |
| Python 缓存与门控 smoke | [报告](tau-bench-python/results/control_smoke/report.html) | 60 次运行 |
| MassCore 优化与退步复核 | [分析](tau-bench-python/OptimizationFindings.md)、[Retail 对照](tau-bench-python/results/optimization_20261021/retail_final/report.html)、[32 样本退货复测](tau-bench-python/results/optimization_20261021/return_diagnostic/report.html) | 六轮共 1030 次策略运行，分协议保存，不合并统计 |
| 统一证明复用与监视索引 | [分析](tau-bench-python/ProofReuseFindings.md)、[最新 Retail](tau-bench-python/results/proof_reuse_20261021/retail_deferred/report.html) | 三轮实现、1485 次策略运行；SAT/UNSAT 蕴含及完整见证复用 |
| 本地性能与可选 C 扩展 | [分析](tau-bench-python/LocalPerformanceFindings.md)、[配对轨迹与统计](tau-bench-python/results/local_optimization_20261021/analysis.json) | 主实验 630 次策略运行，另有 60 次自适应及 380 次短任务复核；各协议分开统计 |
| 主动探索的价值与边界 | [理论、证书与结果](tau-bench-python/ActiveExplorationAnalysis.md)、[完整归因](tau-bench-python/results/active_exploration_20261021/analysis/attribution.json) | 最终 14 个协议 7200 次策略运行，含 10 次单列的未完成项；原生核、强核阴性对照、宽度/容量/硬度与 Retail 复核 |
| 上下文冗余驱动的自适应探索 | [信号、算法与验证](tau-bench-python/ContextAdaptationFindings.md) | 仅使用已观测原生核；比较冗余门控与直接公共条件查询，纳入剩余批量、概率与成本 |
| 三类算法的全面性能对照 | [指标与统一协议](tau-bench-python/ThreeAlgorithmComparison.md) | 拒绝采样、原始核缓存、主动探索；业务/探索总调用、求解时间、本地成本、完成率与固定版本消融 |
| Retail 多操作与状态耦合 | [模型、对照与诊断](tau-bench-python/RetailTrajectoryFindings.md) | 真实目录、余额/历史/订单状态跨步传递；混合先验匹配控制，原始工具逐步核对 |
| 早期 Tapas 合成 pilot | [报告](tau-bench-tapas/experiments/sampling/results/pilot/report.md)、[分析](tau-bench-tapas/experiments/sampling/PilotFindings.md) | 依赖 Tapas，已移入例外目录 |
| 早期 Tapas 有限缓存实验 | [报告](tau-bench-tapas/experiments/sampling/results/finite_cache/report.md)、[分析](tau-bench-tapas/experiments/sampling/CacheFindings.md) | 同上 |
| 早期 Tapas 接口控制实验 | [报告](tau-bench-tapas/experiments/sampling/results/production_controls/report.md)、[分析](tau-bench-tapas/experiments/sampling/ProductionControls.md) | 同上 |
| 上轮迁移验证 | [报告](tau-bench-tapas/experiments/sampling/results/relocation_smoke_20261021/report.md) | 144 次路径 smoke；不是主实验统计 |

这些结果来自不同版本与协议，不能合并成同一统计样本或直接充当最终论文结论。历史元数据、日志和实现快照中可能提及旧机器路径或 Tapas；它们作为只读证据保留字节和哈希，不是当前运行依赖。移动后的实际位置以本索引和后续迁移记录为准。
