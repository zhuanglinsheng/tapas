# Lua 综合算法写法审查

2026-10-18。复核固定 16 项综合算法，使用报告中的 Lua 5.5.1，仅使用语言及标准库。结论是：此前的 Lua 实现确实仍有明显优化空间，不能把旧报告中的差距全部归因为 Lua 的能力限制。本轮保留 11 项改进，三语言正式结果由最终源码统一重新测量。

最终同批结果中，Lua 在 16 项里快于 Python 的有 12 项，Lua/Python 耗时比几何平均为 **0.849×**，旧报告为约 **1.107×**。因此“这套综合算法里 Lua 平均略慢于 Python”的旧结论已不适用于当前源码。Tapas/Python 为 **0.935×**，Tapas/Lua 为 **1.102×**；没有通过既有的 Tapas/Python ≤0.90 目标，如实保留该结果。

这里的目标是选择有证据支持的较快实现，不声称穷举了所有等价程序或证明全局最优。后续发现更快的合理候选，仍应继续比较和修订；不能为了维持任何语言的排名拒绝候选。相同的预处理和工作区复用方式也允许用于 Python、Tapas，最终取舍仍需各自实测。

## 测量与选择

初筛覆盖全部 16 项，共 25 条候选记录，每条两轮、每轮五对有效运行；补充归并和线搜索候选四条记录，每条两轮、每轮七对。每轮另丢弃一对预热，轮内、轮间反转 A/B 顺序。全部使用源码自身输出的进程 CPU 时间，不包含启动和加载。

对每项初筛 B/A 中位数小于 0.97、且每轮都小于 1 的最快候选，另做三轮、每轮十一对的独立确认。保留确认 B/A 小于 0.98 且三轮均小于 1 的候选。补充试验中更快的线搜索候选替换先前的坐标缓存方案。B/A 为候选/原版耗时，越小越快。小于阈值的规则用于避免为不确定的微小收益增加代码，不是统计显著性证明。

测量串行执行，期间没有并行构建或测试。每次运行核对固定负载校验和。原始记录包含完整前后源码、SHA-256、Lua 二进制哈希、逐次时间、配对比值 IQR 和各轮中位数。不同候选的独立测量不可代替最终三语言同批报告。

## 逐项结果

“保留”行列出独立确认值；“不变”行列出初筛候选及其结果，不能将其当作最终实现的耗时变化。

| 算法 | 决定与实现 | B/A | 确认各轮 B/A |
| --- | --- | ---: | --- |
| recursive_fibonacci | 不变；`<= 1` 基例无收益，仍执行双递归 | 1.017 | — |
| sieve | 保留；数值 for 标记倍数，平方根界限只计算一次 | 0.922 | 0.941 / 0.906 / 0.922 |
| merge_sort | 保留；复制输入一次，递归归并复用辅助数组，`table.move` 搬移 | 0.408 | 0.411 / 0.406 / 0.408 |
| n_queens | 保留；等价对角线差值判断，省去逐冲突的 `math.abs` 调用 | 0.697 | 0.699 / 0.697 / 0.699 |
| longest_common_subsequence | 保留；每次调用预解码右串字节，仍为两行逐列 DP | 0.486 | 0.490 / 0.490 / 0.482 |
| matrix_multiply | 不变；省略被覆盖的零初始化未获收益，保留已有右矩阵转置 | 1.018 | — |
| sudoku | 保留；行列扫描后短路执行分组宫检查，避免逐项整除和取模 | 0.759 | 0.761 / 0.759 / 0.759 |
| dijkstra | 保留；数值索引遍历边，直接取出并清空堆尾槽位 | 0.857 | 0.866 / 0.853 / 0.864 |
| optimize | 保留；缓存线搜索坐标，每次线搜索复用试探坐标表 | 0.680 | 0.678 / 0.675 / 0.691 |
| kmp | 保留；每轮预解码模式字节，仍重建前缀表并完整扫描文本 | 0.557 | 0.556 / 0.558 / 0.560 |
| k_nucleotide | 不变；缓存窗口偏移仅为 0.973，未达独立确认筛选线 | 0.973 | — |
| binary_trees | 保留；显式接收两个递归子树，再构造固定双元素 table | 0.842 | 0.849 / 0.842 / 0.838 |
| lru_cache | 不变；缓存命中值未获稳定收益，保留已有链表实现 | 1.001 | — |
| huffman | 不变；gmatch / gsub / 字节键计数分别为 1.069 / 1.266 / 1.071 | ≥1.069 | — |
| fannkuch | 保留；每次调用分配一个工作区，每个排列都完整复制并翻转 | 0.647 | 0.650 / 0.652 / 0.646 |
| n_body | 保留；成对更新时缓存当前天体不变的坐标和质量 | 0.966 | 0.990 / 0.948 / 0.944 |

归并排序仍递归分割、稳定归并，不修改输入，没有调用 `table.sort`。数独仍按首个空格、数字 1–9 的相同顺序搜索。皇后仍遍历相同行列并逐个检查先前皇后。线搜索针对这里的纯 Rosenbrock 目标函数；浮点表达式及运算顺序不变，不将复用可变坐标的结论外推到保存参数引用的任意回调。构树仍逐次新建全部内部节点并执行叶子递归。N-body 没有改变天体对顺序或浮点运算顺序。

新增字符预处理、辅助数组和工作区的分配均在计时内，每次算法调用重新执行，不跨重复次数缓存结果。Huffman 没有找到更快候选，支持的是本次候选范围内的选择，不能证明任何 Lua 实现都不可能更快。

## 正确性与复现

29 条候选记录全部通过额外的小输入差分检查，不只验证单个校验和：排序覆盖长度 0–100、重复和负数并检查输入不变；LCS 含空串；KMP 含重叠匹配；图测试含缺边、零权边及不同起点；k-mer 检查完整计数字典；构树检查左右内部节点不共享；浮点线搜索比较每次目标函数求值的坐标及结果，N-body 比较 17 位表示的完整状态。差分检查说明候选与原实现一致，独立参考答案另由现有跨语言语义校验器覆盖。

最终源码通过 46 项三语言负载校验及 14 组跨语言小输入内核检查。正式中英文报告和原始样本以同一批七次有效运行重新生成，见 [主报告](../README.md)、[英文报告](../README_en.md) 和 [最终样本](comparison_results.json)。语义校验记录见 [semantics_results.json](semantics_results.json)。基础热点没有套用本轮算法预处理改动，其测量合同保持原样。

报告及 A/B 脚本的 13 项现有测试通过。最终保留的 11 份源码哈希与独立确认、统一重测报告完全一致。

复现归档候选与独立确认（输出写到临时目录，不覆盖已发表证据）：

```sh
python3.14 test/benchmarks/lua_source_audit.py --from-report test/benchmarks/hotspots/experiments/lua_audit_screen.json --replay-all --runs 5 --rounds 2 --json /tmp/lua-screen.json
python3.14 test/benchmarks/lua_source_audit.py --from-report test/benchmarks/hotspots/experiments/lua_audit_screen.json --runs 11 --rounds 3 --json /tmp/lua-confirm.json
python3.14 test/benchmarks/lua_source_audit.py --from-report test/benchmarks/hotspots/experiments/lua_audit_extra_screen.json --runs 11 --rounds 3 --json /tmp/lua-extra-confirm.json
python3.14 test/benchmarks/verify_lua_audit.py test/benchmarks/hotspots/experiments/lua_audit_screen.json test/benchmarks/hotspots/experiments/lua_audit_extra_screen.json --json /tmp/lua-probes.json
python3.14 test/benchmarks/verify_semantics.py build-release/bin/tapas
python3.14 test/benchmarks/compare_script.py build-release/bin/tapas --runs 7 --json test/benchmarks/hotspots/comparison_results.json
```

需要重新生成候选时，给 `lua_source_audit.py` 传入 `--baseline-report test/benchmarks/hotspots/experiments/lua_audit_screen.json`，从原版源码重新生成，而不是对已优化源码重复应用替换。

证据：[首轮初筛](experiments/lua_audit_screen.json)、[首轮确认](experiments/lua_audit_confirm.json)、[补充初筛](experiments/lua_audit_extra_screen.json)、[补充确认](experiments/lua_audit_extra_confirm.json)、[保留源码与样本](experiments/lua_audit_selected.json)、[差分检查](experiments/lua_audit_probes.json)。旧结果保留为 [中文报告](experiments/pre_lua_audit_README.md)、[英文报告](experiments/pre_lua_audit_README_en.md) 和 [原始样本](experiments/pre_lua_audit_comparison_results.json)。
