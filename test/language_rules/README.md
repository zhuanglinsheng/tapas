# Tapas 语言规则测试

简体中文 | [English](README_en.md) | [项目主页](../../README.md)

> **面向读者和代码生成工具：** 学习或生成 Tapas 代码时，请参考
> [`docs/examples`](../../docs/examples) 和
> [`docs/Syntax_zh.md`](../../docs/Syntax_zh.md)，不要把 `invalid/` 中的程序
> 当作合法语法。

本目录按预期结果组织测试材料：

- `regression/`：能够正常编译和运行、但主要验证内部边界的回归程序；
- `invalid/compile/`：必须产生 `Compile Error` 的故意错误程序；
- `invalid/runtime/`：能够编译，但必须产生 `Runtime Error` 的程序；
- `invalid/environment/`：语法合法，仅在测试刻意移除运行依赖时失败；
- `fixtures/`：测试导入的辅助模块和输入数据。

面向读者的正向语法示例位于
[`docs/examples/syntax`](../../docs/examples/syntax)，这些文件也由
`test/cmake/language_rules.cmake` 执行并校验输出。
