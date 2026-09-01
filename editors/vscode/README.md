# Tapas Visual Studio Code 扩展

简体中文 | [English](README_en.md)

本扩展将 Visual Studio Code 直接连接到 C 实现的
`tapas-language-server`，识别 `.tap` 文件，并且没有 npm 运行时依赖。

## 当前能力

### 编辑功能

- 高亮 Tapas 的注释、字符串、数字、关键字、内置 Type、常量和运算符。
- 使用 `//` 添加和取消行注释。
- 支持括号匹配、括号与引号自动闭合、选区包围和基于花括号的缩进。
- 将 UTF-8 源码位置转换为 LSP 使用的 UTF-16 位置，正确处理中文标识符、
  emoji 和其他 Unicode 字符。

### 诊断和类型信息

- 文档打开或修改后实时报告语法诊断。
- 源码暂时不完整时，继续提供能够恢复的语义诊断和符号信息。
- 在悬停信息中显示显式类型标注和局部推导出的 Type。
- 当前局部推导覆盖字面量、函数、算术表达式、Pair、列表、字典、导入和带标注
  的绑定。

### 导航和重构

- 在当前文档内以及导入的用户模块之间跳转到定义。
- 在整个工作区查找用户模块导出成员的引用，可以选择是否包含导出声明。
- 显示绑定、函数、导入、参数和迭代变量等文档符号。
- 通过 VS Code 的工作区符号搜索查找所有已索引模块的顶层绑定、函数和导入。
- 输入前几个字母时，补全当前词法作用域内的局部符号、根级内置函数和默认包，
  并显示已有的 Type 或函数签名。
- 输入 `包名::` 后补全 `math`、`types`、`dense`、`time` 等默认包的成员。
- 对 `import ... as package` 导入的用户模块，输入 `package::` 后补全模块最后
  `return` 的字典所公开的成员。
- 模块命名空间经过局部别名转发时仍能解析成员，例如
  `let numbers = math; numbers::sqrt(...)`。
- 检查新的符号名称并预览重命名范围。局部符号重命名修改当前文档内的声明和引用；
  用户模块公开成员重命名修改导出键以及工作区内所有可解析的成员引用。

### 工作区分析

- 初始化时递归索引工作区内的 `.tap` 文件，同时按需加载工作区外被直接导入的模块。
- 导入路径先相对于当前文件解析，再依次查找工作区根目录；目录模块使用
  `__init__.tap`，规则与编译器一致。
- 打开的未保存文档覆盖磁盘版本；关闭后恢复分析磁盘内容。
- 监视 `.tap` 文件的创建、修改和删除，并刷新对应模块接口。
- 报告找不到导入目标、未知模块成员和循环导入，循环依赖不会使分析器递归失控。

### 运行

- 打开 `.tap` 文件后，可单击编辑器右上角的运行按钮，或在命令面板中执行
  **Tapas: Run Current File**。
- 运行前自动保存文件，在 VS Code 集成终端中显示程序输出和错误。
- 当前文件目录作为工作目录，所属工作区根目录自动加入 Tapas 模块搜索路径。
- 运行器与 Language Server 相互独立；分析服务不可用时仍可运行程序。

解析器和语义模型不依赖 Tapas VM。编辑文档不会执行用户代码。

## 当前边界

模块公开接口取自模块最后一条 `return` 返回的字典。跨模块查询目前针对使用
`::` 访问的公开成员；普通局部符号仍按文件和词法作用域解析。签名帮助、语义
Token、增量文本同步和完整结构 Type 悬停信息仍属于后续工作。
当前运行命令不提供断点、单步执行、变量查看或调用栈；这些能力需要单独实现
Tapas Debug Adapter。

## 从仓库构建和运行

Tapas 需要 C23 编译器、CMake 和 GNU Readline。先构建运行器和 Language Server：

```sh
cmake -S . -B build
cmake --build build -j4
```

使用 VS Code 打开本仓库，在 **Run and Debug** 中启动
**Tapas Extension**。仓库内置的调试配置会打开 Extension Development Host，
并自动找到 `build/bin/tapas-language-server`。

## 安装到本机

运行：

```sh
editors/vscode/install.sh
```

脚本会把扩展安装到本机 VS Code 扩展目录，并随扩展复制当前构建的
`build/bin/tapas-language-server` 和 `build/bin/tapas`。安装完成后，在 VS Code 中执行
**Developer: Reload Window**，然后打开 `.tap` 文件。

扩展按以下顺序寻找 Language Server：

1. `tapas.languageServer.path` 设置指定的路径；
2. `install.sh` 随扩展复制的可执行文件；
3. 扩展源码仓库或当前工作区中的 `build/bin/tapas-language-server`；
4. `PATH` 中的 `tapas-language-server`。

自动发现不适用时，可以把 `tapas.languageServer.path` 设置为绝对路径。

扩展按以下顺序寻找 Tapas 运行器：

1. `tapas.runtime.path` 设置指定的路径；
2. `install.sh` 随扩展复制的可执行文件；
3. 扩展源码仓库或当前工作区中的 `build/bin/tapas`；
4. `PATH` 中的 `tapas`。

运行当前文件时，扩展执行的命令等价于：

```sh
tapas -p WORKSPACE_ROOT CURRENT_FILE
```

## 测试

协议测试会启动真正的 C Language Server，并通过扩展使用的同一个 Node 协议客户端
测试初始化、文档同步、诊断、默认包补全、用户模块成员补全、跨文件悬停和定义跳转：

```sh
node editors/vscode/test/protocol.test.js build/bin/tapas-language-server
```

系统能够找到 Node.js 时，该测试也会以 `vscode_protocol` 的名称注册到 CTest。

## 实现结构

- `extension.js` 把 Language Server 响应转换为 VS Code Provider，并管理文档和
  诊断的生命周期。
- `protocol.js` 实现 Content-Length 消息分帧，以及 JSON-RPC 请求、响应和通知。
- `language-configuration.json` 定义注释、括号、自动闭合和缩进。
- `syntaxes/tapas.tmLanguage.json` 定义 TextMate 语法高亮。
- `../../src/lsp/` 包含 C 实现的 Language Server。
- `../../src/compile/module.c` 提取与 VM 无关的模块公开接口，并提供默认环境清单。
- `../../src/compile/workspace.c` 管理文档覆盖、磁盘索引、导入图和跨模块解析。
