# 递归斐波那契数列

简体中文 | [English](Fibonacci_en.md)

本例使用直接递归实现斐波那契数列。这里有意不采用记忆化优化，因为它的目的
是对函数调用、递归栈帧、参数传递和返回值进行压力测试。

基准测试使用 Tapas 内置的 `clock()` 函数。`clock()` 返回当前进程已消耗的
CPU 时间（单位为秒），因此 `clock() - start` 可以测量同一次 Tapas 运行中
两个时间点之间消耗的 CPU 时间。

也可以使用 Shell 的 `time` 命令测量整个进程：

```text
time ./build/bin/tapas --stdout docs/examples/Fibonacci_zh.md
```

修改虚拟机后进行快速比较时，请保持 `n` 不变，再比较 Tapas 输出的耗时。

## 递归函数

`fib` 函数直接遵循数学定义：

- `fib(0) = 0`
- `fib(1) = 1`
- `fib(n) = fib(n - 1) + fib(n - 2)`

```tapas
var fib = (n){
    if(n < 2){
        return n
    }
    return this(n - 1) + this(n - 2)
}
```

## 运行

`fib(26)` 会进行一次规模较大的递归计算。它足以产生大量递归调用栈帧，
同时仍能让文档在较短时间内执行完成。需要更重的基准负载时，可以增大 `n`。

```tapas
var n = 26
var start = clock()
var result = fib(n)
var elapsed = clock() - start

print('fib(', n, ') = ', result)
print('elapsed seconds = ', elapsed)
```
<pre class='Tapas-Return'>
fib(26) = 121393
elapsed seconds = 0.315583
</pre>
