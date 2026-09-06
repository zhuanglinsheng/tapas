# A First Look at Tapas

[简体中文](Basics_zh.md) | English | [Project Home](../../README_en.md)

The program below computes square roots with Newton's method instead of calling
a built-in square-root function. It introduces variables, lists, functions,
loops, conditionals, and argument constraints. Code saved in a `.tap` file can
run directly, and Tapas can also execute `tapas` code blocks in Markdown.

```tapas
let language = 'Tapas'
let numbers = [2, 4, 9]

function square_root(value: Int | Float) -> Float
{
    assert(rule {
        "The radicand cannot be negative": value >= 0
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

`value: Int | Float` accepts integers or floating-point values.
`assert(rule { ... })` then checks whether the argument satisfies the
function's requirement. This square-root function accepts only nonnegative
values; passing a negative value reports that “The radicand cannot be negative”
was not satisfied.

The function handles zero first, then repeatedly updates `estimate` until its
squared error is small enough. The manual implementation demonstrates a
complete program; production code can call `math::sqrt` directly.

Rules can also check business data and generate test inputs. After this example,
continue with the [retail tutorial](../../examples/retail/README_en.md), or see
the [Rules documentation](../Rules_en.md) for the complete syntax and checking
interfaces.

Run it from the repository root:

```sh
build/bin/tapas --stdout docs/examples/Basics_en.md
```
