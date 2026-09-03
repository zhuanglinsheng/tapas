# A First Look at Tapas

[简体中文](Basics_zh.md) | English | [Project Home](../../README_en.md)

The program below introduces variables, lists, functions, loops, and
conditionals in one place. Code saved in a `.tap` file can run directly, and
Tapas can also execute `tapas` code blocks in Markdown documents.

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

Run it from the repository root:

```sh
build/bin/tapas --stdout docs/examples/Basics_en.md
```
