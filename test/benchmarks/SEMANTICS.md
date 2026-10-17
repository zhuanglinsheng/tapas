# 跨语言语义与实现约束

本套基准保留指定算法，允许各语言使用惯用容器、标准库批量操作、局部绑定及提前终止。目标不是逐行翻译或相同指令数。所谓“最佳写法”以合理的惯用实现和实际候选对照为依据，不声称已证明所有可能程序中的全局最优；发现更快的等价实现，应继续修订，不能为了维持 Tapas 排名而排除它。

三语言的输入内容、规模、重复次数、可观察结果和计时范围相同。初始化输入放在计时外；算法需要的分配、转置、计数及预处理放在计时内。只使用各语言本身及标准库，不单独为某个实现增加第三方加速器。Python 的切片、Counter、map 等原生能力属于比较内容。Lua 用一基序列；Dictionary 微基准的整数键仍保持原值，不把键 0 改成键 1。

不允许预存答案、利用固定输入周期跳过计算、把完整树替换为共享子树 DAG，或把 KMP/归并排序换成其他搜索/排序算法。微基准必须执行其声明的读取、写入、追加、调用或复制；仅仅保持最终校验和相同不够，例如不能把 dict_lookup 的循环改成最后一次读取，或把逐键删除改为 clear。

所有字符串案例的输入为 ASCII，比较的是这些输入上的等价语义，不据此声称 Unicode 代码点与 UTF-8 字节索引等价。浮点案例保留运算顺序及既有取整规则，不使用 fast-math。重复运行不得复用上次算法结果。

## 综合算法合同

| 案例 | 固定算法及语义 | 允许的惯用写法与限制 |
| --- | --- | --- |
| recursive_fibonacci | 递归计算 F(24) | 保留两次递归调用，不加入缓存或闭式公式 |
| sieve | 埃氏筛统计 ≤20,000 的素数 | Python bytearray 和步长切片标记允许；不换成试除或查表 |
| merge_sort | 4,096 个整数的递归稳定归并排序 | 使用切片、extend、table.move；不能调用 sorted/table.sort 替代归并 |
| n_queens | 逐行回溯计数 10 皇后解 | 冲突时立即退出；保留行、列遍历和回溯，不预存解数 |
| longest_common_subsequence | 两行滚动 DP，重复 40 次 | 字符迭代、局部值和行交换；不改成位并行算法 |
| matrix_multiply | 48×48 常规三重维度乘加并校验矩阵 | 转置右矩阵、sum/map 等允许，转置计入时间；不改成其他乘法算法 |
| sudoku | 固定数独，首个空格优先、数字 1–9 的 DFS，重复 10 次 | index 查空格、短路检查允许；不改用 MRV/约束传播求解器 |
| dijkstra | 相同图及起点的 Dijkstra | 允许线性选择或最小堆；Python 使用 heapq，Lua 使用二叉堆，Tapas 使用原生 heap 包及记录比较函数 |
| optimize | 同一 Rosenbrock 梯度下降与 Armijo 回溯 | 不可变坐标元组允许；保持容差、步长、迭代上限及浮点计算顺序 |
| kmp | 构建前缀表并扫描全部文本，计入重叠匹配起点和 | 不用 str.find 代替 KMP；计时内每轮重新构造前缀表 |
| k_nucleotide | 每轮统计所有长度 1、2 的重叠 k-mer，重复 6 次 | Counter 和默认值读取允许；长度 1 可直接迭代字符；不能仅计算总长度来代替计数 |
| binary_trees | 逐次递归构树及递归校验深度 4–16 的所有负载 | Python 不可变元组允许；不缓存子树或以节点数公式代替递归构造/遍历 |
| lru_cache | 相同容量、伪随机请求、命中更新顺序、逐次淘汰及校验和 | 允许列表、链表、OrderedDict；选择经比较更适合该负载的实现 |
| huffman | 相同频数，稳定符号初序，反复选取两个最小节点建 Huffman 树 | Counter、原地删除、不可变节点与堆均允许；保留并列处理顺序。八符号负载下经比较保留线性选择 |
| fannkuch | 枚举全部 8 元排列，逐次前缀翻转并累计次数及最大值 | 切片是候选，但本机短排列的交换循环更快，保留循环；不能跳过排列 |
| n_body | 同一天体初态与步长，5,000 次成对更新及能量校验 | slots、局部 sqrt 允许；保留成对更新次序及浮点表达式 |

## 热点合同

基础热点保留名称所描述的原语：算术和分支保留依赖链；调用案例保留直接调用/高阶回调；List 案例保留索引、逐次 append、逐次覆盖、完整复制、从范围构造、完整重复构造；Dictionary 案例保留逐次插入、查询、覆盖、逐键删除、小字典构造以及读取后写回。dict_counting 现已修正为显式循环中的“读默认值、频率加一并写回”，以加权完整频数校验。

八项诊断与对照分别为：逐次线性查找首个匹配位置、DP 行更新、递归遍历内的序列求和、递归构树、可变记录字段累加、完整字符频数统计、完整重叠子串频数统计、带提前退出的数独约束探测。Python 的 index/sum/Counter 和不可变树节点允许；字段更新不能把累计浮点加法换成最终乘法。对照的输入、输出和计时合同同样保持一致。

## 检查与候选证据

```sh
python3 test/benchmarks/verify_semantics.py build-release/bin/tapas
python3.14 test/benchmarks/compare_script.py build-pgo/bin/tapas --runs 7 --require-algorithm-target
```

[semantics.json](semantics.json) 固定 43 项合同的校验和；verify_semantics.py 在三种语言上检查全部结果，还用小输入检验排序完整序列、空输入/重复元素、重叠 KMP、LCS、不同 k-mer 长度的具体计数、缺边/不可达图的距离、完整数独解及不同容量的 LRU 等 14 组内核。检查记录及源码哈希见 [semantics_results.json](hotspots/semantics_results.json)。这些检查加上源码审查提高可信度，不是任意输入的形式化等价证明。

写法修订候选保留逐次样本及两轮方向，见 [首轮候选](hotspots/experiments/fairness_initial_candidates.json)、[替代实现](hotspots/experiments/fairness_alternatives.json) 与 [惯用写法候选对照](hotspots/experiments/fairness_candidates.json)。其中 OrderedDict、短排列切片翻转、Tapas 手写双向链表并未因为“看起来更惯用”而自动保留。Lua LRU 的链表、Lua table.move 批量构造及 Python map 点积有重复收益，保留。

先前的 0.894 来自修订前源码，不再作为当前案例达标的证据。旧源码全文及哈希保留于 [快照](hotspots/experiments/pre_fairness_sources.json)，旧报告也在 experiments/ 中归档。当前指标以本目录最新两份 README 为准。

同算法的堆候选另见 [fairness_heap_candidates.json](hotspots/experiments/fairness_heap_candidates.json)。Dijkstra 的 Python 与 Lua 分别相对本轮线性实现快约 81%、45%，采用；Tapas 手写堆在该规模下慢约 3%–5%，该阶段保留线性实现；新增原生 heap 包后重新 A/B，改为原生堆，见 [heap 包实验](hotspots/HEAP.md)。Python Huffman 的稳定堆需要序号及额外元组，对本例八个符号没有收益，Python 保留线性最小值扫描；Tapas 新增原生堆后另行测量完整任务及独立建树内核，选择依据见上述实验。

基础热路径的逐项适用范围、单行 DP 输入修复与任务/内核区分见 [AUDIT.md](hotspots/AUDIT.md)。
