# Tapas 与 Python、Lua 性能比较

简体中文 | [English](Results_en.md) | [项目主页](../../README.md)

测试日期：2026-09-17

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
| `recursive_fibonacci` | [Tapas](recursive_fibonacci.tap) · [Python](recursive_fibonacci.py) · [Lua](recursive_fibonacci.lua) | 5,140.0 µs | 5,358.0 µs | 1,668.0 µs | 0.959× | 3.082× |
| `sieve` | [Tapas](sieve.tap) · [Python](sieve.py) · [Lua](sieve.lua) | 1,878.0 µs | 1,911.0 µs | 472.0 µs | 0.983× | 3.979× |
| `merge_sort` | [Tapas](merge_sort.tap) · [Python](merge_sort.py) · [Lua](merge_sort.lua) | 6,304.0 µs | 5,595.0 µs | 5,676.0 µs | 1.127× | 1.111× |
| `n_queens` | [Tapas](n_queens.tap) · [Python](n_queens.py) · [Lua](n_queens.lua) | 173,902.0 µs | 150,263.0 µs | 52,340.0 µs | 1.157× | 3.323× |
| `longest_common_subsequence` | [Tapas](longest_common_subsequence.tap) · [Python](longest_common_subsequence.py) · [Lua](longest_common_subsequence.lua) | 20,562.0 µs | 16,648.0 µs | 16,064.0 µs | 1.235× | 1.280× |
| `matrix_multiply` | [Tapas](matrix_multiply.tap) · [Python](matrix_multiply.py) · [Lua](matrix_multiply.lua) | 4,815.0 µs | 5,158.0 µs | 1,315.0 µs | 0.934× | 3.662× |

本组几何平均数：Tapas/Python **1.060×**，Tapas/Lua **2.442×**。

### 基础热路径

这些程序分别放大某一种常见 VM 操作，用来定位解释器的基础开销。

| 项目 | 源码 | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | [Tapas](vm_hot_paths.tap) · [Python](vm_hot_paths.py) · [Lua](vm_hot_paths.lua) | 98,231.0 µs | 295,007.0 µs | 37,208.0 µs | 0.333× | 2.640× |
| `float_arithmetic` | [Tapas](float_arithmetic.tap) · [Python](float_arithmetic.py) · [Lua](float_arithmetic.lua) | 30,167.0 µs | 73,752.0 µs | 9,293.0 µs | 0.409× | 3.246× |
| `function_calls` | [Tapas](function_calls.tap) · [Python](function_calls.py) · [Lua](function_calls.lua) | 10,656.0 µs | 18,112.0 µs | 2,991.0 µs | 0.588× | 3.563× |
| `list_access` | [Tapas](list_access.tap) · [Python](list_access.py) · [Lua](list_access.lua) | 12,722.0 µs | 38,565.0 µs | 4,694.0 µs | 0.330× | 2.710× |
| `tail_recursion` | [Tapas](tail_recursion.tap) · [Python](tail_recursion.py) · [Lua](tail_recursion.lua) | 1,925.0 µs | 3,351.0 µs | 460.0 µs | 0.574× | 4.185× |
| `branch_logic` | [Tapas](branch_logic.tap) · [Python](branch_logic.py) · [Lua](branch_logic.lua) | 101,725.0 µs | 179,922.0 µs | 28,958.0 µs | 0.565× | 3.513× |

本组几何平均数：Tapas/Python **0.453×**，Tapas/Lua **3.267×**。

全部项目的几何平均数：Tapas/Python **0.693×**，Tapas/Lua **2.825×**。

## 说明

这些程序用于比较三种实现执行相同算法时的解释器开销，不代表大型应用的完整性能。
结果会受系统负载、电源状态、编译器版本、Python 版本和 Lua 版本影响。
更新 VM 或运行环境后，应使用本目录 README 中的命令重新生成。
