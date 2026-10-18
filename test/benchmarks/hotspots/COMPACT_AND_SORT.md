# 紧凑序列与排序：逐项优化结果

本轮按上一轮清单逐项实验。改动按值类型、范围合法性和输入顺序分派，不识别 sieve 或其他案例名称。原有未提交工作保留，未提交新 commit。下面区分最终构建测量与实现过程中保存的隔离实验；不同构建和不同输入矩阵不直接合并。

后续代码整理及未保留的成员检查优化见 [代码整理验证](CODE_CLEANUP.md)。

## 完成情况

| 清单项 | 最终处理 |
| --- | --- |
| sort 取样 | 分层扰动九点取样，解决固定步长与周期输入对齐；保留原有深度预算及 heapsort 保底。扩大矩阵后没有额外增加未经验证的重采样策略。 |
| sort 比较与移动 | Int 样本无重复时使用双端分区，有重复时使用单端三路分区；Float 和混合 Number 保留单端分区。分别记录比较次数与数组槽位写入次数。 |
| List fill | 对能够证明全部规范化索引合法的标量范围选择简化循环；其他范围保留逐项路径及部分成功语义。 |
| 字节容器 | 新增 bytes 包自有 Buffer 类型和 buffer(length, value)，每项一字节。 |
| 紧凑批量操作 | 新增 dense::FlatView 及 flat(array)，实现行优先共享视图；Buffer 和 FlatView 复用通用 fill/count 的底层钩子。 |
| 计数扫描 | Int/Float List 计数直接内联 identical；Bool 保持原路径。紧凑字节和浮点扫描经编译器自动向量化，不强制特定平台 SIMD。 |
| 外层循环与 Iterator | 新增精确 math::isqrt，A/B 后选择范围循环和奇数候选；测量已有 Iterator 对象池的构造与释放，未叠加新池化机制。 |

## sieve 与通用批量操作

最新普通 Release 主表：Tapas **39 µs**、Python 3.14 **49 µs**、Lua **591 µs**，Tapas/Python 为 **0.796**。Tapas 使用 BoolArray 一维视图、fill/count 和 isqrt；Python 使用 bytearray、步长切片和 count；三语言均先处理偶数，再检查奇数候选，仍是埃氏筛。

对照冻结的本轮修改前二进制与原 List 源码，3×11 次配对结果如下。单调用和批量重复反映不同的预热、分配与缓存状态，不能混为同一个比例。

| 上限 | 每次样本调用次数 | 最终/原版耗时比 | 配对 IQR |
| --- | ---: | ---: | --- |
| 20000 | 1 | 0.467 | 0.440–0.484 |
| 20000 | 64 | 0.488 | 0.476–0.496 |
| 200000 | 1 | 0.316 | 0.307–0.332 |
| 200000 | 64 | 0.490 | 0.484–0.495 |

独立阶段 A/B 中，8192/65536 项 List 的 fill 耗时比为 0.729–0.847，覆盖步长 1、3、-3。检查仍包含空范围、负索引、跨零范围和极端步长；不能证明有效的范围走原路径。

| List 计数类型 | 512/8192/65536 三组耗时比几何平均 |
| --- | ---: |
| bool | 1.013 |
| int | 0.355 |
| float | 0.311 |

Int/Float 快速路径只比较相同 Type，不转换 Int/Float、不调用重载 ==，保留 NaN 与带符号零语义。Bool 源码保持不变，小规模数据的几百分点变化不能认定为稳定收益或退化。编译器诊断显示原 Bool List 循环的向量化不被成本模型看好；紧凑数据扫描则自动向量化。

循环隔离实验中，20000 输入的 64 次批量调用，从 while 改为 isqrt 范围循环的耗时比约 0.92–0.93，再只检查奇数候选约为 0.93；单调用的微秒级样本更易波动。Python 和 Lua 的奇数候选写法也分别做了 3×11 次对照后保留。Buffer 与 BoolArray 视图在批量调用上接近，20000 单调用视图略好，且能直接保持 Bool 标记语义，因此主案例选择视图。

原生 Iterator 诊断中，参数更新/校验、构造及池化释放相对无对象对照的增量中位数约 **7.97 ns/次**。这是不含 VM 分派和实参准备的微基准，不能据此精确相减得出 VM 时间；但不支持把已有池化对象的 malloc 当成当前主要瓶颈。

证据：[批量操作隔离](experiments/sequence_bulk_ab.json)、[标量计数](experiments/count_scalar_ab.json)、[循环隔离](experiments/sieve_loop_ab.json)、[三语言写法与容器选择](experiments/sieve_final_reference_ab.json)、[最终 sieve A/B](experiments/sieve_end_to_end_ab.json)、[Iterator 原生诊断](experiments/iterator_native_cost.json)、[List 编译器诊断](experiments/list_vectorization.txt)、[紧凑扫描编译器诊断](experiments/compact_vectorization.txt)。

## sort：取样、分区与剩余边界

最终 C 层测试为 2 种数值类型 × 11 种分布 × 4 种规模（512、8192、65521、65536），共 **88 组**；每组 3×11 次配对，逐轮预热、轮换顺序。Tapas/裸 qsort 等权几何平均为 **0.551**，其中 **87 组更快**。这不是任意平台、任意元素表示或任意输入的全面领先保证。

原先周期 8、65536 项的两组回退，最终变为：

| 类型 | Tapas/裸 qsort | Tapas 比较次数 | qsort 比较次数 |
| --- | ---: | ---: | ---: |
| int | 0.448 | 180360 | 213080 |
| float | 0.425 | 180328 | 213080 |

同样使用新取样的独立计数对照中，65536 个随机 Int 的数组槽位写入由 **1536638** 降至 **611012**。此计数含交换和插入移位的数组写入，不含 C 栈临时值、初始化 memcpy，也不能统计 libc 内部移动。它支持双端分区减少移动这一机制，不能等同于内存硬件事件。

最初无条件采用双端分区时，部分重复 Float 明显回退；相等值向两端集中后还需移回中间，增加移动成本。只检查与枢轴相等的样本也不足以识别重复分布。随后扩展到样本间重复检查，但 Float 低基数输入仍有额外比较和路径成本。因此最终只为有净收益的 Int 实例保留选择逻辑，Float/混合 Number 保留单端分区。这是按比较域选择实现，不按具体案例选择。

完整任务 A/B 则计入复制、VM 校验求和等操作，与本轮修改前已优化版本对比；198 组（六类值、11 种分布、8/512/8192 三种规模）的结果如下。原生内核收益会被其他工作稀释，不能把原生比例套用于完整算法。

| 类型 | 完整任务新/旧耗时比几何平均 |
| --- | ---: |
| int | 0.990 |
| float | 0.995 |
| mixed | 0.998 |
| string | 1.003 |
| record | 1.001 |
| nested | 1.001 |

剩余落后项是 **65521 项、随机打乱的八种 Float 值**：

- 最终矩阵 Tapas/裸 qsort 为 **1.044**，配对 IQR 1.030–1.055；独立 3×31 次复测仍约 1.040，不能当作单次噪声。
- Tapas 比较 179863 次，qsort 比较 180253 次，因而不能用比较次数增加解释这一项。
- 给 qsort 增加标量 Type、同质性和 NaN 验证后，Tapas/验证后 qsort 为 **0.981**；独立复测约 0.981。这支持额外必需验证是裸对照差距的重要来源。该控制仅针对本测试生成的标量域，失败分支使用 assert，不宣称完整复现复合值错误路径。

验证单独计时仍包含输入恢复 memcpy，不能把这个时间直接当成纯验证耗时，也不能将其与其他中位数任意相减。本轮保留合法性检查，没有通过省略 NaN/类型检查换取裸 qsort 全胜。

证据：[最终 88 组与验证控制](experiments/sort_final_native_qsort.json)、[剩余 Float 复测](experiments/sort_remaining_float_repeat.json)、[198 组完整任务](experiments/sort_refined_runtime_ab.json)、[首次去相关筛选](experiments/sort_jitter_screen.json)、[无条件双端筛选](experiments/sort_bidirectional_screen.json)、[分区隔离](experiments/sort_partition_isolated_ab.json)、[样本重复检查修订](experiments/sort_partition_refined_ab.json)。原型数据仅用于各自阶段分析，不与最终矩阵混算。

## API、语义与正确性

- `bytes::buffer`、`dense::flat` 是包函数，不是根预定义函数。Buffer 固定长度，读写 Int 0..255，不截断；FlatView 保留源数组引用并共享写入，copy(view) 创建独立存储。原数组仍要求二维索引，不隐式展开字典复合键。
- 通用 fill/count 通过可选 `vtable.bulk_operations` 下沉操作；钩子返回 0 时不得有副作用，随后使用普通逐项路径。扩展 ABI 升为 5，旧扩展须重新编译；未增加新的公开能力类型或核心值类别。
- `math::isqrt` 以整数除法修正浮点初值，完整 Int 范围内精确，不计算可能溢出的整数平方。
- 本轮没有新增语法形式。此前 List 字典序比较属于新增运算符语义，已同步到中英文 Syntax 运算符表；标准库、包 README 与 Foreign 文档同步本轮接口和 ABI。
- ASAN 暴露内建 Type 单例的保留引用可被通用析构路径绕过；已在 Type 析构入口保护进程级单例，并补充通用释放及编译字节码回归。此修正不在值比较或逐元素热循环中增加检查。
- 排序单元测试扩大到 1794 组：六类值、13 种分布、23 个边界规模，检查完整值身份多重集、别名引用计数与复合元素计数。范围证明覆盖 106250 个小范围组合及 729 个极端整数组合。紧凑序列检查快速/通用路径一致性、别名索引、空输入、非法类型、共享视图生命周期及独立复制。
- Release 56 项全部通过；相关 ASAN、UBSAN 各 7 项通过。最后增加的测试及文档示例另做针对性复查。46 组三语言 benchmark、14 组语义内核、94 组完整结果探针及 benchmark 脚本 12 项单元测试通过。

## 复现与追踪

```sh
cmake --build build-release --target tapas benchmark_sort benchmark_iterator
python3.14 test/benchmarks/native_sort_script.py build-release/benchmark_sort build-release/bin/tapas --size 512 --size 8192 --size 65521 --size 65536 --runs 11 --rounds 3 --json /tmp/sort-final.json
python3.14 test/benchmarks/sequence_ab_script.py /tmp/tapas-next-baseline/tapas build-release/bin/tapas --json /tmp/sequence-ab.json
python3.14 test/benchmarks/sieve_loop_ab_script.py build-release/bin/tapas --json /tmp/sieve-loop.json
python3.14 test/benchmarks/compare_script.py build-release/bin/tapas --runs 7
```

旧二进制在改动前冻结于 `/tmp/tapas-next-baseline/tapas`，其 SHA 随 A/B 数据记录；不能用当前 Git HEAD 自动替代该基线。阶段实验的候选 SHA 不同，结果各自对应其保存的源码。最终原生排序、最终 sieve A/B、最新主表及语义探针使用同一最终 Tapas SHA，主表的全部源码摘要已核对。

[基线与原型源码](experiments/compact_sort_baseline_sources.json)、[原 sieve 源码](experiments/pre_compact_sieve_sources.json)、[最终源码快照](experiments/compact_sort_final_sources.json)。
