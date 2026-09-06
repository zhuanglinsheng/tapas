# Tapas 示例

简体中文 | [English](README_en.md) | [项目主页](../README.md)

这里可以从短小的程序开始了解 Tapas，也可以通过换货业务学习如何用同一份 Rule 检查执行结果、生成测试输入。

如果还不熟悉 Tapas 的写法，先读[语言入门](../docs/examples/Basics_zh.md)。想了解规则求解，可以先运行 [solve 示例](solve)，再阅读[换货业务教程](retail/README.md)。

## 运行示例

先按[项目构建说明](../README.md#构建测试与安装)构建 Tapas。下面的命令都在仓库根目录执行，使用本次构建得到的程序：

```sh
build/bin/tapas examples/general/fibonacci.tap
```

`general` 示例只需要 Tapas。`solve` 和 `retail` 还需要 Python 与 OR-Tools，请先按 [solve 包的依赖说明](../src/stdlib/solve/README.md#依赖)准备运行环境。

## 从一条规则开始求解

[solve 示例](solve)用两个独立程序介绍求解与冲突诊断。

第一个程序要把七个库存单位分配给新订单和已有预留。它用 `rules::restrict` 将预留数量固定为四，再调用 `solve::hold` 寻找满足规则的其他参数：

```sh
build/bin/tapas examples/solve/feasibility.tap
```

结果中的 `status` 为 `sat`，表示找到了满足条件的解；`witness` 保存这组参数对应的 Rule 实例。示例还会检查求出的实例是否符合原规则。

第二个程序给出互相矛盾的条件，展示怎样读取冲突信息：

```sh
build/bin/tapas examples/solve/diagnostics.tap
```

例如，数量不能同时大于等于八、小于等于四。查询返回 `unsat`，并在 `conflicts` 中列出相关 Rule 和条件。程序也展示了 `unsupported`：它表示当前求解器不能处理该表达式，不能据此认为规则无解。

## 用规则测试换货业务

[换货业务教程](retail/README.md)从顾客 Yusuf 的订单开始：他收到一把键盘，希望换成同款的另一种规格，并支付差价。零售系统需要核对身份、确认信息、库存和支付额度，再决定接受或拒绝。

示例用 Rule 描述这些业务要求，用模拟器执行换货。测试将前状态、请求、实际输出和后状态固定到顶层 `ExchangeModel`，再通过 `solve::hold` 检查这次处理是否符合要求。

可以先运行正常换货：

```sh
build/bin/tapas examples/retail/test_valid_exchange.tap
```

然后依次尝试下面的实验。每个文件都可以单独运行，不需要先执行其他测试。

| 实验 | 要检查的问题 |
|---|---|
| [正常换货](retail/test_valid_exchange.tap) | 接受请求后，收费和订单变化是否正确？ |
| [拒绝换货](retail/test_rejected_exchange.tap) | 请求不符合条件时，系统是否拒绝、不收费并保留原订单？ |
| [错误的状态转移](retail/test_invalid_transition.tap) | 模拟器遗漏同商品检查、错误地把键盘换成温控器时，模型能否发现？ |
| [生成允许换货的输入](retail/test_generate_valid_exchange.tap) | 只给出额度范围，能否找到允许换货的额度？收紧范围后是否无解？ |
| [生成应当拒绝的输入](retail/test_generate_rejected_exchange.tap) | 能否找到不足以支付差价的额度，再检查模拟器是否正确拒绝？ |
| [同时限制多个参数](retail/test_generate_joint_inputs.tap) | 额度、认证用户和确认目标一起变化时，能否生成指定情形的输入？ |

例如，运行最后一组实验：

```sh
build/bin/tapas examples/retail/test_generate_joint_inputs.tap
```

这里先用一次查询生成输入，再将输入交给模拟器，最后用另一次查询检查实际转移。实验复用原有业务规则，只改变本次查询的限制。

阅读结果时，要区分业务结果与查询结果：正常换货和正确拒绝的执行记录都应得到 `sat`；故意制造的错误转移应得到 `unsat`，说明测试检出了缺陷。生成输入时的 `unsat` 则表示本次限制下找不到符合要求的输入。具体代码和结果解释见教程。

## 通用编程示例

`general` 中的程序使用固定输入展示常见算法，适合熟悉函数、循环和容器的写法。每个文件都可以像开头的 Fibonacci 示例一样直接运行。

| 示例 | 内容 | 可以了解的写法 |
|---|---|---|
| [Fibonacci 数列](general/fibonacci.tap) | 递归与迭代 | 函数调用、循环、列表 |
| [排序](general/sorting.tap) | 七种排序算法 | 高阶函数、切片、原地修改、递归 |
| [二分查找](general/binary_search.tap) | 在有序列表中查找 | 循环边界、提前返回 |
| [欧几里得算法](general/euclidean_algorithm.tap) | 最大公约数、最小公倍数、扩展欧几里得 | 整数运算、返回多个结果 |
| [埃氏筛](general/sieve_of_eratosthenes.tap) | 筛选素数 | 布尔列表、嵌套循环 |
| [广度优先搜索](general/breadth_first_search.tap) | 遍历图 | 字典、队列、成员判断 |
| [最长公共子序列](general/longest_common_subsequence.tap) | 动态规划与结果回溯 | 二维数组、字符串 |
| [牛顿法](general/newton_method.tap) | 迭代求根 | 浮点运算、数学函数 |

更短的语法片段见[语法示例](../docs/examples/syntax)；多个文件如何相互引用，见[模块与目录包](../docs/examples/modules/README.md)。
