# Tapas 与 Python、Lua 性能比较

简体中文 | [English](README_en.md) | [项目主页](../../README.md)

测试日期：2026-10-18

## 测试环境

- 系统：`macOS-27.0.1-arm64-arm-64bit-Mach-O`
- 处理器架构：`arm64`
- Python：`3.14.2`
- Tapas：`Tapas 0.2.1 Copyright (C) 2020-2026`
- Lua 解释器：`Lua 5.5.1  Copyright (C) 1994-2026 Lua.org, PUC-Rio`
- Tapas 可执行文件：`build-release/bin/tapas`
- 有效运行次数：每项 3 次，另预热 1 次
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
| `recursive_fibonacci` | [Tapas](recursive_fibonacci.tap) · [Python](recursive_fibonacci.py) · [Lua](recursive_fibonacci.lua) | 5,338.0 µs | 5,833.0 µs | 2,726.0 µs | 0.915× | 1.958× |
| `sieve` | [Tapas](sieve.tap) · [Python](sieve.py) · [Lua](sieve.lua) | 42.0 µs | 53.0 µs | 625.0 µs | 0.792× | 0.067× |
| `merge_sort` | [Tapas](merge_sort.tap) · [Python](merge_sort.py) · [Lua](merge_sort.lua) | 4,255.0 µs | 4,048.0 µs | 4,862.0 µs | 1.051× | 0.875× |
| `n_queens` | [Tapas](n_queens.tap) · [Python](n_queens.py) · [Lua](n_queens.lua) | 68,333.0 µs | 79,408.0 µs | 47,984.0 µs | 0.861× | 1.424× |
| `longest_common_subsequence` | [Tapas](longest_common_subsequence.tap) · [Python](longest_common_subsequence.py) · [Lua](longest_common_subsequence.lua) | 12,869.0 µs | 9,506.0 µs | 11,295.0 µs | 1.354× | 1.139× |
| `matrix_multiply` | [Tapas](matrix_multiply.tap) · [Python](matrix_multiply.py) · [Lua](matrix_multiply.lua) | 3,062.0 µs | 3,065.0 µs | 1,215.0 µs | 0.999× | 2.520× |
| `sudoku` | [Tapas](sudoku.tap) · [Python](sudoku.py) · [Lua](sudoku.lua) | 259,206.0 µs | 271,860.0 µs | 126,855.0 µs | 0.953× | 2.043× |
| `dijkstra` | [Tapas](dijkstra.tap) · [Python](dijkstra.py) · [Lua](dijkstra.lua) | 3,900.0 µs | 3,368.0 µs | 6,153.0 µs | 1.158× | 0.634× |
| `optimize` | [Tapas](optimize.tap) · [Python](optimize.py) · [Lua](optimize.lua) | 18,269.0 µs | 20,322.0 µs | 14,046.0 µs | 0.899× | 1.301× |
| `kmp` | [Tapas](kmp.tap) · [Python](kmp.py) · [Lua](kmp.lua) | 17,795.0 µs | 18,418.0 µs | 28,074.0 µs | 0.966× | 0.634× |
| `k_nucleotide` | [Tapas](k_nucleotide.tap) · [Python](k_nucleotide.py) · [Lua](k_nucleotide.lua) | 91,238.0 µs | 120,350.0 µs | 68,238.0 µs | 0.758× | 1.337× |
| `binary_trees` | [Tapas](binary_trees.tap) · [Python](binary_trees.py) · [Lua](binary_trees.lua) | 536,859.0 µs | 527,728.0 µs | 643,764.0 µs | 1.017× | 0.834× |
| `lru_cache` | [Tapas](lru_cache.tap) · [Python](lru_cache.py) · [Lua](lru_cache.lua) | 41,567.0 µs | 70,513.0 µs | 34,895.0 µs | 0.589× | 1.191× |
| `huffman` | [Tapas](huffman.tap) · [Python](huffman.py) · [Lua](huffman.lua) | 73.0 µs | 80.0 µs | 1,511.0 µs | 0.912× | 0.048× |
| `fannkuch` | [Tapas](fannkuch.tap) · [Python](fannkuch.py) · [Lua](fannkuch.lua) | 39,015.0 µs | 38,091.0 µs | 28,456.0 µs | 1.024× | 1.371× |
| `n_body` | [Tapas](n_body.tap) · [Python](n_body.py) · [Lua](n_body.lua) | 23,938.0 µs | 20,554.0 µs | 10,105.0 µs | 1.165× | 2.369× |

本组几何平均数：Tapas/Python **0.947×**（16 项）, Tapas/Lua **0.869×**（16 项）。

### 基础热路径

自动收录 `hotspots/` 中的基础操作、算法组合热点及负对照，使用语义等价的惯用写法。缺少 Lua 实现时标为 —，不计入 Lua 几何平均。

| 项目 | 源码 | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | [Tapas](hotspots/vm_hot_paths.tap) · [Python](hotspots/vm_hot_paths.py) · [Lua](hotspots/vm_hot_paths.lua) | 75,390.0 µs | 139,834.0 µs | 43,843.0 µs | 0.539× | 1.720× |
| `float_arithmetic` | [Tapas](hotspots/float_arithmetic.tap) · [Python](hotspots/float_arithmetic.py) · [Lua](hotspots/float_arithmetic.lua) | 21,914.0 µs | 41,584.0 µs | 10,483.0 µs | 0.527× | 2.090× |
| `function_call_baseline` | [Tapas](hotspots/function_call_baseline.tap) · [Python](hotspots/function_call_baseline.py) · [Lua](hotspots/function_call_baseline.lua) | 2,756.0 µs | 6,001.0 µs | 1,193.0 µs | 0.459× | 2.310× |
| `function_calls` | [Tapas](hotspots/function_calls.tap) · [Python](hotspots/function_calls.py) · [Lua](hotspots/function_calls.lua) | 6,546.0 µs | 8,954.0 µs | 3,532.0 µs | 0.731× | 1.853× |
| `function_callbacks` | [Tapas](hotspots/function_callbacks.tap) · [Python](hotspots/function_callbacks.py) · [Lua](hotspots/function_callbacks.lua) | 12,223.0 µs | 12,589.0 µs | 5,767.0 µs | 0.971× | 2.119× |
| `list_access` | [Tapas](hotspots/list_access.tap) · [Python](hotspots/list_access.py) · [Lua](hotspots/list_access.lua) | 6,187.0 µs | 6,858.0 µs | 1,825.0 µs | 0.902× | 3.390× |
| `list_append` | [Tapas](hotspots/list_append.tap) · [Python](hotspots/list_append.py) · [Lua](hotspots/list_append.lua) | 7,649.0 µs | 9,405.0 µs | 6,456.0 µs | 0.813× | 1.185× |
| `list_update` | [Tapas](hotspots/list_update.tap) · [Python](hotspots/list_update.py) · [Lua](hotspots/list_update.lua) | 4,868.0 µs | 6,040.0 µs | 2,481.0 µs | 0.806× | 1.962× |
| `list_copy` | [Tapas](hotspots/list_copy.tap) · [Python](hotspots/list_copy.py) · [Lua](hotspots/list_copy.lua) | 4,902.0 µs | 21,406.0 µs | 166,027.0 µs | 0.229× | 0.030× |
| `list_from_iterable` | [Tapas](hotspots/list_from_iterable.tap) · [Python](hotspots/list_from_iterable.py) · [Lua](hotspots/list_from_iterable.lua) | 10,852.0 µs | 281,315.0 µs | 179,265.0 µs | 0.039× | 0.061× |
| `list_replicate` | [Tapas](hotspots/list_replicate.tap) · [Python](hotspots/list_replicate.py) · [Lua](hotspots/list_replicate.lua) | 5,284.0 µs | 13,761.0 µs | 175,206.0 µs | 0.384× | 0.030× |
| `list_sort` | [Tapas](hotspots/list_sort.tap) · [Python](hotspots/list_sort.py) · [Lua](hotspots/list_sort.lua) | 3,526.0 µs | 8,335.0 µs | 19,212.0 µs | 0.423× | 0.184× |
| `dict_insert` | [Tapas](hotspots/dict_insert.tap) · [Python](hotspots/dict_insert.py) · [Lua](hotspots/dict_insert.lua) | 7,802.0 µs | 8,612.0 µs | 1,296.0 µs | 0.906× | 6.020× |
| `dict_lookup` | [Tapas](hotspots/dict_lookup.tap) · [Python](hotspots/dict_lookup.py) · [Lua](hotspots/dict_lookup.lua) | 5,991.0 µs | 15,627.0 µs | 1,842.0 µs | 0.383× | 3.252× |
| `dict_update` | [Tapas](hotspots/dict_update.tap) · [Python](hotspots/dict_update.py) · [Lua](hotspots/dict_update.lua) | 4,856.0 µs | 19,800.0 µs | 1,917.0 µs | 0.245× | 2.533× |
| `dict_delete` | [Tapas](hotspots/dict_delete.tap) · [Python](hotspots/dict_delete.py) · [Lua](hotspots/dict_delete.lua) | 3,564.0 µs | 7,944.0 µs | 769.0 µs | 0.449× | 4.635× |
| `dict_literal` | [Tapas](hotspots/dict_literal.tap) · [Python](hotspots/dict_literal.py) · [Lua](hotspots/dict_literal.lua) | 16,623.0 µs | 22,694.0 µs | 29,118.0 µs | 0.732× | 0.571× |
| `dict_counting` | [Tapas](hotspots/dict_counting.tap) · [Python](hotspots/dict_counting.py) · [Lua](hotspots/dict_counting.lua) | 21,313.0 µs | 30,997.0 µs | 5,843.0 µs | 0.688× | 3.648× |
| `tail_recursion` | [Tapas](hotspots/tail_recursion.tap) · [Python](hotspots/tail_recursion.py) · [Lua](hotspots/tail_recursion.lua) | 1,758.0 µs | 4,030.0 µs | 682.0 µs | 0.436× | 2.578× |
| `branch_logic` | [Tapas](hotspots/branch_logic.tap) · [Python](hotspots/branch_logic.py) · [Lua](hotspots/branch_logic.lua) | 23,148.0 µs | 28,155.0 µs | 10,903.0 µs | 0.822× | 2.123× |
| `constraint_probe` | [Tapas](hotspots/constraint_probe.tap) · [Python](hotspots/constraint_probe.py) · [Lua](hotspots/constraint_probe.lua) | 68,637.0 µs | 79,389.0 µs | 33,448.0 µs | 0.865× | 2.052× |
| `dp_row` | [Tapas](hotspots/dp_row.tap) · [Python](hotspots/dp_row.py) · [Lua](hotspots/dp_row.lua) | 34,297.0 µs | 28,316.0 µs | 28,631.0 µs | 1.211× | 1.198× |
| `heap_callback` | [Tapas](hotspots/heap_callback.tap) · [Python](hotspots/heap_callback.py) · [Lua](hotspots/heap_callback.lua) | 8,703.0 µs | 3,946.0 µs | 7,893.0 µs | 2.206× | 1.103× |
| `heap_order` | [Tapas](hotspots/heap_order.tap) · [Python](hotspots/heap_order.py) · [Lua](hotspots/heap_order.lua) | 2,798.0 µs | 4,194.0 µs | 9,089.0 µs | 0.667× | 0.308× |
| `record_update` | [Tapas](hotspots/record_update.tap) · [Python](hotspots/record_update.py) · [Lua](hotspots/record_update.lua) | 10,500.0 µs | 12,520.0 µs | 8,022.0 µs | 0.839× | 1.309× |
| `recursive_loops` | [Tapas](hotspots/recursive_loops.tap) · [Python](hotspots/recursive_loops.py) · [Lua](hotspots/recursive_loops.lua) | 15,147.0 µs | 11,773.0 µs | 8,257.0 µs | 1.287× | 1.834× |
| `string_count` | [Tapas](hotspots/string_count.tap) · [Python](hotspots/string_count.py) · [Lua](hotspots/string_count.lua) | 23,935.0 µs | 13,581.0 µs | 20,073.0 µs | 1.762× | 1.192× |
| `substring_count` | [Tapas](hotspots/substring_count.tap) · [Python](hotspots/substring_count.py) · [Lua](hotspots/substring_count.lua) | 17,583.0 µs | 20,688.0 µs | 15,883.0 µs | 0.850× | 1.107× |
| `tree_build` | [Tapas](hotspots/tree_build.tap) · [Python](hotspots/tree_build.py) · [Lua](hotspots/tree_build.lua) | 21,722.0 µs | 24,715.0 µs | 51,175.0 µs | 0.879× | 0.424× |
| `while_search` | [Tapas](hotspots/while_search.tap) · [Python](hotspots/while_search.py) · [Lua](hotspots/while_search.lua) | 52,897.0 µs | 27,755.0 µs | 27,686.0 µs | 1.906× | 1.911× |

本组几何平均数：Tapas/Python **0.648×**（30 项）, Tapas/Lua **1.093×**（30 项）。

内核、任务、表示与语义案例测量的层面不同，不能把本组均值解释成纯 VM 性能。逐项结论见[热路径审查](hotspots/AUDIT.md)。

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
List 字典序和默认堆的写法 A/B 见 [字典序实验](hotspots/LIST_ORDER.md)。
原生 heap 包与 Dijkstra、Huffman 的写法 A/B 见 [heap 实验](hotspots/HEAP.md)；`source_ab_script.py` 比较同一运行时上的两份等价 Tapas 源码。
通用 fill/count 的 sieve 源码 A/B 与 Python count 写法选择见 [批量操作实验](hotspots/BULK.md)。
sort 与 libc qsort 的原生及运行时对照见 [排序优化](hotspots/SORT.md)。本轮逐项优化见 [紧凑序列与排序](hotspots/COMPACT_AND_SORT.md)。
代码结构整理、扩展能力校验与性能保留决策见 [代码整理验证](hotspots/CODE_CLEANUP.md)。
成员检查收益恢复与排序回退处理见 [性能恢复验证](hotspots/PERFORMANCE_RECOVERY.md)。
LCS 的当前写法、历史外移实验与源码 A/B 见 [LCS 写法优化](hotspots/LCS.md)。
字符比较、局部变量初始化优化及作用域清理实验见 [值路径优化](hotspots/VALUE_PATHS.md)。
通用堆策略与回调边界的独立 A/B 见 [通用堆优化](hotspots/HEAP_OPTIMIZATION.md)。
六项综合算法的剩余差距与语法、原生操作及对象表示的区分见 [剩余差距分析](hotspots/REMAINING_GAPS.md)。
现有语法与标准库下的候选写法及三语言源码 A/B 见 [惯用写法优化](hotspots/IDIOMATIC.md)。
保留运行时或基准源码优化后，应使用最终构建同步重测并更新中英文 README、原始样本及校验记录。
性能门槛不属于常规测试，因为 CPU 负载、电源状态、编译器和 Python 版本都会影响结果；发布比较结果时应同时记录运行环境。

## 说明

这些程序用于比较各实现执行语义等价任务时的性能，不代表大型应用的完整性能。
各语言使用语义等价的惯用实现；内建批量操作和数据结构带来的优势属于比较结果的一部分。
比较脚本核对固定负载输出；verify_semantics.py 额外检查小输入与完整结果序列。
结果会受系统负载、电源状态、编译器版本、Python 版本和 Lua 版本影响。
更新 VM 或运行环境后，应使用上文「用法」中的命令重新生成。
