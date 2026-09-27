# Tapas 与 Python、Lua 性能比较

简体中文 | [English](README_en.md) | [项目主页](../../README.md)

测试日期：2026-09-27

## 测试环境

- 系统：`macOS-26.6.2-arm64-arm-64bit-Mach-O`
- 处理器架构：`arm64`
- Python：`3.14.7`
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
| `recursive_fibonacci` | [Tapas](recursive_fibonacci.tap) · [Python](recursive_fibonacci.py) · [Lua](recursive_fibonacci.lua) | 4,974.0 µs | 3,434.0 µs | 1,660.0 µs | 1.448× | 2.996× |
| `sieve` | [Tapas](sieve.tap) · [Python](sieve.py) · [Lua](sieve.lua) | 1,648.0 µs | 1,031.0 µs | 425.0 µs | 1.598× | 3.878× |
| `merge_sort` | [Tapas](merge_sort.tap) · [Python](merge_sort.py) · [Lua](merge_sort.lua) | 6,079.0 µs | 3,716.0 µs | 5,703.0 µs | 1.636× | 1.066× |
| `n_queens` | [Tapas](n_queens.tap) · [Python](n_queens.py) · [Lua](n_queens.lua) | 163,594.0 µs | 98,895.0 µs | 51,218.0 µs | 1.654× | 3.194× |
| `longest_common_subsequence` | [Tapas](longest_common_subsequence.tap) · [Python](longest_common_subsequence.py) · [Lua](longest_common_subsequence.lua) | 19,786.0 µs | 10,220.0 µs | 15,996.0 µs | 1.936× | 1.237× |
| `matrix_multiply` | [Tapas](matrix_multiply.tap) · [Python](matrix_multiply.py) · [Lua](matrix_multiply.lua) | 4,712.0 µs | 3,644.0 µs | 1,256.0 µs | 1.293× | 3.752× |
| `sudoku` | [Tapas](sudoku.tap) · [Python](sudoku.py) · [Lua](sudoku.lua) | 386,062.0 µs | 254,639.0 µs | 131,054.0 µs | 1.516× | 2.946× |
| `dijkstra` | [Tapas](dijkstra.tap) · [Python](dijkstra.py) · [Lua](dijkstra.lua) | 27,904.0 µs | 13,681.0 µs | 8,279.0 µs | 2.040× | 3.370× |
| `optimize` | [Tapas](optimize.tap) · [Python](optimize.py) · [Lua](optimize.lua) | 21,424.0 µs | 16,395.0 µs | 16,773.0 µs | 1.307× | 1.277× |
| `kmp` | [Tapas](kmp.tap) · [Python](kmp.py) · [Lua](kmp.lua) | 28,395.0 µs | 16,584.0 µs | 41,960.0 µs | 1.712× | 0.677× |
| `k_nucleotide` | [Tapas](k_nucleotide.tap) · [Python](k_nucleotide.py) · [Lua](k_nucleotide.lua) | 143,033.0 µs | 123,795.0 µs | 72,015.0 µs | 1.155× | 1.986× |
| `binary_trees` | [Tapas](binary_trees.tap) · [Python](binary_trees.py) · [Lua](binary_trees.lua) | 807,155.0 µs | 442,519.0 µs | 772,585.0 µs | 1.824× | 1.045× |
| `lru_cache` | [Tapas](lru_cache.tap) · [Python](lru_cache.py) · [Lua](lru_cache.lua) | 41,295.0 µs | 54,608.0 µs | 44,152.0 µs | 0.756× | 0.935× |
| `huffman` | [Tapas](huffman.tap) · [Python](huffman.py) · [Lua](huffman.lua) | 2,890.0 µs | 1,432.0 µs | 1,698.0 µs | 2.018× | 1.702× |
| `fannkuch` | [Tapas](fannkuch.tap) · [Python](fannkuch.py) · [Lua](fannkuch.lua) | 60,713.0 µs | 27,062.0 µs | 24,884.0 µs | 2.243× | 2.440× |
| `n_body` | [Tapas](n_body.tap) · [Python](n_body.py) · [Lua](n_body.lua) | 24,910.0 µs | 16,260.0 µs | 9,508.0 µs | 1.532× | 2.620× |

本组几何平均数：Tapas/Python **1.558×**，Tapas/Lua **1.917×**。

### 基础热路径

这些程序分别放大某一种常见 VM 操作，用来定位解释器的基础开销。

| 项目 | 源码 | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | [Tapas](vm_hot_paths.tap) · [Python](vm_hot_paths.py) · [Lua](vm_hot_paths.lua) | 73,949.0 µs | 219,705.0 µs | 36,834.0 µs | 0.337× | 2.008× |
| `float_arithmetic` | [Tapas](float_arithmetic.tap) · [Python](float_arithmetic.py) · [Lua](float_arithmetic.lua) | 24,367.0 µs | 64,929.0 µs | 8,685.0 µs | 0.375× | 2.806× |
| `function_calls` | [Tapas](function_calls.tap) · [Python](function_calls.py) · [Lua](function_calls.lua) | 9,262.0 µs | 13,932.0 µs | 2,978.0 µs | 0.665× | 3.110× |
| `list_access` | [Tapas](list_access.tap) · [Python](list_access.py) · [Lua](list_access.lua) | 11,250.0 µs | 29,913.0 µs | 4,691.0 µs | 0.376× | 2.398× |
| `tail_recursion` | [Tapas](tail_recursion.tap) · [Python](tail_recursion.py) · [Lua](tail_recursion.lua) | 1,919.0 µs | 2,956.0 µs | 458.0 µs | 0.649× | 4.190× |
| `branch_logic` | [Tapas](branch_logic.tap) · [Python](branch_logic.py) · [Lua](branch_logic.lua) | 76,824.0 µs | 158,893.0 µs | 28,742.0 µs | 0.483× | 2.673× |

本组几何平均数：Tapas/Python **0.463×**，Tapas/Lua **2.789×**。

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
