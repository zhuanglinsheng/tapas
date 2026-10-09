# Tapas 与 Python、Lua 性能比较

简体中文 | [English](README_en.md) | [项目主页](../../README.md)

测试日期：2026-10-09

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
| `recursive_fibonacci` | [Tapas](recursive_fibonacci.tap) · [Python](recursive_fibonacci.py) · [Lua](recursive_fibonacci.lua) | 3,151.0 µs | 3,193.0 µs | 1,575.0 µs | 0.987× | 2.001× |
| `sieve` | [Tapas](sieve.tap) · [Python](sieve.py) · [Lua](sieve.lua) | 810.0 µs | 746.0 µs | 414.0 µs | 1.086× | 1.957× |
| `merge_sort` | [Tapas](merge_sort.tap) · [Python](merge_sort.py) · [Lua](merge_sort.lua) | 4,022.0 µs | 3,561.0 µs | 5,138.0 µs | 1.129× | 0.783× |
| `n_queens` | [Tapas](n_queens.tap) · [Python](n_queens.py) · [Lua](n_queens.lua) | 119,539.0 µs | 98,467.0 µs | 49,790.0 µs | 1.214× | 2.401× |
| `longest_common_subsequence` | [Tapas](longest_common_subsequence.tap) · [Python](longest_common_subsequence.py) · [Lua](longest_common_subsequence.lua) | 12,049.0 µs | 10,188.0 µs | 15,953.0 µs | 1.183× | 0.755× |
| `matrix_multiply` | [Tapas](matrix_multiply.tap) · [Python](matrix_multiply.py) · [Lua](matrix_multiply.lua) | 2,576.0 µs | 3,610.0 µs | 1,196.0 µs | 0.714× | 2.154× |
| `sudoku` | [Tapas](sudoku.tap) · [Python](sudoku.py) · [Lua](sudoku.lua) | 339,426.0 µs | 254,848.0 µs | 125,152.0 µs | 1.332× | 2.712× |
| `dijkstra` | [Tapas](dijkstra.tap) · [Python](dijkstra.py) · [Lua](dijkstra.lua) | 15,751.0 µs | 13,550.0 µs | 7,872.0 µs | 1.162× | 2.001× |
| `optimize` | [Tapas](optimize.tap) · [Python](optimize.py) · [Lua](optimize.lua) | 16,243.0 µs | 16,087.0 µs | 16,267.0 µs | 1.010× | 0.999× |
| `kmp` | [Tapas](kmp.tap) · [Python](kmp.py) · [Lua](kmp.lua) | 19,545.0 µs | 16,420.0 µs | 41,124.0 µs | 1.190× | 0.475× |
| `k_nucleotide` | [Tapas](k_nucleotide.tap) · [Python](k_nucleotide.py) · [Lua](k_nucleotide.lua) | 148,404.0 µs | 123,846.0 µs | 71,264.0 µs | 1.198× | 2.082× |
| `binary_trees` | [Tapas](binary_trees.tap) · [Python](binary_trees.py) · [Lua](binary_trees.lua) | 558,445.0 µs | 431,646.0 µs | 754,560.0 µs | 1.294× | 0.740× |
| `lru_cache` | [Tapas](lru_cache.tap) · [Python](lru_cache.py) · [Lua](lru_cache.lua) | 36,987.0 µs | 53,117.0 µs | 44,197.0 µs | 0.696× | 0.837× |
| `huffman` | [Tapas](huffman.tap) · [Python](huffman.py) · [Lua](huffman.lua) | 1,771.0 µs | 1,350.0 µs | 1,667.0 µs | 1.312× | 1.062× |
| `fannkuch` | [Tapas](fannkuch.tap) · [Python](fannkuch.py) · [Lua](fannkuch.lua) | 34,527.0 µs | 26,448.0 µs | 24,468.0 µs | 1.305× | 1.411× |
| `n_body` | [Tapas](n_body.tap) · [Python](n_body.py) · [Lua](n_body.lua) | 18,773.0 µs | 16,361.0 µs | 9,190.0 µs | 1.147× | 2.043× |

本组几何平均数：Tapas/Python **1.104×**，Tapas/Lua **1.350×**。

### 基础热路径

这些程序把计时循环放在函数内，并让循环体只保留被测的那一种 VM 操作或容器访问模式，用来定位解释器的基础开销；与目标操作无关的取模寻址、增长整数累加和模块级变量访问均已移除，避免把其他语言的长短处混进单项结果。`vm_hot_paths` 与 `float_arithmetic` 的循环体本身就是被测的混合算术负载；`function_call_baseline` 与 `function_calls` 共享同一循环体，两者相减可估算直接调用净成本，`function_callbacks` 在其上增加一层高阶回调。

| 项目 | 源码 | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | [Tapas](vm_hot_paths.tap) · [Python](vm_hot_paths.py) · [Lua](vm_hot_paths.lua) | 69,094.0 µs | 99,681.0 µs | 35,105.0 µs | 0.693× | 1.968× |
| `float_arithmetic` | [Tapas](float_arithmetic.tap) · [Python](float_arithmetic.py) · [Lua](float_arithmetic.lua) | 19,931.0 µs | 29,526.0 µs | 7,628.0 µs | 0.675× | 2.613× |
| `function_call_baseline` | [Tapas](function_call_baseline.tap) · [Python](function_call_baseline.py) · [Lua](function_call_baseline.lua) | 2,627.0 µs | 4,243.0 µs | 941.0 µs | 0.619× | 2.792× |
| `function_calls` | [Tapas](function_calls.tap) · [Python](function_calls.py) · [Lua](function_calls.lua) | 6,269.0 µs | 6,074.0 µs | 2,808.0 µs | 1.032× | 2.233× |
| `function_callbacks` | [Tapas](function_callbacks.tap) · [Python](function_callbacks.py) · [Lua](function_callbacks.lua) | 11,419.0 µs | 9,368.0 µs | 6,089.0 µs | 1.219× | 1.875× |
| `list_access` | [Tapas](list_access.tap) · [Python](list_access.py) · [Lua](list_access.lua) | 3,585.0 µs | 4,777.0 µs | 1,134.0 µs | 0.750× | 3.161× |
| `list_append` | [Tapas](list_append.tap) · [Python](list_append.py) · [Lua](list_append.lua) | 5,974.0 µs | 6,056.0 µs | 5,297.0 µs | 0.986× | 1.128× |
| `list_update` | [Tapas](list_update.tap) · [Python](list_update.py) · [Lua](list_update.lua) | 3,275.0 µs | 3,852.0 µs | 1,715.0 µs | 0.850× | 1.910× |
| `list_copy` | [Tapas](list_copy.tap) · [Python](list_copy.py) · [Lua](list_copy.lua) | 4,474.0 µs | 18,822.0 µs | 120,442.0 µs | 0.238× | 0.037× |
| `list_from_iterable` | [Tapas](list_from_iterable.tap) · [Python](list_from_iterable.py) · [Lua](list_from_iterable.lua) | 9,881.0 µs | 170,049.0 µs | 186,695.0 µs | 0.058× | 0.053× |
| `list_replicate` | [Tapas](list_replicate.tap) · [Python](list_replicate.py) · [Lua](list_replicate.lua) | 4,586.0 µs | 11,390.0 µs | 319,469.0 µs | 0.403× | 0.014× |
| `dict_insert` | [Tapas](dict_insert.tap) · [Python](dict_insert.py) · [Lua](dict_insert.lua) | 5,136.0 µs | 5,581.0 µs | 1,024.0 µs | 0.920× | 5.016× |
| `dict_lookup` | [Tapas](dict_lookup.tap) · [Python](dict_lookup.py) · [Lua](dict_lookup.lua) | 3,814.0 µs | 9,702.0 µs | 1,157.0 µs | 0.393× | 3.296× |
| `dict_update` | [Tapas](dict_update.tap) · [Python](dict_update.py) · [Lua](dict_update.lua) | 4,175.0 µs | 11,188.0 µs | 1,723.0 µs | 0.373× | 2.423× |
| `dict_delete` | [Tapas](dict_delete.tap) · [Python](dict_delete.py) · [Lua](dict_delete.lua) | 2,969.0 µs | 5,964.0 µs | 531.0 µs | 0.498× | 5.591× |
| `dict_literal` | [Tapas](dict_literal.tap) · [Python](dict_literal.py) · [Lua](dict_literal.lua) | 15,206.0 µs | 16,859.0 µs | 26,884.0 µs | 0.902× | 0.566× |
| `dict_counting` | [Tapas](dict_counting.tap) · [Python](dict_counting.py) · [Lua](dict_counting.lua) | 15,915.0 µs | 24,998.0 µs | 5,845.0 µs | 0.637× | 2.723× |
| `tail_recursion` | [Tapas](tail_recursion.tap) · [Python](tail_recursion.py) · [Lua](tail_recursion.lua) | 1,365.0 µs | 2,924.0 µs | 428.0 µs | 0.467× | 3.189× |
| `branch_logic` | [Tapas](branch_logic.tap) · [Python](branch_logic.py) · [Lua](branch_logic.lua) | 18,606.0 µs | 19,060.0 µs | 7,028.0 µs | 0.976× | 2.647× |

本组几何平均数：Tapas/Python **0.569×**，Tapas/Lua **1.203×**。

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
