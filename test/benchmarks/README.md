# Tapas 与 Python、Lua 性能比较

简体中文 | [English](README_en.md) | [项目主页](../../README.md)

测试日期：2026-09-18

## 测试环境

- 系统：`macOS-26.6.2-arm64-arm-64bit-Mach-O`
- 处理器架构：`arm64`
- Python：`3.13.3`
- Tapas：`Tapas 0.1.0 Copyright (C) 2020-2026`
- Lua 解释器：`Lua 5.5.1  Copyright (C) 1994-2026 Lua.org, PUC-Rio`
- Tapas 可执行文件：`build-release/bin/tapas`
- 有效运行次数：每项 11 次，另预热 1 次

## 结果

时间为进程 CPU 时间的微秒中位数，不包含进程启动、源码加载和编译。
比值小于 1 表示 Tapas 更快（分别相对 Python 与 Lua）。

### 综合算法

这些程序组合使用递归、分支、容器、索引和内存分配，更接近完整算法负载。

| 项目 | 源码 | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `recursive_fibonacci` | [Tapas](recursive_fibonacci.tap) · [Python](recursive_fibonacci.py) · [Lua](recursive_fibonacci.lua) | 5,168.0 µs | 5,232.0 µs | 1,704.0 µs | 0.988× | 3.033× |
| `sieve` | [Tapas](sieve.tap) · [Python](sieve.py) · [Lua](sieve.lua) | 1,932.0 µs | 1,951.0 µs | 463.0 µs | 0.990× | 4.173× |
| `merge_sort` | [Tapas](merge_sort.tap) · [Python](merge_sort.py) · [Lua](merge_sort.lua) | 6,385.0 µs | 5,702.0 µs | 5,753.0 µs | 1.120× | 1.110× |
| `n_queens` | [Tapas](n_queens.tap) · [Python](n_queens.py) · [Lua](n_queens.lua) | 174,636.0 µs | 152,214.0 µs | 52,698.0 µs | 1.147× | 3.314× |
| `longest_common_subsequence` | [Tapas](longest_common_subsequence.tap) · [Python](longest_common_subsequence.py) · [Lua](longest_common_subsequence.lua) | 21,006.0 µs | 16,754.0 µs | 16,216.0 µs | 1.254× | 1.295× |
| `matrix_multiply` | [Tapas](matrix_multiply.tap) · [Python](matrix_multiply.py) · [Lua](matrix_multiply.lua) | 4,880.0 µs | 5,218.0 µs | 1,311.0 µs | 0.935× | 3.722× |
| `sudoku` | [Tapas](sudoku.tap) · [Python](sudoku.py) · [Lua](sudoku.lua) | 405,558.0 µs | 439,180.0 µs | 131,898.0 µs | 0.923× | 3.075× |
| `dijkstra` | [Tapas](dijkstra.tap) · [Python](dijkstra.py) · [Lua](dijkstra.lua) | 29,740.0 µs | 24,372.0 µs | 8,353.0 µs | 1.220× | 3.560× |
| `optimize` | [Tapas](optimize.tap) · [Python](optimize.py) · [Lua](optimize.lua) | 24,064.0 µs | 19,100.0 µs | 16,937.0 µs | 1.260× | 1.421× |
| `kmp` | [Tapas](kmp.tap) · [Python](kmp.py) · [Lua](kmp.lua) | 31,086.0 µs | 27,941.0 µs | 42,984.0 µs | 1.113× | 0.723× |
| `k_nucleotide` | [Tapas](k_nucleotide.tap) · [Python](k_nucleotide.py) · [Lua](k_nucleotide.lua) | 146,285.0 µs | 153,400.0 µs | 71,174.0 µs | 0.954× | 2.055× |
| `binary_trees` | [Tapas](binary_trees.tap) · [Python](binary_trees.py) · [Lua](binary_trees.lua) | 865,066.0 µs | 611,519.0 µs | 775,906.0 µs | 1.415× | 1.115× |
| `lru_cache` | [Tapas](lru_cache.tap) · [Python](lru_cache.py) · [Lua](lru_cache.lua) | 44,789.0 µs | 55,772.0 µs | 44,076.0 µs | 0.803× | 1.016× |
| `huffman` | [Tapas](huffman.tap) · [Python](huffman.py) · [Lua](huffman.lua) | 3,031.0 µs | 2,059.0 µs | 1,689.0 µs | 1.472× | 1.795× |
| `fannkuch` | [Tapas](fannkuch.tap) · [Python](fannkuch.py) · [Lua](fannkuch.lua) | 62,991.0 µs | 40,373.0 µs | 24,789.0 µs | 1.560× | 2.541× |
| `n_body` | [Tapas](n_body.tap) · [Python](n_body.py) · [Lua](n_body.lua) | 26,740.0 µs | 19,152.0 µs | 9,469.0 µs | 1.396× | 2.824× |

本组几何平均数：Tapas/Python **1.140×**，Tapas/Lua **2.018×**。

### 基础热路径

这些程序分别放大某一种常见 VM 操作，用来定位解释器的基础开销。

| 项目 | 源码 | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | [Tapas](vm_hot_paths.tap) · [Python](vm_hot_paths.py) · [Lua](vm_hot_paths.lua) | 99,193.0 µs | 294,549.0 µs | 36,613.0 µs | 0.337× | 2.709× |
| `float_arithmetic` | [Tapas](float_arithmetic.tap) · [Python](float_arithmetic.py) · [Lua](float_arithmetic.lua) | 30,430.0 µs | 72,838.0 µs | 9,006.0 µs | 0.418× | 3.379× |
| `function_calls` | [Tapas](function_calls.tap) · [Python](function_calls.py) · [Lua](function_calls.lua) | 10,560.0 µs | 18,328.0 µs | 2,938.0 µs | 0.576× | 3.594× |
| `list_access` | [Tapas](list_access.tap) · [Python](list_access.py) · [Lua](list_access.lua) | 12,783.0 µs | 38,002.0 µs | 4,817.0 µs | 0.336× | 2.654× |
| `tail_recursion` | [Tapas](tail_recursion.tap) · [Python](tail_recursion.py) · [Lua](tail_recursion.lua) | 1,892.0 µs | 3,183.0 µs | 458.0 µs | 0.594× | 4.131× |
| `branch_logic` | [Tapas](branch_logic.tap) · [Python](branch_logic.py) · [Lua](branch_logic.lua) | 99,571.0 µs | 186,342.0 µs | 28,869.0 µs | 0.534× | 3.449× |

本组几何平均数：Tapas/Python **0.453×**，Tapas/Lua **3.279×**。

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
结果会受系统负载、电源状态、编译器版本、Python 版本和 Lua 版本影响。
更新 VM 或运行环境后，应使用上文「用法」中的命令重新生成。
