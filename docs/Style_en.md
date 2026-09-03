# Tapas Code Style

[简体中文](Style_zh.md) | English | [Project Home](../README_en.md)

This document defines the recommended layout of Tapas source. Language syntax
determines whether code is valid; this style only normalizes valid code. The
`format` package shipped with Tapas implements the mechanical rules.

## Indentation and Spacing

- Use four spaces per indentation level and no tabs.
- Remove trailing whitespace and preserve the file's existing line-ending form.
- Do not put a space between a function name and `(`.
- Put one space between a keyword and an immediately following `(` or `{`, as
  in `if (`, `rule (`, `rule {`, and `else {`.
- Put one space between a closing parenthesis and a block-opening `{`.

When `rule` is the language keyword, write `rule (`. `types::rule` is an
ordinary function name, so its call remains `types::rule(`.

## Functions

For a single-line function signature, put `{` on the following line. Named
functions and function literals follow the same rule:

```text
function add(left: Int, right: Int) -> Int
{
    return left + right
}
```

When the parameter list already spans multiple lines, keep `{` on the final
signature line instead of adding another line:

```text
function structural_edits(
        source      : String,
        tokens      : List,
        clean_before: List[Bool],
        indents     : List[String],
        line_break  : String,
) -> Dictionary {
    return {}
}
```

Continuation parameters are indented eight spaces from the function
declaration. When a function or Rule parameter list spans multiple lines, align
all parameter `:` characters vertically. The alignment column immediately
follows the longest parameter name: that name has no space before `:`, while
shorter names are padded with spaces. Keep one space between `:` and the Type.
A single-line parameter list retains the compact `name: Type` form. This rule
depends on whether the parameter list crosses a physical line, not on a
line-length threshold. A return type split across lines also forms a multiline
signature.

## Control Flow

Control flow uses a compact C-style layout. Keep `elif` and `else` after the
closing `}` of the preceding block:

```text
if (condition) {
    handle_true()
} elif (alternative) {
    handle_alternative()
} else {
    handle_false()
}

while (condition) {
    advance()
}

for (let value in values) {
    consume(value)
}
```

Do not place a semicolon between `}` and `elif` or `else`. A complex condition
may wrap naturally inside its parentheses; `{` remains on the closing
parenthesis line.

## Rule Literals

Write a Rule used by only one assertion directly inside `assert`, without an
otherwise unused local name:

```text
assert(rule { value is Int | Float })
```

Keep one space between the Rule keyword and a parameter list:

```text
let Positive = rule (value: Int) {
    value > 0
}
```

For a multiline Rule parameter list, use the same alignment as for function
parameters:

```text
let WithinRange = rule (
        value  : Int,
        minimum: Int,
        maximum: Int,
) {
    value >= minimum
    value <= maximum
}
```

When a Condition has a String description, put its proposition on the next
line and indent it one additional level from the description. Leave one blank
line between adjacent described Conditions in the same Rule. Conditions
without String descriptions do not require blank lines:

```text
let Transferable = rule (balance: Int, amount: Int) {
    "amount must be positive":
        amount > 0

    "balance is insufficient":
        balance >= amount
}
```

When a String description introduces a Condition block whose Conditions share
that description, keep `{` after the colon instead of placing it on a separate
line. Indent the block Conditions one level and write them consecutively;
blank lines within the block are not required:

```text
"quantity is outside the range": {
    quantity >= minimum
    quantity <= maximum
}
```

An anonymous Rule declared directly through `assert` inside a function may
remain compact. When it contains exactly one Condition without a String
description, the entire assertion may share one line. Do not use this exception
for a described Condition or a Rule with multiple Conditions.

Blank lines before or after `assert` are not required. Within a function body,
use blank lines to separate logical groups such as argument assertions, local
setup, primary computation, and result handling. Consecutive statements in the
same group may remain compact.

## Formatting

Format files with:

```sh
tapas -m format examples/a.tap examples/b.tap
```

Check without writing with:

```sh
tapas -m format --check examples/a.tap examples/b.tap
```

`--check` is passed directly to the module; an empty `--` separator is not
needed. Check mode returns 0 when every file is formatted. Otherwise it prints
the paths needing changes and returns 1.
