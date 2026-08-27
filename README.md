![Tapas](docs/Logo.png)

# Tapas

简体中文 | [English](README_en.md)

Tapas 是一门轻量、以表达式为核心的编程语言，可用于经济建模、数值实验，也可
作为脚本语言嵌入其他程序。Tapas 使用 C23 编写，配有字节码编译器、栈式虚拟机、
交互式 REPL、模块系统、Markdown 执行工具和公共 C API。

## 主要能力

- 交互式执行、脚本、模块和可复用的 `.tapc` 字节码。
- 一等函数、闭包、递归以及常用容器和稠密数组。
- 编译期类型标注、结构 Type、联合 Type 和运行时反射。
- 可执行 Tapas 代码与 Markdown 文档中的 Tapas 代码块。
- 公共 C API、语言服务器和 Visual Studio Code 扩展。

## 构建

Tapas 需要支持 C23 的编译器、CMake 3.10 或更高版本和 GNU Readline。

在项目根目录运行：

```sh
cmake -S . -B build
cmake --build build
```

依赖安装、测试和安装方式参见[使用说明](docs/Usage_zh.md)。

## 快速开始

创建 `hello.tap`：

```tapas
print('Hello, Tapas!')
```
<pre class='Tapas-Return'>
Hello, Tapas!
</pre>

运行该文件：

```sh
build/bin/tapas hello.tap
```

REPL、命令字符串、字节码、Markdown 执行和模块路径等用法参见
[使用说明](docs/Usage_zh.md)。

## Visual Studio Code 支持

VS Code 扩展位于 [editors/vscode](editors/vscode)，支持语法高亮、实时诊断、
悬停类型信息、代码补全、定义跳转、引用查找、重命名、工作区模块分析，以及运行
当前 Tapas 文件。

构建项目后即可安装扩展：

```sh
editors/vscode/install.sh
```

安装完成后，在 VS Code 中执行 **Developer: Reload Window**。功能与配置说明详见
[VS Code 扩展说明](editors/vscode/README_zh.md)。

## 在 C 程序中嵌入 Tapas

公共头文件位于 `include/tapas`。以下程序通过会话接口执行一段 Tapas 源码：

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

- [使用说明](docs/Usage_zh.md)：构建、命令行选项、脚本、字节码、
  Markdown 执行和模块路径。
- [语言规范](docs/Syntax_zh.md)：语法、值类别、运算符、
  语句、函数、模块、数组和内置接口。
- [类型系统设计](docs/TypeSystem_zh.md)：编译期标注、Type 值、结构 Type
  和 `types` 包。
- [C 交互](docs/Foreign_zh.md)：嵌入会话、注册 C 函数、
  值操作和复合类型扩展。
- [运行机制](docs/Mechanism_zh.md)：编译器、字节码、虚拟机、
  环境和引用计数。
- 示例：[递归斐波那契](docs/examples/Fibonacci_zh.md) 和
  [排序算法](docs/examples/Sort_zh.md)。

## 许可证

Tapas 使用 MIT 许可证发布，详见 [LICENSE](LICENSE)。

## 联系方式

欢迎通过 Issue 反馈问题，也欢迎提交 Pull Request。联系邮箱：
<linsheng.z@outlook.com>。
