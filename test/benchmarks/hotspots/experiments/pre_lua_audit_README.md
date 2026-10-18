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
| `recursive_fibonacci` | [Tapas](recursive_fibonacci.tap) · [Python](recursive_fibonacci.py) · [Lua](recursive_fibonacci.lua) | 4,179.0 µs | 5,354.0 µs | 2,612.0 µs | 0.781× | 1.600× |
| `sieve` | [Tapas](sieve.tap) · [Python](sieve.py) · [Lua](sieve.lua) | 35.0 µs | 51.0 µs | 616.0 µs | 0.686× | 0.057× |
| `merge_sort` | [Tapas](merge_sort.tap) · [Python](merge_sort.py) · [Lua](merge_sort.lua) | 3,917.0 µs | 3,703.0 µs | 4,562.0 µs | 1.058× | 0.859× |
| `n_queens` | [Tapas](n_queens.tap) · [Python](n_queens.py) · [Lua](n_queens.lua) | 67,033.0 µs | 78,345.0 µs | 47,220.0 µs | 0.856× | 1.420× |
| `longest_common_subsequence` | [Tapas](longest_common_subsequence.tap) · [Python](longest_common_subsequence.py) · [Lua](longest_common_subsequence.lua) | 12,350.0 µs | 9,478.0 µs | 11,196.0 µs | 1.303× | 1.103× |
| `matrix_multiply` | [Tapas](matrix_multiply.tap) · [Python](matrix_multiply.py) · [Lua](matrix_multiply.lua) | 3,009.0 µs | 3,005.0 µs | 1,219.0 µs | 1.001× | 2.468× |
| `sudoku` | [Tapas](sudoku.tap) · [Python](sudoku.py) · [Lua](sudoku.lua) | 255,206.0 µs | 269,455.0 µs | 127,679.0 µs | 0.947× | 1.999× |
| `dijkstra` | [Tapas](dijkstra.tap) · [Python](dijkstra.py) · [Lua](dijkstra.lua) | 3,891.0 µs | 3,345.0 µs | 6,015.0 µs | 1.163× | 0.647× |
| `optimize` | [Tapas](optimize.tap) · [Python](optimize.py) · [Lua](optimize.lua) | 18,055.0 µs | 19,961.0 µs | 13,948.0 µs | 0.905× | 1.294× |
| `kmp` | [Tapas](kmp.tap) · [Python](kmp.py) · [Lua](kmp.lua) | 17,192.0 µs | 18,693.0 µs | 27,904.0 µs | 0.920× | 0.616× |
| `k_nucleotide` | [Tapas](k_nucleotide.tap) · [Python](k_nucleotide.py) · [Lua](k_nucleotide.lua) | 90,377.0 µs | 121,868.0 µs | 69,029.0 µs | 0.742× | 1.309× |
| `binary_trees` | [Tapas](binary_trees.tap) · [Python](binary_trees.py) · [Lua](binary_trees.lua) | 533,941.0 µs | 529,231.0 µs | 648,215.0 µs | 1.009× | 0.824× |
| `lru_cache` | [Tapas](lru_cache.tap) · [Python](lru_cache.py) · [Lua](lru_cache.lua) | 41,817.0 µs | 70,327.0 µs | 34,715.0 µs | 0.595× | 1.205× |
| `huffman` | [Tapas](huffman.tap) · [Python](huffman.py) · [Lua](huffman.lua) | 73.0 µs | 75.0 µs | 1,526.0 µs | 0.973× | 0.048× |
| `fannkuch` | [Tapas](fannkuch.tap) · [Python](fannkuch.py) · [Lua](fannkuch.lua) | 38,812.0 µs | 37,548.0 µs | 27,939.0 µs | 1.034× | 1.389× |
| `n_body` | [Tapas](n_body.tap) · [Python](n_body.py) · [Lua](n_body.lua) | 24,021.0 µs | 20,192.0 µs | 10,656.0 µs | 1.190× | 2.254× |

本组几何平均数：Tapas/Python **0.929×**（16 项）, Tapas/Lua **0.839×**（16 项）。

### 基础热路径

用于定位性能瓶颈：保留被测关键操作，在此约束内采用合理写法。任务型结果提示能力缺口，同机制对照和运行时 A/B 用于归因；例如 `heap_callback` 三语言均执行语言级比较回调，原生搜索与显式 while 的比值不能直接解释为循环成本。诊断合同见 [SEMANTICS.md](SEMANTICS.md#热点合同)。缺少 Lua 实现时标为 —，不计入 Lua 几何平均。

| 项目 | 源码 | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | [Tapas](hotspots/vm_hot_paths.tap) · [Python](hotspots/vm_hot_paths.py) · [Lua](hotspots/vm_hot_paths.lua) | 74,201.0 µs | 137,815.0 µs | 43,784.0 µs | 0.538× | 1.695× |
| `float_arithmetic` | [Tapas](hotspots/float_arithmetic.tap) · [Python](hotspots/float_arithmetic.py) · [Lua](hotspots/float_arithmetic.lua) | 22,188.0 µs | 41,621.0 µs | 10,467.0 µs | 0.533× | 2.120× |
| `function_call_baseline` | [Tapas](hotspots/function_call_baseline.tap) · [Python](hotspots/function_call_baseline.py) · [Lua](hotspots/function_call_baseline.lua) | 2,666.0 µs | 5,999.0 µs | 1,183.0 µs | 0.444× | 2.254× |
| `function_calls` | [Tapas](hotspots/function_calls.tap) · [Python](hotspots/function_calls.py) · [Lua](hotspots/function_calls.lua) | 6,764.0 µs | 8,959.0 µs | 3,572.0 µs | 0.755× | 1.894× |
| `function_callbacks` | [Tapas](hotspots/function_callbacks.tap) · [Python](hotspots/function_callbacks.py) · [Lua](hotspots/function_callbacks.lua) | 12,277.0 µs | 12,450.0 µs | 5,758.0 µs | 0.986× | 2.132× |
| `list_access` | [Tapas](hotspots/list_access.tap) · [Python](hotspots/list_access.py) · [Lua](hotspots/list_access.lua) | 6,108.0 µs | 6,831.0 µs | 1,840.0 µs | 0.894× | 3.320× |
| `list_append` | [Tapas](hotspots/list_append.tap) · [Python](hotspots/list_append.py) · [Lua](hotspots/list_append.lua) | 7,510.0 µs | 9,532.0 µs | 6,484.0 µs | 0.788× | 1.158× |
| `list_update` | [Tapas](hotspots/list_update.tap) · [Python](hotspots/list_update.py) · [Lua](hotspots/list_update.lua) | 4,815.0 µs | 6,090.0 µs | 2,361.0 µs | 0.791× | 2.039× |
| `list_copy` | [Tapas](hotspots/list_copy.tap) · [Python](hotspots/list_copy.py) · [Lua](hotspots/list_copy.lua) | 4,822.0 µs | 21,354.0 µs | 151,352.0 µs | 0.226× | 0.032× |
| `list_from_iterable` | [Tapas](hotspots/list_from_iterable.tap) · [Python](hotspots/list_from_iterable.py) · [Lua](hotspots/list_from_iterable.lua) | 10,059.0 µs | 215,479.0 µs | 178,140.0 µs | 0.047× | 0.056× |
| `list_replicate` | [Tapas](hotspots/list_replicate.tap) · [Python](hotspots/list_replicate.py) · [Lua](hotspots/list_replicate.lua) | 5,560.0 µs | 14,094.0 µs | 161,689.0 µs | 0.394× | 0.034× |
| `list_sort` | [Tapas](hotspots/list_sort.tap) · [Python](hotspots/list_sort.py) · [Lua](hotspots/list_sort.lua) | 3,488.0 µs | 7,912.0 µs | 18,605.0 µs | 0.441× | 0.187× |
| `dict_insert` | [Tapas](hotspots/dict_insert.tap) · [Python](hotspots/dict_insert.py) · [Lua](hotspots/dict_insert.lua) | 7,294.0 µs | 8,057.0 µs | 1,292.0 µs | 0.905× | 5.646× |
| `dict_lookup` | [Tapas](hotspots/dict_lookup.tap) · [Python](hotspots/dict_lookup.py) · [Lua](hotspots/dict_lookup.lua) | 5,783.0 µs | 15,576.0 µs | 1,902.0 µs | 0.371× | 3.040× |
| `dict_update` | [Tapas](hotspots/dict_update.tap) · [Python](hotspots/dict_update.py) · [Lua](hotspots/dict_update.lua) | 4,768.0 µs | 19,637.0 µs | 1,917.0 µs | 0.243× | 2.487× |
| `dict_delete` | [Tapas](hotspots/dict_delete.tap) · [Python](hotspots/dict_delete.py) · [Lua](hotspots/dict_delete.lua) | 3,508.0 µs | 7,921.0 µs | 767.0 µs | 0.443× | 4.574× |
| `dict_literal` | [Tapas](hotspots/dict_literal.tap) · [Python](hotspots/dict_literal.py) · [Lua](hotspots/dict_literal.lua) | 16,341.0 µs | 22,570.0 µs | 28,870.0 µs | 0.724× | 0.566× |
| `dict_counting` | [Tapas](hotspots/dict_counting.tap) · [Python](hotspots/dict_counting.py) · [Lua](hotspots/dict_counting.lua) | 19,646.0 µs | 30,717.0 µs | 5,852.0 µs | 0.640× | 3.357× |
| `tail_recursion` | [Tapas](hotspots/tail_recursion.tap) · [Python](hotspots/tail_recursion.py) · [Lua](hotspots/tail_recursion.lua) | 1,799.0 µs | 4,046.0 µs | 707.0 µs | 0.445× | 2.545× |
| `branch_logic` | [Tapas](hotspots/branch_logic.tap) · [Python](hotspots/branch_logic.py) · [Lua](hotspots/branch_logic.lua) | 21,839.0 µs | 27,292.0 µs | 10,781.0 µs | 0.800× | 2.026× |
| `constraint_probe` | [Tapas](hotspots/constraint_probe.tap) · [Python](hotspots/constraint_probe.py) · [Lua](hotspots/constraint_probe.lua) | 68,613.0 µs | 77,739.0 µs | 33,121.0 µs | 0.883× | 2.072× |
| `dp_row` | [Tapas](hotspots/dp_row.tap) · [Python](hotspots/dp_row.py) · [Lua](hotspots/dp_row.lua) | 34,447.0 µs | 28,330.0 µs | 28,825.0 µs | 1.216× | 1.195× |
| `heap_callback` | [Tapas](hotspots/heap_callback.tap) · [Python](hotspots/heap_callback.py) · [Lua](hotspots/heap_callback.lua) | 8,623.0 µs | 8,344.0 µs | 8,046.0 µs | 1.033× | 1.072× |
| `heap_order` | [Tapas](hotspots/heap_order.tap) · [Python](hotspots/heap_order.py) · [Lua](hotspots/heap_order.lua) | 2,738.0 µs | 4,184.0 µs | 9,020.0 µs | 0.654× | 0.304× |
| `record_update` | [Tapas](hotspots/record_update.tap) · [Python](hotspots/record_update.py) · [Lua](hotspots/record_update.lua) | 10,440.0 µs | 12,493.0 µs | 7,772.0 µs | 0.836× | 1.343× |
| `recursive_loops` | [Tapas](hotspots/recursive_loops.tap) · [Python](hotspots/recursive_loops.py) · [Lua](hotspots/recursive_loops.lua) | 15,299.0 µs | 11,817.0 µs | 8,721.0 µs | 1.295× | 1.754× |
| `string_count` | [Tapas](hotspots/string_count.tap) · [Python](hotspots/string_count.py) · [Lua](hotspots/string_count.lua) | 24,713.0 µs | 13,953.0 µs | 20,959.0 µs | 1.771× | 1.179× |
| `substring_count` | [Tapas](hotspots/substring_count.tap) · [Python](hotspots/substring_count.py) · [Lua](hotspots/substring_count.lua) | 21,377.0 µs | 21,408.0 µs | 16,445.0 µs | 0.999× | 1.300× |
| `tree_build` | [Tapas](hotspots/tree_build.tap) · [Python](hotspots/tree_build.py) · [Lua](hotspots/tree_build.lua) | 21,674.0 µs | 24,311.0 µs | 50,437.0 µs | 0.892× | 0.430× |
| `while_search` | [Tapas](hotspots/while_search.tap) · [Python](hotspots/while_search.py) · [Lua](hotspots/while_search.lua) | 52,342.0 µs | 27,727.0 µs | 27,644.0 µs | 1.888× | 1.893× |

本组几何平均数：Tapas/Python **0.637×**（30 项）, Tapas/Lua **1.091×**（30 项）。

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
堆回调的测量合同、Python 写法选择与解释限制见 [公平性审查](hotspots/HEAP_CALLBACK.md)。
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
