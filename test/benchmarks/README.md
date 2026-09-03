# VM 性能基准

简体中文 | [English](README_en.md) | [项目主页](../../README.md)

本目录使用逐项对应的 Tapas 和 Python 程序比较完整算法与 VM 基础热路径的性能。

- 综合算法包括递归 Fibonacci、埃氏筛、归并排序、N 皇后、最长公共子序列和矩阵乘法，覆盖递归、回溯、动态规划、容器访问和多层循环。
- 基础热路径分别测试整数控制流、浮点运算、普通函数调用、尾递归、嵌套分支和列表访问，主要用于定位 VM 的具体开销。

两种实现使用相同的算法和输入，并以进程 CPU 时间记录实际负载，不计进程启动、源码加载和编译时间。
每组程序还会输出计算结果；结果不一致时，比较立即失败。

测试应使用 Release 构建。
脚本先预热一次，再比较七次运行的中位数：

```sh
python3 test/benchmarks/compare_python.py build-release/bin/tapas
```

要求每一项都快于 Python 时，使用：

```sh
python3 test/benchmarks/compare_python.py \
    build-release/bin/tapas --require-faster
```

以下命令使用每项 11 次有效运行生成或更新中英文结果文档：

```sh
python3 test/benchmarks/compare_python.py \
    build-release/bin/tapas --runs 11 \
    --markdown test/benchmarks/Results_zh.md

python3 test/benchmarks/compare_python.py \
    build-release/bin/tapas --runs 11 \
    --markdown test/benchmarks/Results_en.md
```

当前记录见[中文结果](Results_zh.md)和[英文结果](Results_en.md)。
性能门槛不属于常规测试，因为 CPU 负载、电源设置、编译器和 Python 版本都会影响结果。
发布比较结果时应同时记录运行环境。
