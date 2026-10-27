# 抽样论文的早期 Tapas 实验

本目录属于明确依赖外层 Tapas 的例外区域。它已从论文根目录的 `experiments/tapas-sampling` 移入 `tau-bench-tapas/experiments/sampling`，不参与独立 Python 实验的加载、测试或编译。

本目录由原 `test/sampling` 迁入，保存早期实现与原始实验结果。论文当前主实现见 [独立 Python/OR-Tools 实验](../../../tau-bench-python/README.md)，总资料入口见 [论文目录](../../../README.md)。

这套实验调用实际的 `solve::sample`，Python 只负责生成实验、枚举目标分布、检查输出和汇总结果。没有另写一个模拟采样器来替代 Tapas 实现。

运行完整 pilot（需要已经构建的 Tapas、其 solve 后端，以及绘图用的 matplotlib）：

```sh
python3 external/tau-bench-tapas/experiments/sampling/run_experiments.py
python3 -m unittest discover -s external/tau-bench-tapas/experiments/sampling -p 'test_*.py'
```

快速检查或另存一轮结果：

```sh
python3 external/tau-bench-tapas/experiments/sampling/run_experiments.py --seeds 11 29 --horizons 8 --output /tmp/tapas-sampling-smoke --no-plot
```

`--tapas` 指定二进制。`--seeds` 支持 2、3、5 或 10 个不同 seed，使用相应自由度的双侧 95% Student 区间。`--horizons` 是接受样本数，不是原始候选数。正式运行宜使用新的 `--output` 目录，避免混合历史输出。

## 问题族

六个独立二元输入 `x0` 至 `x5`；未参与约束的输入也必须保留正确的条件分布。每条禁止模式列出不能同时取 1 的坐标。

| 名称 | 各输入取 1 的概率 | 禁止模式 | 检验目的 |
|---|---|---|---|
| equal_width | .8, .8, .2, .2, .5, .5 | (0,1), (2,3) | 同宽度 core 的质量差异 |
| overlap_low | .8, .8, .6, .6, .1, .5 | (0,1), (2,3), (0,4) | 低缓存重叠 |
| overlap_high | .8, .8, .6, .6, .9, .5 | 同上 | 总质量与新增质量的选择逆转 |
| cardinality | .02, .5, .5, .5, .5, .5 | (0), (1,2) | 最小基数与最大质量的分离 |

局部 oracle 穷举失败输入全 1 的所有绑定子集，检查每个区域的全部输入是否不可行，并枚举其 MUS。重叠实验冻结缓存为 `x0=1 ∧ x4=1`。这是已知安全候选集上的局部选择问题，不能解释为完整在线算法的性能保证。

## 配对协议

- 5 个 seed：11、29、47、71、97；接受批量为 8、32、128。
- 9 个配置：rejection、full、raw core、deletion MUS、minimum-cardinality、MassCore β=1/4，以及 β=4 的 FIFO/绑定数调度。
- 每个配置跑 trace 开、关两次，共 1,080 次记录运行；另有每个问题族的一次 worker 预热。
- 配置顺序随 seed 固定打乱；比较关闭 trace 后的采样器内部时间。该时间不包含 Tapas 编译、进程启动和 JSON 输出，不用于声称部署端到端加速。
- 接受输入序列、接受候选位置、不同批量的接受前缀必须逐项一致；trace 开关的调用计数也必须一致。
- 对整个 64 点空间枚举可行集及条件分布；检查样本可行性、单调缓存质量和平方根预算上界。
- 统计 TV 时只合并每个 seed 的最大批量 rejection 样本，不重复计算同一流的策略副本或批量前缀。
- 调用差值使用同 seed 的 raw core 作配对基线；总调用包含业务查询和泛化查询。所有配置都受同一个累计平方根预算约束，但相同 β 不代表每个算法实际花费相同。

测试还会故意调换接受输入顺序、改变接受位置或污染缓存质量，确认这些错误不能通过实验检查。

## 输出

默认结果位于 `results/pilot/`：

- `report.md`：枚举 oracle、完整均值与区间、分布检查。
- `summary.json`：机器可读统计。
- `rows.json.gz`：完整样本、逐候选 trace、计数和时间；用 Python `gzip.open` 读取。
- `metadata.json`：先验、约束、seed、二进制哈希和 Git 状态。
- 每个问题族的 `.tap`：实际执行源文件；`.stderr.txt`：后端诊断。
- `total_calls` 和 `extra_calls` 的 PNG/PDF：总调用与相对 raw core 的配对差值。

本轮研究判断见 [PilotFindings.md](PilotFindings.md)。这些是小域、独立先验、无限安全缓存下的机制实验；尚未验证大域、相关先验、多业务域或外部采样器基线。

## 第二轮：受限缓存与发现成本

```sh
python3 external/tau-bench-tapas/experiments/sampling/run_cache_experiments.py
```

结果保存于 `results/finite_cache/`，研究判断见 [CacheFindings.md](CacheFindings.md)。

这轮是受控的 oracle 调用成本实验，区别于上面的生产采样器性能实验：

- 每个问题族公开 21 个候选区域，即单变量和双变量同时取 1；其中包含实际可行的干扰区域。Tapas 的 84 次 `hold` 查询与有限域枚举逐项核对。
- 固定同一候选池，所有选择器都先移除被其他安全区域包含的区域。比较宽度排序的两种 tie-break、总质量排序、新增质量贪心和精确最大覆盖。
- 缓存容量 1、2、4，候选输入长度 32、128、512、2048，使用区别于 pilot 的 10 个 seed，共 2,880 次配对回放。
- `equal_width` 将前两变量和中间两变量的概率顺序交换，以显露按变量顺序打破宽度平局的影响；此改变及参数均公开，两轮的数值不直接混比。
- `gated_exact` 在生成候选输入前，按 `H × 单次查询的乐观覆盖增量 > 1` 选择并验证区域；达不到阈值时停止。未知区域先被乐观地视为安全，只在求解器证明 UNSAT 后才加入缓存。
- 每条回放从冷启动计入所有发现查询，包括返回 SAT 的失败探索。查询轨迹完全不读取未来候选输入；质量计算和缓存优化的 CPU 成本未计入。
- 对高重叠及 cardinality 问题族另执行 24 条实际 Tapas 查询脚本，核对每个发现查询、每个缓存命中和每个业务查询的状态及总计数。脚本按受控规划器生成的轨迹运行，不代表已把调度策略接入生产 `solve::sample`。
- 保存完整字典证书、发现轨迹、回放计数、查询审计日志、统计区间与图表。固定候选长度与固定接受样本数是不同协议。

门控只根据一个查询的乐观增益决策，不能证明整段发现过程回本；也未包含多个查询的联合价值。第二轮保留了它仍然亏损的短批量案例。

## 生产接口上的控制实验

当前生产 `solve::sample` 已支持缓存容量、独有质量/FIFO/绑定数交换、探索关闭与收益门控、累计泛化查询上限，以及独立的详细事件和分项计时。接口说明见 [solve README](../../../../src/stdlib/solve/README.md)，可运行示例见 [sampling_controls.tap](../../../../examples/solve/sampling_controls.tap)。

```sh
python3 external/tau-bench-tapas/experiments/sampling/run_experiments.py --controls --seeds 101 137 173 211 257
python3 test/solve/test_sample_controls.py build/bin/tapas
```

控制实验默认保存到 `results/production_controls/`，调用真正的生产接口，所有策略都进行 trace 开关的配对运行；详细事件关闭以便单独比较计时。它与第二轮的固定字典回放是不同实验。`raw` 和 `mass_b1` 使用无限缓存；其余控制配置使用容量 1。相同容量的选择比较应使用 `bounded_mass`、`bounded_fifo`、`bounded_width`。`capped`、`gain_capped` 的额外查询上限预先固定为 8；`off` 禁用额外查询。

实现范围及本轮结果见 [ProductionControls.md](ProductionControls.md)。

集成测试会逐项枚举核查准入区域安全性、事件查询状态、真实缓存质量和准入新增质量，核对非连续 Rule 形参索引、相关联合先验、存在性 witness，以及源码/字节码的一致性；并注入 UNKNOWN、unsupported、error 检查它们不学习新区域。FIFO 和绑定数淘汰不要求缓存总质量单调。测试已注册为 CTest 的 `solve_sample_controls`（找到 OR-Tools 解释器时启用）。
