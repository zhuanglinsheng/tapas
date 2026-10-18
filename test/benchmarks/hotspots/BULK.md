# fill / count 与 sieve 的 A/B

本页保留首次引入 fill/count 的实验；后续 List 快速路径、紧凑序列和 sieve 更新见[本轮逐项优化](COMPACT_AND_SORT.md)。

新增根内建函数 `fill(target, value, indices)`、`count(values, value)`。
fill 接受 IndexSettable 与显式 Iterable 索引，count 接受 Iterable，均不包含筛法专用逻辑。
接口与边界见[标准库](../../../docs/Stdlib_zh.md#批量填充与计数)。

## 同一运行时上的源码对照

使用当前普通 Release 二进制，对同一份原始 sieve 分别仅替换合数标记、仅替换最终计数、同时替换两者。
输入仍为 20,000，结果均为 2,262，初始化和外层候选检查保留不变。
每组预热一次，三轮各九对有效样本，交替执行顺序并在轮次间反转起始顺序。
计时使用程序内部进程 CPU 时间，测量期间没有并行构建或测试。

| 源码变化 | 原版中位数 | 候选中位数 | 配对 B/A | 三轮 B/A |
|---|---:|---:|---:|---|
| 仅 fill | 1.020 ms | 0.370 ms | 0.359 | 0.351 / 0.356 / 0.372 |
| 仅 count | 1.043 ms | 0.758 ms | 0.727 | 0.730 / 0.742 / 0.710 |
| fill + count | 1.054 ms | 0.093 ms | 0.086 | 0.090 / 0.085 / 0.085 |

原始样本、完整源程序与二进制哈希：
[仅 fill](experiments/sieve_fill_ab.json)、
[仅 count](experiments/sieve_count_ab.json)、
[两者组合](experiments/sieve_fill_count_ab.json)。
三次实验各自配对，不能把独立实验的百分比相乘来预测组合收益。

收益来自将逐元素的 VM 解释循环移动到底层：List + Iterator 的 fill 直接写入现有槽位，
纯标量列表无需逐元素引用操作；count 对 Bool 列表直接扫描类型与值。
含复合对象的写入仍维护引用计数和 compo_count；扩展对象仍走能力接口。
这项优化没有变更埃氏筛算法，也没有复用筛选结果。

## Python 参考写法

同时比较完整 Python 筛法中的 sum(bytearray) 和 bytearray.count(1)。
三轮各九对样本，每个样本重复相同完整筛法 100 次并核对结果，按调用次数归一化时间。
本机 CPython 3.14.2 上 count/sum 的配对中位数为 0.325，三轮为 0.333 / 0.321 / 0.330；
因此 Python 参考实现也改为 count(1)，没有继续使用较慢的 sum 作为对照。
完整源程序与逐次样本见[Python 对照](experiments/sieve_python_count_ab.json)。
Tapas 使用通用 List 保存值，Python 使用紧凑 bytearray；批量操作消除了 VM 内层循环，
并不意味着两者的数据布局和剩余成本已经相同。

## 运行时对照与解释边界

[运行时对照](experiments/bulk_runtime_controls.md)使用修改前后的普通 Release 二进制，
对 Fibonacci、字典更新、List 复制、直接调用与 Huffman 的同一份源码进行三轮交替测量。
基线在修改前从 build-release 复制，本次没有修改构建参数；复制路径下没有 CMakeCache，
因此其 JSON build_config 字段为空。
配对中位数分别为 0.985、0.982、0.982、1.007、0.997。
直接调用三轮均略慢（约 0.3%–1.7%），其配对四分位区间为 0.987–1.020，
当前结果不足以确认稳定退化，也不能把其他案例的小幅变化认定为收益。
这些既有 VM 执行路径未修改；新增标准库代码会改变链接布局，而系统测量波动也可能参与，
现有证据不能把这 0.7% 精确归因于其中之一。原始样本保留，不因此恢复能消除实际解释循环的接口。

首轮对照曾因自动字节码缓存暴露注册槽位变化的问题，未生成完整结果文件。
最终将新根绑定追加到注册表末尾，保留已有绑定的槽位，再清理相关生成缓存重新检查和测量。
[早期源码对照](experiments/sieve_bulk_initial_registration.json)只对应修正注册顺序前的二进制，
不能与最终三个源码实验合并。

## 当前报告与正确性

本次主表由当前普通 Release 构建与 Python 3.14 的同批七次测量生成。
此前主表采用完整 LTO + PGO；旧表与原始数据归档于
[中文旧表](experiments/pre_bulk_README.md)、[英文旧表](experiments/pre_bulk_README_en.md)、
[旧测量](experiments/pre_bulk_comparison_results.json)、[旧 sieve 源码](experiments/pre_bulk_sieve_sources.json)。
不能把不同构建的主表直接作为本次 API 的运行时 A/B。

回归覆盖源码和字节码、扩展 Iterable/IndexSettable、重复键、负索引与负/零/极端步长、
空输入、引用共享与释放、Int/Float 区分、NaN、正负零、String 字节与嵌入 NUL，
以及只读目标、错误参数和越界。String 不具备 IndexSettable 的编译期能力判断也已与运行时对齐。
