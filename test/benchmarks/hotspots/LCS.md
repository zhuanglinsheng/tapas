# LCS 写法优化

当前源码已恢复内层 `let above`，并在每行缓存左侧字符。字符混合比较、局部变量初始化与清理的通用优化及最新 A/B 见 [值路径优化](VALUE_PATHS.md)。以下记录保留早期运行时上的外移实验；其中跨语言数值为历史批次，当前值以主 README 为准。

早期一轮仅调整 [Tapas LCS 源码](../longest_common_subsequence.tap)，保持双行动态规划算法、输入和计时范围不变，时间复杂度仍为 O(mn)，额外空间为 O(n)。

## 早期保留的改写

- 在两层循环外声明 `above`，每个单元只更新它，减少循环内局部槽位的创建与清理。每次读取前都会执行 `above = previous[j]`，不会依赖上一单元遗留的值。
- 缓存 `row_size = right.len() + 1`，用于两行初始化、内层循环范围和结果列索引。输入字符串在函数内不变。
- 保留已有的 `diagonal`、`preceding` 缓存、两行交换以及直接的字符串下标比较。

## 源码 A/B

A 为本轮修改前的源码，B 为上述改写。两者使用同一个 Release 可执行文件，交替运行，测量期间没有并行构建或测试。这里的 A 并非 Git HEAD；完整源码与哈希保存在实验 JSON 中。

| 项目 | 结果 |
| --- | ---: |
| A 中位耗时 | 13,147 µs |
| B 中位耗时 | 12,606 µs |
| 配对 B/A 中位数 | 0.960 |
| 配对 B/A 四分位区间 | 0.952–0.971 |
| 五轮 B/A 中位数 | 0.965 / 0.957 / 0.959 / 0.966 / 0.957 |

共 5 轮，每轮每个版本预热一次、正式测量 11 次；所有校验和均为 1480。五轮均有收益，配对中位耗时约降低 4.0%。

[初步筛选](experiments/lcs_above_scope_screen.json)比较了每行复用 `above`、两层循环外复用，以及后者加行长度缓存。[最终 A/B](experiments/lcs_above_scope_final_ab.json)包含全部样本、运行环境、二进制哈希和两个版本源码。使用 `source_ab_script.py`，将 JSON 中的基线源码恢复到临时文件后，可复测：

```sh
python3 test/benchmarks/source_ab_script.py build-release/bin/tapas \
  /tmp/lcs_baseline.tap test/benchmarks/longest_common_subsequence.tap \
  --runs 11 --rounds 5 --json /tmp/lcs_ab.json
```

## 正确性与三语言结果

88 组输入与独立的穷举子序列实现对照全部通过，覆盖空字符串、相同字符串、重复字符、长度不等和无公共字符，以及固定随机种子的 80 组短字符串。

随后使用现有生成脚本完整刷新三语言性能表，每项每种语言预热一次、正式测量 7 次；各语言结果校验通过。LCS 本批次中位耗时如下：

| Tapas | Python 3.14 | Lua | Tapas/Python | Tapas/Lua |
| ---: | ---: | ---: | ---: | ---: |
| 13,052 µs | 10,104 µs | 12,066 µs | 1.292× | 1.082× |

三语言表与源码 A/B 属于不同测量批次，改写收益应以配对 A/B 为准。完整结果见[中文性能表](../README.md)、[英文性能表](../README_en.md)和[原始数据](comparison_results.json)。两份 README 的生成模板也已加入本记录入口。
