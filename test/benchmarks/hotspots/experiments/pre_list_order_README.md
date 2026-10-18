# Tapas 与 Python、Lua 性能比较

简体中文 | [English](../../README_en.md) | [项目主页](../../README.md)

测试日期：2026-10-17

## 测试环境

- 系统：`macOS-27.0.1-arm64-arm-64bit-Mach-O`
- 处理器架构：`arm64`
- Python：`3.14.2`
- Tapas：`Tapas 0.1.0 Copyright (C) 2020-2026`
- Lua 解释器：`Lua 5.5.1  Copyright (C) 1994-2026 Lua.org, PUC-Rio`
- Tapas 可执行文件：`build-release/bin/tapas`
- 有效运行次数：每项 7 次，另预热 1 次
- 构建参数：`CMAKE_BUILD_TYPE:STRING=Release`
- 构建参数：`CMAKE_C_COMPILER:FILEPATH=/usr/bin/cc`
- 构建参数：`CMAKE_C_FLAGS:STRING=`
- 构建参数：`CMAKE_C_FLAGS_RELEASE:STRING=-O3 -DNDEBUG`
- 构建参数：`CMAKE_EXE_LINKER_FLAGS:STRING=`
- 构建参数：`TAPAS_ENABLE_IPO:BOOL=ON`

## 结果

时间为进程 CPU 时间的微秒中位数，不包含进程启动、源码加载和编译。
比值小于 1 表示 Tapas 更快（分别相对 Python 与 Lua）。

### 综合算法

保留指定算法，允许各语言使用惯用容器和标准库批量操作；不要求逐行翻译。输入、重复次数、输出及计时边界保持等价。规则见 SEMANTICS.md。

| 项目 | 源码 | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `recursive_fibonacci` | [Tapas](../../recursive_fibonacci.tap) · [Python](../../recursive_fibonacci.py) · [Lua](../../recursive_fibonacci.lua) | 4,166.0 µs | 5,508.0 µs | 2,636.0 µs | 0.756× | 1.580× |
| `sieve` | [Tapas](../../sieve.tap) · [Python](../../sieve.py) · [Lua](../../sieve.lua) | 92.0 µs | 51.0 µs | 595.0 µs | 1.804× | 0.155× |
| `merge_sort` | [Tapas](../../merge_sort.tap) · [Python](../../merge_sort.py) · [Lua](../../merge_sort.lua) | 3,995.0 µs | 3,808.0 µs | 4,537.0 µs | 1.049× | 0.881× |
| `n_queens` | [Tapas](../../n_queens.tap) · [Python](../../n_queens.py) · [Lua](../../n_queens.lua) | 72,144.0 µs | 79,205.0 µs | 48,719.0 µs | 0.911× | 1.481× |
| `longest_common_subsequence` | [Tapas](../../longest_common_subsequence.tap) · [Python](../../longest_common_subsequence.py) · [Lua](../../longest_common_subsequence.lua) | 13,427.0 µs | 9,633.0 µs | 11,302.0 µs | 1.394× | 1.188× |
| `matrix_multiply` | [Tapas](../../matrix_multiply.tap) · [Python](../../matrix_multiply.py) · [Lua](../../matrix_multiply.lua) | 3,034.0 µs | 3,058.0 µs | 1,195.0 µs | 0.992× | 2.539× |
| `sudoku` | [Tapas](../../sudoku.tap) · [Python](../../sudoku.py) · [Lua](../../sudoku.lua) | 267,970.0 µs | 276,465.0 µs | 135,927.0 µs | 0.969× | 1.971× |
| `dijkstra` | [Tapas](../../dijkstra.tap) · [Python](../../dijkstra.py) · [Lua](../../dijkstra.lua) | 7,222.0 µs | 3,319.0 µs | 5,917.0 µs | 2.176× | 1.221× |
| `optimize` | [Tapas](../../optimize.tap) · [Python](../../optimize.py) · [Lua](../../optimize.lua) | 18,393.0 µs | 19,989.0 µs | 14,068.0 µs | 0.920× | 1.307× |
| `kmp` | [Tapas](../../kmp.tap) · [Python](../../kmp.py) · [Lua](../../kmp.lua) | 17,314.0 µs | 18,896.0 µs | 28,227.0 µs | 0.916× | 0.613× |
| `k_nucleotide` | [Tapas](../../k_nucleotide.tap) · [Python](../../k_nucleotide.py) · [Lua](../../k_nucleotide.lua) | 91,471.0 µs | 123,323.0 µs | 68,723.0 µs | 0.742× | 1.331× |
| `binary_trees` | [Tapas](../../binary_trees.tap) · [Python](../../binary_trees.py) · [Lua](../../binary_trees.lua) | 534,349.0 µs | 534,670.0 µs | 650,140.0 µs | 0.999× | 0.822× |
| `lru_cache` | [Tapas](../../lru_cache.tap) · [Python](../../lru_cache.py) · [Lua](../../lru_cache.lua) | 42,089.0 µs | 70,396.0 µs | 34,835.0 µs | 0.598× | 1.208× |
| `huffman` | [Tapas](../../huffman.tap) · [Python](../../huffman.py) · [Lua](../../huffman.lua) | 73.0 µs | 71.0 µs | 1,543.0 µs | 1.028× | 0.047× |
| `fannkuch` | [Tapas](../../fannkuch.tap) · [Python](../../fannkuch.py) · [Lua](../../fannkuch.lua) | 39,633.0 µs | 38,531.0 µs | 28,092.0 µs | 1.029× | 1.411× |
| `n_body` | [Tapas](../../n_body.tap) · [Python](../../n_body.py) · [Lua](../../n_body.lua) | 24,215.0 µs | 20,074.0 µs | 10,550.0 µs | 1.206× | 2.295× |

本组几何平均数：Tapas/Python **1.038×**（16 项）, Tapas/Lua **0.941×**（16 项）。

### 基础热路径

本表自动收录 `hotspots/` 中全部 Tapas/Python 案例，包括基础 VM 操作、综合算法提取的组合热点及负对照。Python 列运行各文件的默认实现；新增诊断案例默认使用惯用优化写法；八组 Python 结构对照在下表单独列出。缺少 Lua 实现时标为 —，不计入 Lua 几何平均；两种比值分别标注实际样本数。`function_call_baseline` 与 `function_calls` 共享循环体，可估算直接调用净成本；`function_callbacks` 增加高阶回调。

| 项目 | 测量内容 | 源码 | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | 内核：整数循环、取模、分支组合 | [Tapas](../vm_hot_paths.tap) · [Python](../vm_hot_paths.py) · [Lua](../vm_hot_paths.lua) | 75,364.0 µs | 139,323.0 µs | 44,460.0 µs | 0.541× | 1.695× |
| `float_arithmetic` | 内核：有依赖的浮点乘加链 | [Tapas](../float_arithmetic.tap) · [Python](../float_arithmetic.py) · [Lua](../float_arithmetic.lua) | 22,006.0 µs | 41,909.0 µs | 11,686.0 µs | 0.525× | 1.883× |
| `function_call_baseline` | 内核：调用对照：循环、取模、累加 | [Tapas](../function_call_baseline.tap) · [Python](../function_call_baseline.py) · [Lua](../function_call_baseline.lua) | 2,875.0 µs | 6,336.0 µs | 1,188.0 µs | 0.454× | 2.420× |
| `function_calls` | 内核：固定标量函数的直接调用 | [Tapas](../function_calls.tap) · [Python](../function_calls.py) · [Lua](../function_calls.lua) | 6,785.0 µs | 9,154.0 µs | 3,615.0 µs | 0.741× | 1.877× |
| `function_callbacks` | 内核：固定回调经包装函数转发 | [Tapas](../function_callbacks.tap) · [Python](../function_callbacks.py) · [Lua](../function_callbacks.lua) | 12,390.0 µs | 12,693.0 µs | 5,838.0 µs | 0.976× | 2.122× |
| `list_access` | 内核：顺序 List 读取与循环 | [Tapas](../list_access.tap) · [Python](../list_access.py) · [Lua](../list_access.lua) | 6,167.0 µs | 7,262.0 µs | 1,875.0 µs | 0.849× | 3.289× |
| `list_append` | 内核：逐次追加及扩容 | [Tapas](../list_append.tap) · [Python](../list_append.py) · [Lua](../list_append.lua) | 7,740.0 µs | 9,772.0 µs | 6,579.0 µs | 0.792× | 1.176× |
| `list_update` | 内核：已有 List 槽位覆盖 | [Tapas](../list_update.tap) · [Python](../list_update.py) · [Lua](../list_update.lua) | 4,868.0 µs | 6,033.0 µs | 2,373.0 µs | 0.807× | 2.051× |
| `list_copy` | 任务：完整浅复制、分配及回收 | [Tapas](../list_copy.tap) · [Python](../list_copy.py) · [Lua](../list_copy.lua) | 4,825.0 µs | 21,528.0 µs | 152,792.0 µs | 0.224× | 0.032× |
| `list_from_iterable` | 任务：从整数范围构造序列 | [Tapas](../list_from_iterable.tap) · [Python](../list_from_iterable.py) · [Lua](../list_from_iterable.lua) | 10,056.0 µs | 217,219.0 µs | 180,089.0 µs | 0.046× | 0.056× |
| `list_replicate` | 任务：完整序列重复构造 | [Tapas](../list_replicate.tap) · [Python](../list_replicate.py) · [Lua](../list_replicate.lua) | 5,604.0 µs | 14,072.0 µs | 166,533.0 µs | 0.398× | 0.034× |
| `dict_insert` | 内核：顺序整数键插入及扩容 | [Tapas](../dict_insert.tap) · [Python](../dict_insert.py) · [Lua](../dict_insert.lua) | 7,344.0 µs | 8,573.0 µs | 1,267.0 µs | 0.857× | 5.796× |
| `dict_lookup` | 内核：顺序已有整数键查询 | [Tapas](../dict_lookup.tap) · [Python](../dict_lookup.py) · [Lua](../dict_lookup.lua) | 6,052.0 µs | 15,757.0 µs | 1,991.0 µs | 0.384× | 3.040× |
| `dict_update` | 内核：已有整数键的标量覆盖 | [Tapas](../dict_update.tap) · [Python](../dict_update.py) · [Lua](../dict_update.lua) | 4,814.0 µs | 20,179.0 µs | 1,927.0 µs | 0.239× | 2.498× |
| `dict_delete` | 内核：逐键删除已有整数键 | [Tapas](../dict_delete.tap) · [Python](../dict_delete.py) · [Lua](../dict_delete.lua) | 3,557.0 µs | 8,026.0 µs | 783.0 µs | 0.443× | 4.543× |
| `dict_literal` | 内核：小字典构造、读取及回收 | [Tapas](../dict_literal.tap) · [Python](../dict_literal.py) · [Lua](../dict_literal.lua) | 16,860.0 µs | 23,094.0 µs | 29,131.0 µs | 0.730× | 0.579× |
| `dict_counting` | 内核：显式循环：读默认值、加一、写回 | [Tapas](../dict_counting.tap) · [Python](../dict_counting.py) · [Lua](../dict_counting.lua) | 19,796.0 µs | 32,709.0 µs | 5,910.0 µs | 0.605× | 3.350× |
| `tail_recursion` | 语义：尾调用与栈/帧管理 | [Tapas](../tail_recursion.tap) · [Python](../tail_recursion.py) · [Lua](../tail_recursion.lua) | 1,770.0 µs | 3,958.0 µs | 711.0 µs | 0.447× | 2.489× |
| `branch_logic` | 内核：固定状态分布的短路条件链 | [Tapas](../branch_logic.tap) · [Python](../branch_logic.py) · [Lua](../branch_logic.lua) | 21,815.0 µs | 27,365.0 µs | 10,839.0 µs | 0.797× | 2.013× |
| `constraint_probe` | 内核：数独约束索引、比较与提前退出 | [Tapas](../constraint_probe.tap) · [Python](../constraint_probe.py) · [Lua](../constraint_probe.lua) | 71,531.0 µs | 78,645.0 µs | 33,171.0 µs | 0.910× | 2.156× |
| `dp_row` | 内核：有效 LCS 状态上的单行更新；缓存左侧与左上值 | [Tapas](../dp_row.tap) · [Python](../dp_row.py) · [Lua](../dp_row.lua) | 38,981.0 µs | 28,983.0 µs | 29,285.0 µs | 1.345× | 1.331× |
| `heap_callback` | 任务：优先级记录堆反复调用比较函数；含复制、建堆与有序出堆 | [Tapas](../heap_callback.tap) · [Python](../heap_callback.py) · [Lua](../heap_callback.lua) | 8,748.0 µs | 3,996.0 µs | 8,027.0 µs | 2.189× | 1.090× |
| `record_update` | 表示：固定字段访问、累加与对象布局 | [Tapas](../record_update.tap) · [Python](../record_update.py) · [Lua](../record_update.lua) | 10,558.0 µs | 12,555.0 µs | 7,334.0 µs | 0.841× | 1.440× |
| `recursive_loops` | 任务：递归树内的短序列求和 | [Tapas](../recursive_loops.tap) · [Python](../recursive_loops.py) · [Lua](../recursive_loops.lua) | 15,350.0 µs | 12,023.0 µs | 8,421.0 µs | 1.277× | 1.823× |
| `string_count` | 任务：完整字符频数统计 | [Tapas](../string_count.tap) · [Python](../string_count.py) · [Lua](../string_count.lua) | 24,755.0 µs | 13,702.0 µs | 20,366.0 µs | 1.807× | 1.216× |
| `substring_count` | 任务：切片、散列及重叠子串计数 | [Tapas](../substring_count.tap) · [Python](../substring_count.py) · [Lua](../substring_count.lua) | 18,071.0 µs | 21,018.0 µs | 16,566.0 µs | 0.860× | 1.091× |
| `tree_build` | 表示：只读树构造与释放；不可变叶标记及新建内部节点 | [Tapas](../tree_build.tap) · [Python](../tree_build.py) · [Lua](../tree_build.lua) | 21,830.0 µs | 24,972.0 µs | 50,949.0 µs | 0.874× | 0.428× |
| `while_search` | 任务：首个匹配位置的线性搜索 | [Tapas](../while_search.tap) · [Python](../while_search.py) · [Lua](../while_search.lua) | 53,223.0 µs | 27,906.0 µs | 28,099.0 µs | 1.907× | 1.894× |

本组几何平均数：Tapas/Python **0.659×**（28 项）, Tapas/Lua **1.218×**（28 项）。

内核、任务、表示与语义案例测量的层面不同，不能把本组均值解释成纯 VM 性能。逐项结论见[热路径审查](../AUDIT.md)。

### Python 结构对照

主表保留惯用默认写法；本表另测显式循环或常规对象表示。它有助于区分原生批量操作、迭代方式及对象布局的影响，但仍不等于单条 VM 指令成本。两种 Python 写法的输出必须一致；结构对照不计入任何几何平均。

| 案例 | Python 默认 | Python 结构对照 | Tapas/默认 | Tapas/结构对照 |
| --- | ---: | ---: | ---: | ---: |
| `dp_row` | 28,983.0 µs | 29,998.0 µs | 1.345× | 1.299× |
| `heap_callback` | 3,996.0 µs | 15,716.0 µs | 2.189× | 0.557× |
| `record_update` | 12,555.0 µs | 13,525.0 µs | 0.841× | 0.781× |
| `recursive_loops` | 12,023.0 µs | 14,251.0 µs | 1.277× | 1.077× |
| `string_count` | 13,702.0 µs | 23,558.0 µs | 1.807× | 1.051× |
| `substring_count` | 21,018.0 µs | 21,802.0 µs | 0.860× | 0.829× |
| `tree_build` | 24,972.0 µs | 46,034.0 µs | 0.874× | 0.474× |
| `while_search` | 27,906.0 µs | 59,009.0 µs | 1.907× | 0.902× |

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
算法退化热点另见 [诊断分析](../ANALYSIS.md)，包含优化 Python 写法、同结构对照及单因素实验。
运行：`python3.14 test/benchmarks/diagnose_script.py build-release/bin/tapas --algorithms --ablations --controls`。
运行时优化另见 [实施说明](../OPTIMIZATION.md) 与 [A/B 结果](../AB_RESULTS.md)；使用 `ab_script.py BASELINE CANDIDATE --rounds 3 --runs 8` 对相同源码多轮交替测量，传入同一二进制可检查 A/A 噪声。
PGO 的热点训练与构建步骤见实施说明；`profile_script.py` 会记录训练输入、二进制和 profile 的哈希。
原生 heap 包与 Dijkstra、Huffman 的写法 A/B 见 [heap 实验](../HEAP.md)；`source_ab_script.py` 比较同一运行时上的两份等价 Tapas 源码。
通用 fill/count 的 sieve 源码 A/B 与 Python count 写法选择见 [批量操作实验](../BULK.md)。
通用堆策略与回调边界的独立 A/B 见 [通用堆优化](../HEAP_OPTIMIZATION.md)。
六项综合算法的剩余差距与语法、原生操作及对象表示的区分见 [剩余差距分析](../REMAINING_GAPS.md)。
现有语法与标准库下的候选写法及三语言源码 A/B 见 [惯用写法优化](../IDIOMATIC.md)。
保留运行时或基准源码优化后，应使用最终构建同步重测并更新中英文 README、原始样本及校验记录。
性能门槛不属于常规测试，因为 CPU 负载、电源状态、编译器和 Python 版本都会影响结果；发布比较结果时应同时记录运行环境。

## 说明

这些程序用于比较各实现执行语义等价任务时的性能，不代表大型应用的完整性能。
各语言使用语义等价的惯用实现；内建批量操作和数据结构带来的优势属于比较结果的一部分。
比较脚本核对固定负载输出；verify_semantics.py 额外检查小输入与完整结果序列。
结果会受系统负载、电源状态、编译器版本、Python 版本和 Lua 版本影响。
更新 VM 或运行环境后，应使用上文「用法」中的命令重新生成。
