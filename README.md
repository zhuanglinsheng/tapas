![Tapas](docs/Logo.png)

# Tapas

简体中文 | [English](README_en.md)

Tapas 是一个面向业务约束测试的 DSL 和求解器。它可以将订单、支付、权限、AI Agent 与仿真环境中的约束写成可执行 Rule，用于检查真实系统的输入、输出和状态变化，也用于生成边界数据或故意违反指定条件的测试输入。

Tapas 的核心特点是让检查标准与数据生成模型共用同一份 Rule。Rule 可以组合、限制或反转，使测试者能够按目标构造数据，而不必分别维护规则和样例；当一组条件无法同时成立时，求解结果还会指出相关规则和冲突条件。

## 一个简单的例子：定向生成失败输入

**场景：** 仓库需要测试库存不足的销售请求。每笔订单可以买 1 至 6 件商品，但销售数量不能超过当前库存。

(1) **写下业务规则。** `SaleRequest` 描述所有正常销售请求都应满足的条件：

```tapas
let SaleRequest = rule (
        current_stock: Int,
        sale_quantity: Int
) {
    '每笔订单可以购买 1 至 6 件':
        sale_quantity in rules::range(1, 6)

    '销售数量不能超过当前库存':
        sale_quantity <= current_stock
}
```

(2) **生成库存不足的数据。** 用 `rules::violate` 反转“销售数量不能超过当前库存”，再从目标场景中生成一份数据：

```tapas
let InsufficientStockRequest = rules::violate(
    SaleRequest,
    '销售数量不能超过当前库存'
)

let generated = solve::sample(InsufficientStockRequest, 1, {
    'current_stock': rules::range(0, 7),
    'sale_quantity': rules::range(1, 6)
}, rng = random::generator(random::pcg32_xsh_rr, 42))

let test_input = generated::samples[0]
pprint('test input = ', test_input)
```
<pre class='Tapas-Return'>
test input = {
    sale_quantity : 2,
    current_stock : 0
}
</pre>

(3) **确认生成目标。** 将样本交回原始 Rule。结果应为 `unsat`，而冲突应指向刚才反转的条件：

```tapas
let result = solve::hold(SaleRequest(test_input))
pprint('status    = ', result::status)
pprint('conflicts = ', result::conflicts)
```
<pre class='Tapas-Return'>
status    = unsat
conflicts = [{
    rule : Rule #1[Int, Int],
    condition : (sale_quantity <= current_stock)
}]
</pre>

这不是随机撞到一个异常值：Tapas 保留“每笔购买 1 至 6 件”，只反转选中的库存条件。固定随机源后，示例输出可以复现。分布、随机源和采样上限等设置见 [solve 包说明](src/stdlib/solve/README.md)；完整程序见[生成失败测试数据示例](examples/solve/generate_violations.tap)。

## 继续了解

| 想了解的内容 | 从这里开始 |
| --- | --- |
| 怎样检查完整的业务状态转移 | [换货业务教程](examples/retail/README.md)：将前状态、请求、实际输出和后状态绑定到模型，检查正确拒绝和故意缺陷 |
| 怎样生成失败测试数据 | [生成失败测试数据示例](examples/solve/generate_violations.tap)：从具名规则条件派生场景、采样并检查结果 |
| 怎样求解规则、读取失败原因 | [可行性示例](examples/solve/feasibility.tap)与[冲突诊断示例](examples/solve/diagnostics.tap)：两个可独立运行的小程序 |
| 代码的基本写法 | [Tapas 入门](docs/examples/Basics_zh.md)：变量、函数和控制流；[模块与目录包](docs/examples/modules/README.md)：跨文件组织代码 |
| 还有哪些可运行案例 | [示例目录](examples/README.md)：求解、业务测试与通用算法 |

## 安装和构建

可以从 [GitHub Releases](https://github.com/zhuanglinsheng/tapas/releases) 下载 Linux 或 macOS 的预编译包，也可以从源码构建。

Tapas 依赖 GNU Readline 和 Python 3，Python 环境中需要安装 OR-Tools 包。请确保系统能从默认搜索路径找到 GNU Readline 和 Python，并且 Python 可以导入 `ortools`。从源码构建还需要支持 C23 的编译器和 CMake 3.21 或更高版本。在仓库根目录运行：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

测试覆盖源码与字节码执行、合法与非法语言规则、求解、语言服务器、格式化以及文档中的可运行示例。

将 Tapas 安装到用户目录：

```sh
cmake --install build --prefix "$HOME/.tapas"
export PATH="$HOME/.tapas/bin:$PATH"
```

依赖的安装方法见[使用说明](docs/Usage_zh.md)，求解支持范围见 [solve 包说明](src/stdlib/solve/README.md)。

## Visual Studio Code 扩展

Tapas 的 Visual Studio Code 扩展提供语法高亮、实时诊断、类型悬停与补全，以及跨模块的跳转、引用查找和重命名。语言服务会索引工作区中的 `.tap` 文件，并在源码尚未写完整时继续提供可恢复的分析结果。

安装 Tapas Core 后，可以从 [Visual Studio Code Marketplace](https://marketplace.visualstudio.com/items?itemName=tapas-language.tapas-language) 安装扩展。VSIX 安装、运行器与语言服务器路径配置等内容见[扩展文档](https://github.com/zhuanglinsheng/tapas/blob/main/editors/vscode/README_zh.md)。

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
