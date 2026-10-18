# List 字典序与默认记录堆优化

List 的 `<`、`<=`、`>`、`>=` 使用原生递归字典序；heap 的默认比较和 `sort` 共享同一顺序。Dijkstra 移除 `distance_less`，直接使用 `[distance, vertex]`；Python 和 Lua 已使用相同平局顺序。没有增加包函数、缓存算法结果或改变指定算法。

新增 `heap_order` 三语言热点，包含重复优先级及第二字段比较。原有 `heap_callback` 保留，继续诊断原生堆进入 VM 比较函数的开销；不能用消除回调的写法替代这个诊断。Huffman 的 Pair 节点及树载荷继续使用显式频率比较函数，避免把树结构当作平局排序键。

## 比较合同

- 从首项开始，首个不同元素决定顺序；共同前缀相同时较短 List 在前。嵌套 List 递归使用同一规则。
- 默认顺序支持数值、按完整字节内容比较的 String，以及这些值组成的 List。Int/Float 允许混用且保留大整数精度；NaN 无默认顺序。数值、String、List 三类不能相互排序。
- List 运算符只访问决定结果需要的位置。sort 和默认 heapify 检查全部输入值，默认 push 检查新增值；因此不支持的尾部载荷也需要显式堆比较器。验证成本为 O(v)，比较成本取决于公共前缀长度；循环及超过 512 层的嵌套受限。
- 现有 `==`/`!=` 的结构匹配保持不变。例如 `[1] <= [1.0]` 与 `[1] >= [1.0]` 均为 true，`[1] == [1.0]` 仍为 false。
- List 与其他对象比较时继续保留对侧的反向运算符分派，包括动态 RuleTerm 构造。这一行为有独立回归覆盖，没有针对 Rule 类型硬编码。
- sort 不保证稳定性。非法类别的比较先记录错误，并以内部类别顺序保证 qsort 比较器仍满足排序条件；排序结束后报错。此类别顺序不作为语言语义暴露，报错时仍保留所有元素及其引用。

## 同一运行时的源码 A/B

三轮、每轮九对，交替先后，每轮各丢弃一次预热；计时为程序内部进程 CPU 时间，包含任务内复制及归约。每次校验和一致。B/A 是候选耗时/原版耗时，以下结果不与跨语言比值混用。

| 变更 | 原版 ms | 候选 ms | 配对 B/A | IQR | 三轮 B/A |
|---|---:|---:|---:|---|---|
| Dijkstra：原距离比较回调 → 默认字典序 | 7.096 | 4.040 | 0.573 | 0.566–0.587 | 0.582 / 0.574 / 0.571 |
| Dijkstra：显式字典序回调 → 默认字典序 | 9.279 | 4.037 | 0.441 | 0.434–0.450 | 0.443 / 0.435 / 0.441 |
| heap_order：显式字典序回调 → 默认字典序 | 12.561 | 2.889 | 0.231 | 0.226–0.237 | 0.237 / 0.235 / 0.230 |

第一行同时改变了同距离时的顶点出队顺序，可能影响过期条目数量。第二行两侧都使用距离、顶点的字典序，适合衡量取消 VM 回调的收益；不把第一行全部收益归因于回调边界。完整源码、哈希和原始样本见 [原写法对照](experiments/list_order_dijkstra_priority_ab.json)、[同字典序对照](experiments/list_order_dijkstra_lex_ab.json)、[独立堆任务](experiments/list_order_heap_order_ab.json)。

## 数值与回调路径的回归分析

初版把数值也送入递归比较分派，纯浮点堆 B/A 约 1.115、整数 sort 约 1.454、浮点 sort 约 1.296。恢复同类数值的直接比较后，纯浮点堆和浮点排序的退化消除，整数排序的退化明显缩小；再发现混合数值堆约 1.084、混合 sort 约 1.184。单纯拆分函数没有解决混合路径的问题。把精确数值比较放入内部 `value_order.h`，由堆、排序和递归元素比较共享可内联实现，才消除了主要分派成本。没有放宽 NaN、边界或精度检查。

输入验证也曾给整数排序带来约 4% 开销。整数输入的验证改为直接检查标签后，小幅开销一度降到约 1.5%；共享混合数值比较内联后，最终组合的三轮整数排序对照又出现约 5%–7% 的持续回退，不能视为随机误差。随后在输入扫描时识别纯 Int/Float 列表，一次性选择独立的简单比较函数，减少每次比较的通用分派开销；数值精确性、输入验证和错误输入处理保留。剩余开销与新增扫描和类型识别有关，并不宣称纯整数排序也获得了收益。下表区分正式测量值及其波动，不把所有小幅回退都解释为随机误差。

| 原有功能，同源码跨运行时 | 配对 B/A | IQR | 三轮 B/A |
|---|---:|---|---|
| numeric_float_heap | 0.899 | 0.865–0.934 | 0.884 / 0.882 / 0.920 |
| numeric_int_sort | 1.025 | 1.004–1.037 | 1.031 / 1.011 / 1.025 |
| numeric_float_sort | 0.910 | 0.886–0.933 | 0.904 / 0.919 / 0.910 |
| mixed_heap | 1.026 | 1.004–1.054 | 1.036 / 1.015 / 1.025 |
| mixed_sort | 1.139 | 1.106–1.164 | 1.162 / 1.139 / 1.134 |

纯数值三项使用三轮、每轮九对；混合输入使用三轮、每轮二十一对复测。原有十五类堆控制覆盖有序、倒序、全相等、重复、最大堆、记录、捕获、昂贵回调、原生比较函数及交错 push/pop，每项三轮、每轮七对。另对固定十六项综合算法做同源码跨运行时测试，Dijkstra 特意保留旧比较器，让基线也能运行，以隔离运行时变更；正式主表使用优化后的默认字典序写法。

[纯数值原始数据](experiments/list_order_scalar_controls.json) · [混合输入复测](experiments/list_order_mixed_repeat.json) · [十五类堆控制](experiments/list_order_runtime_controls.json) · [十六项同源码运行时 A/B](experiments/list_order_algorithm_runtime_ab.json)。初版退化数据保留在 [数值分派退化](experiments/list_order_before_scalar_fast_scalar_controls.json) 与 [混合分派退化](experiments/list_order_before_numeric_dispatch_mixed_controls.json)，以及 [独立数值比较器之前的最终组合](experiments/list_order_before_homogeneous_sort_scalar_controls.json)。`list_order_initial_controls.json` 曾与正确性检查并行，仅作初筛记录，不作为正式性能结论。


十五类堆控制的最终配对比值如下。原生比较函数第一轮控制的 IQR 跨过 1，随后单独增加到三轮、每轮二十一对复测：B/A 为 1.001，三轮为 0.996 / 0.997 / 1.009；没有复现原先约 1.4% 的中位数回退。回调执行工作量和调用协议未改变，小幅波动不据此判为新分派逻辑造成的确定性退化。[复测样本](experiments/list_order_native_comparator_repeat.json)。

| 堆控制 | 配对 B/A | IQR |
|---|---:|---|
| numeric_random | 0.885 | 0.868–0.905 |
| numeric_ascending | 0.926 | 0.922–0.949 |
| numeric_descending | 0.922 | 0.908–0.941 |
| numeric_equal | 0.994 | 0.975–1.016 |
| callback_random | 0.981 | 0.969–1.000 |
| callback_equal | 0.985 | 0.965–1.007 |
| callback_duplicates | 0.979 | 0.971–0.988 |
| callback_maximum | 0.986 | 0.964–1.000 |
| callback_records | 0.987 | 0.968–1.011 |
| callback_captured | 0.994 | 0.977–1.004 |
| callback_expensive | 1.001 | 0.987–1.016 |
| native_comparator | 1.014 | 0.998–1.054 |
| incremental_build | 0.987 | 0.964–1.007 |
| interleaved | 0.994 | 0.983–1.037 |
| heapify_ordered | 0.997 | 0.983–1.012 |

## 最新三语言主表摘录

完整四十五组、每组三语言七次正式运行及一次预热，见 [中文主表](../README.md)、[英文主表](../README_en.md)。新增热点不改变固定十六项综合算法集合及其等权规则。

| 案例 | Tapas ms | Python ms | Lua ms | Tapas/Python |
|---|---:|---:|---:|---:|
| dijkstra | 4.002 | 3.408 | 5.922 | 1.174 |
| heap_order | 2.797 | 4.261 | 9.159 | 0.656 |
| heap_callback | 8.714 | 3.993 | 8.142 | 2.182 |

最终 Release SHA-256：`7777c261957cc1780d2e9ad58b08e4f79fed29ae692ef55d68e9afea4ea7d41c`。
基线 SHA-256：`80d8465438372b58270f8201cdc5cd9cfc244cee0b5d50ba5ff1da75470072c4`。构建配置与原始样本在各 JSON 中保存，当前主表含全部 135 份源码与 catalog 哈希。

## 正确性与复现

- Release 54 项测试、ASAN 5 项相关测试通过，包括源码与字节码模式。新增默认比较单元测试覆盖混合大整数边界、NaN、内嵌 NUL、循环保护及内部错误回退顺序的反对称性和传递性。
- 四十五组三语言案例及十四组小输入内核通过；七十九项完整结果探针通过。新增默认堆探针比较完整双字段记录，包含逆序平局、负值、重复记录及空输入。
- 十二项 benchmark 脚本单元测试通过；两份 README 由最终同一批样本自动生成。中英文 Syntax、Stdlib 与包文档同步更新。
- [实现前源码快照](experiments/pre_list_order_sources.json) 和 [最终运行时源码](experiments/list_order_runtime_sources.json) 记录实现与对照；所有性能运行顺序执行，没有并行构建或正确性测试。

```sh
python3.14 test/benchmarks/verify_semantics.py build-release/bin/tapas
python3.14 test/benchmarks/verify_hotspots.py build-release/bin/tapas
python3.14 test/benchmarks/compare_script.py build-release/bin/tapas --runs 7 --json test/benchmarks/hotspots/comparison_results.json
```
