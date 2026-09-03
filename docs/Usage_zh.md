# Tapas 使用说明

简体中文 | [English](Usage_en.md) | [项目主页](../README.md)

本文说明如何构建和运行 Tapas 命令行程序。
语言语法见[语言规范](Syntax_zh.md)，内建函数和包见[标准库](Stdlib_zh.md)。

## 构建

Tapas 使用 CMake 构建，需要支持 C23 的编译器、CMake 3.21 或更高版本和 GNU Readline。

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

安装 Tapas Core（语言运行器、Language Server 和标准库）：

```sh
cmake --install build --prefix /path/to/prefix
```

安装结果采用可重定位布局：`bin/tapas`、`bin/tapas-language-server` 和
`share/tapas/stdlib`。将`/path/to/prefix/bin`加入`PATH`后，命令行和薄版
VS Code 扩展都可以发现所需程序。VS Code 扩展单独从 Marketplace 或 VSIX
安装，不属于 Tapas Core 的 CMake 安装内容。

构建生成的可执行文件位于：

```text
build/bin/tapas
```

如果已将 Tapas 安装到`PATH`，可以直接使用命令名`tapas`。

## 命令概览

```text
tapas [选项] [文件或命令]
```

不提供选项和文件时，Tapas 会启动交互式 REPL。
支持的选项如下：

```text
-h                显示命令行帮助
-v                显示版本信息
-c FILE           将 .tap 或 .md 源文件编译为 .tapc 字节码
-e FILE           执行 .tapc 字节码
-r FILE           显示 .tapc 字节码
-ce FILE          编译源文件并执行生成的字节码
-cr FILE          编译源文件并显示生成的字节码
-m MODULE [ARGS]  执行源码包导出的 main(arguments)
-p PATH           为本次会话中后续命令添加模块搜索路径
-i CMD            执行一段 Tapas 命令字符串
--stdout          对 Markdown 输入仅向标准输出写入执行结果
```

选项从左到右处理。
例如，`-p examples -ce main.tap`会先把`examples`加入模块搜索路径，再编译并执行`main.tap`。

## 交互模式

不带参数运行 Tapas 即可启动 REPL：

```sh
build/bin/tapas
```

输入单元完整时提示符为`>>`；函数或代码块尚未结束、需要继续输入时为`..`。

```text
>> let x = 1 + 2
>> print(x)
3
```

输入以下命令退出 REPL：

```text
exit()
```

在 Tapas 代码中调用`__binary__()`可以查看字节码。

## 执行脚本

Tapas 源文件通常使用`.tap`后缀。
以下是`hello.tap`：

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

该命令在内存中编译并执行文件，不会保留`.tapc`字节码文件。

## 执行源码包

`-m`解析包目录中的`__init__.tap`，并调用它导出的`main(arguments: List[String])`。
`main`可以返回`Int`作为进程退出码，也可以返回`Nil`表示成功。
模块之后的参数原样传入，不需要空`--`分隔：

```sh
build/bin/tapas -m format --check examples/a.tap examples/b.tap
```

Tapas 标准库同时包含原生包和源码包。
原生包承担运行时或宿主系统边界，源码包使用普通 Tapas 组合这些能力；二者使用相同的导入与模块查找模型。
因此不需要把现有标准库改名为`corelib`。
只有语言执行不可回避的最小运行时能力才属于内核；随发行版维护、具有稳定公共接口的源码包属于标准库。
示例和项目专用工具则可以留在标准库之外。

## 执行命令字符串

使用`-i`执行一段 Tapas 命令：

```sh
build/bin/tapas -i "print(1 + 2)"
```

命令会作为交互式输入块执行，因此命令行程序会在 Tapas 输出前显示`Result:`。

## 编译和运行字节码

使用`-c`将源文件编译为字节码：

```sh
build/bin/tapas -c hello.tap
```

该命令生成：

```text
hello.tapc
```

使用`-e`执行已编译的字节码：

```sh
build/bin/tapas -e hello.tapc
```

`-e`也接受源文件名。
这两个命令会加载同一个字节码文件：

```sh
build/bin/tapas -e hello.tap
build/bin/tapas -e hello.tapc
```

使用`-ce`一次完成编译和执行：

```sh
build/bin/tapas -ce hello.tap
```

## 查看字节码

使用`-r`显示`.tapc`文件：

```sh
build/bin/tapas -r hello.tapc
```

使用`-cr`编译源文件并显示字节码：

```sh
build/bin/tapas -cr hello.tap
```

调试函数`__binary__()`也能在 Tapas 代码内打印当前代码对应的字节码，详见[运行机制](Mechanism_zh.md#查看字节码)。

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

仓库文档使用`tapas`标记可执行的 Tapas 源码；纯语法片段、占位写法、故意无效的示例，以及依赖标准二进制未提供的宿主扩展的代码，使用`text`。
C、Shell 和 EBNF 等其他语言使用各自的常规标记。

表格单元格中的竖线即使位于行内代码中，也必须写成`\|`，以免被 GFM 解释为列分隔符。
例如，表格源码中的`` `String \| Nil` ``会显示为`String | Nil`。
普通段落中的行内代码不需要转义竖线。

同一文档内的多个`tapas`代码块会按文档顺序组成一个程序，因此后面的代码块可以使用前面已经声明的名称。
除归档的`cpp/`实现外，仓库中的每份 Markdown 文档都必须能通过以下命令执行：

```sh
build/bin/tapas --stdout path/to/document.md
```

应优先编写可执行示例。
只有在改成可执行形式会掩盖所讲规则或行为时，才使用`text`。

以普通文件参数传入 Markdown 时，Tapas 会执行其中的代码块，并直接在文件中插入或替换返回值区域：

```sh
build/bin/tapas docs/example.md
```

生成的输出格式为：

```html
<pre class='Tapas-Return'>
3
</pre>
```

使用`--stdout`可以执行 Markdown 而不修改原文件：

```sh
build/bin/tapas --stdout docs/example.md
```

Markdown 文件也可通过`-c`编译：

```sh
build/bin/tapas -c docs/example.md
```

该命令生成`docs/example.tapc`。

## 模块搜索路径

`import`中的相对路径首先相对于当前源码文件解析。
因此，[`docs/examples/modules/main.tap`](examples/modules/main.tap)可以直接导入同目录的单文件模块和目录包：

```text
import library.tap as library
import greeter as greeter
```

其中，`greeter`目录以`greeter/__init__.tap`为入口。
这个自包含示例可以直接运行，不需要额外搜索路径：

```sh
build/bin/tapas docs/examples/modules/main.tap
```

只有模块不在当前源码文件附近时，才需要使用`-p`添加搜索目录：

```text
build/bin/tapas -p path/to/modules app.tap
```

在 Tapas 中使用`__path__().pprint()`查看会话搜索路径。
结果包含显式添加的路径以及相对于当前可执行文件定位的标准库路径：

```text
__path__().pprint()
```

使用`__ls__().pprint()`查看当前根库中注册的对象：

```tapas
__ls__().pprint()
```
<pre class='Tapas-Return'>
[print, pprint, input, int, float, bool, str, list, push_front, push_back, pop_front, pop_back, insert, concat, array, pair, idx, append, delete, iter, keys, values, sort, len, type, copy, identical, clock, clock_ns, now, __ls__, __path__, __param__, __nparam__, __binary__, dense, io, time, math, types]
</pre>

## 常用示例

运行 Markdown 示例并把结果写到终端：

```sh
build/bin/tapas --stdout docs/examples/Basics_zh.md
```

编译并执行 Markdown 源文件：

```sh
build/bin/tapas -ce docs/examples/Basics_zh.md
```

执行已有字节码文件：

```sh
build/bin/tapas -e docs/examples/Basics_zh.tapc
```

显示版本或帮助：

```sh
build/bin/tapas -v
build/bin/tapas -h
```

## 注意事项与限制

- 命令行程序支持 `-h`，不支持 `--help`。
- 矩阵乘法运算符`@`以及`dense::inner`、`dense::norm`、`dense::normalize`、`dense::outer`、`dense::copy_into`、`dense::scale_inplace`、`dense::add_scaled_inplace`、`dense::gemm`在运行时需要 LP64 CBLAS 动态库。Tapas 只在首次执行依赖 BLAS 的数组运算时加载该库；如果无法自动找到合适的库，可通过`TAPAS_BLAS_LIBRARY`指定动态库路径。
- 默认 Markdown 模式会修改输入文件；仅需终端输出时使用 `--stdout`。
- `-p PATH` 只影响同一命令中位于它之后的参数，应把它放在需要该路径的文件前。
- `-e`和`-r`加载`.tapc`字节码；传入源文件名时，Tapas 会将后缀替换为`.tapc`。
- `.tapc`是与当前 Tapas 运行时配套的生成文件，不保证跨版本兼容。更新 Tapas 后，应使用`-c`或`-ce`从源文件重新编译。运行时会拒绝格式版本不匹配的字节码。
- 发生编译错误或运行时错误时，Tapas 会以错误状态退出。
