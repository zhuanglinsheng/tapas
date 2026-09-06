# Tapas Visual Studio Code 扩展

简体中文 | [English](https://github.com/zhuanglinsheng/tapas/blob/main/editors/vscode/README.md) | [项目主页](https://github.com/zhuanglinsheng/tapas)

本扩展将 Visual Studio Code 直接连接到 C 实现的
`tapas-language-server`，识别 `.tap` 文件，并且没有 npm 运行时依赖。

Rule 表达式中的 `not`、`and`、`or` 支持 `Bool` 和 `RuleInstance` 操作数，
其中 `and`、`or` 短路求值。Rule 依赖直接写为 `Valid(x)` 等实例表达式；
`require` 已不再是关键字。Rule 表达式之外，这些逻辑运算符仅接受 `Bool`。

请使用同一 Core 构建提供的 `tapas-language-server`、`tapas` 和 `format` 包。
仅更新扩展会更新静态高亮，不会更新诊断、执行或格式化行为：
扩展调用外部 Core 工具，并不内置这些工具。

## 当前能力

### 编辑功能

- 高亮 Tapas 的注释、字符串、数字、关键字、内置 Type、常量和运算符。
- 根据编译器前端的符号解析结果区分命名空间、Type、函数、参数和变量；服务尚未
  就绪时由静态 TextMate 语法提供即时高亮。
- 使用 `//` 添加和取消行注释。
- 支持括号匹配、括号与引号自动闭合、选区包围和基于花括号的缩进。
- 将 UTF-8 源码位置转换为 LSP 使用的 UTF-16 位置，正确处理中文标识符、
  emoji 和其他 Unicode 字符。

### 诊断和类型信息

- 文档打开或修改后实时报告语法诊断。
- 源码暂时不完整时，继续提供能够恢复的语义诊断和符号信息。
- 在悬停信息中显示显式类型标注、局部推导 Type、根级内建函数签名，以及默认包
  和包成员详情。
- 当前局部推导覆盖字面量、函数、算术表达式、Pair、列表、字典、导入和带标注
  的绑定。
- `state::status` 等固定成员读取支持成员 Type 悬停；`state::` 根据结构 Type
  补全字段并显示类型，包括函数／Rule 参数、嵌套成员和导入的 `model::State`。
  AST 和类型分析已经支持成员访问，服务器直接复用这些事实，不运行用户代码。
- 类型展示保留命名结构引用，例如 `Rule[State]`、`Function[State] -> List[State]`，
  支持别名与嵌套标注。悬停和补全共用此视图；悬停 Type 值本身时展示定义。
  显式匿名结构 Type 仍完整展开；字典字面量推断为 `Dictionary`，不声明字段约束。
  内建 Type 使用标准名称。
- 参数、返回值和变量的类型标注通过独立引用索引支持名称与 `::` 成员的悬停、
  定义跳转、引用查找和重命名，包括 `InstanceOf[model::Exchange]` 内的规则引用。
  参数标注使用定义环境，即使参数与类型或模块同名，
  也不会改变标注中的引用目标。

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

### 格式化

- 支持 VS Code 的 **Format Document**、保存时格式化和命令面板中的
  **Tapas: Format Document**。
- 格式化提供器把当前编辑器文本交给随 Tapas 发布的 `format` 源码包，再以单个
  VS Code 文本编辑返回结果；未保存的修改不会先写入原文件。
- 插件不维护第二套排版规则。单行函数签名、多行参数列表和紧凑控制流的行为与
  `tapas -m format` 完全一致。
- 格式化只依赖 Tapas 运行器和 `format` 包；Language Server 不可用时仍然工作。

解析器和语义模型不依赖 Tapas VM。编辑文档不会执行用户代码。

## 当前边界

模块公开接口取自模块最后一条 `return` 返回的字典。跨模块查询目前针对使用
`::` 访问的公开成员；普通局部符号仍按文件和词法作用域解析。签名帮助、语义
Token 增量响应和增量文本同步仍属于后续工作。结构字段的跳转／重命名，以及
联合或递归结构的成员补全尚未提供；无法静态确定的成员不保证有类型提示。
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

## 安装扩展

扩展是一个不包含原生二进制的薄客户端。请先单独安装 Tapas Core，确保
`tapas`、`tapas-language-server` 和标准库可以使用；也可以在 VS Code 设置中
显式指定两个可执行文件的路径。

从 Visual Studio Code Marketplace 安装 **Tapas**：

```sh
code --install-extension tapas-language.tapas-language
```

也可以从 GitHub Release 下载 VSIX，并安装该文件：

```sh
code --install-extension /path/to/tapas-language-VERSION.vsix
```

从开发仓库直接安装客户端时，可以运行：

```sh
editors/vscode/install.sh
```

该脚本只会把扩展客户端安装到本机 VS Code 扩展目录，不会复制运行器、Language Server 或标准库。安装完成后，在 VS Code 中执行 **Developer: Reload Window**，然后打开 `.tap` 文件。

扩展按以下顺序寻找 Language Server：

1. `tapas.languageServer.path` 设置指定的路径；
2. 调试扩展时，扩展源码仓库中的 `build/bin/tapas-language-server`；
3. `$HOME/.tapas/bin/tapas-language-server`；
4. `PATH` 中的 `tapas-language-server`。

扩展不会自动执行当前工作区中的 Language Server。

自动发现不适用时，可以把 `tapas.languageServer.path` 设置为绝对路径。

如果命令行可运行的 `rule` 或 `types::enum` 在编辑器里报语法错误，请检查
是否仍在使用旧扩展内打包的服务器。开发 Tapas 本身时，在工作区设置里将
`tapas.languageServer.path` 和 `tapas.runtime.path` 分别指向同一构建目录下
的 `build/bin/tapas-language-server` 和 `build/bin/tapas`（使用绝对路径），
重新构建后执行 VS Code 的 `Developer: Reload Window`。仅更新源码不会更新已运行的服务器。

扩展按以下顺序寻找 Tapas 运行器：

1. `tapas.runtime.path` 设置指定的路径；
2. 调试扩展时，扩展源码仓库中的 `build/bin/tapas`；
3. `$HOME/.tapas/bin/tapas`；
4. `PATH` 中的 `tapas`。

扩展不会自动执行当前工作区中的 Tapas 运行器。

运行当前文件时，扩展执行的命令等价于：

```sh
tapas -p WORKSPACE_ROOT CURRENT_FILE
```

格式化时，扩展在临时目录中执行等价命令，并把结果作为编辑器修改返回：

```sh
tapas -m format TEMPORARY_FILE
```

## 打包 VSIX

在扩展目录运行官方 `vsce` 打包工具：

```sh
cd editors/vscode
npx --yes @vscode/vsce package --no-dependencies
```

打包前会自动执行清单检查。生成的 VSIX 只包含扩展客户端、语法文件、文档和许可证。

## 测试

协议测试会先通过真实 Tapas 运行器验证格式化，再启动真正的 C Language Server，
并通过扩展使用的同一个 Node 协议客户端测试初始化、文档同步、诊断、语义 Token、
默认包补全、用户模块成员补全、跨文件悬停和定义跳转。测试还会将编译器导出的关键字、内置 Type 和默认包清单与
TextMate 后备语法比较，防止两者随版本演进而漂移：

```sh
node editors/vscode/test/protocol.test.js \
  build/bin/tapas-language-server build/bin/tapas
```

系统能够找到 Node.js 时，该测试也会以 `vscode_protocol` 的名称注册到 CTest。
