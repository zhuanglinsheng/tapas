![Tapas](docs/Logo.png)

# Tapas

简体中文 | [English](README_en.md) | [项目主页](README.md)

Tapas 是一个面向复杂业务的规则驱动测试系统。
业务规则只需定义一次，就能用于生成测试输入和检查执行结果。

## 一个简单的例子

**场景：** 仓库有 7 件商品，每笔订单最多购买 6 件。你准备测试“一笔订单清空库存”，这组要求能同时满足吗？

把限购规则和库存变化写下来，让 Tapas 检查这个测试场景是否可行：

```tapas
let StockChange = rule (
        before: Int,   // 卖出前的库存
        sold: Int,     // 本次卖出的数量
        after: Int     // 卖出后的库存
) {
    "每笔订单可以购买 1 至 6 件":
        sold in rules::range(1, 6)

    "卖出数量不能超过当前库存":
        sold <= before

    "剩余库存必须等于原库存减去卖出数量":
        after == before - sold
}
```

这条规则描述了每次销售都要遵守的要求。接下来固定销售前后的库存，构造“一笔订单清空库存”的测试场景：

```tapas
// 要求：一笔订单清空库存
let trial_1 = rules::restrict(StockChange, 'before': 7, 'after': 0)

// 执行检测
let test_result_1 = solve::hold(trial_1)

// 查看查询结论和参与冲突的条件
pprint('status    = ', test_result_1::status)
pprint('conflicts = ', test_result_1::conflicts)
```
<pre class='Tapas-Return'>
status    = unsat
conflicts = [{
    rule : Rule #2[Int, Int, Int],
    condition : (sold in rules::range(1, 6))
}, {
    rule : Rule #2[Int, Int, Int],
    condition : (after == (before - sold))
}, {
    rule : Rule #3[Int, Int, Int],
    condition : (before == 7)
}, {
    rule : Rule #3[Int, Int, Int],
    condition : (after == 0)
}]
</pre>

查询结果是 **`unsat`（无解）**。清空库存需要卖出 7 件，但每笔订单最多购买 6 件，这两项要求发生了冲突。`conflicts` 会列出参与冲突的规则和条件，帮助定位原因。

将目标改为剩下 1 件，就能找到卖出 **6 件**的输入。业务规则没有改变，只调整了本次测试的目标。这时再把输入交给库存系统执行，就可以用同一条规则检查实际结果。

```tapas
// 要求：库存还剩下 1 件
let trial_2 = rules::restrict(StockChange, 'before': 7, 'after': 1)

// 执行检测
let test_result_2 = solve::hold(trial_2)

// 查看查询结论和参与冲突的条件
pprint('status    = ', test_result_2::status)
pprint('conflicts = ', test_result_2::conflicts)
```
<pre class='Tapas-Return'>
status    = sat
conflicts = []
</pre>

## 系统特点

- 同一份规则用于生成与检查。输入条件、成功与拒绝时的状态变化可以组合成一个业务模型，供不同测试复用。
- 按测试目标寻找输入。可以要求“恰好清空库存”，也可以固定身份与确认信息，寻找“只因额度不足而被拒绝”的请求。
- 场景通过限制来调整。固定值、整数区间和离散候选可以继续叠加；条件无法同时满足时，查询返回无解，并可列出相关 Rule 和冲突条件。

Tapas 适用于订单、支付、权限和工作流等有状态业务，也面向 AI Agent 与仿真环境。当前示例由测试脚本连接输入生成、实际执行和结果检查，多步状态探索与失败缩减尚未实现。

## 继续了解

| 想了解的内容 | 从这里开始 |
| --- | --- |
| 完整的业务测试如何编写 | [换货业务教程](examples/retail/README.md)：从订单建模到正确拒绝、缺陷检测和多变量输入生成 |
| 怎样求解规则、读取失败原因 | [可行性示例](examples/solve/feasibility.tap)与[冲突诊断示例](examples/solve/diagnostics.tap)：两个可独立运行的小程序 |
| 代码的基本写法 | [Tapas 入门](docs/examples/Basics_zh.md)：变量、函数和控制流；[模块与目录包](docs/examples/modules/README.md)：跨文件组织代码 |
| 还有哪些可运行案例 | [示例目录](examples/README.md)：求解、业务测试与通用算法 |

## 构建、测试与安装

准备支持 C23 的编译器、CMake 3.21 或更高版本、GNU Readline 和 Python 3，然后在仓库根目录构建并运行测试：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

将 Tapas 安装到用户目录：

```sh
cmake --install build --prefix "$HOME/.tapas"
export PATH="$HOME/.tapas/bin:$PATH"
```

运行求解功能前，请确保系统默认的 `python3` 已安装 OR-Tools。下面的命令会运行一个库存分配示例并打印求解结果：

```sh
tapas examples/solve/feasibility.tap
```

构建工具的安装方法见[使用说明](docs/Usage_zh.md)，求解环境与支持范围见 [solve 包说明](src/stdlib/solve/README.md)。普通规则检查不需要 Python 或 OR-Tools。

## 编辑与接入

[Visual Studio Code 扩展](https://marketplace.visualstudio.com/items?itemName=tapas-language.tapas-language)提供语法高亮、诊断、补全、类型悬停、跳转、运行和格式化。扩展需要单独安装 Tapas Core；安装与配置见[扩展文档](editors/vscode/README_zh.md)。

Tapas 可以作为脚本运行，也可以通过公共 C API 嵌入其他程序。会话创建、C 函数注册和业务数据接入见 [C 交互](docs/Foreign_zh.md)。

## 文档

- [求解接口](src/stdlib/solve/README.md)：可行性查询、冲突诊断、支持范围和运行配置。
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
