# 热点性能案例

本目录统一存放基础热路径、从综合算法提取的组合热点及负对照的源码。
综合算法源码和运行入口保留在上一级目录。

- [compare_script.py](../compare_script.py) 自动发现本目录全部 `.tap`/`.py` 案例，与综合算法一起重新测量，并更新上一级两份 README 的“基础热路径”表。目前收录 27 组：19 组基础热点、6 组算法衍生诊断热点、2 组负对照。热点优化后可以转为领先，不要求它持续慢于 Python。
- 目前全部 27 组热点均已提供 Tapas、Python、Lua 三种实现，三语言结果均进入上一级报告。以后若有案例暂缺 Lua，报告会标为“—”并排除其 Lua 几何平均。新增 `.tap` 案例必须提供同名 `.py` 参考，脚本会检查遗漏。
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

全部 27 组的测量目的、默认写法差异、修复及结论边界见 [逐项审查](AUDIT.md)。主表新增测量内容，七组案例同时生成 Python 结构对照；完整行/频数检查可运行 `python3 test/benchmarks/verify_hotspots.py build-release/bin/tapas`。

原生 heap 包及 Dijkstra、Huffman 的源码 A/B 见 [heap 实验](HEAP.md)。
通用源码对照入口为 [source_ab_script.py](../source_ab_script.py)，比较同一运行时上的两份等价 Tapas 实现。
