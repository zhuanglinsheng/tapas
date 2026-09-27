# 测试

简体中文 | [English](README_en.md) | [项目主页](../README.md)

测试文件按用途分为：

- `unit/`：运行时数据结构、编译器前端和语言服务器的 C 单元测试；
- `support/`：仅供测试使用的本地库；
- [`language_rules/`](language_rules/README.md)：按合法回归、预期编译错误、预期运行错误和辅助数据分类的语言测试；
- `../docs/examples/syntax/`：面向读者、同时由语言测试执行的正向语法示例；
- `cmake/`：集成测试驱动；
- [`benchmarks/`](benchmarks/README.md)：逐项对应的 Tapas 与 Python 性能程序及中英文结果。

显式字节码测试可能在源文件旁生成`.tapc`，直接执行源码与导入模块则在测试工作目录的`__tapas_build__`中保存自动缓存。
这些文件都属于已被 Git 忽略的构建产物，可以随时删除；测试会在需要验证重新编译行为时更新时间戳或清理对应产物。
