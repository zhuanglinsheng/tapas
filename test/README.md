# 测试

简体中文 | [English](README_en.md) | [项目主页](../README.md)

测试文件按用途分为：

- `unit/`：运行时数据结构、编译器前端和语言服务器的 C 单元测试；
- `support/`：仅供测试使用的本地库；
- [`language_rules/`](language_rules/README.md)：按合法回归、预期编译错误、预期运行错误和辅助数据分类的语言测试；
- `../docs/examples/syntax/`：面向读者、同时由语言测试执行的正向语法示例；
- `cmake/`：集成测试驱动；
- [`benchmarks/`](benchmarks/README.md)：逐项对应的 Tapas 与 Python 性能程序及中英文结果。

测试和文档命令可能在源文件旁生成 `.tapc`。
这些文件属于构建产物，已被 Git 忽略，可以随时删除。
普通语言测试直接传入 Tapas 源文件，因此每次运行都会重新编译。
需要字节码或导入已编译模块的测试会先删除现有 `.tapc`，从对应源文件重新编译，完成验证后再删除生成文件。
