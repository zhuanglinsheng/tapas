# 编译器结构

简体中文 | [English](README_en.md) | [项目主页](../../README.md)

编译器按可复用前端、静态 Type 分析和字节码后端分层。Tapas 文件、Markdown
代码块、单语句编译和 REPL 都走同一条 AST 管线。

```text
source -> frontend/{document, syntax, ast, parser, semantic, control_flow}
       -> types/{static_type, type_info, type_check}
       -> backend/{context, emit, type_bridge, source}
       -> bytecode
```

`frontend/` 与 `types/` 不包含 VM、字节码或运行时对象头文件；启用标准环境的
前端构建目标链接 `tapas_stdlib`，以消费与实现同源的 Type 描述。`backend/` 可以
依赖可复用前端和运行时。CLI 输入状态位于 `src/cli/`。这一依赖方向是目录划分的
主要约束，文件数量和行数不是继续拆目录的充分理由。

标准库位于 `src/stdlib/`，根内建函数集中在 `builtins/`，语言包使用同名目录。
每组 C 函数的实现、参数范围和 Tapas Type
共同定义在所属 `.c` 文件中，并通过 `textension_descriptor` 同时提供给运行时、
编译器和 LSP。`types` 包的 Type 构造器身份也来自同一描述符。

## 可复用前端

编译器接口是实现内部接口，头文件与实现共同位于 `src/compile/`。对外嵌入 API
通过 `include/tapas/tsession.h` 提供，不公开 AST、语义模型和工作区布局。

- `frontend/document.c` 管理不可变 UTF-8 源码、行索引和左闭右开源码范围。
- `frontend/syntax.c` 生成保留空白、换行和注释的无损 Token。
- `frontend/ast.c` 管理紧凑节点 Arena 和稳定 `tast_id`。
- `frontend/parser.c` 以统一边界扫描器解析模块、语句和 Pratt 表达式；一条
  `if`／`elif`／`else` 条件链对应一个 AST 节点及其分支列表。
- `frontend/semantic.c` 建立词法作用域、声明、符号和名称解析。
- `frontend/control_flow.c` 一次建立父子关系、子节点角色、所在函数和确定返回事实。
- `frontend/definite_assignment.c` 计算每个名称读取和编译单元出口的确定初始化状态。
- `frontend/module.c` 提取模块公开接口并生成标准环境符号。
- `frontend/workspace.c` 管理 URI、文件索引、导入图、模块成员和 LSP 工作区。
- `frontend/workspace_reference.c` 维护跨模块导出成员的排序引用索引。
- `frontend/frontend.c` 统一管理文档、Token、AST、语义、Type 和诊断生命周期。

解析器递归仅对应语法嵌套，并受显式深度限制。CLI 与 LSP 消费相同的
`tfrontend` 结果，不再维护独立的语法或 Type 规则。

## 静态 Type 分析

当前语言 Type 语义由 [TypeSystem_zh.md](../../docs/TypeSystem_zh.md) 维护；本节只描述
编译器中的实现边界。

- `types/static_type.c` 定义静态 Type Arena、解析、格式化、结构相等和可赋值关系。
- `types/type_info.c` 是表达式 Type、符号 Type 和静态 Type 值的唯一推断来源。
- `types/type_check.c` 统一产生赋值、调用、返回、索引和容器修改诊断。
- `types/type_constructor.c` 为静态求值与检查提供由清单生成的 Type 构造器身份和参数范围。
- 外部解析器向 TypeInfo 提供预载 C 函数和导入模块签名。CLI 从实际 `tlib`
  描述符读取，LSP 工作区从同一标准环境清单读取。
- `Unknown` 是分析状态，不是 `AnyType`。规范签名可以显式使用 `Unknown`
  表示当前模式系统尚不能表达的依赖结果，但不能据此制造确定 Type。
- 字段构造顺序直接来自静态结构 Type；跨模块边界时保存在模块接口中。

## 字节码后端

- `backend/context.c` 管理寄存器、槽位、编译上下文和生命周期。
- `backend/binding.c` 管理绑定 Type、静态 Type 值、初始化状态和模块接口。
- `backend/ast_emit.c` 生成表达式和函数字面量字节码。
- `backend/statement_ast_emit.c` 生成声明、赋值、控制流、导入和模块字节码。
- `backend/type_bridge.c` 集中转换静态 Type IR 与运行时 `ttypeval`。
- `backend/type_emit.c` 物化静态 Type 值和字段构造顺序。
- `backend/source.c` 统筹文件、Markdown、导入和公共编译入口。

`tast_emitter` 持有指令、常量和导入路径。语句与控制流生成器不再重复转发这些
上下文参数。公共 `compiler.h` 只公开不透明 `tcp` 和编译入口；上下文布局及槽位
操作保留在 `backend/` 私有头文件中。

## 本轮结构优化

`tcontrol_flow` 在解析后建立 parent、first-child、sibling、child-role、
enclosing-function、definitely-returns 和 definite-assignment。现在：

- `this`、返回语句和函数上下文通过所在函数索引直接定位；
- 索引写入、可选字段包装通过 parent 与 child-role 判断；
- 真分支 Type 收窄只沿祖先链传播，不再为每个名称扫描所有控制语句；
- LSP 光标节点定位沿语法子树下降，成员解析再沿祖先链上行；
- Semantic 保存 declaration-to-symbol 反向索引，TypeInfo 与 TypeCheck 不再各自扫描或重建；
- 前端对所有继续执行路径合并初始化状态，排除确定返回路径，并保守处理循环；
- 条件链的语义、收窄、确定返回和 definite-assignment 直接遍历分支，不再扫描
  相邻兄弟语句；
- 分段编译的已有绑定作为 Semantic 外部符号输入，初始化语义不再依赖后端特例；
- Workspace 在文档分析时建立按目标模块与导出名称排序的引用索引。

Type 构造器的参数范围、静态参数、`make_type` 字段和间接调用规则均由前端检查。
后端不再递归解释或验证构造器 AST，只把 TypeInfo 的 `TypeId` 物化为 `ttypeval`。
字段顺序也从静态 Type 或模块接口读取。`backend/type_emit.c` 因此从约 586 行缩减到
约 220 行，CLI 与 LSP 使用完全相同的构造器诊断。

## 当前结构审计

已清除查询路径中的嵌套全树扫描、Type 构造器重复以及后端初始化快照。LSP 跨模块
引用通过二分定位引用区间，重命名直接按已排序的来源文档生成编辑，不再扫描候选 AST。
仍保留的整树遍历均是建立 Semantic、关系索引、TypeInfo、诊断和文档引用所需的
线性编译阶段。

剩余结构问题主要是：

- `parser.c` 约一千行，同时包含表达式、函数、集合和语句语法；
- `statement_ast_emit.c` 仍混合控制流跳转、绑定写入和模块发射；
- `type_info.c` 仍混合静态 Type 求值、表达式推断、上下文传播和收窄；
- `workspace.c` 同时承担文件系统索引、URI、文档生命周期和命名空间解析；
- Semantic 的作用域名称查找仍是线性符号搜索；
- Type 字符串仍为所有节点和符号提前格式化，标注也会重复解析。

当前三层目录边界仍然合理，不需要新的顶层目录。

## 后续优化顺序

### 1. 建立运算符签名表

用只读签名表替代分散在推断器和 VM 中的运算符分支。每条签名包含运算符、左右
Type pattern、结果规则和运行时约束；pattern 支持具体 Type、Numeric／Ordered／
Array 类型族、共享类型变量和参数化容器，结果支持固定 Type、类型变量、数值提升
和数组广播。

首批表项覆盖数值运算与提升、String/List 拼接和重复、标量 Bool 与 BoolArray
逻辑、比较、成员关系、范围、矩阵运算及 Time 偏移。数组形状仍由运行时检查。
每个 VM 分支必须对应唯一表项，测试从表项生成接受、拒绝和结果 Type 用例，避免
前后端规则漂移。

### 2. 拆分 `Unknown` 分析状态

语言 Type `AnyType` 保持不变；内部 `Unknown` 拆为等待固定点的 `Unresolved`、
显式动态边界 `Dynamic`、抑制级联诊断的 `ErrorType` 和等待上下文求解的
`InferredHole`。先记录现有 Unknown 的来源并消除可推断的 hole，再为
`Dynamic -> T` 在赋值、调用、返回和容器写入处生成 guard，最终禁止
`Unresolved` 离开前端，并允许严格模式拒绝隐式动态转换。

迁移必须保持渐进保证：减少标注不改变成功执行时的行为；增加标注只能提前报告
错误或收紧 guard，不能改变运行值。

### 3. 扩展控制流数据流

现有共享模型已经覆盖结构关系、确定返回与 definite-assignment。下一步加入条件的
真假分支事实、联合差集形成的假分支 Type、`and`／`or`／`not` 短路组合、循环
回边、字段存在性、用户 type guard、穷尽检查和 `Never`。事实绑定符号与 CFG 边；
赋值或可能修改别名的调用必须使相关收窄失效。

### 4. 按内部边界拆大文件

在上述模型稳定后再拆文件：

- `parser.c` -> `parser_expression.c`、`parser_statement.c` 和私有解析器状态；
- `type_info.c` -> 静态 Type 求值、表达式推断和流分析；
- `statement_ast_emit.c` -> 绑定发射、控制流发射和模块发射；
- `workspace.c` -> 路径/索引、文档存储和解析查询；
- `source.c` -> 源码读取、导入编译和公共入口。

这些仍应留在现有 `frontend/`、`types/`、`backend/` 下，不需要新的顶层目录。

### 5. 收紧公共头文件

`compiler.h` 已经足够小。后续头文件精简应优先减少公开状态，而不是机械移动声明：

- 用文档级解析入口替代公开 `tparser` 布局，再把解析器状态移入私有头文件；
- 为 LSP 增加 Frontend、Workspace 和 Semantic 访问器后，再逐步隐藏其可变数组；
- 保持 AST 节点只描述语法，所有关系与流事实通过 `control_flow.h` 暴露；
- 保持 `type_constructor.h`、发射器和编译上下文头文件私有；
- 不合并 AST、Semantic 和 TypeInfo，它们具有不同生命周期和消费者。

### 6. 降低编辑器分配与查询成本

Type 字符串改为按 hover、补全或诊断请求惰性格式化；规范 C 函数签名和标注在每个
Type Arena 内缓存解析结果；作用域改用名称索引；局部符号引用也建立紧凑反向索引。
