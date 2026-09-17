# Tapas 与 Python 性能比较

简体中文 | [English](Results_en.md) | [项目主页](../../README.md)

测试日期：2026-09-17

## 测试环境

- 系统：`macOS-26.6.2-arm64-arm-64bit-Mach-O`
- 处理器架构：`arm64`
- Python：`3.13.3`
- Tapas：`Tapas 0.1.0 Copyright (C) 2020-2026`
- Tapas 可执行文件：`build-release/bin/tapas`
- 有效运行次数：每项 11 次，另预热 1 次

## 结果

时间为进程 CPU 时间的中位数，不包含进程启动、源码加载和编译。
`Tapas/Python` 小于 1 表示 Tapas 更快。

### 综合算法

这些程序组合使用递归、分支、容器、索引和内存分配，更接近完整算法负载。

| 项目 | 源码 | 计算结果 | Tapas | Python | Tapas/Python |
| --- | --- | ---: | ---: | ---: | ---: |
| `recursive_fibonacci` | [Tapas](recursive_fibonacci.tap) · [Python](recursive_fibonacci.py) | 46368 | 6,010,000 ns | 5,144,000 ns | 1.168× |
| `sieve` | [Tapas](sieve.tap) · [Python](sieve.py) | 2262 | 1,780,000 ns | 1,916,000 ns | 0.929× |
| `merge_sort` | [Tapas](merge_sort.tap) · [Python](merge_sort.py) | 5638402144 | 6,898,000 ns | 5,618,000 ns | 1.228× |
| `n_queens` | [Tapas](n_queens.tap) · [Python](n_queens.py) | 724 | 175,513,000 ns | 151,226,000 ns | 1.161× |
| `longest_common_subsequence` | [Tapas](longest_common_subsequence.tap) · [Python](longest_common_subsequence.py) | 1480 | 20,779,000 ns | 16,531,000 ns | 1.257× |
| `matrix_multiply` | [Tapas](matrix_multiply.tap) · [Python](matrix_multiply.py) | 817318963 | 4,347,000 ns | 5,236,000 ns | 0.830× |

本组比值的几何平均数为 **1.083×**。

### 基础热路径

这些程序分别放大某一种常见 VM 操作，用来定位解释器的基础开销。

| 项目 | 源码 | 计算结果 | Tapas | Python | Tapas/Python |
| --- | --- | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | [Tapas](vm_hot_paths.tap) · [Python](vm_hot_paths.py) | 8999994 | 104,667,000 ns | 294,751,000 ns | 0.355× |
| `float_arithmetic` | [Tapas](float_arithmetic.tap) · [Python](float_arithmetic.py) | 125627 | 32,871,000 ns | 75,281,000 ns | 0.437× |
| `function_calls` | [Tapas](function_calls.tap) · [Python](function_calls.py) | 599994 | 10,454,000 ns | 19,433,000 ns | 0.538× |
| `list_access` | [Tapas](list_access.tap) · [Python](list_access.py) | 1937500 | 13,534,000 ns | 38,818,000 ns | 0.349× |
| `tail_recursion` | [Tapas](tail_recursion.tap) · [Python](tail_recursion.py) | 50000 | 1,828,000 ns | 3,332,000 ns | 0.549× |
| `branch_logic` | [Tapas](branch_logic.tap) · [Python](branch_logic.py) | -2626268 | 102,258,000 ns | 184,042,000 ns | 0.556× |

本组比值的几何平均数为 **0.455×**。

全部项目的几何平均数为 **0.702×**。

## 说明

这些程序用于比较两种实现执行相同算法时的解释器开销，不代表大型应用的完整性能。
结果会受系统负载、电源状态、编译器版本和 Python 版本影响。
更新 VM 或运行环境后，应使用本目录 README 中的命令重新生成。
