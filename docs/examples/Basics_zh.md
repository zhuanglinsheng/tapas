# Tapas 入门示例

简体中文 | [English](Basics_en.md) | [项目主页](../../README.md)

下面的程序不调用现成的平方根函数，而是用牛顿法逐步逼近结果，同时展示变量、列表、函数、循环、条件判断和参数约束。
将代码保存为`.tap`文件可以直接运行；Tapas 也能执行 Markdown 文档中的`tapas`代码块。

```tapas
let language = 'Tapas'
let numbers = [2, 4, 9]

function square_root(value: Int | Float) -> Float
{
    assert(rule {
        "被开方数不能为负数": value >= 0
    })

    if (value == 0) {
        return 0.0
    }

    let estimate: Float = float(value)
    while (math::abs(estimate * estimate - value) > 1e-10) {
        estimate = (estimate + value / estimate) / 2.0
    }
    return float(estimate)
}

print('Hello, ', language, '!')
pprint('numbers: ', numbers)

for (let number in numbers) {
    print('sqrt(', number, ') = ', square_root(number))
}
```
<pre class='Tapas-Return'>
Hello, Tapas!
numbers: [2, 4, 9]
sqrt(2) = 1.41421
sqrt(4) = 2
sqrt(9) = 3
</pre>

`value: Int | Float`允许传入整数或浮点数，`assert(rule { ... })`继续检查参数是否满足函数要求。这里的平方根只接受非负数；如果传入负数，程序会显示“被开方数不能为负数”这项约束没有满足。

函数先处理零，再从输入值开始反复修正`estimate`，直到平方误差足够小。这里实现平方根是为了展示完整程序，实际项目也可以直接使用`math::sqrt`。

Rule 还可以用于检查业务数据和生成测试输入。完成这个入门示例后，可以继续阅读[换货业务教程](../../examples/retail/README.md)；Rule 的完整语法和检查方式见 [Rule 文档](../Rules_zh.md)。

在仓库根目录运行：

```sh
build/bin/tapas --stdout docs/examples/Basics_zh.md
```
