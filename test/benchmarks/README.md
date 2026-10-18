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
| `recursive_fibonacci` | [Tapas](recursive_fibonacci.tap) · [Python](recursive_fibonacci.py) · [Lua](recursive_fibonacci.lua) | 4,184.0 µs | 5,362.0 µs | 2,618.0 µs | 0.780× | 1.598× |
| `sieve` | [Tapas](sieve.tap) · [Python](sieve.py) · [Lua](sieve.lua) | 38.0 µs | 45.0 µs | 550.0 µs | 0.844× | 0.069× |
| `merge_sort` | [Tapas](merge_sort.tap) · [Python](merge_sort.py) · [Lua](merge_sort.lua) | 4,012.0 µs | 3,800.0 µs | 1,831.0 µs | 1.056× | 2.191× |
| `n_queens` | [Tapas](n_queens.tap) · [Python](n_queens.py) · [Lua](n_queens.lua) | 67,200.0 µs | 79,618.0 µs | 33,315.0 µs | 0.844× | 2.017× |
| `longest_common_subsequence` | [Tapas](longest_common_subsequence.tap) · [Python](longest_common_subsequence.py) · [Lua](longest_common_subsequence.lua) | 12,478.0 µs | 9,445.0 µs | 5,406.0 µs | 1.321× | 2.308× |
| `matrix_multiply` | [Tapas](matrix_multiply.tap) · [Python](matrix_multiply.py) · [Lua](matrix_multiply.lua) | 2,998.0 µs | 3,081.0 µs | 1,212.0 µs | 0.973× | 2.474× |
| `sudoku` | [Tapas](sudoku.tap) · [Python](sudoku.py) · [Lua](sudoku.lua) | 256,699.0 µs | 272,829.0 µs | 97,308.0 µs | 0.941× | 2.638× |
| `dijkstra` | [Tapas](dijkstra.tap) · [Python](dijkstra.py) · [Lua](dijkstra.lua) | 3,894.0 µs | 3,380.0 µs | 5,125.0 µs | 1.152× | 0.760× |
| `optimize` | [Tapas](optimize.tap) · [Python](optimize.py) · [Lua](optimize.lua) | 18,320.0 µs | 20,303.0 µs | 9,489.0 µs | 0.902× | 1.931× |
| `kmp` | [Tapas](kmp.tap) · [Python](kmp.py) · [Lua](kmp.lua) | 17,304.0 µs | 18,712.0 µs | 15,680.0 µs | 0.925× | 1.104× |
| `k_nucleotide` | [Tapas](k_nucleotide.tap) · [Python](k_nucleotide.py) · [Lua](k_nucleotide.lua) | 90,551.0 µs | 123,312.0 µs | 69,107.0 µs | 0.734× | 1.310× |
| `binary_trees` | [Tapas](binary_trees.tap) · [Python](binary_trees.py) · [Lua](binary_trees.lua) | 540,982.0 µs | 546,828.0 µs | 558,296.0 µs | 0.989× | 0.969× |
| `lru_cache` | [Tapas](lru_cache.tap) · [Python](lru_cache.py) · [Lua](lru_cache.lua) | 43,484.0 µs | 70,803.0 µs | 36,086.0 µs | 0.614× | 1.205× |
| `huffman` | [Tapas](huffman.tap) · [Python](huffman.py) · [Lua](huffman.lua) | 80.0 µs | 82.0 µs | 1,576.0 µs | 0.976× | 0.051× |
| `fannkuch` | [Tapas](fannkuch.tap) · [Python](fannkuch.py) · [Lua](fannkuch.lua) | 39,954.0 µs | 38,693.0 µs | 18,136.0 µs | 1.033× | 2.203× |
| `n_body` | [Tapas](n_body.tap) · [Python](n_body.py) · [Lua](n_body.lua) | 24,717.0 µs | 22,094.0 µs | 10,687.0 µs | 1.119× | 2.313× |

本组几何平均数：Tapas/Python **0.935×**（16 项）, Tapas/Lua **1.102×**（16 项）。

### 基础热路径

用于定位性能瓶颈：保留被测关键操作，在此约束内采用合理写法。任务型结果提示能力缺口，同机制对照和运行时 A/B 用于归因；例如 `heap_callback` 三语言均执行语言级比较回调，原生搜索与显式 while 的比值不能直接解释为循环成本。诊断合同见 [SEMANTICS.md](SEMANTICS.md#热点合同)。缺少 Lua 实现时标为 —，不计入 Lua 几何平均。

| 项目 | 源码 | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | [Tapas](hotspots/vm_hot_paths.tap) · [Python](hotspots/vm_hot_paths.py) · [Lua](hotspots/vm_hot_paths.lua) | 76,644.0 µs | 139,001.0 µs | 43,978.0 µs | 0.551× | 1.743× |
| `float_arithmetic` | [Tapas](hotspots/float_arithmetic.tap) · [Python](hotspots/float_arithmetic.py) · [Lua](hotspots/float_arithmetic.lua) | 22,124.0 µs | 41,795.0 µs | 10,485.0 µs | 0.529× | 2.110× |
| `function_call_baseline` | [Tapas](hotspots/function_call_baseline.tap) · [Python](hotspots/function_call_baseline.py) · [Lua](hotspots/function_call_baseline.lua) | 2,695.0 µs | 5,995.0 µs | 1,183.0 µs | 0.450× | 2.278× |
| `function_calls` | [Tapas](hotspots/function_calls.tap) · [Python](hotspots/function_calls.py) · [Lua](hotspots/function_calls.lua) | 6,728.0 µs | 9,164.0 µs | 3,528.0 µs | 0.734× | 1.907× |
| `function_callbacks` | [Tapas](hotspots/function_callbacks.tap) · [Python](hotspots/function_callbacks.py) · [Lua](hotspots/function_callbacks.lua) | 12,437.0 µs | 12,534.0 µs | 5,740.0 µs | 0.992× | 2.167× |
| `list_access` | [Tapas](hotspots/list_access.tap) · [Python](hotspots/list_access.py) · [Lua](hotspots/list_access.lua) | 6,157.0 µs | 7,085.0 µs | 1,809.0 µs | 0.869× | 3.404× |
| `list_append` | [Tapas](hotspots/list_append.tap) · [Python](hotspots/list_append.py) · [Lua](hotspots/list_append.lua) | 7,495.0 µs | 9,755.0 µs | 6,723.0 µs | 0.768× | 1.115× |
| `list_update` | [Tapas](hotspots/list_update.tap) · [Python](hotspots/list_update.py) · [Lua](hotspots/list_update.lua) | 5,102.0 µs | 8,708.0 µs | 2,599.0 µs | 0.586× | 1.963× |
| `list_copy` | [Tapas](hotspots/list_copy.tap) · [Python](hotspots/list_copy.py) · [Lua](hotspots/list_copy.lua) | 7,748.0 µs | 22,688.0 µs | 158,678.0 µs | 0.342× | 0.049× |
| `list_from_iterable` | [Tapas](hotspots/list_from_iterable.tap) · [Python](hotspots/list_from_iterable.py) · [Lua](hotspots/list_from_iterable.lua) | 10,152.0 µs | 220,112.0 µs | 179,016.0 µs | 0.046× | 0.057× |
| `list_replicate` | [Tapas](hotspots/list_replicate.tap) · [Python](hotspots/list_replicate.py) · [Lua](hotspots/list_replicate.lua) | 5,487.0 µs | 13,885.0 µs | 164,371.0 µs | 0.395× | 0.033× |
| `list_sort` | [Tapas](hotspots/list_sort.tap) · [Python](hotspots/list_sort.py) · [Lua](hotspots/list_sort.lua) | 3,825.0 µs | 8,361.0 µs | 18,803.0 µs | 0.457× | 0.203× |
| `dict_insert` | [Tapas](hotspots/dict_insert.tap) · [Python](hotspots/dict_insert.py) · [Lua](hotspots/dict_insert.lua) | 7,468.0 µs | 8,255.0 µs | 1,312.0 µs | 0.905× | 5.692× |
| `dict_lookup` | [Tapas](hotspots/dict_lookup.tap) · [Python](hotspots/dict_lookup.py) · [Lua](hotspots/dict_lookup.lua) | 5,737.0 µs | 15,349.0 µs | 1,887.0 µs | 0.374× | 3.040× |
| `dict_update` | [Tapas](hotspots/dict_update.tap) · [Python](hotspots/dict_update.py) · [Lua](hotspots/dict_update.lua) | 4,857.0 µs | 20,566.0 µs | 2,024.0 µs | 0.236× | 2.400× |
| `dict_delete` | [Tapas](hotspots/dict_delete.tap) · [Python](hotspots/dict_delete.py) · [Lua](hotspots/dict_delete.lua) | 3,622.0 µs | 8,380.0 µs | 774.0 µs | 0.432× | 4.680× |
| `dict_literal` | [Tapas](hotspots/dict_literal.tap) · [Python](hotspots/dict_literal.py) · [Lua](hotspots/dict_literal.lua) | 16,410.0 µs | 22,667.0 µs | 29,254.0 µs | 0.724× | 0.561× |
| `dict_counting` | [Tapas](hotspots/dict_counting.tap) · [Python](hotspots/dict_counting.py) · [Lua](hotspots/dict_counting.lua) | 19,579.0 µs | 31,392.0 µs | 5,849.0 µs | 0.624× | 3.347× |
| `tail_recursion` | [Tapas](hotspots/tail_recursion.tap) · [Python](hotspots/tail_recursion.py) · [Lua](hotspots/tail_recursion.lua) | 1,816.0 µs | 4,074.0 µs | 687.0 µs | 0.446× | 2.643× |
| `branch_logic` | [Tapas](hotspots/branch_logic.tap) · [Python](hotspots/branch_logic.py) · [Lua](hotspots/branch_logic.lua) | 21,933.0 µs | 27,433.0 µs | 10,736.0 µs | 0.800× | 2.043× |
| `constraint_probe` | [Tapas](hotspots/constraint_probe.tap) · [Python](hotspots/constraint_probe.py) · [Lua](hotspots/constraint_probe.lua) | 68,434.0 µs | 77,499.0 µs | 33,192.0 µs | 0.883× | 2.062× |
| `dp_row` | [Tapas](hotspots/dp_row.tap) · [Python](hotspots/dp_row.py) · [Lua](hotspots/dp_row.lua) | 34,390.0 µs | 28,591.0 µs | 29,510.0 µs | 1.203× | 1.165× |
| `heap_callback` | [Tapas](hotspots/heap_callback.tap) · [Python](hotspots/heap_callback.py) · [Lua](hotspots/heap_callback.lua) | 11,374.0 µs | 8,930.0 µs | 8,459.0 µs | 1.274× | 1.345× |
| `heap_order` | [Tapas](hotspots/heap_order.tap) · [Python](hotspots/heap_order.py) · [Lua](hotspots/heap_order.lua) | 2,882.0 µs | 4,298.0 µs | 9,191.0 µs | 0.671× | 0.314× |
| `record_update` | [Tapas](hotspots/record_update.tap) · [Python](hotspots/record_update.py) · [Lua](hotspots/record_update.lua) | 10,749.0 µs | 12,508.0 µs | 7,328.0 µs | 0.859× | 1.467× |
| `recursive_loops` | [Tapas](hotspots/recursive_loops.tap) · [Python](hotspots/recursive_loops.py) · [Lua](hotspots/recursive_loops.lua) | 15,943.0 µs | 11,859.0 µs | 8,416.0 µs | 1.344× | 1.894× |
| `string_count` | [Tapas](hotspots/string_count.tap) · [Python](hotspots/string_count.py) · [Lua](hotspots/string_count.lua) | 24,422.0 µs | 12,788.0 µs | 20,278.0 µs | 1.910× | 1.204× |
| `substring_count` | [Tapas](hotspots/substring_count.tap) · [Python](hotspots/substring_count.py) · [Lua](hotspots/substring_count.lua) | 18,591.0 µs | 21,822.0 µs | 16,103.0 µs | 0.852× | 1.155× |
| `tree_build` | [Tapas](hotspots/tree_build.tap) · [Python](hotspots/tree_build.py) · [Lua](hotspots/tree_build.lua) | 21,752.0 µs | 24,453.0 µs | 50,962.0 µs | 0.890× | 0.427× |
| `while_search` | [Tapas](hotspots/while_search.tap) · [Python](hotspots/while_search.py) · [Lua](hotspots/while_search.lua) | 52,252.0 µs | 27,820.0 µs | 28,116.0 µs | 1.878× | 1.858× |

本组几何平均数：Tapas/Python **0.642×**（30 项）, Tapas/Lua **1.120×**（30 项）。

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
Lua 的 16 项综合算法复核、候选源码与独立复测见 [Lua 写法审查](hotspots/LUA_AUDIT.md)；当前结果采用经验证保留的实现，不声称全局最优。
保留运行时或基准源码优化后，应使用最终构建同步重测并更新中英文 README、原始样本及校验记录。
性能门槛不属于常规测试，因为 CPU 负载、电源状态、编译器和 Python 版本都会影响结果；发布比较结果时应同时记录运行环境。

## 说明

这些程序用于比较各实现执行语义等价任务时的性能，不代表大型应用的完整性能。
各语言使用语义等价的惯用实现；内建批量操作和数据结构带来的优势属于比较结果的一部分。
比较脚本核对固定负载输出；verify_semantics.py 额外检查小输入与完整结果序列。
结果会受系统负载、电源状态、编译器版本、Python 版本和 Lua 版本影响。
更新 VM 或运行环境后，应使用上文「用法」中的命令重新生成。
