![Tapas](docs/Logo.png)

# Tapas

简体中文 | [English](README_en.md) | [项目主页](README.md)

Tapas 是一门以表达式为核心、注重程序可读性并具有结构化类型系统的编程语言，正在发展为面向复杂系统测试的可编程声明语言。
它的目标是让领域规则成为可以动态构造、组合和解释的一等值，使验证器、生成器和其他解释器能够从同一份规则定义中派生合法状态、行为检查、边界场景、冲突解释和失败缩减。

Tapas 当前主要用于描述复杂有状态系统和 AI Agent 业务环境中的测试规则。
它强调让代码直接对应业务概念和约束，使读者不必先了解规则如何执行、生成或求解，也能理解测试要验证什么。

## 语言特色

- 业务规则可以像普通数据一样保存、传递和组合，同一份规则既可以直接用于检查，也可以交给不同工具分析和解释。
- 灵活的类型系统可以在编译期发现错误，也允许程序在运行时读取和组合类型，便于描述复杂的业务状态。
- 以表达式为核心的简洁语法支持一等函数、闭包、递归、常用容器和稠密数组。
- 语言服务器和 Visual Studio Code 扩展提供实时诊断、补全、悬停信息和跨模块符号跳转。

Tapas 使用 C23 实现，源码被编译为字节码并由栈式虚拟机执行。
Tapas 程序可以作为脚本运行，也可以通过交互式 REPL 或 Markdown 代码块执行。
公共 C API 用于将 Tapas 运行时嵌入其他程序。

## 示例

[Tapas 入门示例](docs/examples/Basics_zh.md)用一个短小且可执行的程序串联变量、列表、函数和控制流，直观呈现语言的基本风格。
下面是一些完整示例，涵盖斐波那契数列、经典排序、图搜索和动态规划等常见问题：

| 示例 | 内容与展示重点 |
| --- | --- |
| [Fibonacci](examples/general/fibonacci.tap) | 用递归和迭代生成 Fibonacci 数列，展示函数递归、循环与列表 |
| [排序算法](examples/general/sorting.tap) | 实现冒泡、选择、插入、希尔、归并、快速和堆排序，展示切片、高阶函数与原地修改 |
| [二分查找](examples/general/binary_search.tap) | 在有序列表中查找目标，展示循环边界和提前返回 |
| [欧几里得算法](examples/general/euclidean_algorithm.tap) | 计算最大公约数、最小公倍数和 Bézout 系数，展示整数运算与多值结果 |
| [埃氏筛](examples/general/sieve_of_eratosthenes.tap) | 筛选指定范围内的素数，展示布尔列表和嵌套循环 |
| [广度优先搜索](examples/general/breadth_first_search.tap) | 遍历字典表示的图，展示队列、字典和成员判断 |
| [最长公共子序列](examples/general/longest_common_subsequence.tap) | 用动态规划求两个字符串的公共子序列，展示二维数组与结果回溯 |
| [牛顿法](examples/general/newton_method.tap) | 迭代求平方根和非线性方程的根，展示浮点计算与数学函数 |

更多例子参见[语法示例集](docs/examples/syntax)；模块组织方式参见[模块与目录包](docs/examples/modules/README.md)。

## 构建、测试与安装

Tapas 需要支持 C23 的编译器、CMake 3.21 或更高版本和 GNU Readline。
在项目根目录构建并测试 Release 版本，然后安装到用户目录：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build --prefix "$HOME/.tapas"
```

将安装目录加入`PATH`后，可以验证版本并运行示例：

```sh
export PATH="$HOME/.tapas/bin:$PATH"
tapas --version
tapas examples/general/fibonacci.tap
```

依赖安装、安装、REPL、字节码和模块等更多用法参见[使用说明](docs/Usage_zh.md)。

## Visual Studio Code 扩展

Tapas 提供独立的薄版 VS Code 扩展，支持语法高亮、实时诊断、悬停类型信息、代码补全、定义跳转、引用查找、重命名、工作区模块分析、运行和格式化。扩展不包含 Tapas Core。

请先按上一节把 Tapas Core 安装到`$HOME/.tapas`。可以从 Visual Studio Code Marketplace 安装 [Tapas](https://marketplace.visualstudio.com/items?itemName=tapas-language.tapas-language)，也可以从 GitHub Release 下载独立 VSIX：

```sh
code --install-extension tapas-language.tapas-language
```

扩展依次使用显式配置、`$HOME/.tapas/bin`和系统`PATH`查找`tapas`与`tapas-language-server`，不会执行当前工作区中的二进制。从源码调试扩展、开发目录安装、测试、打包及详细配置参见[VS Code 扩展文档](editors/vscode/README_zh.md)。

## 在 C 程序中嵌入 Tapas

公共头文件位于 `include/tapas`。
以下程序通过会话接口执行一段 Tapas 源码：

```c
#include "tapas/tapas.h"

int main(void)
{
    tsession *session = tsession_new();

    tsession_execute_str(session, "print(6 * 7)", 1);
    tsession_free(session);
    return 0;
}
```

接口、链接方式和扩展类型说明参见 [C 交互](docs/Foreign_zh.md)。

## 文档

- [使用说明](docs/Usage_zh.md)：构建、命令行选项、脚本、字节码、Markdown 执行和模块路径。
- [语言规范](docs/Syntax_zh.md)：语法、值类别、运算符、语句、函数、模块和数组。
- [标准库](docs/Stdlib_zh.md)：根内建函数、原生包和随发行版提供的源码包。
- [代码风格](docs/Style_zh.md)：缩进、函数与控制流布局，以及 `format` 包。
- [类型系统设计](docs/TypeSystem_zh.md)：编译期标注、Type 值、结构 Type 和 `types` 包。
- [Rule](docs/Rules_zh.md)：Rule 字面量、规则组合、标准检查和 Rule IR。
- [C 交互](docs/Foreign_zh.md)：嵌入会话、注册 C 函数、值操作和复合类型扩展。
- [运行机制](docs/Mechanism_zh.md)：编译器、字节码、虚拟机、环境和引用计数。
- [性能基准](test/benchmarks/Results_zh.md)：Tapas 与 Python 在 VM 热路径、函数调用、递归、列表访问和埃氏筛等负载上的逐项比较。

## 许可证

Tapas 使用 MIT 许可证发布，详见 [LICENSE](LICENSE)。

## 联系方式

欢迎通过 Issue 反馈问题，也欢迎提交 Pull Request。
联系邮箱：<linsheng.z@outlook.com>。
