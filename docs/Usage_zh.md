# Tapas 使用说明

简体中文 | [English](Usage_en.md) | [项目主页](../README.md)

本文说明如何构建和运行 Tapas 命令行程序。语言语法和内置函数请参阅
`Syntax_zh.md`。

## 构建

Tapas 使用 CMake 构建，需要支持 C23 的编译器、CMake 3.10 或更高版本和
GNU Readline。

在 macOS 上，可以通过 Homebrew 安装 Readline：

```sh
brew install readline
```

在 Debian 或 Ubuntu 上，可以安装所需的构建工具和 Readline 开发包：

```sh
sudo apt install build-essential cmake libreadline-dev
```

在项目根目录配置并构建：

```sh
cmake -S . -B build
cmake --build build
```

构建并运行测试：

```sh
ctest --test-dir build --output-on-failure
```

将程序安装到 CMake 配置的安装前缀：

```sh
cmake --install build
```

构建生成的可执行文件位于：

```text
build/bin/tapas
```

如果已将 Tapas 安装到 `PATH`，也可以使用命令名 `tap`。

## 命令概览

```text
tap [选项] [文件或命令]
```

不提供选项和文件时，Tapas 会启动交互式 REPL。支持的选项如下：

```text
-h                显示命令行帮助
-v                显示版本信息
-c FILE           将 .tap 或 .md 源文件编译为 .tapc 字节码
-e FILE           执行 .tapc 字节码
-r FILE           显示 .tapc 字节码
-ce FILE          编译源文件并执行生成的字节码
-cr FILE          编译源文件并显示生成的字节码
-p PATH           为本次会话中后续命令添加模块搜索路径
-i CMD            执行一段 Tapas 命令字符串
--stdout          对 Markdown 输入仅向标准输出写入执行结果
```

选项从左到右处理。例如，`-p examples -ce main.tap` 会先把 `examples`
加入模块搜索路径，再编译并执行 `main.tap`。

## 交互模式

不带参数运行 Tapas 即可启动 REPL：

```sh
build/bin/tapas
```

输入单元完整时提示符为 `>>`；函数或代码块尚未结束、需要继续输入时为 `..`。

```text
>> let x = 1 + 2
>> print(x)
3
```

输入以下命令退出 REPL：

```text
exit()
```

在 Tapas 代码中调用 `__binary__()` 可以查看字节码。

## 执行脚本

Tapas 源文件通常使用 `.tap` 后缀。以下是 `hello.tap`：

```tapas
print('hello, Tapas')
```
<pre class='Tapas-Return'>
hello, Tapas
</pre>

直接运行该文件：

```sh
build/bin/tapas hello.tap
```

该命令在内存中编译并执行文件，不会保留 `.tapc` 字节码文件。

## 执行命令字符串

使用 `-i` 执行一段 Tapas 命令：

```sh
build/bin/tapas -i "print(1 + 2)"
```

命令会作为交互式输入块执行，因此命令行程序会在 Tapas 输出前显示 `Result:`。

## 编译和运行字节码

使用 `-c` 将源文件编译为字节码：

```sh
build/bin/tapas -c hello.tap
```

该命令生成：

```text
hello.tapc
```

使用 `-e` 执行已编译的字节码：

```sh
build/bin/tapas -e hello.tapc
```

`-e` 也接受源文件名。这两个命令会加载同一个字节码文件：

```sh
build/bin/tapas -e hello.tap
build/bin/tapas -e hello.tapc
```

使用 `-ce` 一次完成编译和执行：

```sh
build/bin/tapas -ce hello.tap
```

## 查看字节码

使用 `-r` 显示 `.tapc` 文件：

```sh
build/bin/tapas -r hello.tapc
```

使用 `-cr` 编译源文件并显示字节码：

```sh
build/bin/tapas -cr hello.tap
```

调试函数 `__binary__()` 也能在 Tapas 代码内打印当前代码对应的字节码。详见
`Mechanism_zh.md`。

## Markdown 文件

Tapas 可以读取 Markdown 文件并执行其中带 Tapas 标记的代码块：

````markdown
```tapas
print(1 + 2)
```
<pre class='Tapas-Return'>
3
</pre>
````

仓库文档使用 `tapas` 标记可执行的 Tapas 源码；纯语法片段、占位写法、故意
无效的示例，以及依赖标准二进制未提供的宿主扩展的代码，使用 `text`。
C、Shell 和 EBNF 等其他语言使用各自的常规标记。

同一文档内的多个 `tapas` 代码块会按文档顺序组成一个程序，因此后面的代码块
可以使用前面已经声明的名称。除归档的 `cpp/` 实现外，仓库中的每份 Markdown
文档都必须能通过以下命令执行：

```sh
build/bin/tapas --stdout path/to/document.md
```

应优先编写可执行示例。只有在改成可执行形式会掩盖所讲规则或行为时，才使用
`text`。

以普通文件参数传入 Markdown 时，Tapas 会执行其中的代码块，并直接在文件中
插入或替换返回值区域：

```sh
build/bin/tapas docs/example.md
```

生成的输出格式为：

```html
<pre class='Tapas-Return'>
3
</pre>
```

使用 `--stdout` 可以执行 Markdown 而不修改原文件：

```sh
build/bin/tapas --stdout docs/example.md
```

Markdown 文件也可通过 `-c` 编译：

```sh
build/bin/tapas -c docs/example.md
```

该命令生成 `docs/example.tapc`。

## 模块搜索路径

Tapas 使用 `import ... as ...` 导入模块：

```tapas
import examples/modules/demo-lib-1.tap as demo
```

使用 `-p` 在编译或执行文件前添加搜索路径：

```sh
build/bin/tapas -p docs -ce examples/main.tap
```

在 Tapas 中使用 `__path__().sprint()` 查看会话搜索路径：

```tapas
__path__().sprint()
```
<pre class='Tapas-Return'>
[docs]
</pre>

使用 `__ls__().sprint()` 查看当前根库中注册的对象：

```tapas
__ls__().sprint()
```
<pre class='Tapas-Return'>
[print, sprint, len, type, copy, identical, clock, clock_ns, now, array, int, float, bool, str, list, push, append, insert, pop, delete, idx, keys, dkeys, dvalues, union, pair, iter, sort, dense, math, __ls__, __path__, __param__, __nparam__, __binary__]
</pre>

## 常用示例

运行 Markdown 示例并把结果写到终端：

```sh
build/bin/tapas --stdout docs/examples/Sort_zh.md
```

编译并执行 Markdown 源文件：

```sh
build/bin/tapas -ce docs/examples/Sort_zh.md
```

执行已有字节码文件：

```sh
build/bin/tapas -e docs/examples/Sort_zh.tapc
```

显示版本或帮助：

```sh
build/bin/tapas -v
build/bin/tapas -h
```

## 注意事项与限制

- 命令行程序支持 `-h`，不支持 `--help`。
- 矩阵乘法运算符 `@` 在运行时需要 LP64 CBLAS 动态库。Tapas 只在首次执行
  依赖 BLAS 的数组运算时加载该库；如果无法自动找到合适的库，可通过
  `TAPAS_BLAS_LIBRARY` 指定动态库路径。
- 默认 Markdown 模式会修改输入文件；仅需终端输出时使用 `--stdout`。
- `-p PATH` 只影响同一命令中位于它之后的参数，应把它放在需要该路径的文件前。
- `-e` 和 `-r` 加载 `.tapc` 字节码；传入源文件名时，Tapas 会将后缀替换为
  `.tapc`。
- `.tapc` 是与当前 Tapas 运行时配套的生成文件，不保证跨版本兼容。更新
  Tapas 后，应使用 `-c` 或 `-ce` 从源文件重新编译。运行时会拒绝格式版本
  不匹配的字节码。
- 发生编译错误或运行时错误时，Tapas 会以错误状态退出。
