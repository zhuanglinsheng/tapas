# Tapas 与 Python、Lua 性能比较

简体中文 | [English](README_en.md) | [项目主页](../../README.md)

测试日期：2026-09-28

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
| `recursive_fibonacci` | [Tapas](recursive_fibonacci.tap) · [Python](recursive_fibonacci.py) · [Lua](recursive_fibonacci.lua) | 4,500.0 µs | 3,343.0 µs | 1,725.0 µs | 1.346× | 2.609× |
| `sieve` | [Tapas](sieve.tap) · [Python](sieve.py) · [Lua](sieve.lua) | 1,543.0 µs | 832.0 µs | 449.0 µs | 1.855× | 3.437× |
| `merge_sort` | [Tapas](merge_sort.tap) · [Python](merge_sort.py) · [Lua](merge_sort.lua) | 5,616.0 µs | 3,804.0 µs | 5,780.0 µs | 1.476× | 0.972× |
| `n_queens` | [Tapas](n_queens.tap) · [Python](n_queens.py) · [Lua](n_queens.lua) | 153,193.0 µs | 99,967.0 µs | 51,749.0 µs | 1.532× | 2.960× |
| `longest_common_subsequence` | [Tapas](longest_common_subsequence.tap) · [Python](longest_common_subsequence.py) · [Lua](longest_common_subsequence.lua) | 17,449.0 µs | 10,074.0 µs | 15,775.0 µs | 1.732× | 1.106× |
| `matrix_multiply` | [Tapas](matrix_multiply.tap) · [Python](matrix_multiply.py) · [Lua](matrix_multiply.lua) | 5,132.0 µs | 3,655.0 µs | 1,225.0 µs | 1.404× | 4.189× |
| `sudoku` | [Tapas](sudoku.tap) · [Python](sudoku.py) · [Lua](sudoku.lua) | 381,909.0 µs | 252,845.0 µs | 128,815.0 µs | 1.510× | 2.965× |
| `dijkstra` | [Tapas](dijkstra.tap) · [Python](dijkstra.py) · [Lua](dijkstra.lua) | 26,983.0 µs | 13,604.0 µs | 8,023.0 µs | 1.983× | 3.363× |
| `optimize` | [Tapas](optimize.tap) · [Python](optimize.py) · [Lua](optimize.lua) | 16,751.0 µs | 16,121.0 µs | 16,694.0 µs | 1.039× | 1.003× |
| `kmp` | [Tapas](kmp.tap) · [Python](kmp.py) · [Lua](kmp.lua) | 27,158.0 µs | 16,228.0 µs | 41,684.0 µs | 1.674× | 0.652× |
| `k_nucleotide` | [Tapas](k_nucleotide.tap) · [Python](k_nucleotide.py) · [Lua](k_nucleotide.lua) | 146,791.0 µs | 127,826.0 µs | 68,201.0 µs | 1.148× | 2.152× |
| `binary_trees` | [Tapas](binary_trees.tap) · [Python](binary_trees.py) · [Lua](binary_trees.lua) | 776,779.0 µs | 429,244.0 µs | 761,426.0 µs | 1.810× | 1.020× |
| `lru_cache` | [Tapas](lru_cache.tap) · [Python](lru_cache.py) · [Lua](lru_cache.lua) | 36,116.0 µs | 53,686.0 µs | 43,861.0 µs | 0.673× | 0.823× |
| `huffman` | [Tapas](huffman.tap) · [Python](huffman.py) · [Lua](huffman.lua) | 2,093.0 µs | 1,386.0 µs | 1,665.0 µs | 1.510× | 1.257× |
| `fannkuch` | [Tapas](fannkuch.tap) · [Python](fannkuch.py) · [Lua](fannkuch.lua) | 43,211.0 µs | 26,768.0 µs | 25,115.0 µs | 1.614× | 1.721× |
| `n_body` | [Tapas](n_body.tap) · [Python](n_body.py) · [Lua](n_body.lua) | 26,892.0 µs | 16,277.0 µs | 8,464.0 µs | 1.652× | 3.177× |

本组几何平均数：Tapas/Python **1.455×**，Tapas/Lua **1.775×**。

### 基础热路径

这些程序把计时循环放在函数内，并让循环体只保留被测的那一种 VM 操作或容器访问模式，用来定位解释器的基础开销；与目标操作无关的取模寻址、增长整数累加和模块级变量访问均已移除，避免把其他语言的长短处混进单项结果。`vm_hot_paths` 与 `float_arithmetic` 的循环体本身就是被测的混合算术负载；`function_call_baseline` 与 `function_calls` 共享同一循环体，两者相减可估算直接调用净成本，`function_callbacks` 在其上增加一层高阶回调。

| 项目 | 源码 | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | [Tapas](vm_hot_paths.tap) · [Python](vm_hot_paths.py) · [Lua](vm_hot_paths.lua) | 84,224.0 µs | 101,088.0 µs | 35,372.0 µs | 0.833× | 2.381× |
| `float_arithmetic` | [Tapas](float_arithmetic.tap) · [Python](float_arithmetic.py) · [Lua](float_arithmetic.lua) | 25,732.0 µs | 29,649.0 µs | 9,159.0 µs | 0.868× | 2.809× |
| `function_call_baseline` | [Tapas](function_call_baseline.tap) · [Python](function_call_baseline.py) · [Lua](function_call_baseline.lua) | 3,012.0 µs | 4,074.0 µs | 932.0 µs | 0.739× | 3.232× |
| `function_calls` | [Tapas](function_calls.tap) · [Python](function_calls.py) · [Lua](function_calls.lua) | 6,952.0 µs | 6,641.0 µs | 2,881.0 µs | 1.047× | 2.413× |
| `function_callbacks` | [Tapas](function_callbacks.tap) · [Python](function_callbacks.py) · [Lua](function_callbacks.lua) | 12,519.0 µs | 9,351.0 µs | 5,840.0 µs | 1.339× | 2.144× |
| `list_access` | [Tapas](list_access.tap) · [Python](list_access.py) · [Lua](list_access.lua) | 3,464.0 µs | 4,745.0 µs | 1,130.0 µs | 0.730× | 3.065× |
| `list_append` | [Tapas](list_append.tap) · [Python](list_append.py) · [Lua](list_append.lua) | 5,578.0 µs | 6,070.0 µs | 5,355.0 µs | 0.919× | 1.042× |
| `list_update` | [Tapas](list_update.tap) · [Python](list_update.py) · [Lua](list_update.lua) | 4,661.0 µs | 3,659.0 µs | 1,716.0 µs | 1.274× | 2.716× |
| `list_copy` | [Tapas](list_copy.tap) · [Python](list_copy.py) · [Lua](list_copy.lua) | 4,625.0 µs | 18,705.0 µs | 121,427.0 µs | 0.247× | 0.038× |
| `list_from_iterable` | [Tapas](list_from_iterable.tap) · [Python](list_from_iterable.py) · [Lua](list_from_iterable.lua) | 9,887.0 µs | 156,414.0 µs | 187,323.0 µs | 0.063× | 0.053× |
| `list_replicate` | [Tapas](list_replicate.tap) · [Python](list_replicate.py) · [Lua](list_replicate.lua) | 4,602.0 µs | 11,599.0 µs | 320,036.0 µs | 0.397× | 0.014× |
| `dict_insert` | [Tapas](dict_insert.tap) · [Python](dict_insert.py) · [Lua](dict_insert.lua) | 4,692.0 µs | 5,696.0 µs | 1,044.0 µs | 0.824× | 4.494× |
| `dict_lookup` | [Tapas](dict_lookup.tap) · [Python](dict_lookup.py) · [Lua](dict_lookup.lua) | 3,826.0 µs | 9,937.0 µs | 1,158.0 µs | 0.385× | 3.304× |
| `dict_update` | [Tapas](dict_update.tap) · [Python](dict_update.py) · [Lua](dict_update.lua) | 3,970.0 µs | 11,181.0 µs | 1,734.0 µs | 0.355× | 2.290× |
| `dict_delete` | [Tapas](dict_delete.tap) · [Python](dict_delete.py) · [Lua](dict_delete.lua) | 2,709.0 µs | 5,911.0 µs | 530.0 µs | 0.458× | 5.111× |
| `dict_literal` | [Tapas](dict_literal.tap) · [Python](dict_literal.py) · [Lua](dict_literal.lua) | 26,966.0 µs | 27,850.0 µs | 30,910.0 µs | 0.968× | 0.872× |
| `dict_counting` | [Tapas](dict_counting.tap) · [Python](dict_counting.py) · [Lua](dict_counting.lua) | 23,786.0 µs | 26,332.0 µs | 6,274.0 µs | 0.903× | 3.791× |
| `tail_recursion` | [Tapas](tail_recursion.tap) · [Python](tail_recursion.py) · [Lua](tail_recursion.lua) | 1,681.0 µs | 2,925.0 µs | 439.0 µs | 0.575× | 3.829× |
| `branch_logic` | [Tapas](branch_logic.tap) · [Python](branch_logic.py) · [Lua](branch_logic.lua) | 31,192.0 µs | 19,318.0 µs | 7,628.0 µs | 1.615× | 4.089× |

本组几何平均数：Tapas/Python **0.633×**，Tapas/Lua **1.338×**。

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
