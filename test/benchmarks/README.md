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
| `recursive_fibonacci` | [Tapas](recursive_fibonacci.tap) · [Python](recursive_fibonacci.py) · [Lua](recursive_fibonacci.lua) | 3,308.0 µs | 3,204.0 µs | 1,614.0 µs | 1.032× | 2.050× |
| `sieve` | [Tapas](sieve.tap) · [Python](sieve.py) · [Lua](sieve.lua) | 844.0 µs | 745.0 µs | 419.0 µs | 1.133× | 2.014× |
| `merge_sort` | [Tapas](merge_sort.tap) · [Python](merge_sort.py) · [Lua](merge_sort.lua) | 3,813.0 µs | 3,842.0 µs | 5,740.0 µs | 0.992× | 0.664× |
| `n_queens` | [Tapas](n_queens.tap) · [Python](n_queens.py) · [Lua](n_queens.lua) | 117,598.0 µs | 98,876.0 µs | 48,159.0 µs | 1.189× | 2.442× |
| `longest_common_subsequence` | [Tapas](longest_common_subsequence.tap) · [Python](longest_common_subsequence.py) · [Lua](longest_common_subsequence.lua) | 12,249.0 µs | 10,145.0 µs | 15,710.0 µs | 1.207× | 0.780× |
| `matrix_multiply` | [Tapas](matrix_multiply.tap) · [Python](matrix_multiply.py) · [Lua](matrix_multiply.lua) | 2,571.0 µs | 3,589.0 µs | 1,304.0 µs | 0.716× | 1.972× |
| `sudoku` | [Tapas](sudoku.tap) · [Python](sudoku.py) · [Lua](sudoku.lua) | 312,263.0 µs | 248,064.0 µs | 123,435.0 µs | 1.259× | 2.530× |
| `dijkstra` | [Tapas](dijkstra.tap) · [Python](dijkstra.py) · [Lua](dijkstra.lua) | 15,872.0 µs | 13,514.0 µs | 7,823.0 µs | 1.174× | 2.029× |
| `optimize` | [Tapas](optimize.tap) · [Python](optimize.py) · [Lua](optimize.lua) | 16,446.0 µs | 15,884.0 µs | 17,281.0 µs | 1.035× | 0.952× |
| `kmp` | [Tapas](kmp.tap) · [Python](kmp.py) · [Lua](kmp.lua) | 20,031.0 µs | 16,519.0 µs | 40,702.0 µs | 1.213× | 0.492× |
| `k_nucleotide` | [Tapas](k_nucleotide.tap) · [Python](k_nucleotide.py) · [Lua](k_nucleotide.lua) | 144,844.0 µs | 127,703.0 µs | 70,636.0 µs | 1.134× | 2.051× |
| `binary_trees` | [Tapas](binary_trees.tap) · [Python](binary_trees.py) · [Lua](binary_trees.lua) | 565,062.0 µs | 439,061.0 µs | 759,778.0 µs | 1.287× | 0.744× |
| `lru_cache` | [Tapas](lru_cache.tap) · [Python](lru_cache.py) · [Lua](lru_cache.lua) | 35,905.0 µs | 53,683.0 µs | 45,713.0 µs | 0.669× | 0.785× |
| `huffman` | [Tapas](huffman.tap) · [Python](huffman.py) · [Lua](huffman.lua) | 1,908.0 µs | 1,411.0 µs | 1,695.0 µs | 1.352× | 1.126× |
| `fannkuch` | [Tapas](fannkuch.tap) · [Python](fannkuch.py) · [Lua](fannkuch.lua) | 34,070.0 µs | 26,538.0 µs | 24,062.0 µs | 1.284× | 1.416× |
| `n_body` | [Tapas](n_body.tap) · [Python](n_body.py) · [Lua](n_body.lua) | 18,698.0 µs | 16,071.0 µs | 8,714.0 µs | 1.163× | 2.146× |

本组几何平均数：Tapas/Python **1.097×**，Tapas/Lua **1.335×**。

### 基础热路径

这些程序把计时循环放在函数内，并让循环体只保留被测的那一种 VM 操作或容器访问模式，用来定位解释器的基础开销；与目标操作无关的取模寻址、增长整数累加和模块级变量访问均已移除，避免把其他语言的长短处混进单项结果。`vm_hot_paths` 与 `float_arithmetic` 的循环体本身就是被测的混合算术负载；`function_call_baseline` 与 `function_calls` 共享同一循环体，两者相减可估算直接调用净成本，`function_callbacks` 在其上增加一层高阶回调。

| 项目 | 源码 | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | [Tapas](vm_hot_paths.tap) · [Python](vm_hot_paths.py) · [Lua](vm_hot_paths.lua) | 66,664.0 µs | 102,687.0 µs | 36,355.0 µs | 0.649× | 1.834× |
| `float_arithmetic` | [Tapas](float_arithmetic.tap) · [Python](float_arithmetic.py) · [Lua](float_arithmetic.lua) | 20,227.0 µs | 30,513.0 µs | 8,269.0 µs | 0.663× | 2.446× |
| `function_call_baseline` | [Tapas](function_call_baseline.tap) · [Python](function_call_baseline.py) · [Lua](function_call_baseline.lua) | 2,445.0 µs | 4,292.0 µs | 1,014.0 µs | 0.570× | 2.411× |
| `function_calls` | [Tapas](function_calls.tap) · [Python](function_calls.py) · [Lua](function_calls.lua) | 5,782.0 µs | 6,203.0 µs | 2,957.0 µs | 0.932× | 1.955× |
| `function_callbacks` | [Tapas](function_callbacks.tap) · [Python](function_callbacks.py) · [Lua](function_callbacks.lua) | 11,074.0 µs | 9,484.0 µs | 5,977.0 µs | 1.168× | 1.853× |
| `list_access` | [Tapas](list_access.tap) · [Python](list_access.py) · [Lua](list_access.lua) | 3,837.0 µs | 4,913.0 µs | 1,195.0 µs | 0.781× | 3.211× |
| `list_append` | [Tapas](list_append.tap) · [Python](list_append.py) · [Lua](list_append.lua) | 8,238.0 µs | 6,331.0 µs | 5,507.0 µs | 1.301× | 1.496× |
| `list_update` | [Tapas](list_update.tap) · [Python](list_update.py) · [Lua](list_update.lua) | 3,401.0 µs | 3,890.0 µs | 1,723.0 µs | 0.874× | 1.974× |
| `list_copy` | [Tapas](list_copy.tap) · [Python](list_copy.py) · [Lua](list_copy.lua) | 4,715.0 µs | 18,852.0 µs | 122,006.0 µs | 0.250× | 0.039× |
| `list_from_iterable` | [Tapas](list_from_iterable.tap) · [Python](list_from_iterable.py) · [Lua](list_from_iterable.lua) | 9,851.0 µs | 160,860.0 µs | 188,792.0 µs | 0.061× | 0.052× |
| `list_replicate` | [Tapas](list_replicate.tap) · [Python](list_replicate.py) · [Lua](list_replicate.lua) | 4,531.0 µs | 11,419.0 µs | 323,755.0 µs | 0.397× | 0.014× |
| `dict_insert` | [Tapas](dict_insert.tap) · [Python](dict_insert.py) · [Lua](dict_insert.lua) | 4,822.0 µs | 5,932.0 µs | 1,152.0 µs | 0.813× | 4.186× |
| `dict_lookup` | [Tapas](dict_lookup.tap) · [Python](dict_lookup.py) · [Lua](dict_lookup.lua) | 3,862.0 µs | 9,969.0 µs | 1,152.0 µs | 0.387× | 3.352× |
| `dict_update` | [Tapas](dict_update.tap) · [Python](dict_update.py) · [Lua](dict_update.lua) | 4,158.0 µs | 11,214.0 µs | 1,712.0 µs | 0.371× | 2.429× |
| `dict_delete` | [Tapas](dict_delete.tap) · [Python](dict_delete.py) · [Lua](dict_delete.lua) | 3,008.0 µs | 5,979.0 µs | 514.0 µs | 0.503× | 5.852× |
| `dict_literal` | [Tapas](dict_literal.tap) · [Python](dict_literal.py) · [Lua](dict_literal.lua) | 16,029.0 µs | 17,702.0 µs | 27,535.0 µs | 0.905× | 0.582× |
| `dict_counting` | [Tapas](dict_counting.tap) · [Python](dict_counting.py) · [Lua](dict_counting.lua) | 15,577.0 µs | 25,112.0 µs | 5,907.0 µs | 0.620× | 2.637× |
| `tail_recursion` | [Tapas](tail_recursion.tap) · [Python](tail_recursion.py) · [Lua](tail_recursion.lua) | 1,395.0 µs | 2,998.0 µs | 449.0 µs | 0.465× | 3.107× |
| `branch_logic` | [Tapas](branch_logic.tap) · [Python](branch_logic.py) · [Lua](branch_logic.lua) | 19,400.0 µs | 19,485.0 µs | 7,223.0 µs | 0.996× | 2.686× |

本组几何平均数：Tapas/Python **0.569×**，Tapas/Lua **1.189×**。

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
