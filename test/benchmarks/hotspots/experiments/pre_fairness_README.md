> Historical pre-audit results. Source snapshot: pre_fairness_sources.json. Current results are in ../../README.md.

# Tapas 与 Python、Lua 性能比较

简体中文 | [English](../../README_en.md) | [项目主页](../../../../README.md)

测试日期：2026-10-17

## 测试环境

- 系统：`macOS-27.0.1-arm64-arm-64bit-Mach-O`
- 处理器架构：`arm64`
- Python：`3.14.2`
- Tapas：`Tapas 0.1.0 Copyright (C) 2020-2026`
- Lua 解释器：`Lua 5.5.1  Copyright (C) 1994-2026 Lua.org, PUC-Rio`
- Tapas 可执行文件：`build-pgo/bin/tapas`
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

这些程序组合使用递归、分支、容器、索引和内存分配，更接近完整算法负载。

| 项目 | 源码 | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `recursive_fibonacci` | [Tapas](../../recursive_fibonacci.tap) · [Python](../../recursive_fibonacci.py) · [Lua](../../recursive_fibonacci.lua) | 3,478.0 µs | 5,485.0 µs | 2,592.0 µs | 0.634× | 1.342× |
| `sieve` | [Tapas](../../sieve.tap) · [Python](../../sieve.py) · [Lua](../../sieve.lua) | 1,000.0 µs | 1,139.0 µs | 630.0 µs | 0.878× | 1.587× |
| `merge_sort` | [Tapas](../../merge_sort.tap) · [Python](../../merge_sort.py) · [Lua](../../merge_sort.lua) | 4,528.0 µs | 4,817.0 µs | 6,363.0 µs | 0.940× | 0.712× |
| `n_queens` | [Tapas](../../n_queens.tap) · [Python](../../n_queens.py) · [Lua](../../n_queens.lua) | 124,862.0 µs | 124,908.0 µs | 62,315.0 µs | 1.000× | 2.004× |
| `longest_common_subsequence` | [Tapas](../../longest_common_subsequence.tap) · [Python](../../longest_common_subsequence.py) · [Lua](../../longest_common_subsequence.lua) | 13,805.0 µs | 12,755.0 µs | 18,693.0 µs | 1.082× | 0.739× |
| `matrix_multiply` | [Tapas](../../matrix_multiply.tap) · [Python](../../matrix_multiply.py) · [Lua](../../matrix_multiply.lua) | 2,977.0 µs | 4,860.0 µs | 2,322.0 µs | 0.613× | 1.282× |
| `sudoku` | [Tapas](../../sudoku.tap) · [Python](../../sudoku.py) · [Lua](../../sudoku.lua) | 346,773.0 µs | 336,307.0 µs | 162,740.0 µs | 1.031× | 2.131× |
| `dijkstra` | [Tapas](../../dijkstra.tap) · [Python](../../dijkstra.py) · [Lua](../../dijkstra.lua) | 19,347.0 µs | 18,911.0 µs | 10,675.0 µs | 1.023× | 1.812× |
| `optimize` | [Tapas](../../optimize.tap) · [Python](../../optimize.py) · [Lua](../../optimize.lua) | 17,643.0 µs | 21,772.0 µs | 19,044.0 µs | 0.810× | 0.926× |
| `kmp` | [Tapas](../../kmp.tap) · [Python](../../kmp.py) · [Lua](../../kmp.lua) | 19,967.0 µs | 20,618.0 µs | 50,943.0 µs | 0.968× | 0.392× |
| `k_nucleotide` | [Tapas](../../k_nucleotide.tap) · [Python](../../k_nucleotide.py) · [Lua](../../k_nucleotide.lua) | 143,772.0 µs | 180,751.0 µs | 86,165.0 µs | 0.795× | 1.669× |
| `binary_trees` | [Tapas](../../binary_trees.tap) · [Python](../../binary_trees.py) · [Lua](../../binary_trees.lua) | 690,711.0 µs | 600,713.0 µs | 864,314.0 µs | 1.150× | 0.799× |
| `lru_cache` | [Tapas](../../lru_cache.tap) · [Python](../../lru_cache.py) · [Lua](../../lru_cache.lua) | 41,643.0 µs | 70,635.0 µs | 55,454.0 µs | 0.590× | 0.751× |
| `huffman` | [Tapas](../../huffman.tap) · [Python](../../huffman.py) · [Lua](../../huffman.lua) | 1,972.0 µs | 1,991.0 µs | 1,980.0 µs | 0.990× | 0.996× |
| `fannkuch` | [Tapas](../../fannkuch.tap) · [Python](../../fannkuch.py) · [Lua](../../fannkuch.lua) | 37,767.0 µs | 37,591.0 µs | 27,975.0 µs | 1.005× | 1.350× |
| `n_body` | [Tapas](../../n_body.tap) · [Python](../../n_body.py) · [Lua](../../n_body.lua) | 22,712.0 µs | 21,076.0 µs | 10,204.0 µs | 1.078× | 2.226× |

本组几何平均数：Tapas/Python **0.894×**（16 项）, Tapas/Lua **1.169×**（16 项）。

综合性能目标：Tapas/Python 耗时比几何平均 ≤ 0.90（固定 16 项，等权）；**已达到**。当前耗时还需降低 0.0% 才达到目标。

### 基础热路径

本表自动收录 `hotspots/` 中全部 Tapas/Python 案例，包括基础 VM 操作、综合算法提取的组合热点及负对照。Python 列运行各文件的默认实现；新增诊断案例默认使用惯用优化写法，同结构 Python 对照见诊断分析及 diagnose_script.py。缺少 Lua 实现时标为 —，不计入 Lua 几何平均；两种比值分别标注实际样本数。`function_call_baseline` 与 `function_calls` 共享循环体，可估算直接调用净成本；`function_callbacks` 增加高阶回调。

| 项目 | 源码 | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | [Tapas](../../hotspots/vm_hot_paths.tap) · [Python](../../hotspots/vm_hot_paths.py) · [Lua](../../hotspots/vm_hot_paths.lua) | 73,567.0 µs | 138,920.0 µs | 43,545.0 µs | 0.530× | 1.689× |
| `float_arithmetic` | [Tapas](../../hotspots/float_arithmetic.tap) · [Python](../../hotspots/float_arithmetic.py) · [Lua](../../hotspots/float_arithmetic.lua) | 21,149.0 µs | 41,987.0 µs | 10,403.0 µs | 0.504× | 2.033× |
| `function_call_baseline` | [Tapas](../../hotspots/function_call_baseline.tap) · [Python](../../hotspots/function_call_baseline.py) · [Lua](../../hotspots/function_call_baseline.lua) | 2,677.0 µs | 6,152.0 µs | 1,176.0 µs | 0.435× | 2.276× |
| `function_calls` | [Tapas](../../hotspots/function_calls.tap) · [Python](../../hotspots/function_calls.py) · [Lua](../../hotspots/function_calls.lua) | 6,268.0 µs | 9,083.0 µs | 3,599.0 µs | 0.690× | 1.742× |
| `function_callbacks` | [Tapas](../../hotspots/function_callbacks.tap) · [Python](../../hotspots/function_callbacks.py) · [Lua](../../hotspots/function_callbacks.lua) | 11,651.0 µs | 12,525.0 µs | 5,803.0 µs | 0.930× | 2.008× |
| `list_access` | [Tapas](../../hotspots/list_access.tap) · [Python](../../hotspots/list_access.py) · [Lua](../../hotspots/list_access.lua) | 4,479.0 µs | 6,950.0 µs | 1,848.0 µs | 0.644× | 2.424× |
| `list_append` | [Tapas](../../hotspots/list_append.tap) · [Python](../../hotspots/list_append.py) · [Lua](../../hotspots/list_append.lua) | 6,938.0 µs | 9,586.0 µs | 6,548.0 µs | 0.724× | 1.060× |
| `list_update` | [Tapas](../../hotspots/list_update.tap) · [Python](../../hotspots/list_update.py) · [Lua](../../hotspots/list_update.lua) | 4,791.0 µs | 6,181.0 µs | 2,380.0 µs | 0.775× | 2.013× |
| `list_copy` | [Tapas](../../hotspots/list_copy.tap) · [Python](../../hotspots/list_copy.py) · [Lua](../../hotspots/list_copy.lua) | 4,760.0 µs | 21,456.0 µs | 152,574.0 µs | 0.222× | 0.031× |
| `list_from_iterable` | [Tapas](../../hotspots/list_from_iterable.tap) · [Python](../../hotspots/list_from_iterable.py) · [Lua](../../hotspots/list_from_iterable.lua) | 10,003.0 µs | 217,411.0 µs | 222,777.0 µs | 0.046× | 0.045× |
| `list_replicate` | [Tapas](../../hotspots/list_replicate.tap) · [Python](../../hotspots/list_replicate.py) · [Lua](../../hotspots/list_replicate.lua) | 5,522.0 µs | 13,784.0 µs | 380,133.0 µs | 0.401× | 0.015× |
| `dict_insert` | [Tapas](../../hotspots/dict_insert.tap) · [Python](../../hotspots/dict_insert.py) · [Lua](../../hotspots/dict_insert.lua) | 7,249.0 µs | 8,105.0 µs | 1,418.0 µs | 0.894× | 5.112× |
| `dict_lookup` | [Tapas](../../hotspots/dict_lookup.tap) · [Python](../../hotspots/dict_lookup.py) · [Lua](../../hotspots/dict_lookup.lua) | 5,434.0 µs | 15,472.0 µs | 1,833.0 µs | 0.351× | 2.965× |
| `dict_update` | [Tapas](../../hotspots/dict_update.tap) · [Python](../../hotspots/dict_update.py) · [Lua](../../hotspots/dict_update.lua) | 5,246.0 µs | 19,721.0 µs | 2,358.0 µs | 0.266× | 2.225× |
| `dict_delete` | [Tapas](../../hotspots/dict_delete.tap) · [Python](../../hotspots/dict_delete.py) · [Lua](../../hotspots/dict_delete.lua) | 3,474.0 µs | 8,014.0 µs | 757.0 µs | 0.433× | 4.589× |
| `dict_literal` | [Tapas](../../hotspots/dict_literal.tap) · [Python](../../hotspots/dict_literal.py) · [Lua](../../hotspots/dict_literal.lua) | 16,450.0 µs | 22,667.0 µs | 29,253.0 µs | 0.726× | 0.562× |
| `dict_counting` | [Tapas](../../hotspots/dict_counting.tap) · [Python](../../hotspots/dict_counting.py) · [Lua](../../hotspots/dict_counting.lua) | 20,387.0 µs | 36,749.0 µs | 7,088.0 µs | 0.555× | 2.876× |
| `tail_recursion` | [Tapas](../../hotspots/tail_recursion.tap) · [Python](../../hotspots/tail_recursion.py) · [Lua](../../hotspots/tail_recursion.lua) | 1,538.0 µs | 4,056.0 µs | 700.0 µs | 0.379× | 2.197× |
| `branch_logic` | [Tapas](../../hotspots/branch_logic.tap) · [Python](../../hotspots/branch_logic.py) · [Lua](../../hotspots/branch_logic.lua) | 22,020.0 µs | 27,175.0 µs | 10,844.0 µs | 0.810× | 2.031× |
| `constraint_probe` | [Tapas](../../hotspots/constraint_probe.tap) · [Python](../../hotspots/constraint_probe.py) · [Lua](../../hotspots/constraint_probe.lua) | 66,412.0 µs | 78,531.0 µs | 33,356.0 µs | 0.846× | 1.991× |
| `dp_row` | [Tapas](../../hotspots/dp_row.tap) · [Python](../../hotspots/dp_row.py) · [Lua](../../hotspots/dp_row.lua) | 38,411.0 µs | 31,757.0 µs | 31,473.0 µs | 1.210× | 1.220× |
| `record_update` | [Tapas](../../hotspots/record_update.tap) · [Python](../../hotspots/record_update.py) · [Lua](../../hotspots/record_update.lua) | 8,731.0 µs | 12,538.0 µs | 7,354.0 µs | 0.696× | 1.187× |
| `recursive_loops` | [Tapas](../../hotspots/recursive_loops.tap) · [Python](../../hotspots/recursive_loops.py) · [Lua](../../hotspots/recursive_loops.lua) | 13,798.0 µs | 11,775.0 µs | 8,372.0 µs | 1.172× | 1.648× |
| `string_count` | [Tapas](../../hotspots/string_count.tap) · [Python](../../hotspots/string_count.py) · [Lua](../../hotspots/string_count.lua) | 23,444.0 µs | 13,574.0 µs | 20,352.0 µs | 1.727× | 1.152× |
| `substring_count` | [Tapas](../../hotspots/substring_count.tap) · [Python](../../hotspots/substring_count.py) · [Lua](../../hotspots/substring_count.lua) | 17,578.0 µs | 21,098.0 µs | 16,062.0 µs | 0.833× | 1.094× |
| `tree_build` | [Tapas](../../hotspots/tree_build.tap) · [Python](../../hotspots/tree_build.py) · [Lua](../../hotspots/tree_build.lua) | 27,392.0 µs | 24,554.0 µs | 69,460.0 µs | 1.116× | 0.394× |
| `while_search` | [Tapas](../../hotspots/while_search.tap) · [Python](../../hotspots/while_search.py) · [Lua](../../hotspots/while_search.lua) | 47,594.0 µs | 27,716.0 µs | 28,035.0 µs | 1.717× | 1.698× |

本组几何平均数：Tapas/Python **0.602×**（27 项）, Tapas/Lua **1.099×**（27 项）。

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
算法退化热点另见 [诊断分析](../../hotspots/ANALYSIS.md)，包含优化 Python 写法、同结构对照及单因素实验。
运行：`python3.14 test/benchmarks/diagnose_script.py build-release/bin/tapas --algorithms --ablations --controls`。
运行时优化另见 [实施说明](../../hotspots/OPTIMIZATION.md) 与 [A/B 结果](../../hotspots/AB_RESULTS.md)；使用 `ab_script.py BASELINE CANDIDATE --rounds 3 --runs 8` 对相同源码多轮交替测量，传入同一二进制可检查 A/A 噪声。
PGO 的热点训练与构建步骤见实施说明；`profile_script.py` 会记录训练输入、二进制和 profile 的哈希。
性能门槛不属于常规测试，因为 CPU 负载、电源状态、编译器和 Python 版本都会影响结果；发布比较结果时应同时记录运行环境。

## 说明

这些程序用于比较各实现执行语义等价任务时的性能，不代表大型应用的完整性能。
各语言使用语义等价的惯用实现；内建批量操作和数据结构带来的优势属于比较结果的一部分。
计时区域都位于函数内部，各语言保持一致；比较脚本会校验所有可用实现的输出完全相同。
结果会受系统负载、电源状态、编译器版本、Python 版本和 Lua 版本影响。
更新 VM 或运行环境后，应使用上文「用法」中的命令重新生成。
