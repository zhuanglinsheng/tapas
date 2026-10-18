# 热点性能案例

本目录统一存放基础热路径、从综合算法提取的组合热点及负对照的源码。
综合算法源码和运行入口保留在上一级目录。

综合算法比较同一算法下各语言的最佳惯用实现；本目录主要用于定位瓶颈，需要保留要测的关键操作。任务型差距提示能力缺口，同机制对照及运行时 A/B 用于归因，不能把原生批量接口与显式循环的差距直接解释为循环成本。诊断要求见 [热点合同](../SEMANTICS.md#热点合同)。

- [compare_script.py](../compare_script.py) 自动发现本目录全部 `.tap`/`.py` 案例，与综合算法一起重新测量，并更新上一级两份 README 的“基础热路径”表。目前收录 30 组：20 组基础热点、8 组算法衍生诊断热点、2 组负对照。热点优化后可以转为领先，不要求它持续慢于 Python。
- 目前全部 30 组热点均已提供 Tapas、Python、Lua 三种实现，三语言结果均进入上一级报告。以后若有案例暂缺 Lua，报告会标为“—”并排除其 Lua 几何平均。新增 `.tap` 案例必须提供同名 `.py` 参考，脚本会检查遗漏。
- [diagnose_script.py](../diagnose_script.py) 对 6 组退化热点和 2 组负对照提供优化 Python/同结构 Python 对比，以及可选的综合算法替换实验和 JSON 样本导出。
- [分析报告](ANALYSIS.md) 解释退化原因、对照实验和结论边界；[results.json](results.json) 保留测量时的环境与原始样本。
- 三语言结果见上一级的 [中文报告](../README.md) 与 [English report](../README_en.md)。
- 目标为固定 16 项综合算法的 Tapas/Python 耗时比几何平均 ≤ 0.90。运行时优化的范围、通用性及验证见 [实施说明](OPTIMIZATION.md)；[A/B 报告](AB_RESULTS.md) 和 [原始样本](ab_results.json) 比较修改前后运行时执行同一份源码的结果。

在仓库根目录运行：

```sh
python3 test/benchmarks/compare_script.py build-release/bin/tapas
python3.14 test/benchmarks/diagnose_script.py build-release/bin/tapas --algorithms --ablations --controls
python3 test/benchmarks/ab_script.py build-release/bin/tapas-ab-baseline build-release/bin/tapas --rounds 3 --runs 8
```

完整 LTO + PGO 的热点训练、构建及严格目标验收见 [PGO.md](PGO.md)；[构建 A/B](PGO_AB_RESULTS.md) 与相同配置下的源码 A/B 分开报告。

跨语言实现的同算法合同、惯用写法选择及小输入验证见 [SEMANTICS.md](../SEMANTICS.md)。历史诊断与旧源码的 A/B 不能当成当前跨语言结果。

全部 30 组的测量目的、默认写法差异、修复及结论边界见 [逐项审查](AUDIT.md)。主表与综合算法表使用相同列布局；测量内容见逐项审查，Python 结构对照保留在原始数据中；完整行/频数/堆输出检查可运行 `python3 test/benchmarks/verify_hotspots.py build-release/bin/tapas`。

原生 heap 包及 Dijkstra、Huffman 的源码 A/B 见 [heap 实验](HEAP.md)。
通用源码对照入口为 [source_ab_script.py](../source_ab_script.py)，比较同一运行时上的两份等价 Tapas 实现。

通用堆下沉策略、重复优先级的退化修复及公共回调入口的独立 A/B 见 [通用堆优化](HEAP_OPTIMIZATION.md)。

现有语法与标准库下的算法及对应热点写法优化见 [惯用写法优化](IDIOMATIC.md)，包括 Python/Lua 参考复核、源码 A/B 和边界检查。

默认 List 字典序、Dijkstra 写法与回调对照见 [字典序优化](LIST_ORDER.md)。

sort 的原生 C 对照、同源码 A/B 及复杂值的取舍见 [排序优化](SORT.md)。

List fill/count、bytes::Buffer、dense::FlatView 和 sort 后续优化见[紧凑序列与排序](COMPACT_AND_SORT.md)。

字符混合比较、动态局部初始化、标量作用域清理及回退原因见 [值路径优化](VALUE_PATHS.md)。

`heap_callback` 三语言均保留显式优先级比较；Python 采用实测更快的 slotted record。完整堆回调任务与默认排序任务的区分、独立比较次数和不可排序 payload 检查见 [公平性审查](HEAP_CALLBACK.md)。
