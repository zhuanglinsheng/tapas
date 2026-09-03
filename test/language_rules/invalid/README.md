# Invalid Tapas Programs

简体中文 | [English](README_en.md) | [项目主页](../../../README.md)

> **DO NOT COPY THESE FILES AS VALID TAPAS SYNTAX.**

这里的程序故意触发失败，用来验证编译诊断、运行时错误或缺失依赖时的行为。
每个 `.tap` 文件的第一行都会标明预期失败类型：

- `compile/` 中的程序必须产生 `Compile Error`；
- `runtime/` 中的程序必须在成功编译后产生 `Runtime Error`；
- `environment/` 中的程序语法合法，只在测试指定的依赖缺失时失败。

合法、可学习的 Tapas 示例请参阅
[`docs/examples`](../../../docs/examples) 和
[`docs/Syntax_zh.md`](../../../docs/Syntax_zh.md)。
