# Tapas 入门示例

简体中文 | [English](Basics_en.md) | [项目主页](../../README.md)

下面的程序集中展示变量、列表、函数、循环和条件判断。
将代码保存为`.tap`文件可以直接运行；Tapas 也能执行 Markdown 文档中的`tapas`代码块。

```tapas
let language = 'Tapas'
let numbers = [1, 2, 3, 4, 5]

function double(value)
{
    return value * 2
}

print('Hello, ', language, '!')
pprint('numbers: ', numbers)

for (let number in numbers) {
    if (number % 2 == 0) {
        print(number, ' doubled is ', double(number))
    }
}
```
<pre class='Tapas-Return'>
Hello, Tapas!
numbers: [1, 2, 3, 4, 5]
2 doubled is 4
4 doubled is 8
</pre>

在仓库根目录运行：

```sh
build/bin/tapas --stdout docs/examples/Basics_zh.md
```
