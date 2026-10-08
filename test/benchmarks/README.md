# Tapas 与 Python、Lua 性能比较

简体中文 | [English](README_en.md) | [项目主页](../../README.md)

测试日期：2026-10-08

## 测试环境

- 系统：`macOS-26.6.2-arm64-arm-64bit-Mach-O`
- 处理器架构：`arm64`
- Python：`3.14.7`
- Tapas：`Tapas 0.1.0 Copyright (C) 2020-2026`
- Lua 解释器：`Lua 5.5.1  Copyright (C) 1994-2026 Lua.org, PUC-Rio`
- Tapas 可执行文件：`build-release/bin/tapas`
- 有效运行次数：每项 3 次，另预热 1 次

## 结果

时间为进程 CPU 时间的微秒中位数，不包含进程启动、源码加载和编译。
比值小于 1 表示 Tapas 更快（分别相对 Python 与 Lua）。

### 综合算法

这些程序组合使用递归、分支、容器、索引和内存分配，更接近完整算法负载。

| 项目 | 源码 | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `recursive_fibonacci` | [Tapas](recursive_fibonacci.tap) · [Python](recursive_fibonacci.py) · [Lua](recursive_fibonacci.lua) | 4,117.0 µs | 5,498.0 µs | 4,358.0 µs | 0.749× | 0.945× |
| `sieve` | [Tapas](sieve.tap) · [Python](sieve.py) · [Lua](sieve.lua) | 990.0 µs | 2,263.0 µs | 1,012.0 µs | 0.437× | 0.978× |
| `merge_sort` | [Tapas](merge_sort.tap) · [Python](merge_sort.py) · [Lua](merge_sort.lua) | 7,212.0 µs | 5,187.0 µs | 8,737.0 µs | 1.390× | 0.825× |
| `n_queens` | [Tapas](n_queens.tap) · [Python](n_queens.py) · [Lua](n_queens.lua) | 175,247.0 µs | 147,560.0 µs | 68,256.0 µs | 1.188× | 2.567× |
| `longest_common_subsequence` | [Tapas](longest_common_subsequence.tap) · [Python](longest_common_subsequence.py) · [Lua](longest_common_subsequence.lua) | 17,204.0 µs | 12,494.0 µs | 24,752.0 µs | 1.377× | 0.695× |
| `matrix_multiply` | [Tapas](matrix_multiply.tap) · [Python](matrix_multiply.py) · [Lua](matrix_multiply.lua) | 6,081.0 µs | 5,246.0 µs | 3,372.0 µs | 1.159× | 1.803× |
| `sudoku` | [Tapas](sudoku.tap) · [Python](sudoku.py) · [Lua](sudoku.lua) | 449,432.0 µs | 387,177.0 µs | 171,619.0 µs | 1.161× | 2.619× |
| `dijkstra` | [Tapas](dijkstra.tap) · [Python](dijkstra.py) · [Lua](dijkstra.lua) | 24,248.0 µs | 20,002.0 µs | 11,943.0 µs | 1.212× | 2.030× |
| `optimize` | [Tapas](optimize.tap) · [Python](optimize.py) · [Lua](optimize.lua) | 25,175.0 µs | 21,993.0 µs | 22,727.0 µs | 1.145× | 1.108× |
| `kmp` | [Tapas](kmp.tap) · [Python](kmp.py) · [Lua](kmp.lua) | 30,440.0 µs | 22,799.0 µs | 59,432.0 µs | 1.335× | 0.512× |
| `k_nucleotide` | [Tapas](k_nucleotide.tap) · [Python](k_nucleotide.py) · [Lua](k_nucleotide.lua) | 206,894.0 µs | 183,256.0 µs | 96,980.0 µs | 1.129× | 2.133× |
| `binary_trees` | [Tapas](binary_trees.tap) · [Python](binary_trees.py) · [Lua](binary_trees.lua) | 852,199.0 µs | 626,822.0 µs | 1,017,804.0 µs | 1.360× | 0.837× |
| `lru_cache` | [Tapas](lru_cache.tap) · [Python](lru_cache.py) · [Lua](lru_cache.lua) | 46,897.0 µs | 68,794.0 µs | 59,117.0 µs | 0.682× | 0.793× |
| `huffman` | [Tapas](huffman.tap) · [Python](huffman.py) · [Lua](huffman.lua) | 3,408.0 µs | 3,188.0 µs | 2,480.0 µs | 1.069× | 1.374× |
| `fannkuch` | [Tapas](fannkuch.tap) · [Python](fannkuch.py) · [Lua](fannkuch.lua) | 47,325.0 µs | 33,590.0 µs | 32,991.0 µs | 1.409× | 1.434× |
| `n_body` | [Tapas](n_body.tap) · [Python](n_body.py) · [Lua](n_body.lua) | 25,982.0 µs | 26,492.0 µs | 10,875.0 µs | 0.981× | 2.389× |

本组几何平均数：Tapas/Python **1.068×**，Tapas/Lua **1.276×**。

### 基础热路径

这些程序把计时循环放在函数内，并让循环体只保留被测的那一种 VM 操作或容器访问模式，用来定位解释器的基础开销；与目标操作无关的取模寻址、增长整数累加和模块级变量访问均已移除，避免把其他语言的长短处混进单项结果。`vm_hot_paths` 与 `float_arithmetic` 的循环体本身就是被测的混合算术负载；`function_call_baseline` 与 `function_calls` 共享同一循环体，两者相减可估算直接调用净成本，`function_callbacks` 在其上增加一层高阶回调。

| 项目 | 源码 | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | [Tapas](vm_hot_paths.tap) · [Python](vm_hot_paths.py) · [Lua](vm_hot_paths.lua) | 93,839.0 µs | 146,973.0 µs | 42,528.0 µs | 0.638× | 2.207× |
| `float_arithmetic` | [Tapas](float_arithmetic.tap) · [Python](float_arithmetic.py) · [Lua](float_arithmetic.lua) | 29,203.0 µs | 41,863.0 µs | 11,213.0 µs | 0.698× | 2.604× |
| `function_call_baseline` | [Tapas](function_call_baseline.tap) · [Python](function_call_baseline.py) · [Lua](function_call_baseline.lua) | 2,948.0 µs | 4,567.0 µs | 2,168.0 µs | 0.646× | 1.360× |
| `function_calls` | [Tapas](function_calls.tap) · [Python](function_calls.py) · [Lua](function_calls.lua) | 10,084.0 µs | 6,783.0 µs | 6,059.0 µs | 1.487× | 1.664× |
| `function_callbacks` | [Tapas](function_callbacks.tap) · [Python](function_callbacks.py) · [Lua](function_callbacks.lua) | 15,721.0 µs | 11,902.0 µs | 8,806.0 µs | 1.321× | 1.785× |
| `list_access` | [Tapas](list_access.tap) · [Python](list_access.py) · [Lua](list_access.lua) | 7,438.0 µs | 7,062.0 µs | 1,212.0 µs | 1.053× | 6.137× |
| `list_append` | [Tapas](list_append.tap) · [Python](list_append.py) · [Lua](list_append.lua) | 9,997.0 µs | 6,621.0 µs | 8,898.0 µs | 1.510× | 1.124× |
| `list_update` | [Tapas](list_update.tap) · [Python](list_update.py) · [Lua](list_update.lua) | 7,398.0 µs | 5,532.0 µs | 4,536.0 µs | 1.337× | 1.631× |
| `list_copy` | [Tapas](list_copy.tap) · [Python](list_copy.py) · [Lua](list_copy.lua) | 7,720.0 µs | 25,250.0 µs | 175,539.0 µs | 0.306× | 0.044× |
| `list_from_iterable` | [Tapas](list_from_iterable.tap) · [Python](list_from_iterable.py) · [Lua](list_from_iterable.lua) | 13,345.0 µs | 212,673.0 µs | 236,438.0 µs | 0.063× | 0.056× |
| `list_replicate` | [Tapas](list_replicate.tap) · [Python](list_replicate.py) · [Lua](list_replicate.lua) | 6,689.0 µs | 12,819.0 µs | 402,398.0 µs | 0.522× | 0.017× |
| `dict_insert` | [Tapas](dict_insert.tap) · [Python](dict_insert.py) · [Lua](dict_insert.lua) | 8,973.0 µs | 6,212.0 µs | 2,516.0 µs | 1.444× | 3.566× |
| `dict_lookup` | [Tapas](dict_lookup.tap) · [Python](dict_lookup.py) · [Lua](dict_lookup.lua) | 5,926.0 µs | 13,734.0 µs | 1,486.0 µs | 0.431× | 3.988× |
| `dict_update` | [Tapas](dict_update.tap) · [Python](dict_update.py) · [Lua](dict_update.lua) | 4,452.0 µs | 17,718.0 µs | 1,794.0 µs | 0.251× | 2.482× |
| `dict_delete` | [Tapas](dict_delete.tap) · [Python](dict_delete.py) · [Lua](dict_delete.lua) | 4,310.0 µs | 6,127.0 µs | 1,361.0 µs | 0.703× | 3.167× |
| `dict_literal` | [Tapas](dict_literal.tap) · [Python](dict_literal.py) · [Lua](dict_literal.lua) | 19,059.0 µs | 19,862.0 µs | 34,133.0 µs | 0.960× | 0.558× |
| `dict_counting` | [Tapas](dict_counting.tap) · [Python](dict_counting.py) · [Lua](dict_counting.lua) | 19,653.0 µs | 31,174.0 µs | 6,468.0 µs | 0.630× | 3.038× |
| `tail_recursion` | [Tapas](tail_recursion.tap) · [Python](tail_recursion.py) · [Lua](tail_recursion.lua) | 1,551.0 µs | 3,222.0 µs | 1,135.0 µs | 0.481× | 1.367× |
| `branch_logic` | [Tapas](branch_logic.tap) · [Python](branch_logic.py) · [Lua](branch_logic.lua) | 23,207.0 µs | 22,780.0 µs | 11,489.0 µs | 1.019× | 2.020× |

本组几何平均数：Tapas/Python **0.662×**，Tapas/Lua **1.095×**。

## 用法

测试应使用 Release 构建，并需要 `PATH` 中的 `lua` 可执行文件（可用 `--lua` 指定）。

```sh
python3 test/benchmarks/compare_script.py build-release/bin/tapas
```

脚本先预热一次，再比较每项 3 次有效运行的中位数（可用 `--runs` 调整），打印摘要并更新本目录的两份 README 文档。
只查看控制台输出、不更新文档时，加 `--no-markdown`；用 `--markdown 路径` 可指定其他报告位置。
重复 `--markdown` 会用同一组测量数据生成多份报告，避免数字因重复运行而不同。
要求每一项都快于 Python 时，使用 `--require-faster`。
性能门槛不属于常规测试，因为 CPU 负载、电源状态、编译器和 Python 版本都会影响结果；发布比较结果时应同时记录运行环境。

## 说明

这些程序用于比较三种实现执行相同算法时的解释器开销，不代表大型应用的完整性能。
各语言使用语义等价的惯用实现；内建批量操作和数据结构带来的优势属于比较结果的一部分。
计时区域都位于函数内部，三种语言保持一致；比较脚本会校验三种实现的输出完全相同。
结果会受系统负载、电源状态、编译器版本、Python 版本和 Lua 版本影响。
更新 VM 或运行环境后，应使用上文「用法」中的命令重新生成。
