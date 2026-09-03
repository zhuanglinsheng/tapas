# Tapas Examples

简体中文 | [English](README_en.md) | [项目主页](../README.md)

本目录收录可直接执行的独立 `.tap` 程序，侧重展示完整实现。更短的基础语法和
模块示例位于 [`docs/examples`](../docs/examples)。

从项目根目录运行任一示例：

```sh
build/bin/tapas examples/fibonacci.tap
```

| 文件 | 内容 | 主要展示的 Tapas 能力 |
| --- | --- | --- |
| [`fibonacci.tap`](fibonacci.tap) | 递归和迭代 Fibonacci | 递归调用、循环、列表 |
| [`sorting.tap`](sorting.tap) | 七种经典排序算法 | 高阶函数、切片、原地修改、递归 |
| [`binary_search.tap`](binary_search.tap) | 二分查找 | 循环、边界处理、提前返回 |
| [`euclidean_algorithm.tap`](euclidean_algorithm.tap) | 最大公约数、最小公倍数和扩展欧几里得 | 整数运算、多值结果 |
| [`sieve_of_eratosthenes.tap`](sieve_of_eratosthenes.tap) | 埃拉托斯特尼筛法 | 布尔列表、嵌套循环 |
| [`breadth_first_search.tap`](breadth_first_search.tap) | 图的广度优先遍历 | 字典、队列、成员判断 |
| [`longest_common_subsequence.tap`](longest_common_subsequence.tap) | 最长公共子序列 | 动态规划、二维数组、字符串 |
| [`newton_method.tap`](newton_method.tap) | 牛顿迭代法 | 浮点运算、数学函数、数值收敛 |

所有顶层示例都会作为 CTest 测试运行。新增示例应保持独立、使用确定性输入，并且
不依赖网络、交互输入或预先生成的 `.tapc` 文件。
