# heap 包与算法案例的 A/B

新增的 `heap` 是普通 List 上的通用原生二叉堆，仅提供 `heapify`、`push`、
`pop`、`peek`。支持默认数值顺序及自定义严格弱序比较函数，不包含图、字符
频率或 benchmark 专用逻辑。接口、复杂度和错误行为见
[包说明](../../../src/stdlib/heap/README.md)。注册方式与其他原生标准库包一致，
没有增加核心对象类型、字节码或 VM 特殊分支。

## 实验方法

下面比较的是**同一个二进制执行修改前后的两份 Tapas 源码**，不是旧版运行时
与新版运行时的比较。基线来自引入 heap 前的工作区，已经包含之前的惯用写法
修订，不能用 Git HEAD 的更旧版本替代。完整源码保存在各 JSON 的 `sources`
字段中，连同二进制与源码哈希、构建参数、校验和及全部原始样本。

每轮先做一次不计入统计的预热，之后交替 A/B 顺序，三轮反转起始顺序。
完整算法每轮 9 对样本，建树内核每轮 7 对。报告逐对耗时比的中位数，B/A
越低越好。测量期间没有并行构建或测试。

| 比较内容 | 普通 Release B/A | Full LTO + PGO B/A | 选择 |
|---|---:|---:|---|
| Dijkstra 完整案例 | 0.416 | 0.404 | 使用原生堆 |
| Huffman 完整案例 | 1.002 | 0.993 | 完整任务没有可确认的改善或退化 |
| Huffman 建树内核 | 0.818 | 0.814 | 使用原生堆，建树部分有稳定收益 |

原始数据：
[Dijkstra Release](experiments/heap_dijkstra_release.json)、
[Dijkstra PGO](experiments/heap_dijkstra_pgo.json)、
[Huffman Release](experiments/heap_huffman_release.json)、
[Huffman PGO](experiments/heap_huffman_pgo.json)、
[Huffman 内核 Release](experiments/heap_huffman_kernel_release.json)、
[Huffman 内核 PGO](experiments/heap_huffman_kernel_pgo.json)。

## 为什么保留

Dijkstra 从反复扫描所有未访问顶点，改为二叉最小堆。发现更短距离时插入新
记录，弹出时跳过过期记录，保持相同的 Dijkstra 算法、输入图、起点和最终
距离。比较函数只读取记录的距离字段；相同距离的访问顺序不会改变距离结果。
该改动消除了稀疏图上反复线性选择最小值的成本。Release 三轮 B/A 为
0.413、0.416、0.416；PGO 为 0.406、0.406、0.400，收益在两种构建中一致。

Huffman 保留字符计数、构造完整树及递归计算带权路径长度，只把反复扫描并
删除最小节点改为堆操作。完整任务仍主要消耗在字符频数统计。Release 三轮
B/A 为 1.023、0.996、0.975，PGO 为 0.980、0.990、1.015；两组四分位区间
均跨过 1，不能把其中的小幅回退或领先当成稳定变化。

为区分统计与建树，额外用相同的八个符号、每个频数 5,000，重复构造完整树
10,000 次并归约全部结果。这个独立内核排除了字符计数，且没有复用树或预存
答案。两种构建的三轮 B/A 都稳定在约 0.81–0.82，说明实际被替换的建树路径
得到改善，而完整任务掩盖了这部分收益。因此保留堆实现，但不声称完整
Huffman 案例已有显著提升。此内核不计入固定 16 项综合算法或基础热路径表。

批量内核的单次建树约 10 微秒，完整案例约 2 毫秒；缓存热度不同，不能直接
相减，但这个量级解释了为何建树部分的改善在完整任务中难以超过测量波动。

Python 与 Lua 的参考仍保留各自已验证的惯用实现，不要求逐行照搬 Tapas。
算法合同见 [SEMANTICS.md](../SEMANTICS.md)。

## 正确性与复现

Release、PGO 的 CTest 均 52/52 通过，ASAN 的核心、数据结构、语言回归测试
3/3 通过。堆回归同时覆盖源码与字节码入口、空堆、0–128 个元素、重复值、
负数、混合 Int/Float、大整数边界、最大堆、记录载荷、别名和引用生命周期、
嵌套比较函数及错误定位。

完整三语言校验为 43 组案例与 14 组小输入探针；Dijkstra 探针核对完整距离
向量，包括不可达顶点、缺失邻接表、零权边、距离相同及过期队列记录。
另有 40 个热点完整行/频数检查。PGO 仍只用 27 个热点训练，不使用完整综合
算法训练。两份 benchmark README 由新二进制的同批七次测量生成。

源程序 A/B 可使用新增的通用脚本：

```sh
python3 test/benchmarks/source_ab_script.py build-release/bin/tapas \
  /tmp/dijkstra-before.tap test/benchmarks/dijkstra.tap \
  --rounds 3 --runs 9 --json /tmp/dijkstra-ab.json
```

其中 `/tmp/dijkstra-before.tap` 的内容取自 Dijkstra JSON 中的
`sources.baseline.source`。Huffman 完整任务及内核同样可从对应 JSON 还原两端。
每次运行记录并检查源码和二进制哈希，避免把不同版本的输入混入同一结果。
