# Tapas 模块示例

简体中文 | [English](README_en.md) | [项目主页](../../../README.md)

本目录同时展示 Tapas 的两种模块组织方式：

- [`library.tap`](library.tap) 是单文件模块；
- [`greeter/__init__.tap`](greeter/__init__.tap) 是目录包的入口；
- [`main.tap`](main.tap) 使用相对于自身位置的路径导入并调用两者。

从仓库根目录运行：

```sh
build/bin/tapas docs/examples/modules/main.tap
```
