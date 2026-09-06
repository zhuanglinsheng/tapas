# Tapas 语言服务器

简体中文 | [English](README_en.md) | [项目主页](../../README.md)

`tapas-language-server` 是一个通过标准输入输出通信的独立 Language Server Protocol 进程。
它只链接可复用的编译器前端，不会初始化虚拟机，也不会执行用户代码。

目前支持以下协议功能：

- 全文档同步（`didOpen`、`didChange`、`didClose`）；
- 可从语法错误中恢复的解析，以及语义诊断；
- 在 UTF-8 Tapas 源码与 LSP UTF-16 位置之间转换；
- 显示局部变量 Type、根级内建函数签名、默认包及包成员的悬停信息；
- 查找当前文件和跨模块的定义与引用；
- 提供文档符号和工作区符号；
- 根据已输入前缀补全局部名称和标准环境中的名称；
- 在 `::` 后补全默认包和已导入模块的成员；
- 基于结构 Type 提供字段悬停及 `::` 补全，包括带标注的参数、嵌套成员和导入 Type；
- 使用前端已有的 Token、AST 和符号解析结果提供全文档语义 Token；
- 支持重命名预检、局部重命名，以及公开模块成员的工作区编辑；
- 递归建立工作区索引，维护打开文档的内存版本，诊断导入问题，并在受监视文件
  变化后刷新分析结果。

编辑器通过以下命令启动服务器：

```text
tapas-language-server
```

仓库在 `editors/vscode/` 中提供了一个不依赖第三方包的 VS Code 客户端。
客户端可以从仓库的构建目录、`PATH` 或 `tapas.languageServer.path` 设置中找到语言服务器。

## 分层结构

`json.c` 实现协议所需的轻量 JSON 解析器。`server.c` 负责消息分帧、请求、通知和 LSP 数据序列化。
`src/compile/frontend/module.c` 定义公开模块接口和标准环境；
`src/compile/frontend/workspace.c` 管理文档快照、磁盘索引、导入关系和跨文件符号身份；
`workspace_reference.c` 在文档更新时维护模块成员引用索引。
类型标注使用独立的 `tannotation_index`，在语义分析时记录名称位置、作用域、
局部符号 ID 和成员链，不向表达式 AST 添加节点，不修改参数范围或控制流。
每次文档重分析都会重建索引；模块成员按需解析，避免缓存失效的导出对象指针。
悬停、定义跳转、引用查找和重命名共用这些引用信息，不执行类型表达式。
协议处理程序负责将这些分析结果转换为 LSP 位置和工作区编辑。

类型展示与展开格式分开：`tstatic_type_display` 保留具体标注引用的名称来源，
`tstatic_type_format` 输出完整结构。命名引用拥有独立的静态类型节点，保留原结构，
不会把名称写到共享定义上，也不参与类型相等性或可赋值关系。`State` 与 `Snapshot`
即使指向同一结构，也可按各自标注显示；不会通过结构相等反向猜测名称。
内建 Type 保持标准名称。跨模块传播仅限定可验证的公开名称，私有名称回退到展开结构。
前端生成的显示类型供 hover、补全和模块详情复用；不在 hover 中专门重拼 Rule 声明。

语义 Token 的标准名称来自服务启动时建立的只读签名索引，不会对每个标识符线性
扫描标准库。`tapas/syntaxCatalog` 向协议测试公开关键字、内置 Type 和默认包清单，
用于校验静态 TextMate 后备语法；编辑器运行时不调用它。

后续编辑器支持还包括签名提示、语义 Token 增量响应和增量文本同步。

## Symbol presentation

Hover and completion details share the semantic formatter in `presentation.c`. See [the presentation rules](Presentation.md) for categories, aliases, record expansion, and the boundary with runtime printing. Handwritten package `detail` strings no longer define signatures.
