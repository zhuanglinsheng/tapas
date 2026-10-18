# Tapas 与 Python、Lua 性能比较

简体中文 | [English](../../README_en.md) | [项目主页](../../../../README.md)

测试日期：2026-10-17

## 测试环境

- 系统：`macOS-27.0.1-arm64-arm-64bit-Mach-O`
- 处理器架构：`arm64`
- Python：`3.14.2`
- Tapas：`Tapas 0.1.0 Copyright (C) 2020-2026`
- Lua 解释器：`Lua 5.5.1  Copyright (C) 1994-2026 Lua.org, PUC-Rio`
- Tapas 可执行文件：`/Users/zhuanglinsheng/Documents/Github/tapas/build-release/bin/tapas`
- 有效运行次数：每项 7 次，另预热 1 次
- 构建参数：`CMAKE_BUILD_TYPE:STRING=Release`
- 构建参数：`CMAKE_C_COMPILER:FILEPATH=/usr/bin/cc`
- 构建参数：`CMAKE_C_FLAGS:STRING=`
- 构建参数：`CMAKE_C_FLAGS_RELEASE:STRING=-O3 -DNDEBUG`
- 构建参数：`CMAKE_EXE_LINKER_FLAGS:STRING=`
- 构建参数：`TAPAS_ENABLE_IPO:BOOL=ON`

## 结果

时间为进程 CPU 时间的微秒中位数，不包含进程启动、源码加载和编译。
比值小于 1 表示 Tapas 更快（分别相对 Python 与 Lua）。

### 综合算法

保留指定算法，允许各语言使用惯用容器和标准库批量操作；不要求逐行翻译。输入、重复次数、输出及计时边界保持等价。规则见 SEMANTICS.md。

| 项目 | 源码 | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `recursive_fibonacci` | [Tapas](../../recursive_fibonacci.tap) · [Python](../../recursive_fibonacci.py) · [Lua](../../recursive_fibonacci.lua) | 4,130.0 µs | 5,402.0 µs | 2,573.0 µs | 0.765× | 1.605× |
| `sieve` | [Tapas](../../sieve.tap) · [Python](../../sieve.py) · [Lua](../../sieve.lua) | 86.0 µs | 51.0 µs | 608.0 µs | 1.686× | 0.141× |
| `merge_sort` | [Tapas](../../merge_sort.tap) · [Python](../../merge_sort.py) · [Lua](../../merge_sort.lua) | 4,847.0 µs | 4,528.0 µs | 5,510.0 µs | 1.070× | 0.880× |
| `n_queens` | [Tapas](../../n_queens.tap) · [Python](../../n_queens.py) · [Lua](../../n_queens.lua) | 67,293.0 µs | 79,359.0 µs | 48,189.0 µs | 0.848× | 1.396× |
| `longest_common_subsequence` | [Tapas](../../longest_common_subsequence.tap) · [Python](../../longest_common_subsequence.py) · [Lua](../../longest_common_subsequence.lua) | 14,477.0 µs | 10,510.0 µs | 12,074.0 µs | 1.377× | 1.199× |
| `matrix_multiply` | [Tapas](../../matrix_multiply.tap) · [Python](../../matrix_multiply.py) · [Lua](../../matrix_multiply.lua) | 3,040.0 µs | 3,091.0 µs | 1,243.0 µs | 0.984× | 2.446× |
| `sudoku` | [Tapas](../../sudoku.tap) · [Python](../../sudoku.py) · [Lua](../../sudoku.lua) | 365,509.0 µs | 306,801.0 µs | 158,438.0 µs | 1.191× | 2.307× |
| `dijkstra` | [Tapas](../../dijkstra.tap) · [Python](../../dijkstra.py) · [Lua](../../dijkstra.lua) | 8,434.0 µs | 3,349.0 µs | 6,088.0 µs | 2.518× | 1.385× |
| `optimize` | [Tapas](../../optimize.tap) · [Python](../../optimize.py) · [Lua](../../optimize.lua) | 18,729.0 µs | 20,379.0 µs | 14,004.0 µs | 0.919× | 1.337× |
| `kmp` | [Tapas](../../kmp.tap) · [Python](../../kmp.py) · [Lua](../../kmp.lua) | 21,103.0 µs | 19,237.0 µs | 28,821.0 µs | 1.097× | 0.732× |
| `k_nucleotide` | [Tapas](../../k_nucleotide.tap) · [Python](../../k_nucleotide.py) · [Lua](../../k_nucleotide.lua) | 112,211.0 µs | 123,888.0 µs | 69,548.0 µs | 0.906× | 1.613× |
| `binary_trees` | [Tapas](../../binary_trees.tap) · [Python](../../binary_trees.py) · [Lua](../../binary_trees.lua) | 738,612.0 µs | 570,079.0 µs | 877,616.0 µs | 1.296× | 0.842× |
| `lru_cache` | [Tapas](../../lru_cache.tap) · [Python](../../lru_cache.py) · [Lua](../../lru_cache.lua) | 42,902.0 µs | 71,112.0 µs | 35,747.0 µs | 0.603× | 1.200× |
| `huffman` | [Tapas](../../huffman.tap) · [Python](../../huffman.py) · [Lua](../../huffman.lua) | 2,059.0 µs | 1,199.0 µs | 1,596.0 µs | 1.717× | 1.290× |
| `fannkuch` | [Tapas](../../fannkuch.tap) · [Python](../../fannkuch.py) · [Lua](../../fannkuch.lua) | 40,044.0 µs | 38,691.0 µs | 28,697.0 µs | 1.035× | 1.395× |
| `n_body` | [Tapas](../../n_body.tap) · [Python](../../n_body.py) · [Lua](../../n_body.lua) | 24,467.0 µs | 20,608.0 µs | 10,839.0 µs | 1.187× | 2.257× |

本组几何平均数：Tapas/Python **1.131×**（16 项）, Tapas/Lua **1.194×**（16 项）。

### 基础热路径

本表自动收录 `hotspots/` 中全部 Tapas/Python 案例，包括基础 VM 操作、综合算法提取的组合热点及负对照。Python 列运行各文件的默认实现；新增诊断案例默认使用惯用优化写法；七组 Python 结构对照在下表单独列出。缺少 Lua 实现时标为 —，不计入 Lua 几何平均；两种比值分别标注实际样本数。`function_call_baseline` 与 `function_calls` 共享循环体，可估算直接调用净成本；`function_callbacks` 增加高阶回调。

| 项目 | 测量内容 | 源码 | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| `vm_hot_paths` | 内核：整数循环、取模、分支组合 | [Tapas](../../hotspots/vm_hot_paths.tap) · [Python](../../hotspots/vm_hot_paths.py) · [Lua](../../hotspots/vm_hot_paths.lua) | 75,002.0 µs | 140,269.0 µs | 44,447.0 µs | 0.535× | 1.687× |
| `float_arithmetic` | 内核：有依赖的浮点乘加链 | [Tapas](../../hotspots/float_arithmetic.tap) · [Python](../../hotspots/float_arithmetic.py) · [Lua](../../hotspots/float_arithmetic.lua) | 22,003.0 µs | 42,155.0 µs | 10,537.0 µs | 0.522× | 2.088× |
| `function_call_baseline` | 内核：调用对照：循环、取模、累加 | [Tapas](../../hotspots/function_call_baseline.tap) · [Python](../../hotspots/function_call_baseline.py) · [Lua](../../hotspots/function_call_baseline.lua) | 2,733.0 µs | 6,110.0 µs | 1,198.0 µs | 0.447× | 2.281× |
| `function_calls` | 内核：固定标量函数的直接调用 | [Tapas](../../hotspots/function_calls.tap) · [Python](../../hotspots/function_calls.py) · [Lua](../../hotspots/function_calls.lua) | 6,605.0 µs | 9,197.0 µs | 3,679.0 µs | 0.718× | 1.795× |
| `function_callbacks` | 内核：固定回调经包装函数转发 | [Tapas](../../hotspots/function_callbacks.tap) · [Python](../../hotspots/function_callbacks.py) · [Lua](../../hotspots/function_callbacks.lua) | 12,357.0 µs | 12,861.0 µs | 5,947.0 µs | 0.961× | 2.078× |
| `list_access` | 内核：顺序 List 读取与循环 | [Tapas](../../hotspots/list_access.tap) · [Python](../../hotspots/list_access.py) · [Lua](../../hotspots/list_access.lua) | 6,347.0 µs | 7,102.0 µs | 1,863.0 µs | 0.894× | 3.407× |
| `list_append` | 内核：逐次追加及扩容 | [Tapas](../../hotspots/list_append.tap) · [Python](../../hotspots/list_append.py) · [Lua](../../hotspots/list_append.lua) | 7,774.0 µs | 9,475.0 µs | 6,488.0 µs | 0.820× | 1.198× |
| `list_update` | 内核：已有 List 槽位覆盖 | [Tapas](../../hotspots/list_update.tap) · [Python](../../hotspots/list_update.py) · [Lua](../../hotspots/list_update.lua) | 4,926.0 µs | 6,150.0 µs | 2,456.0 µs | 0.801× | 2.006× |
| `list_copy` | 任务：完整浅复制、分配及回收 | [Tapas](../../hotspots/list_copy.tap) · [Python](../../hotspots/list_copy.py) · [Lua](../../hotspots/list_copy.lua) | 4,971.0 µs | 21,715.0 µs | 153,989.0 µs | 0.229× | 0.032× |
| `list_from_iterable` | 任务：从整数范围构造序列 | [Tapas](../../hotspots/list_from_iterable.tap) · [Python](../../hotspots/list_from_iterable.py) · [Lua](../../hotspots/list_from_iterable.lua) | 10,396.0 µs | 218,369.0 µs | 179,308.0 µs | 0.048× | 0.058× |
| `list_replicate` | 任务：完整序列重复构造 | [Tapas](../../hotspots/list_replicate.tap) · [Python](../../hotspots/list_replicate.py) · [Lua](../../hotspots/list_replicate.lua) | 5,517.0 µs | 14,023.0 µs | 165,315.0 µs | 0.393× | 0.033× |
| `dict_insert` | 内核：顺序整数键插入及扩容 | [Tapas](../../hotspots/dict_insert.tap) · [Python](../../hotspots/dict_insert.py) · [Lua](../../hotspots/dict_insert.lua) | 7,753.0 µs | 8,511.0 µs | 1,270.0 µs | 0.911× | 6.105× |
| `dict_lookup` | 内核：顺序已有整数键查询 | [Tapas](../../hotspots/dict_lookup.tap) · [Python](../../hotspots/dict_lookup.py) · [Lua](../../hotspots/dict_lookup.lua) | 5,896.0 µs | 15,913.0 µs | 1,906.0 µs | 0.371× | 3.093× |
| `dict_update` | 内核：已有整数键的标量覆盖 | [Tapas](../../hotspots/dict_update.tap) · [Python](../../hotspots/dict_update.py) · [Lua](../../hotspots/dict_update.lua) | 4,869.0 µs | 20,581.0 µs | 1,984.0 µs | 0.237× | 2.454× |
| `dict_delete` | 内核：逐键删除已有整数键 | [Tapas](../../hotspots/dict_delete.tap) · [Python](../../hotspots/dict_delete.py) · [Lua](../../hotspots/dict_delete.lua) | 3,668.0 µs | 8,173.0 µs | 773.0 µs | 0.449× | 4.745× |
| `dict_literal` | 内核：小字典构造、读取及回收 | [Tapas](../../hotspots/dict_literal.tap) · [Python](../../hotspots/dict_literal.py) · [Lua](../../hotspots/dict_literal.lua) | 17,115.0 µs | 22,698.0 µs | 29,587.0 µs | 0.754× | 0.578× |
| `dict_counting` | 内核：显式循环：读默认值、加一、写回 | [Tapas](../../hotspots/dict_counting.tap) · [Python](../../hotspots/dict_counting.py) · [Lua](../../hotspots/dict_counting.lua) | 20,694.0 µs | 32,338.0 µs | 5,909.0 µs | 0.640× | 3.502× |
| `tail_recursion` | 语义：尾调用与栈/帧管理 | [Tapas](../../hotspots/tail_recursion.tap) · [Python](../../hotspots/tail_recursion.py) · [Lua](../../hotspots/tail_recursion.lua) | 1,821.0 µs | 4,747.0 µs | 714.0 µs | 0.384× | 2.550× |
| `branch_logic` | 内核：固定状态分布的短路条件链 | [Tapas](../../hotspots/branch_logic.tap) · [Python](../../hotspots/branch_logic.py) · [Lua](../../hotspots/branch_logic.lua) | 22,453.0 µs | 28,374.0 µs | 11,525.0 µs | 0.791× | 1.948× |
| `constraint_probe` | 内核：数独约束索引、比较与提前退出 | [Tapas](../../hotspots/constraint_probe.tap) · [Python](../../hotspots/constraint_probe.py) · [Lua](../../hotspots/constraint_probe.lua) | 70,214.0 µs | 80,512.0 µs | 33,878.0 µs | 0.872× | 2.073× |
| `dp_row` | 内核：有效 LCS 状态上的单行更新 | [Tapas](../../hotspots/dp_row.tap) · [Python](../../hotspots/dp_row.py) · [Lua](../../hotspots/dp_row.lua) | 41,837.0 µs | 31,009.0 µs | 31,429.0 µs | 1.349× | 1.331× |
| `record_update` | 表示：固定字段访问、累加与对象布局 | [Tapas](../../hotspots/record_update.tap) · [Python](../../hotspots/record_update.py) · [Lua](../../hotspots/record_update.lua) | 10,912.0 µs | 12,909.0 µs | 7,490.0 µs | 0.845× | 1.457× |
| `recursive_loops` | 任务：递归树内的短序列求和 | [Tapas](../../hotspots/recursive_loops.tap) · [Python](../../hotspots/recursive_loops.py) · [Lua](../../hotspots/recursive_loops.lua) | 15,980.0 µs | 12,069.0 µs | 8,476.0 µs | 1.324× | 1.885× |
| `string_count` | 任务：完整字符频数统计 | [Tapas](../../hotspots/string_count.tap) · [Python](../../hotspots/string_count.py) · [Lua](../../hotspots/string_count.lua) | 25,033.0 µs | 14,276.0 µs | 21,008.0 µs | 1.754× | 1.192× |
| `substring_count` | 任务：切片、散列及重叠子串计数 | [Tapas](../../hotspots/substring_count.tap) · [Python](../../hotspots/substring_count.py) · [Lua](../../hotspots/substring_count.lua) | 23,459.0 µs | 23,851.0 µs | 18,396.0 µs | 0.984× | 1.275× |
| `tree_build` | 表示：递归构造只读树及生命周期 | [Tapas](../../hotspots/tree_build.tap) · [Python](../../hotspots/tree_build.py) · [Lua](../../hotspots/tree_build.lua) | 30,506.0 µs | 26,121.0 µs | 71,700.0 µs | 1.168× | 0.425× |
| `while_search` | 任务：首个匹配位置的线性搜索 | [Tapas](../../hotspots/while_search.tap) · [Python](../../hotspots/while_search.py) · [Lua](../../hotspots/while_search.lua) | 54,064.0 µs | 28,710.0 µs | 28,199.0 µs | 1.883× | 1.917× |

本组几何平均数：Tapas/Python **0.639×**（27 项）, Tapas/Lua **1.238×**（27 项）。

内核、任务、表示与语义案例测量的层面不同，不能把本组均值解释成纯 VM 性能。逐项结论见[热路径审查](../../hotspots/AUDIT.md)。

### Python 结构对照

主表保留惯用默认写法；本表另测显式循环或常规对象表示。它有助于区分原生批量操作、迭代方式及对象布局的影响，但仍不等于单条 VM 指令成本。两种 Python 写法的输出必须一致；结构对照不计入任何几何平均。

| 案例 | Python 默认 | Python 结构对照 | Tapas/默认 | Tapas/结构对照 |
| --- | ---: | ---: | ---: | ---: |
| `dp_row` | 31,009.0 µs | 32,799.0 µs | 1.349× | 1.276× |
| `record_update` | 12,909.0 µs | 14,248.0 µs | 0.845× | 0.766× |
| `recursive_loops` | 12,069.0 µs | 14,378.0 µs | 1.324× | 1.111× |
| `string_count` | 14,276.0 µs | 23,997.0 µs | 1.754× | 1.043× |
| `substring_count` | 23,851.0 µs | 23,153.0 µs | 0.984× | 1.013× |
| `tree_build` | 26,121.0 µs | 47,333.0 µs | 1.168× | 0.644× |
| `while_search` | 28,710.0 µs | 60,851.0 µs | 1.883× | 0.888× |

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
原生 heap 包与 Dijkstra、Huffman 的写法 A/B 见 [heap 实验](../../hotspots/HEAP.md)；`source_ab_script.py` 比较同一运行时上的两份等价 Tapas 源码。
通用 fill/count 的 sieve 源码 A/B 与 Python count 写法选择见 [批量操作实验](../../hotspots/BULK.md)。
性能门槛不属于常规测试，因为 CPU 负载、电源状态、编译器和 Python 版本都会影响结果；发布比较结果时应同时记录运行环境。

## 说明

这些程序用于比较各实现执行语义等价任务时的性能，不代表大型应用的完整性能。
各语言使用语义等价的惯用实现；内建批量操作和数据结构带来的优势属于比较结果的一部分。
比较脚本核对固定负载输出；verify_semantics.py 额外检查小输入与完整结果序列。
结果会受系统负载、电源状态、编译器版本、Python 版本和 Lua 版本影响。
更新 VM 或运行环境后，应使用上文「用法」中的命令重新生成。
