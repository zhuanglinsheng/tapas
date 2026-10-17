# Tapas 与 Python、Lua 性能比较

简体中文 | [English](README_en.md) | [项目主页](../../README.md)

测试日期：2026-10-17

## 测试环境

- 系统：`macOS-27.0.1-arm64-arm-64bit-Mach-O`
- 处理器架构：`arm64`
- Python：`3.14.2`
- Tapas：`Tapas 0.1.0 Copyright (C) 2020-2026`
- Lua 解释器：`Lua 5.5.1  Copyright (C) 1994-2026 Lua.org, PUC-Rio`
- Tapas 可执行文件：`/Users/zhuanglinsheng/Documents/Github/tapas/build-pgo/bin/tapas`
- 有效运行次数：每项 7 次，另预热 1 次
- 构建参数：`CMAKE_BUILD_TYPE:STRING=Release`
- 构建参数：`CMAKE_C_COMPILER:FILEPATH=/usr/bin/cc`
- 构建参数：`CMAKE_C_FLAGS:STRING=-flto=full -fprofile-instr-use=/Users/zhuanglinsheng/Documents/Github/tapas/build-pgo/profile/hotspots.profdata`
- 构建参数：`CMAKE_C_FLAGS_RELEASE:STRING=-O3 -DNDEBUG`
- 构建参数：`CMAKE_EXE_LINKER_FLAGS:STRING=`
- 构建参数：`TAPAS_ENABLE_IPO:BOOL=OFF`

## 结果

时间为进程 CPU 时间的微秒中位数，不包含进程启动、源码加载和编译。
比值小于 1 表示 Tapas 更快（分别相对 Python 与 Lua）。

### 综合算法

保留指定算法，允许各语言使用惯用容器和标准库批量操作；不要求逐行翻译。输入、重复次数、输出及计时边界保持等价。规则见 SEMANTICS.md。

| 项目 | 源码 | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `recursive_fibonacci` | [Tapas](recursive_fibonacci.tap) · [Python](recursive_fibonacci.py) · [Lua](recursive_fibonacci.lua) | 3,580.0 µs | 5,578.0 µs | 2,749.0 µs | 0.642× | 1.302× |
| `sieve` | [Tapas](sieve.tap) · [Python](sieve.py) · [Lua](sieve.lua) | 1,031.0 µs | 115.0 µs | 604.0 µs | 8.965× | 1.707× |
| `merge_sort` | [Tapas](merge_sort.tap) · [Python](merge_sort.py) · [Lua](merge_sort.lua) | 4,533.0 µs | 4,586.0 µs | 5,462.0 µs | 0.988× | 0.830× |
| `n_queens` | [Tapas](n_queens.tap) · [Python](n_queens.py) · [Lua](n_queens.lua) | 68,438.0 µs | 79,744.0 µs | 48,275.0 µs | 0.858× | 1.418× |
| `longest_common_subsequence` | [Tapas](longest_common_subsequence.tap) · [Python](longest_common_subsequence.py) · [Lua](longest_common_subsequence.lua) | 14,152.0 µs | 10,747.0 µs | 12,282.0 µs | 1.317× | 1.152× |
| `matrix_multiply` | [Tapas](matrix_multiply.tap) · [Python](matrix_multiply.py) · [Lua](matrix_multiply.lua) | 2,980.0 µs | 3,136.0 µs | 1,217.0 µs | 0.950× | 2.449× |
| `sudoku` | [Tapas](sudoku.tap) · [Python](sudoku.py) · [Lua](sudoku.lua) | 348,821.0 µs | 297,860.0 µs | 156,866.0 µs | 1.171× | 2.224× |
| `dijkstra` | [Tapas](dijkstra.tap) · [Python](dijkstra.py) · [Lua](dijkstra.lua) | 7,918.0 µs | 3,485.0 µs | 5,998.0 µs | 2.272× | 1.320× |
| `optimize` | [Tapas](optimize.tap) · [Python](optimize.py) · [Lua](optimize.lua) | 17,778.0 µs | 20,179.0 µs | 14,106.0 µs | 0.881× | 1.260× |
| `kmp` | [Tapas](kmp.tap) · [Python](kmp.py) · [Lua](kmp.lua) | 20,167.0 µs | 18,772.0 µs | 28,348.0 µs | 1.074× | 0.711× |
| `k_nucleotide` | [Tapas](k_nucleotide.tap) · [Python](k_nucleotide.py) · [Lua](k_nucleotide.lua) | 104,759.0 µs | 123,390.0 µs | 71,206.0 µs | 0.849× | 1.471× |
| `binary_trees` | [Tapas](binary_trees.tap) · [Python](binary_trees.py) · [Lua](binary_trees.lua) | 697,364.0 µs | 564,594.0 µs | 864,780.0 µs | 1.235× | 0.806× |
| `lru_cache` | [Tapas](lru_cache.tap) · [Python](lru_cache.py) · [Lua](lru_cache.lua) | 41,464.0 µs | 70,524.0 µs | 34,928.0 µs | 0.588× | 1.187× |
| `huffman` | [Tapas](huffman.tap) · [Python](huffman.py) · [Lua](huffman.lua) | 1,978.0 µs | 1,157.0 µs | 1,513.0 µs | 1.710× | 1.307× |
| `fannkuch` | [Tapas](fannkuch.tap) · [Python](fannkuch.py) · [Lua](fannkuch.lua) | 37,724.0 µs | 38,003.0 µs | 28,324.0 µs | 0.993× | 1.332× |
| `n_body` | [Tapas](n_body.tap) · [Python](n_body.py) · [Lua](n_body.lua) | 22,643.0 µs | 20,428.0 µs | 10,512.0 µs | 1.108× | 2.154× |

本组几何平均数：Tapas/Python **1.197×**（16 项）, Tapas/Lua **1.336×**（16 项）。

### 基础热路径

本表自动收录 `hotspots/` 中全部 Tapas/Python 案例，包括基础 VM 操作、综合算法提取的组合热点及负对照。Python 列运行各文件的默认实现；新增诊断案例默认使用惯用优化写法；七组 Python 结构对照在下表单独列出。缺少 Lua 实现时标为 —，不计入 Lua 几何平均；两种比值分别标注实际样本数。`function_call_baseline` 与 `function_calls` 共享循环体，可估算直接调用净成本；`function_callbacks` 增加高阶回调。

| 项目 | 测量内容 | 源码 | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | 内核：整数循环、取模、分支组合 | [Tapas](hotspots/vm_hot_paths.tap) · [Python](hotspots/vm_hot_paths.py) · [Lua](hotspots/vm_hot_paths.lua) | 73,364.0 µs | 139,106.0 µs | 45,347.0 µs | 0.527× | 1.618× |
| `float_arithmetic` | 内核：有依赖的浮点乘加链 | [Tapas](hotspots/float_arithmetic.tap) · [Python](hotspots/float_arithmetic.py) · [Lua](hotspots/float_arithmetic.lua) | 21,781.0 µs | 42,124.0 µs | 10,722.0 µs | 0.517× | 2.031× |
| `function_call_baseline` | 内核：调用对照：循环、取模、累加 | [Tapas](hotspots/function_call_baseline.tap) · [Python](hotspots/function_call_baseline.py) · [Lua](hotspots/function_call_baseline.lua) | 2,732.0 µs | 6,200.0 µs | 1,205.0 µs | 0.441× | 2.267× |
| `function_calls` | 内核：固定标量函数的直接调用 | [Tapas](hotspots/function_calls.tap) · [Python](hotspots/function_calls.py) · [Lua](hotspots/function_calls.lua) | 6,278.0 µs | 9,065.0 µs | 3,567.0 µs | 0.693× | 1.760× |
| `function_callbacks` | 内核：固定回调经包装函数转发 | [Tapas](hotspots/function_callbacks.tap) · [Python](hotspots/function_callbacks.py) · [Lua](hotspots/function_callbacks.lua) | 11,686.0 µs | 12,506.0 µs | 5,807.0 µs | 0.934× | 2.012× |
| `list_access` | 内核：顺序 List 读取与循环 | [Tapas](hotspots/list_access.tap) · [Python](hotspots/list_access.py) · [Lua](hotspots/list_access.lua) | 4,554.0 µs | 6,951.0 µs | 1,873.0 µs | 0.655× | 2.431× |
| `list_append` | 内核：逐次追加及扩容 | [Tapas](hotspots/list_append.tap) · [Python](hotspots/list_append.py) · [Lua](hotspots/list_append.lua) | 7,112.0 µs | 9,804.0 µs | 6,567.0 µs | 0.725× | 1.083× |
| `list_update` | 内核：已有 List 槽位覆盖 | [Tapas](hotspots/list_update.tap) · [Python](hotspots/list_update.py) · [Lua](hotspots/list_update.lua) | 4,892.0 µs | 6,194.0 µs | 2,420.0 µs | 0.790× | 2.021× |
| `list_copy` | 任务：完整浅复制、分配及回收 | [Tapas](hotspots/list_copy.tap) · [Python](hotspots/list_copy.py) · [Lua](hotspots/list_copy.lua) | 4,857.0 µs | 21,500.0 µs | 153,800.0 µs | 0.226× | 0.032× |
| `list_from_iterable` | 任务：从整数范围构造序列 | [Tapas](hotspots/list_from_iterable.tap) · [Python](hotspots/list_from_iterable.py) · [Lua](hotspots/list_from_iterable.lua) | 10,149.0 µs | 214,444.0 µs | 175,904.0 µs | 0.047× | 0.058× |
| `list_replicate` | 任务：完整序列重复构造 | [Tapas](hotspots/list_replicate.tap) · [Python](hotspots/list_replicate.py) · [Lua](hotspots/list_replicate.lua) | 5,488.0 µs | 13,804.0 µs | 164,196.0 µs | 0.398× | 0.033× |
| `dict_insert` | 内核：顺序整数键插入及扩容 | [Tapas](hotspots/dict_insert.tap) · [Python](hotspots/dict_insert.py) · [Lua](hotspots/dict_insert.lua) | 7,225.0 µs | 8,271.0 µs | 1,271.0 µs | 0.874× | 5.685× |
| `dict_lookup` | 内核：顺序已有整数键查询 | [Tapas](hotspots/dict_lookup.tap) · [Python](hotspots/dict_lookup.py) · [Lua](hotspots/dict_lookup.lua) | 5,562.0 µs | 15,544.0 µs | 1,883.0 µs | 0.358× | 2.954× |
| `dict_update` | 内核：已有整数键的标量覆盖 | [Tapas](hotspots/dict_update.tap) · [Python](hotspots/dict_update.py) · [Lua](hotspots/dict_update.lua) | 5,279.0 µs | 19,886.0 µs | 1,930.0 µs | 0.265× | 2.735× |
| `dict_delete` | 内核：逐键删除已有整数键 | [Tapas](hotspots/dict_delete.tap) · [Python](hotspots/dict_delete.py) · [Lua](hotspots/dict_delete.lua) | 3,551.0 µs | 9,552.0 µs | 775.0 µs | 0.372× | 4.582× |
| `dict_literal` | 内核：小字典构造、读取及回收 | [Tapas](hotspots/dict_literal.tap) · [Python](hotspots/dict_literal.py) · [Lua](hotspots/dict_literal.lua) | 16,739.0 µs | 23,091.0 µs | 29,625.0 µs | 0.725× | 0.565× |
| `dict_counting` | 内核：显式循环：读默认值、加一、写回 | [Tapas](hotspots/dict_counting.tap) · [Python](hotspots/dict_counting.py) · [Lua](hotspots/dict_counting.lua) | 19,184.0 µs | 31,671.0 µs | 5,803.0 µs | 0.606× | 3.306× |
| `tail_recursion` | 语义：尾调用与栈/帧管理 | [Tapas](hotspots/tail_recursion.tap) · [Python](hotspots/tail_recursion.py) · [Lua](hotspots/tail_recursion.lua) | 1,536.0 µs | 4,150.0 µs | 715.0 µs | 0.370× | 2.148× |
| `branch_logic` | 内核：固定状态分布的短路条件链 | [Tapas](hotspots/branch_logic.tap) · [Python](hotspots/branch_logic.py) · [Lua](hotspots/branch_logic.lua) | 22,205.0 µs | 27,270.0 µs | 10,675.0 µs | 0.814× | 2.080× |
| `constraint_probe` | 内核：数独约束索引、比较与提前退出 | [Tapas](hotspots/constraint_probe.tap) · [Python](hotspots/constraint_probe.py) · [Lua](hotspots/constraint_probe.lua) | 67,028.0 µs | 78,647.0 µs | 34,112.0 µs | 0.852× | 1.965× |
| `dp_row` | 内核：有效 LCS 状态上的单行更新 | [Tapas](hotspots/dp_row.tap) · [Python](hotspots/dp_row.py) · [Lua](hotspots/dp_row.lua) | 38,480.0 µs | 30,477.0 µs | 31,425.0 µs | 1.263× | 1.225× |
| `record_update` | 表示：固定字段访问、累加与对象布局 | [Tapas](hotspots/record_update.tap) · [Python](hotspots/record_update.py) · [Lua](hotspots/record_update.lua) | 8,751.0 µs | 12,653.0 µs | 7,808.0 µs | 0.692× | 1.121× |
| `recursive_loops` | 任务：递归树内的短序列求和 | [Tapas](hotspots/recursive_loops.tap) · [Python](hotspots/recursive_loops.py) · [Lua](hotspots/recursive_loops.lua) | 13,947.0 µs | 11,856.0 µs | 8,341.0 µs | 1.176× | 1.672× |
| `string_count` | 任务：完整字符频数统计 | [Tapas](hotspots/string_count.tap) · [Python](hotspots/string_count.py) · [Lua](hotspots/string_count.lua) | 23,371.0 µs | 13,925.0 µs | 20,573.0 µs | 1.678× | 1.136× |
| `substring_count` | 任务：切片、散列及重叠子串计数 | [Tapas](hotspots/substring_count.tap) · [Python](hotspots/substring_count.py) · [Lua](hotspots/substring_count.lua) | 18,024.0 µs | 21,036.0 µs | 17,817.0 µs | 0.857× | 1.012× |
| `tree_build` | 表示：递归构造只读树及生命周期 | [Tapas](hotspots/tree_build.tap) · [Python](hotspots/tree_build.py) · [Lua](hotspots/tree_build.lua) | 27,767.0 µs | 24,693.0 µs | 80,137.0 µs | 1.124× | 0.346× |
| `while_search` | 任务：首个匹配位置的线性搜索 | [Tapas](hotspots/while_search.tap) · [Python](hotspots/while_search.py) · [Lua](hotspots/while_search.lua) | 52,651.0 µs | 27,887.0 µs | 28,008.0 µs | 1.888× | 1.880× |

本组几何平均数：Tapas/Python **0.606×**（27 项）, Tapas/Lua **1.157×**（27 项）。

内核、任务、表示与语义案例测量的层面不同，不能把本组均值解释成纯 VM 性能。逐项结论见[热路径审查](hotspots/AUDIT.md)。

### Python 结构对照

主表保留惯用默认写法；本表另测显式循环或常规对象表示。它有助于区分原生批量操作、迭代方式及对象布局的影响，但仍不等于单条 VM 指令成本。两种 Python 写法的输出必须一致；结构对照不计入任何几何平均。

| 案例 | Python 默认 | Python 结构对照 | Tapas/默认 | Tapas/结构对照 |
| --- | ---: | ---: | ---: | ---: |
| `dp_row` | 30,477.0 µs | 32,791.0 µs | 1.263× | 1.173× |
| `record_update` | 12,653.0 µs | 13,413.0 µs | 0.692× | 0.652× |
| `recursive_loops` | 11,856.0 µs | 14,179.0 µs | 1.176× | 0.984× |
| `string_count` | 13,925.0 µs | 23,844.0 µs | 1.678× | 0.980× |
| `substring_count` | 21,036.0 µs | 22,672.0 µs | 0.857× | 0.795× |
| `tree_build` | 24,693.0 µs | 46,552.0 µs | 1.124× | 0.596× |
| `while_search` | 27,887.0 µs | 59,601.0 µs | 1.888× | 0.883× |

## 用法

测试应使用 Release 构建，并需要 `PATH` 中的 `lua` 可执行文件（可用 `--lua` 指定）。

```sh
python3 test/benchmarks/compare_script.py build-release/bin/tapas
```

脚本会重新测量全部综合算法和 `hotspots/` 案例，先预热一次，再比较每项 3 次有效运行的中位数（可用 `--runs` 调整），打印摘要并更新本目录的两份 README 文档。
只查看控制台输出、不更新文档时，加 `--no-markdown`；用 `--markdown 路径` 可指定其他报告位置。
重复 `--markdown` 会用同一组测量数据生成多份报告，避免数字因重复运行而不同。
要求每一项都快于 Python 时，使用 `--require-faster`。
验收综合性能目标时，使用 Python 3.14 运行并加 `--require-algorithm-target`：固定 16 项综合算法的 Tapas/Python 耗时比几何平均必须 ≤ 0.90，热点不计入。

综合算法源码保留在本目录；基础热点与算法退化热点统一放在 `hotspots/`，运行入口仍在本目录。
算法退化热点另见 [诊断分析](hotspots/ANALYSIS.md)，包含优化 Python 写法、同结构对照及单因素实验。
运行：`python3.14 test/benchmarks/diagnose_script.py build-release/bin/tapas --algorithms --ablations --controls`。
运行时优化另见 [实施说明](hotspots/OPTIMIZATION.md) 与 [A/B 结果](hotspots/AB_RESULTS.md)；使用 `ab_script.py BASELINE CANDIDATE --rounds 3 --runs 8` 对相同源码多轮交替测量，传入同一二进制可检查 A/A 噪声。
PGO 的热点训练与构建步骤见实施说明；`profile_script.py` 会记录训练输入、二进制和 profile 的哈希。
原生 heap 包与 Dijkstra、Huffman 的写法 A/B 见 [heap 实验](hotspots/HEAP.md)；`source_ab_script.py` 比较同一运行时上的两份等价 Tapas 源码。
性能门槛不属于常规测试，因为 CPU 负载、电源状态、编译器和 Python 版本都会影响结果；发布比较结果时应同时记录运行环境。

## 说明

这些程序用于比较各实现执行语义等价任务时的性能，不代表大型应用的完整性能。
各语言使用语义等价的惯用实现；内建批量操作和数据结构带来的优势属于比较结果的一部分。
比较脚本核对固定负载输出；verify_semantics.py 额外检查小输入与完整结果序列。
结果会受系统负载、电源状态、编译器版本、Python 版本和 Lua 版本影响。
更新 VM 或运行环境后，应使用上文「用法」中的命令重新生成。
