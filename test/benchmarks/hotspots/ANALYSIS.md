# 从综合算法反推 Tapas 的性能缺口

当前三语言语义合同及惯用写法审查见 [SEMANTICS.md](../SEMANTICS.md)，本页的历史绝对时间不代表当前结果。

本文记录首次诊断，当时只新增基准、未修改运行时。重点是找到综合算法中会退化的组合操作，并区分 VM 执行开销、数据表示和标准库能力。后续运行时修改与取舍见 [实施说明](OPTIMIZATION.md)，修改前后对比见 [A/B 结果](AB_RESULTS.md)。

后续已补齐六组热点及两组负对照的 Lua 实现；最新三语言测量见上一级 [中文报告](../README.md) 和 [English report](../README_en.md)。本文与 `results.json` 保留首次 Python 诊断的测量快照，专用诊断脚本仍用于 Python 对照与替换实验。

测量日期：2026-10-17；Python **3.14.2**；arm64；Release `-O3 -DNDEBUG`、IPO 开启；运行时提交 `99ea72197b3e`。
每项预热 1 次、有效测量 7 次，取进程 CPU 时间中位数。进程启动和编译不计入；每轮轮换语言顺序，串行运行。完整环境、可执行文件哈希和全部原始样本见 [results.json](results.json)。

旧 README 的 Python 3.14.7 数据不是本次测量。本机实际可用版本为 3.14.2。不要把两组绝对时间混在一起。

## 已确认的退化热点

六组热点都比优化 Python 参考慢；`python_matched` 保留相同循环/表示方式，`python_optimized` 使用适合该任务的惯用优化。两列必须同时看，不能把全部差距算成解释器调度成本。

| 热点 | Tapas ms | 优化 Python ms | Tapas/优化 Python | Tapas/同结构 Python | 对应负载 |
| --- | ---: | ---: | ---: | ---: | --- |
| [while_search](while_search.tap) · [Python](while_search.py) | 59.017 | 28.239 | 2.090× | 0.977× | 带提前退出的索引扫描；数独/搜索算法的相关模式 |
| [dp_row](dp_row.tap) · [Python](dp_row.py) | 43.485 | 31.960 | 1.361× | 1.276× | LCS：字符比较、依赖前一单元的条件读写 |
| [recursive_loops](recursive_loops.tap) · [Python](recursive_loops.py) | 16.615 | 11.955 | 1.390× | 1.195× | N 皇后/回溯相关模式：每层递归执行短循环 |
| [tree_build](tree_build.tap) · [Python](tree_build.py) | 31.495 | 25.013 | 1.259× | 0.693× | binary_trees：反复构造、释放读用树 |
| [record_update](record_update.tap) · [Python](record_update.py) | 16.022 | 12.554 | 1.276× | 1.196× | n_body：每步读取、修改浮点字段 |
| [string_count](string_count.tap) · [Python](string_count.py) | 26.456 | 13.044 | 2.028× | 1.103× | huffman / k_nucleotide：字符串键真实累加 |

### 1. 组合式索引和条件更新，而非单条 list_access

`dp_row` 在同结构 Python 下仍慢 **1.276×**。Python 换成 `enumerate(right, 1)`，进一步省去索引寻址，差距达到 **1.361×**。Tapas 已把左字符读取移出内层循环，没有故意重复读取左字符串。
这个核测量的是 `ch == right[j-1]`、`previous[j] >= current[j-1]` 和依赖前一个单元的写回；简单 `sink = values[index]` 无法表示这种负载。应重点看多操作之间的栈中间值、索引结果消费与分支融合，而不是继续只优化独立整数列表读取。
代码入口：[vm_idxr_fast / vm_cmp_branch_fused](../../../src/runtime/tvm.c)。这是由工作负载及源码推导的优化方向，没有把整段耗时归因到某一条 C 指令。

### 2. 递归里带短循环，和浅层纯标量调用不是同一热路径

`recursive_loops` 访问完整二叉递归树，每个调用累加 4 项：同循环 Python 比值 **1.195×**，Python 用 `sum(values)` 后为 **1.390×**。
原 `function_calls`、`tail_recursion` 和 Fibonacci 不能单独覆盖这一组合。源码中 `vm_function_has_loops` 会影响调用帧的循环状态保存/切换/恢复，循环还涉及局部变量和对象引用的生命周期。这些是需进一步采样定位的候选成本；该核确认组合负载退化，但没有单独证明循环状态保存占多少周期。

### 3. 字段读取—算术—字段写回，和纯浮点算术不同

`record_update` 比普通 Python 实例属性慢 **1.196×**，比 `__slots__` 字段慢 **1.276×**。
Tapas 使用与 n_body 一样的 `types::make_type`、`body::x` 和 `body['x']`，类型对象和实例都在计时前创建。该声明没有把存储变为原生固定偏移的浮点结构：VM 仍会进入 Dictionary 字符串字段读取/写回路径，并校验缓存槽位。
把记录改为 `[x, vx]` 的实验保留浮点更新次数、Python 参考和结果，验证改变存储/访问方式能否改善，见下面的对照表。不能由此宣称全部差距都是字符串哈希；字段缓存本来就存在。

### 4. 真正的字符串计数，而非无变化的字典写回

`string_count` 处理非均匀文本 `abacabad`，确实执行 `counts[ch] = idx_or(counts, ch, 0) + 1`。同结构 `dict.get` 比值 **1.103×**，标准库 `Counter` 比值 **2.028×**。
旧 `dict_counting` 命中后做的是 `counts[key] = counts[key]`，既没有递增，也使用整数键。VM 的 `vm_same_stored_value` 还可跳过相同值替换；因此该结果不能用于解释 huffman 的字符串计数性能。
应分别检查字符遍历、复合键读取/写回、原生 `idx_or` 调用和真实值变更，而不是把旧 dict_counting 的领先当成直方图计数已经解决。`Counter` 的额外优势属于批量原生循环，应与同结构比较分开。

### 5. Python 的数据表示和内建能力确实是算法性能的一部分

`tree_build` 对元组树慢 **1.259×**，对每次新建列表的树则为 **0.693×**。元组更紧凑，空元组叶子可以共享；Tapas 每个 `[]` 叶子都创建列表。
任务只要求构造并读取树，不修改节点，因此元组是有效且合适的 Python 参考。基准仍创建全部内部节点，禁止整树常量复用/缓存；循环后的完整遍历检查最后一棵树，计时还包含旧树释放。这个结果不支持“Tapas 的一般小对象分配都慢”的结论。
`while_search` 对同样逐项扫描的 Python 为 **0.977×**，对 `list.index` 为 **2.090×**。主要缺口是扫描缺少对应批量原生操作；不能把这个差距报告成 while 调度慢两倍。搜索每轮目标都变化，保持相同线性查找任务，没有换成预建索引字典。

## 对综合算法做单因素替换

所有变体由 `--ablations` 从当前算法源码生成到临时目录，替换前检查匹配次数；源文件变化会明确报错。没有修改原算法文件。校验和始终与原算法一致。时间是独立测量中位数，小幅变化仍有噪声。

| 替换 | Tapas ms 原→变体 | Python ms 原→变体 | Tapas/Python 原→变体 |
| --- | ---: | ---: | ---: |
| String 改为字符列表，转换在计时外 | 17.631 → 23.590 | 12.988 → 12.632 | 1.357× → 1.867× |
| 仅 Python 元组/共享空叶改为新列表 | 759.576 → 753.458 | 603.060 → 797.777 | 1.260× → 0.944× |
| 仅 Tapas 复用 placed、distance 局部槽位 | 134.543 → 128.894 | 125.606 → 127.615 | 1.071× → 1.010× |
| 两边把候选校验的 k while 改为 for | 400.733 → 393.799 | 336.405 → 358.334 | 1.191× → 1.099× |
| 两边缓存合并循环里的列表长度 | 5.249 → 4.400 | 4.780 → 4.039 | 1.098× → 1.089× |
| 仅 Tapas 字段存储改为浮点列表 | 16.022 → 14.783 | 12.554 → 13.135 | 1.276× → 1.125× |

这些实验支持的结论：

- LCS 预拆字符列表使 Tapas 更慢。直接字符串比较已有 `VM_INDEXED_BYTE_NAMELOC` 字节快路径；列表字符是复合值，读取还需维护引用。应保留这两条不同路径，不能笼统归结为字符提取分配慢。
- binary_trees 的 Python 表示优势足以改变快慢顺序，优先考虑紧凑不可变节点/共享空叶的表示能力，不能只盯 malloc。
- N 皇后复用局部槽位有改善，但幅度有限；不能说已经解释全部回溯开销。
- 缓存长度对 merge_sort 两边都有帮助；原热路径没有衡量反复长度调用与容器循环结合的成本。
- Sudoku 换成 for 的两种语言收益并不相同。还需要分开看重启扫描、回溯调用及表达式组合，不能从换循环语法推断单一原因。

## 必须保留的反例

| 负对照 | Tapas/优化 Python | Tapas/同结构 Python | 意义 |
| --- | ---: | ---: | --- |
| [substring_count](substring_count.tap) | 0.880× | 0.859× | 120,000 字符、49,003 种 8-mer；多字符切片和大量字符串键并未普遍退化 |
| [constraint_probe](constraint_probe.tap) | 0.944× | 0.939× | 原样提取 Sudoku 的候选校验，复合下标/除余/短路/提前退出；孤立内核仍可领先 |

原 k_nucleotide 使用 LCG 的 `seed % 4`，低两位只循环，长输入并不意味着多样的 k-mer；本负对照使用高位选字符，并以独立的 16 位整数滚动编码核对分布校验和。不能只用低熵重复输入泛化字符串字典性能。
constraint_probe 的 Python 两列使用同一个直接实现，重复测量的轻微差异属于噪声。它与 while_search 的结果共同说明：while 或复杂下标本身并不总是慢。尚未用这些实验完整分摊 Sudoku、Dijkstra、KMP、fannkuch 的所有剩余耗时；报告不把未验证的细分原因当作结论。

## 当前综合算法结果

同轮重测的综合算法 Tapas/Python 几何平均 **1.014×**；这组平均值不代表每个算法的结果。

| 算法 | Tapas ms | Python ms | Tapas/Python |
| --- | ---: | ---: | ---: |
| `recursive_fibonacci` | 4.381 | 5.496 | 0.797× |
| `sieve` | 1.051 | 1.141 | 0.921× |
| `merge_sort` | 5.249 | 4.780 | 1.098× |
| `n_queens` | 134.543 | 125.606 | 1.071× |
| `longest_common_subsequence` | 17.631 | 12.988 | 1.357× |
| `matrix_multiply` | 3.338 | 4.993 | 0.669× |
| `sudoku` | 400.733 | 336.405 | 1.191× |
| `dijkstra` | 20.597 | 18.682 | 1.103× |
| `optimize` | 19.947 | 21.928 | 0.910× |
| `kmp` | 24.325 | 21.236 | 1.145× |
| `k_nucleotide` | 185.401 | 179.578 | 1.032× |
| `binary_trees` | 759.576 | 603.060 | 1.260× |
| `lru_cache` | 46.424 | 72.076 | 0.644× |
| `huffman` | 2.256 | 2.001 | 1.127× |
| `fannkuch` | 41.421 | 38.044 | 1.089× |
| `n_body` | 23.604 | 20.840 | 1.133× |

## 运行与维护

```sh
cmake --build build-release -j 4
python3.14 test/benchmarks/diagnose_script.py build-release/bin/tapas \
  --runs 7 --algorithms --ablations --controls --require-slower \
  --json test/benchmarks/hotspots/results.json
```

- 默认运行六组退化热点；`--case dp_row` 等可重复指定来筛选。Python 由启动脚本的 3.14 解释器确定，没有另找更慢版本。
- `--controls` 增加两个可领先的反例。`--require-slower` 仅检查选中热点相对优化 Python 的中位数，不检查反例/综合算法；它用于确认诊断案例是否仍暴露问题，不应作为要求项目持续更慢的 CI 门槛。优化后某个热点超过 Python，属于成功，应调整诊断分类。
- Python 的优化参考使用标准库和惯用实现：enumerate、sum、Counter、元组、slots、list.index。保留相同任务和需要执行的递归/扫描/更新；不做预计算答案、整树缓存、闭式化简、JIT 或第三方数值库替换。这里的“优化参考”不是声称穷举了所有可能的 Python 程序。
- 所有程序输出结果与 CPU 纳秒两行；运行器校验固定期望值、结果稳定、正计时和进程退出状态。DP、搜索、树、浮点更新、字符频数和两个负对照的期望值均用数学关系或独立表示交叉验证。
- 不把不同热点的时间直接相减当作某条指令净成本；循环、调用、分支、引用管理、对象表示会相互影响。基础路径领先仍有诊断价值，但其几何平均不能替代完整算法结果。
- 这次验证包括 Release 构建、原有 Tapas/Python/Lua 全套对比、六组新热点、两个负对照和六组单因素实验。新诊断器专注 Python，不虚构对应 Lua 结果。

优化顺序建议：先以 `dp_row`、`recursive_loops`、`record_update`、`string_count` 作为组合操作的回归指标，再针对元组/节点表示与批量扫描补能力；每次优化都回到综合算法验证。局部变量槽位、循环状态、引用计数和融合分支的确切 CPU 占比仍需原生采样或专门插桩测定。
