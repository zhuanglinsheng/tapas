# Recursive Fibonacci

[简体中文](Fibonacci_zh.md) | English

This example implements the Fibonacci sequence with direct recursion. It is
intentionally not optimized with memoization, because the goal is to stress
function calls, recursive frames, parameter passing, and return values.

The benchmark uses Tapas's built-in `clock()` function. `clock()` returns the
current process CPU time in seconds, so `clock() - start` measures the CPU time
spent between two points in the same Tapas run.

You can also use the shell `time` command to measure the whole process:

```text
time ./build/bin/tapas --stdout docs/examples/Fibonacci_en.md
```

For a quick comparison after VM changes, keep `n` fixed and compare the elapsed
time printed by Tapas.



## Recursive Function

The `fib` function follows the mathematical definition:

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



## Run

`fib(26)` runs one relatively large recursive calculation. It is large enough
to exercise many recursive call frames, while still keeping the document
responsive. Increase `n` when you want a heavier benchmark.

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
elapsed seconds = 0.320289
</pre>
