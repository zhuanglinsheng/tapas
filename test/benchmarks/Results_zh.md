# Tapas 与 Python 性能比较

简体中文 | [English](Results_en.md) | [项目主页](../../README.md)

测试日期：2026-09-02

## 测试环境

- 系统：`macOS-26.5.2-arm64-arm-64bit-Mach-O`
- 处理器架构：`arm64`
- Python：`3.13.3`
- Tapas：`Tapas 0.1.0 Copyright (C) 2020-2026 Zhuang Linsheng <zhuanglinsheng@outlook.com>.`
- Tapas 可执行文件：`build-release/bin/tapas`
- 有效运行次数：每项 11 次，另预热 1 次

## 结果

时间为进程 CPU 时间的中位数，不包含进程启动、源码加载和编译。
`Tapas/Python` 小于 1 表示 Tapas 更快。

### 综合算法

这些程序组合使用递归、分支、容器、索引和内存分配，更接近完整算法负载。

| 项目 | 源码 | 计算结果 | Tapas | Python | Tapas/Python |
| --- | --- | ---: | ---: | ---: | ---: |
| `recursive_fibonacci` | [Tapas](recursive_fibonacci.tap) · [Python](recursive_fibonacci.py) | 46368 | 6,902,000 ns | 5,301,000 ns | 1.302× |
| `sieve` | [Tapas](sieve.tap) · [Python](sieve.py) | 2262 | 2,287,000 ns | 1,991,000 ns | 1.149× |
| `merge_sort` | [Tapas](merge_sort.tap) · [Python](merge_sort.py) | 5638402144 | 9,359,000 ns | 5,706,000 ns | 1.640× |
| `n_queens` | [Tapas](n_queens.tap) · [Python](n_queens.py) | 724 | 226,331,000 ns | 152,538,000 ns | 1.484× |
| `longest_common_subsequence` | [Tapas](longest_common_subsequence.tap) · [Python](longest_common_subsequence.py) | 1480 | 43,013,000 ns | 16,870,000 ns | 2.550× |
| `matrix_multiply` | [Tapas](matrix_multiply.tap) · [Python](matrix_multiply.py) | 817318963 | 5,645,000 ns | 5,254,000 ns | 1.074× |

本组比值的几何平均数为 **1.467×**。

### 基础热路径

这些程序分别放大某一种常见 VM 操作，用来定位解释器的基础开销。

| 项目 | 源码 | 计算结果 | Tapas | Python | Tapas/Python |
| --- | --- | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | [Tapas](vm_hot_paths.tap) · [Python](vm_hot_paths.py) | 8999994 | 161,430,000 ns | 280,901,000 ns | 0.575× |
| `float_arithmetic` | [Tapas](float_arithmetic.tap) · [Python](float_arithmetic.py) | 125627 | 42,606,000 ns | 73,943,000 ns | 0.576× |
| `function_calls` | [Tapas](function_calls.tap) · [Python](function_calls.py) | 599994 | 11,964,000 ns | 18,487,000 ns | 0.647× |
| `list_access` | [Tapas](list_access.tap) · [Python](list_access.py) | 1937500 | 18,422,000 ns | 38,664,000 ns | 0.476× |
| `tail_recursion` | [Tapas](tail_recursion.tap) · [Python](tail_recursion.py) | 50000 | 2,589,000 ns | 3,454,000 ns | 0.750× |
| `branch_logic` | [Tapas](branch_logic.tap) · [Python](branch_logic.py) | -2626268 | 127,680,000 ns | 184,419,000 ns | 0.692× |

本组比值的几何平均数为 **0.613×**。

全部项目的几何平均数为 **0.948×**。

## 说明

这些程序用于比较两种实现执行相同算法时的解释器开销，不代表大型应用的完整性能。
结果会受系统负载、电源状态、编译器版本和 Python 版本影响。
更新 VM 或运行环境后，应使用本目录 README 中的命令重新生成。
