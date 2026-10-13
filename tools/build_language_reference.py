#!/usr/bin/env python3
"""Build the self-contained Tapas language reference from the maintained docs."""

from __future__ import annotations

import html
import json
import os
import re
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "docs" / "LanguageReference.html"

SOURCES = [
    ("syntax", "docs/Syntax_zh.md", "词法、语法与核心语义"),
    ("types", "docs/TypeSystem_zh.md", "Type 系统"),
    ("rules", "docs/Rules_zh.md", "Rule 语言"),
]

INCLUDED = {str((ROOT / path).resolve()): anchor for anchor, path, _ in SOURCES}
DOCUMENT_TARGETS = {
    "syntax": "grammar",
    "types": "type-system",
    "rules": "rule-language",
}

REFERENCE_INTRO = r"""
# 阅读本参考 {#language-overview}

<div class="lead-grid">
<div class="lead-card"><strong>目标</strong><span>只依赖本页，理解并写出语法正确、语义明确的 Tapas 程序。</span></div>
<div class="lead-card"><strong>规范范围</strong><span>词法、值、作用域、表达式、函数、Type、Rule、模块与错误边界。</span></div>
<div class="lead-card"><strong>规范用语</strong><span>“必须”和“不得”是要求；“可以”表示允许；“未规定”表示程序不能依赖该行为。</span></div>
<div class="lead-card"><strong>查阅方式</strong><span>从头阅读可学习语言；按 <kbd>/</kbd> 或 <kbd>Ctrl K</kbd> 可作为参考手册搜索。</span></div>
</div>

本页是 Tapas 语言的规范性参考，同时提供一条从第一个程序到完整语义的学习路径。语言工具、命令行、标准库 API、求解器、C 接口、字节码和实现机制不属于本文范围；示例使用的 `print`、`assert`、`types::matches` 等名称由标准环境提供，本文只在它们与语言语义相交时说明其作用。

文中的每项规则按以下层次理解：示例先建立直觉；随后文字给出规范行为；标记为“编译错误”或“运行时错误”的示例说明边界。语法与上下文约束对实现具有规范意义，推荐写法则用于帮助人类和 AI 生成一致、清晰的程序。

## 第一个完整程序 {#first-program}

```tapas
var Positive = rule (value: Int) {
    'value must be positive':
        value > 0
}

function double_positive(value: Int) -> Int
{
    assert(Positive(value))
    return value * 2
}

let numbers: List[Int] = [1, 2, 3]
for (let number in numbers) {
    print(double_positive(number))
}
```

<pre class='Tapas-Return'>
2
4
6
</pre>

这个程序展示了 Tapas 的主要层次：`let` 建立绑定；`List[Int]` 是编译期 Type 约束；函数调用产生普通值；调用 `Positive(number)` 只建立 `RuleInstance`；`assert` 消费该实例时才检查条件。Rule 与 Type 都是一等值，但它们承担不同职责：Type 描述程序在编译期可以证明的值形状，Rule 描述需要在检查时成立的业务条件。

## 三个核心模型 {#three-models}

### 值与 Type

每个运行时值都有一个运行时 Type。源码中的 Type 标注是编译期约束，不执行隐式转换，也不会自动插入运行时检查。编译器无法确定一个动态值时可以允许程序继续；需要在动态边界确认结构时，程序必须显式检查。

```text
表达式 ──求值──► 运行时值 ──具有──► 运行时 Type
绑定     ──可带有──► 编译期 Type 约束
```

### Rule 与 RuleInstance

Rule 是可保存、传递和组合的规则值。调用 Rule 只绑定参数并返回 RuleInstance，不会立即检查。实例由检查接口消费时，Rule 体才按声明顺序求值。

```text
Rule ──绑定参数──► RuleInstance ──检查──► 通过或违规结果
```

### 值、引用与身份

`Bool`、`Int` 和 `Float` 是值类型，赋值和传参复制值本身。其他内置对象是引用类型，赋值和传参复制引用；两个绑定可以指向同一个可变对象。`copy` 对复合值执行浅复制，`identical` 检查运行时身份。

## 快速语言地图 {#language-map}

| 主题 | 核心写法 | 规范位置 |
|---|---|---|
| 源文本 | 注释、标识符、关键字、字面量、语句分隔 | [词法结构](#lexical-structure) |
| 值 | 标量、字符串、容器、数组、函数和其他引用值 | [值与内置类型](#values) |
| 绑定 | `let name[: Type] = value`、`var name = value` | [绑定、作用域与生命周期](#bindings) |
| 表达式 | 调用、索引、成员、隧道调用和运算符 | [表达式](#expressions) |
| 函数 | `function name(args) -> Type { ... }` | [函数与控制流](#functions-control) |
| Type | `List[Int]`、`A \| B`、字段 Type、递归 Type | [Type 系统](#type-system) |
| Rule | `rule (...) { ... }`、RuleInstance、组合与检查 | [Rule 语言](#rule-language) |
| 模块 | `import`、`from ... import`、文件模块与目录包 | [模块](#modules) |
| 错误 | 编译期、运行时和检查时边界 | [错误模型](#errors) |
| 完整形式 | EBNF、上下文约束和运算符语义 | [语法与参考表](#grammar) |

## AI 与代码生成检查表 {#ai-checklist}

生成 Tapas 程序时，必须特别注意以下边界：

- 条件只接受 `Bool`，不存在普通值的隐式真假转换。
- Type 标注不转换值，也不保证动态输入在运行时自动检查。
- 源码没有 `nil` 字面量；变量和集合不能保存 `Nil`。
- 调用 Rule 返回 RuleInstance，不等于执行或检查 Rule。
- Rule 体与普通函数体允许的语句不同；Rule 体不接受 `var`、赋值、控制流、导入或直接 IO。
- `require` 已移除；组合子 Rule 时直接写裸 RuleInstance。
- `dictionary::name` 和不跟调用的 `dictionary.name` 是只读成员访问；不能用它们赋值。
- `receiver.name(args)` 是隧道调用，等价于 `name(receiver, args)`，不是对象方法分派。
- `and`、`or` 短路；`&`、`|` 不短路，并可逐元素处理布尔数组。
- 比较、`to` 和 `in` 不能在同一未加括号表达式中连续组合。
- 字符串按字节索引，不按 Unicode 码点索引。
- 导入语法、Type 构造器和 Rule Type 应使用本文给出的形式，不得类比其他语言发明语法。
"""

VALUE_OVERVIEW = r"""
# 值与内置类型 {#values}

Tapas 程序只操作值。值分为值类型和引用类型；这个分类决定赋值、传参、捕获和容器存储时复制的是值本身还是对象引用。Type 标注描述允许出现的值，不会建立新的运行时存储方式。

| Type | 类别 | 可变性与核心行为 |
|---|---|---|
| `Nil` | 内部空值 | 表示过程没有可用结果；源码没有 `nil` 字面量，变量和集合不得保存它。 |
| `Bool` | 值类型 | 只有 `true` 与 `false`；条件不执行隐式真值转换。 |
| `Int` | 值类型 | 有符号整数；算术溢出和整数除零是运行时错误。 |
| `Float` | 值类型 | 浮点数；定义域、无穷与 NaN 行为遵循相应运算规定。 |
| `String` | 引用类型 | 可索引、切片和修改；长度与索引按字节计算。 |
| `List` | 引用类型 | 可变、有序，元素可以具有不同 Type，但不能保存 `Nil`。 |
| `Pair` | 引用类型 | 固定两个成员，由右结合的 `:` 构造。 |
| `Dictionary` | 引用类型 | 可变键值集合；成员访问是字符串键只读查找的语法糖。 |
| `Iterator` | 引用类型 | 表示整数范围等可迭代序列。 |
| `RealArray` / `BoolArray` | 引用类型 | 二维同质稠密数组；切片产生新数组，不进行形状广播。 |
| `Function` | 引用类型 | 可保存、传递和捕获环境；调用前计算调用目标，再从左到右计算参数。 |
| `Type` | 引用类型 | 不可修改的 Type 值；可作为普通值保存和传递。 |
| `Rule` | 引用类型 | 可绑定参数、组合并检查的规则值。 |
| `RuleInstance` | 引用类型 | Rule 与实参的绑定；不是 Bool，构造时不检查条件。 |
| `Library` | 引用类型 | 模块加载后暴露的只读名称集合。 |
| `Time` | 引用类型 | 绝对时间点；具体构造与格式化由标准环境提供。 |

以下章节先说明日常程序需要的值行为，再在 Type 系统与 Rule 章节给出完整静态规则。
"""

TYPE_BOUNDARY = r"""
## 运行时 Type 检查的边界 {#runtime-type-boundary}

Type 标注属于静态分析。静态已知的不兼容值必须被编译器拒绝；值的 Type 为 `Unknown` 时，编译器可以允许它进入带标注位置，运行时不会因为标注自动执行检查。程序跨越动态模块、宿主扩展或其他未知边界时，应显式使用标准环境提供的 `types::matches(value, Expected)`，并只在成功路径中把值作为更具体的 Type 使用。

`types::of`、`types::matches` 和反射函数属于标准库 API，不定义新的 Type 语法。它们不能替代编译期 Type 表达式，也不能使任意运行时 Type 值自动成为合法标注。
"""

RULE_INTRO = r"""
# Rule 语言 {#rule-language}

Rule 是 Tapas 的语言构件，不是返回 Bool 的普通函数。普通函数在调用时执行；Rule 在调用时只把实参与参数绑定成 RuleInstance，在 `assert`、`rules::check` 或其他检查器消费实例时才执行条件。这个延迟检查模型使条件保留名称、说明、依赖和违规路径，并允许同一份 Rule 被检查工具或求解工具消费。

学习本章时始终区分三个层次：Rule 是规则定义；RuleInstance 是规则与实参的绑定；检查结果是执行实例后得到的结论。三者不能互换，RuleInstance 也不存在通用的 Bool 隐式转换。
"""

RULE_CHECKING = r"""
## 检查 Rule {#rule-checking}

检查接口消费 RuleInstance，或消费不需要参数的 Rule。检查从第一项开始，按源码顺序执行；每个 Bool 条件必须为 `true`，每个裸 RuleInstance 必须成立。空 Rule 恒成立。条件求值错误、参数错误和循环依赖是运行时错误，不等同于条件不成立。

`assert(instance)` 是根内建函数。检查成功时它返回 `Nil`；第一个不成立的条件会产生不可恢复的运行时错误。它适合函数契约和程序不变量：

```tapas
function withdraw(balance: Int, amount: Int) -> Int
{
    assert(rule {
        'amount must be positive': amount > 0
        'insufficient balance': balance >= amount
    })
    return balance - amount
}
```

需要把“不成立”作为普通数据处理时，可以使用标准环境的 `rules::check(instance)`。它执行相同的语言检查语义，但返回可供程序读取的结果，而不把普通条件失败提升为运行时错误。检查结果的字段属于标准库 API；语言层只保证检查顺序、短路、错误传播以及通过／不通过的含义。
"""

RULE_EXECUTION = r"""
## Rule 执行模型 {#rule-execution-model}

Rule 字面量创建不可变的 Rule 值并捕获其自由变量；调用 Rule 绑定实参并创建 RuleInstance；只有检查实例时才执行 Rule 体。这个顺序是可观察语义，程序不能假定创建实例时已经验证条件。

```text
Rule 字面量 ──捕获环境──► Rule
Rule + 实参 ────────────► RuleInstance
RuleInstance ──检查─────► 通过 / 不通过 / 运行时错误
```

捕获值遵守普通闭包规则：可捕获的环境绑定按引用读取，可变对象仍共享身份。`parameters(rule)` 可取得参数名和 Type，`arguments(instance)` 可取得已绑定实参；这些观察操作不执行 Rule，也不能证明实例成立。返回的 List 是新容器，但其中的引用值不是深复制。

检查器可以采用解释、编译或缓存等实现方式，但不得改变项目顺序、短路行为、闭包读取、错误位置或循环检测。重复出现的子实例要求或逻辑表达式分别执行；需要复用一次计算时，应显式保存其 Bool 结果。
"""

RULE_BOUNDARIES = r"""
## Rule 语言边界 {#rule-boundaries}

- 每个 Rule 参数必须有 Type 标注；静态可知的参数数量或 Type 不匹配是编译错误。
- Rule 体只允许局部 `let`、Bool／RuleInstance 项和 `implies`；不允许 `var`、赋值、控制流、导入或直接 IO。
- 裸 Rule 不是条件，也不会自动变成实例；零参数 Rule 仍写成 `R()`，或直接交给允许零参数 Rule 的检查接口。
- RuleInstance 没有普通的 Bool 隐式转换。只有 Rule 自身表达式中的 `and`、`or`、`not` 和 `implies` 按本章定义检查其实例成立性。
- 空 Rule 恒成立；重复项目分别检查；递归依赖在实际检查路径上形成循环时产生运行时错误。
- `require` 已从语法中移除；组合子规则时直接写裸 RuleInstance。
- `assert` 的条件失败不可恢复；需要继续处理失败时使用返回普通检查结果的标准环境接口。
"""

ERROR_MODEL = r"""
# 错误模型 {#errors}

正确的 Tapas 程序不仅必须符合 EBNF，还必须满足上下文约束和运行时前置条件。错误按发生阶段分为以下类别：

| 阶段 | 含义 | 典型情况 |
|---|---|---|
| 词法／语法错误 | 源文本不能形成合法程序 | 无效转义、未闭合字符串、缺少分隔符 |
| 名称与上下文错误 | 语法形式合法，但当前位置不允许 | 未声明名称、循环外 `break`、Rule 体中的赋值 |
| 静态 Type 错误 | 编译器能够证明值不满足目标 Type | `String` 赋值给 `Int`、错误参数数量、无效 Type 构造 |
| 加载错误 | 模块或导入无法解析 | 路径不存在、包入口缺失、导入接口无目标名称 |
| 运行时错误 | 只有执行具体值时才能判定 | 动态类型错误、索引越界、整数溢出、除零、无效数组形状 |
| Rule 检查失败 | 程序合法运行，但业务条件不成立 | `rules::check` 返回违规；`assert` 将违规提升为运行时错误 |

Tapas 不以“未定义行为”允许程序任意执行。违反静态规则的程序必须被拒绝；违反动态前置条件时必须产生运行时错误。字典迭代顺序等明确写为“未规定”的行为仍可能在不同运行或实现中变化，程序不得依赖它。

## 常见非法假设 {#common-invalid-assumptions}

```text
if (1) { ... }                       // 错误：条件必须是 Bool
let value: Int = '1'                 // 错误：标注不执行字符串到整数转换
let empty = nil                      // 错误：源码没有 nil 字面量
object::field = value                // 错误：成员访问不是赋值位置
let checked = SomeRule(value)        // 合法，但结果是 RuleInstance，尚未检查
require SomeRule(value)              // 错误：require 已移除
matrix_a + matrix_b                  // 仅在元素 Type 和形状满足对应规则时合法
```

具体错误条件与相应语言构件一同定义；本章最后集中说明兼容形式和诊断保证。
"""

REFERENCE_END = r"""
# 正确程序检查表 {#correct-program-checklist}

在提交或生成 Tapas 程序前，应逐项确认：

1. 每个名称在使用前声明，并在所有到达读取位置的路径上完成初始化。
2. `let` 与 `var` 的选择符合所需作用域和闭包捕获行为。
3. Type 标注只使用静态 Type 表达式，赋值和返回值满足可赋值关系。
4. 动态边界需要的运行时验证已经显式执行。
5. 调用参数、索引、成员和运算符按照本文规定的 Type 与数量使用。
6. 条件产生 Bool；没有依赖其他语言的 truthiness 或隐式转换。
7. Rule 参数均带 Type 标注，Rule 体只包含允许的 Rule 项。
8. Rule 调用得到的实例由检查接口消费；程序没有把实例误当成 Bool。
9. 模块路径、导入别名和限定成员遵守模块语法与名称解析规则。
10. 程序没有依赖字典迭代顺序、诊断文本或其他明确未规定的行为。

附录中的 EBNF 定义语法形状，上下文约束定义仅靠 EBNF 无法表达的合法性，运算符表定义内置值的默认语义。若教程性解释与这些规范条目发生歧义，应以更具体的规范条目为准。
"""


def rewrite_links(text: str, source: Path) -> str:
    """Make links correct from docs/LanguageReference.html and internalize included docs."""

    def replace(match: re.Match[str]) -> str:
        destination = match.group(1)
        if destination.startswith(("http://", "https://", "mailto:", "#", "data:")):
            return match.group(0)
        target, marker, fragment = destination.partition("#")
        resolved = (source.parent / target).resolve() if target else source.resolve()
        included_anchor = INCLUDED.get(str(resolved))
        if included_anchor:
            return f"](#{DOCUMENT_TARGETS[included_anchor]})"
        from_docs = Path(os.path.relpath(resolved, OUTPUT.parent))
        suffix = f"#{fragment}" if marker else ""
        return f"]({from_docs.as_posix()}{suffix})"

    return re.sub(r"]\(([^)\s]+)\)", replace, text)


def extract_section(
    relative: str,
    heading_text: str,
    title: str,
    anchor: str,
    target_level: int = 2,
) -> str:
    """Extract one maintained section and re-level it inside this reference."""
    source = ROOT / relative
    text = rewrite_links(source.read_text(encoding="utf-8"), source)
    lines = text.splitlines()
    start = None
    source_level = None
    for index, line in enumerate(lines):
        match = re.match(r"^(#{1,6})\s+(.+?)\s*$", line)
        if match and match.group(2) == heading_text:
            start = index
            source_level = len(match.group(1))
            break
    if start is None or source_level is None:
        raise ValueError(f"missing heading {heading_text!r} in {relative}")

    end = len(lines)
    for index in range(start + 1, len(lines)):
        match = re.match(r"^(#{1,6})\s+", lines[index])
        if match and len(match.group(1)) <= source_level:
            end = index
            break

    body = lines[start:end]
    normalized = [
        f'<div id="{anchor}" class="source-anchor" aria-hidden="true"></div>',
        "",
        f"{'#' * target_level} {title}",
    ]
    for line in body[1:]:
        match = re.match(r"^(#{1,6})\s+(.+?)\s*$", line)
        if not match:
            normalized.append(line)
            continue
        level = min(6, target_level + len(match.group(1)) - source_level)
        label = re.sub(r"^\d+(?:\.\d+)*\.?\s+", "", match.group(2))
        normalized.append(f"{'#' * level} {label}")
    extracted = "\n".join(normalized).strip()
    if relative == "docs/Syntax_zh.md":
        return language_only_syntax_section(extracted)
    return extracted


def part(title: str, anchor: str, introduction: str = "") -> str:
    return f'<div id="{anchor}" class="source-anchor" aria-hidden="true"></div>\n\n# {title}\n\n{introduction}'.strip()


def language_only_rule_section(text: str) -> str:
    """Remove IR, serialization, evaluator, and result-schema details from Rule prose."""
    replacements = {
        "| `rules` | 标准包 | 检查、查看、构造和序列化 Rule IR |\n": "",
        "| `evaluators` | 标准包 | 通过自定义解释器消费 Rule IR |\n": "",
        "检查发生在`assert`、`rules::check`或`evaluators::eval`消费实例时。":
            "检查发生在`assert`、`rules::check`或其他检查接口消费实例时。",
        "小写单数`rule`是关键字，复数`rules`是标准包，首字母大写的`Rule`表示值类别或 Type。":
            "小写单数`rule`是关键字，首字母大写的`Rule`表示值类别或 Type；`rules`是标准环境提供的普通包名。",
        "`implies` 是语言内建的独立 Rule 项（IR kind 为 `Implication`），不是普通 Bool 表达式运算符。":
            "`implies` 是语言内建的独立 Rule 项，不是普通 Bool 表达式运算符。",
        "求值错误、循环或过深的检查依赖、不支持的 Term/Extension 仍然报错，不会被当作“不成立”。":
            "求值错误、循环或过深的检查依赖仍然报错，不会被当作“不成立”。",
        "RuleInstance 前件示例（无需手动调用 checker）：":
            "RuleInstance 前件示例（无需手动发起子规则检查）：",
        "构造外层 RuleInstance 不会执行检查。只有 checker 执行到`not`时才求值操作数；对实例执行一次完整检查，再取反其`passed`。":
            "构造外层 RuleInstance 不会执行检查。只有检查执行到 `not` 时才求值操作数；对实例执行一次完整检查，再对结论取反。",
        "子实例的不通过记录不并入外层 violations；独立否定条件不成立时记录外层 Condition，否定前件为假时蕴含不触发。默认 checker 检查子规则的全部条件，因此其中发生的错误不会被此前的 false 掩盖。":
            "子实例的失败记录不并入外层结果；独立否定条件不成立时记录外层 Condition，否定前件为假时蕴含不触发。默认检查会执行子规则的全部条件，因此其中发生的错误不会被此前的 false 掩盖。",
    }
    for old, new in replacements.items():
        text = text.replace(old, new)

    text = re.sub(
        r"\n公共 IR 只暴露捕获描述.*?构造调用点。\n",
        "\n",
        text,
        flags=re.DOTALL,
    )
    text = re.sub(
        r"\n这里的`Positive\(value\)`保留 Requirement 关系.*?Requirement 关系。\n",
        "\n这里的 `Positive(value)` 使用子规则自己的签名与捕获环境。重复项目分别检查；子规则不成立时，诊断应保留从外层项目到子规则的路径；循环依赖产生运行时错误。\n",
        text,
        flags=re.DOTALL,
    )
    text = re.sub(
        r"内部 Requirement 节点、`rules::requirement`动态构造接口和违规路径继续保留。",
        "诊断仍可保留子规则的违规路径。",
        text,
    )
    for pattern in [
        r"\n`rules::check` 的 `implications` 列表记录.*?分支已覆盖。\n",
        r"\n动态构造使用 `rules::implication.*?区分 Bool 值与 RuleInstance 的成立性检查，不能把实例当作普通 Bool。\n",
        r"\n原始前件实例仍可通过 IR 来源读取.*?并非缓存查询。\n",
        r"\nIR 用`Not` Term 表示否定.*?完整的数据流展开。\n",
        r"\n动态构造写为`rules::negation.*?```\n",
        r"\n包含`Not`的 IR 使用版本.*?运行时闭包。\n",
        r"\n自定义 evaluator 必须解释`Not`.*?源码否定子项会报错。\n",
        r"\nIR 使用`And`、`Or` Term.*?强制执行该分支。\n",
        r"\n动态接口为`rules::conjunction.*?配套的新运行时。(?:\n|$)",
    ]:
        text = re.sub(pattern, "\n", text, flags=re.DOTALL)
    text = text.replace(
        "`rule`是保留字；`assert`、`rules::check`和 evaluator API 使用普通函数调用文法。",
        "`rule`是保留字；`assert`、`rules::check`和其他检查接口使用普通函数调用文法。",
    )
    return re.sub(r"\n{3,}", "\n\n", text).strip()


def language_only_syntax_section(text: str) -> str:
    """Keep observable language behavior while removing implementation-facing detail."""
    text = text.replace(
        "Rule 系统还提供`Rule`、`RuleInstance`、`RuleIR`、`RuleTerm`、`RuleItem`和`Evaluator`等引用 Type。\n"
        "原生扩展可以通过 Tapas 对象接口提供新的引用 Type。",
        "Rule 语言还提供 `Rule` 与 `RuleInstance` 两种引用 Type。实现可以提供其他引用 Type，但它们不改变本页定义的核心语法。",
    )
    text = re.sub(
        r"Type 的构造和静态检查详见.*?完整程序见\[Type 示例\]\([^)]*\)。",
        "Type 的完整构造与静态规则见本页的 [Type 系统](#type-system)，Rule 值见 [Rule 语言](#rule-language)。",
        text,
        flags=re.DOTALL,
    )
    text = text.replace(
        "其中，矩阵乘法、同形数组加减、数组与标量相乘和数组取负需要在运行时使用 CBLAS；其余逐元素运算由 Tapas 自己实现。\n"
        "安装 Tapas 时不要求系统已经安装 BLAS，详细规则见第四部分。",
        "部分实数数组运算需要可用的数组后端；后端不可用时，该运算产生运行时错误。",
    )
    text = text.replace(
        "实数数组的部分运算必须通过 BLAS 完成。\n"
        "Tapas 编译和安装时不链接 BLAS；程序第一次执行这类运算时，Tapas 才尝试加载用户环境中的 LP64 CBLAS 动态库。\n"
        "如果找不到兼容的动态库，程序会在该运算处报运行时错误，不会改用 Tapas 内部的循环实现。",
        "标记为需要数组后端的运算只有在兼容后端可用时才能执行；否则在该运算处产生运行时错误。",
    )
    text = text.replace("需要 CBLAS", "需要数组后端")
    text = re.sub(r"CBLAS `[^`]+`", "需要数组后端", text)

    lines = text.splitlines()
    in_implementation_table = False
    cleaned = []
    for line in lines:
        if line.startswith("| 运算符 |") and line.rstrip().endswith("| 实现要求 |"):
            in_implementation_table = True
        if in_implementation_table and line.startswith("|"):
            line = line.rsplit("|", 2)[0] + "|"
        elif in_implementation_table and line.strip():
            in_implementation_table = False
        cleaned.append(line)
    return "\n".join(cleaned)


def assemble_markdown() -> str:
    syntax = "docs/Syntax_zh.md"
    types = "docs/TypeSystem_zh.md"
    rules = "docs/Rules_zh.md"
    pieces = [REFERENCE_INTRO]

    pieces.append(part(
        "词法结构",
        "lexical-structure",
        "词法规则决定源字节如何形成 token。编译器、格式化器和语言服务器必须共享这些边界；程序不得依赖未写入语法的自动修正。",
    ))
    for heading, title, anchor in [
        ("1. 源文本与位置", "源文本与位置", "source-text"),
        ("2. 空白、嵌套与语句分隔", "空白、嵌套与语句分隔", "whitespace-statements"),
        ("3. 注释", "注释", "comments"),
        ("4. 标识符与关键字", "标识符与关键字", "identifiers-keywords"),
        ("5. 数值字面量", "数值字面量", "numeric-literals"),
        ("6. 字符串字面量", "字符串字面量", "string-literals"),
        ("7. 运算符和标点", "运算符和标点", "operators-punctuation"),
        ("8. 导入路径", "导入路径 token", "import-path-tokens"),
        ("9. Markdown 源码提取", "Markdown 中的 Tapas 源码", "markdown-source"),
    ]:
        pieces.append(extract_section(syntax, heading, title, anchor))

    pieces.append(VALUE_OVERVIEW)
    for heading, title, anchor in [
        ("3. 值类型、引用类型与类型标注", "值类型、引用类型与 Type 标注", "value-reference-types"),
        ("5. 复制与同一性", "复制、别名与同一性", "copy-identity"),
        ("6. 字符串、列表、对和字典", "字符串、列表、Pair 与 Dictionary", "collections"),
        ("7. 稠密数组", "稠密数组", "dense-arrays"),
    ]:
        pieces.append(extract_section(syntax, heading, title, anchor))

    pieces.append(part(
        "绑定、作用域与生命周期",
        "bindings",
        "绑定把名称连接到值。声明、确定初始化、作用域和捕获规则共同决定在每个源码位置可以读取或修改哪个值。",
    ))
    pieces.append(extract_section(syntax, "4. 变量、作用域与赋值", "声明、初始化、作用域与赋值", "variables-scope"))

    pieces.append(part(
        "表达式",
        "expressions",
        "表达式先确定操作对象，再按照本章规定的顺序计算操作数。除短路逻辑外，不能假定编译器跳过具有可观察行为的子表达式。",
    ))
    pieces.append(extract_section(syntax, "8. 调用、索引、成员与隧道调用", "调用、索引、成员与隧道调用", "calls-index-members"))
    pieces.append(extract_section(syntax, "9. 运算符", "运算符与优先级", "operator-precedence"))

    pieces.append(part(
        "函数与控制流",
        "functions-control",
        "函数建立调用边界和闭包环境；控制流决定哪些表达式执行以及绑定是否确定初始化。",
    ))
    pieces.append(extract_section(syntax, "10. 函数与闭包", "函数与闭包", "functions"))
    pieces.append(extract_section(syntax, "12. 条件与循环", "条件、循环与控制转移", "control-flow"))

    pieces.append(part(
        "Type 系统",
        "type-system",
        "Tapas 的 Type 系统提供可选的编译期约束，同时保留 Type 作为一等运行时值。下面的规则定义静态 Type 表达式、Type 构造、可赋值关系和动态边界。",
    ))
    for heading, title, anchor in [
        ("1. 基本概念", "基本概念", "type-concepts"),
        ("2. 建立 Type", "建立 Type", "type-construction"),
        ("3. 类型标注与值构造", "Type 标注与值构造", "type-annotations"),
        ("4. 静态检查", "静态检查与可赋值关系", "static-checking"),
        ("6. 模块中的 Type", "模块中的 Type", "types-in-modules"),
        ("7. 当前限制与实现边界", "Type 系统边界", "type-boundaries"),
    ]:
        pieces.append(extract_section(types, heading, title, anchor))
    pieces.append(TYPE_BOUNDARY)

    pieces.append(RULE_INTRO)
    pieces.append(language_only_rule_section(
        extract_section(rules, "1. 概念与执行时机", "概念与执行时机", "rule-evaluation-time")
    ))
    pieces.append(language_only_rule_section(
        extract_section(rules, "2. Rule 字面量", "Rule 字面量", "rule-literals")
    ))
    pieces.append(RULE_CHECKING)
    pieces.append(language_only_rule_section(
        extract_section(rules, "4. Rule Type", "Rule Type", "rule-types")
    ))
    pieces.extend([RULE_EXECUTION, RULE_BOUNDARIES])
    pieces.append(language_only_rule_section(
        extract_section(rules, "8. 文法", "Rule 文法", "rule-grammar")
    ))

    pieces.append(part(
        "模块",
        "modules",
        "模块把源码文件组织为 Library 值并为名称解析提供边界。这里定义源码中的模块形式；命令行搜索路径选项属于工具接口。",
    ))
    pieces.append(extract_section(syntax, "13. 模块与导入", "模块与导入", "module-imports"))

    pieces.append(ERROR_MODEL)
    pieces.append(extract_section(syntax, "14. 错误与兼容写法", "错误、弃用与兼容形式", "compatibility-errors"))

    pieces.append(part(
        "语法与参考表",
        "grammar",
        "本部分集中提供完整 EBNF、上下文约束和内置运算符语义。EBNF 定义语法形状；仍须同时满足上下文、Type 和 Rule 规则。",
    ))
    for heading, title, anchor in [
        ("1. 记号与词法终结符", "记号与词法终结符", "grammar-notation"),
        ("2. 模块、语句列表与代码块", "模块、语句列表与代码块", "grammar-modules"),
        ("3. 声明与赋值", "声明与赋值", "grammar-declarations"),
        ("4. 控制流", "控制流", "grammar-control"),
        ("5. 导入", "导入", "grammar-imports"),
        ("6. 表达式与优先级", "表达式与优先级", "grammar-expressions"),
        ("7. 基本表达式与字面量", "基本表达式与字面量", "grammar-primary"),
        ("8. 上下文相关约束", "上下文相关约束", "context-constraints"),
    ]:
        pieces.append(extract_section(syntax, heading, title, anchor))

    pieces.append(part(
        "内置运算符语义",
        "operator-semantics",
        "下表定义内置值的默认运算行为。用户程序不能假定未列出的操作数组合会被隐式转换。",
    ))
    for heading, title, anchor in [
        ("1. 通用规则", "通用规则", "operator-general"),
        ("2. 一元和算术运算符", "一元和算术运算符", "arithmetic-operators"),
        ("3. 比较运算符", "比较运算符", "comparison-operators"),
        ("4. 逻辑、范围、成员测试和二元组", "逻辑、范围、成员测试和 Pair", "logical-range-pair"),
        ("5. 调用、索引和成员运算", "调用、索引和成员运算", "call-index-member-semantics"),
        ("7. 数组运算汇总", "数组运算汇总", "array-operator-summary"),
    ]:
        pieces.append(extract_section(syntax, heading, title, anchor))

    pieces.append(REFERENCE_END)
    sources = "、".join(
        f'<a href="{Path(os.path.relpath(ROOT / relative, OUTPUT.parent)).as_posix()}">{html.escape(relative)}</a>'
        for _, relative, _ in SOURCES
    )
    pieces.append(f'<p class="source-note">规范内容由 {sources} 中维护的规则重组；标准库、工具链和实现文档不纳入本页。</p>')
    return "\n\n".join(pieces)


CSS = r"""
:root{color-scheme:light dark;--bg:#f7f4ed;--paper:#fffdf8;--ink:#24221e;--muted:#6c675e;--line:#ddd6c8;--accent:#a53b27;--accent-2:#1f6f68;--code:#201f1d;--code-ink:#f3efe6;--shadow:0 10px 35px rgba(55,42,25,.08);--sidebar:19rem;--content:58rem;font-synthesis:none}
[data-theme="dark"]{--bg:#151615;--paper:#1d1f1d;--ink:#e9e6de;--muted:#aaa59b;--line:#393b37;--accent:#f08a70;--accent-2:#74c7bd;--code:#101210;--code-ink:#e9e6de;--shadow:none}
*{box-sizing:border-box}html{scroll-behavior:smooth;scroll-padding-top:5rem}body{margin:0;background:var(--bg);color:var(--ink);font:16px/1.72 system-ui,-apple-system,"Segoe UI","Noto Sans SC",sans-serif}a{color:var(--accent-2);text-decoration-thickness:.08em;text-underline-offset:.18em}button,input{font:inherit}.skip{position:fixed;left:1rem;top:-5rem;z-index:100;background:var(--paper);padding:.6rem 1rem}.skip:focus{top:1rem}.topbar{position:sticky;top:0;z-index:30;height:3.65rem;border-bottom:1px solid var(--line);background:color-mix(in srgb,var(--paper) 92%,transparent);backdrop-filter:blur(12px);display:flex;align-items:center;padding:0 1rem;gap:.8rem}.brand{font-weight:760;letter-spacing:-.02em;color:var(--ink);text-decoration:none}.brand small{color:var(--muted);font-weight:500}.top-actions{margin-left:auto;display:flex;gap:.4rem}.icon-button,.search-button{border:1px solid var(--line);background:var(--paper);color:var(--ink);border-radius:.55rem;padding:.42rem .7rem;cursor:pointer}.search-button{min-width:10rem;text-align:left;color:var(--muted)}.key{float:right;margin-left:1rem;font-size:.76rem;border:1px solid var(--line);border-radius:.25rem;padding:0 .3rem}.layout{display:grid;grid-template-columns:var(--sidebar) minmax(0,var(--content));justify-content:center;gap:3.5rem;padding:2.2rem 2rem 7rem}.sidebar{position:sticky;top:5.8rem;height:calc(100vh - 7rem);overflow:auto;padding-right:1rem}.sidebar-label{color:var(--muted);font-size:.72rem;font-weight:750;text-transform:uppercase;letter-spacing:.14em}.toc{list-style:none;padding:0;margin:.75rem 0}.toc li{margin:.08rem 0}.toc a{display:block;border-left:2px solid transparent;padding:.26rem .55rem;color:var(--muted);text-decoration:none;font-size:.88rem;line-height:1.35}.toc .level-3 a{padding-left:1.3rem;font-size:.82rem}.toc a:hover,.toc a.active{color:var(--accent);border-left-color:var(--accent)}main{min-width:0;background:var(--paper);border:1px solid var(--line);box-shadow:var(--shadow);border-radius:.8rem;padding:clamp(1.3rem,4vw,4rem)}.hero{border-bottom:1px solid var(--line);margin-bottom:3.5rem;padding-bottom:2.5rem}.eyebrow{color:var(--accent);font-size:.78rem;font-weight:800;letter-spacing:.14em;text-transform:uppercase}.hero h1{font-family:ui-serif,Georgia,"Noto Serif SC",serif;font-size:clamp(2.7rem,7vw,5rem);line-height:.95;letter-spacing:-.055em;margin:.45rem 0 1.2rem}.hero p{font-size:1.08rem;color:var(--muted);max-width:46rem}.hero-meta{display:flex;gap:1rem;flex-wrap:wrap;color:var(--muted);font-size:.84rem}.lead-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:.8rem;margin:1.5rem 0 2.5rem}.lead-card{border:1px solid var(--line);border-radius:.65rem;padding:1rem;display:flex;flex-direction:column}.lead-card strong{color:var(--accent);font-size:.82rem}.lead-card span{color:var(--muted);font-size:.9rem}.source-anchor{position:relative;top:-5rem}h1,h2,h3,h4{font-family:ui-serif,Georgia,"Noto Serif SC",serif;line-height:1.22;letter-spacing:-.025em;scroll-margin-top:5rem}main>h2{font-size:2.25rem;margin:5rem 0 1rem;padding-top:2.2rem;border-top:2px solid var(--ink)}main>h2:first-of-type{margin-top:0;border-top:0;padding-top:0}h3{font-size:1.6rem;margin:3.2rem 0 1rem}h4{font-size:1.23rem;margin:2.2rem 0 .7rem}h5{font-size:1.04rem}.heading-anchor{opacity:0;margin-left:.4rem;color:var(--muted);text-decoration:none;font:400 .78em system-ui}h1:hover .heading-anchor,h2:hover .heading-anchor,h3:hover .heading-anchor,.heading-anchor:focus{opacity:1}p,li{max-width:48rem}blockquote{border-left:3px solid var(--accent);margin:1.4rem 0;padding:.3rem 1rem;color:var(--muted);background:color-mix(in srgb,var(--accent) 5%,transparent)}code,kbd{font-family:"SFMono-Regular",Consolas,"Liberation Mono",monospace}p code,li code,td code{font-size:.88em;background:color-mix(in srgb,var(--accent) 8%,var(--paper));border:1px solid var(--line);border-radius:.25rem;padding:.08rem .28rem}pre{position:relative;overflow:auto;background:var(--code);color:var(--code-ink);border-radius:.6rem;padding:1.1rem 1.2rem;margin:1.3rem 0;font-size:.86rem;line-height:1.62;tab-size:4}pre code{font:inherit}.code-label{position:absolute;right:3.3rem;top:.55rem;color:#928f88;font:600 .68rem system-ui;text-transform:uppercase;letter-spacing:.08em}.copy-code{position:absolute;right:.55rem;top:.45rem;border:1px solid #474843;background:#292b28;color:#d6d3ca;border-radius:.35rem;padding:.2rem .42rem;cursor:pointer;font-size:.7rem}.tok-comment{color:#7f9182}.tok-string{color:#dfb56f}.tok-number{color:#b8a4e3}.tok-keyword{color:#ef8270;font-weight:650}.tok-type{color:#73c6bc}.tok-builtin{color:#8db6e2}.tok-operator{color:#d6d3ca}.Tapas-Return{border-left:3px solid var(--accent-2);background:color-mix(in srgb,var(--accent-2) 7%,var(--paper));color:var(--ink)}.Tapas-Return::before{content:"输出";display:block;color:var(--accent-2);font:750 .69rem system-ui;letter-spacing:.08em;margin-bottom:.45rem}table{display:block;overflow:auto;border-collapse:collapse;width:max-content;max-width:100%;margin:1.3rem 0;font-size:.9rem}th,td{border-bottom:1px solid var(--line);padding:.58rem .75rem;text-align:left;vertical-align:top}th{background:color-mix(in srgb,var(--ink) 4%,var(--paper));font-weight:730}.source-note{margin-top:3rem;border-top:1px dashed var(--line);padding-top:.8rem;color:var(--muted);font-size:.78rem}.search-dialog{width:min(44rem,calc(100vw - 2rem));max-height:min(42rem,calc(100vh - 2rem));border:1px solid var(--line);border-radius:.75rem;padding:0;background:var(--paper);color:var(--ink);box-shadow:0 24px 80px rgba(0,0,0,.3)}.search-dialog::backdrop{background:rgba(15,15,13,.62);backdrop-filter:blur(3px)}.search-head{position:sticky;top:0;background:var(--paper);border-bottom:1px solid var(--line);padding:.8rem;display:flex;gap:.55rem}.search-head input{width:100%;border:1px solid var(--line);background:var(--bg);color:var(--ink);border-radius:.45rem;padding:.65rem .8rem;outline:none}.search-results{list-style:none;margin:0;padding:.5rem;overflow:auto;max-height:34rem}.search-results a{display:block;text-decoration:none;padding:.65rem .75rem;border-radius:.45rem}.search-results a:hover,.search-results a:focus{background:color-mix(in srgb,var(--accent) 8%,var(--paper));outline:none}.result-title{display:block;color:var(--ink);font-weight:700}.result-text{display:block;color:var(--muted);font-size:.82rem}.empty{padding:2rem;color:var(--muted);text-align:center}.menu-button{display:none}.footer{color:var(--muted);font-size:.8rem;margin-top:5rem;padding-top:1.2rem;border-top:1px solid var(--line)}
@media(max-width:900px){.layout{display:block;padding:1rem}.sidebar{position:fixed;z-index:40;inset:3.65rem auto 0 0;width:min(21rem,88vw);height:auto;background:var(--paper);padding:1rem;transform:translateX(-105%);transition:transform .2s;border-right:1px solid var(--line)}body.nav-open .sidebar{transform:none}.menu-button{display:inline-block}main{padding:clamp(1rem,5vw,2.2rem)}.search-button{min-width:0}.search-button .label{display:none}}@media(max-width:560px){.lead-grid{grid-template-columns:1fr}.layout{padding:0}.layout main{border-radius:0;border-left:0;border-right:0}.brand small{display:none}.hero h1{font-size:2.8rem}}@media print{.topbar,.sidebar,.copy-code,.heading-anchor,.search-dialog{display:none!important}.layout{display:block;padding:0}main{box-shadow:none;border:0;padding:0}pre{white-space:pre-wrap;break-inside:avoid}a{color:inherit;text-decoration:none}}
"""

JS = r"""
(() => {
  const root = document.documentElement;
  const storedTheme = localStorage.getItem('tapas-reference-theme');
  if (storedTheme) root.dataset.theme = storedTheme;
  const themeButton = document.querySelector('#theme-toggle');
  const setThemeLabel = () => themeButton.setAttribute('aria-label', root.dataset.theme === 'dark' ? '使用浅色主题' : '使用深色主题');
  setThemeLabel();
  themeButton.addEventListener('click', () => {
    root.dataset.theme = root.dataset.theme === 'dark' ? 'light' : 'dark';
    localStorage.setItem('tapas-reference-theme', root.dataset.theme);
    setThemeLabel();
  });

  const headings = [...document.querySelectorAll('main h1[id], main h2[id], main h3[id]')];
  headings.forEach(heading => {
    heading.dataset.label = heading.textContent.trim();
    const anchor = document.createElement('a');
    anchor.className = 'heading-anchor'; anchor.href = '#' + heading.id; anchor.textContent = '§'; anchor.title = '复制本节链接';
    anchor.addEventListener('click', event => {
      if (navigator.clipboard) { event.preventDefault(); history.replaceState(null, '', anchor.href); navigator.clipboard.writeText(location.href); heading.scrollIntoView(); }
    });
    heading.append(anchor);
  });

  const toc = document.querySelector('#toc');
  headings.filter(h => h.tagName !== 'H3' || h.closest('main')).forEach(heading => {
    const li = document.createElement('li'); li.className = 'level-' + heading.tagName.slice(1);
    const a = document.createElement('a'); a.href = '#' + heading.id; a.textContent = heading.dataset.label;
    a.addEventListener('click', () => document.body.classList.remove('nav-open'));
    li.append(a); toc.append(li);
  });
  const tocLinks = new Map([...toc.querySelectorAll('a')].map(a => [a.hash.slice(1), a]));
  const observer = new IntersectionObserver(entries => entries.forEach(entry => {
    if (entry.isIntersecting) { toc.querySelectorAll('.active').forEach(a => a.classList.remove('active')); tocLinks.get(entry.target.id)?.classList.add('active'); }
  }), {rootMargin: '-10% 0px -78%'});
  headings.forEach(h => observer.observe(h));

  const escapeHtml = value => value.replace(/[&<>]/g, char => ({'&':'&amp;','<':'&lt;','>':'&gt;'}[char]));
  const tapasPattern = /(#[^\n]*|\/\/[^\n]*)|('(?:\\.|[^'\\])*'|"(?:\\.|[^"\\])*")|\b(\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)\b|\b(let|var|function|return|if|elif|else|while|for|break|continue|in|is|not|and|or|to|rule|implies|import|from|as|true|false|nil)\b|\b(AnyType|Nil|Bool|Int|Float|String|List|Pair|Dictionary|Iterator|Function|Rule|RuleInstance|Type|Library|RealArray|BoolArray|Time)\b|\b(assert|print|pprint|len|type|copy|identical|int|float|bool|str|list|pair|iter|array)\b|(::|->|==|!=|<=|>=|\+|-|\*|\/|%|@|=|<|>|\||:)/gm;
  document.querySelectorAll('pre > code').forEach(code => {
    const pre = code.parentElement;
    const classes = [...code.classList, ...pre.classList].join(' ');
    const isTapas = /\b(tapas|language-tapas)\b/i.test(classes);
    if (isTapas) {
      let cursor = 0, output = '', match;
      const source = code.textContent;
      tapasPattern.lastIndex = 0;
      while ((match = tapasPattern.exec(source))) {
        output += escapeHtml(source.slice(cursor, match.index));
        const kind = match[1] ? 'comment' : match[2] ? 'string' : match[3] ? 'number' : match[4] ? 'keyword' : match[5] ? 'type' : match[6] ? 'builtin' : 'operator';
        output += '<span class="tok-' + kind + '">' + escapeHtml(match[0]) + '</span>'; cursor = tapasPattern.lastIndex;
      }
      code.innerHTML = output + escapeHtml(source.slice(cursor));
    }
    const label = document.createElement('span'); label.className = 'code-label'; label.textContent = isTapas ? 'Tapas' : (code.className.match(/language-([\w-]+)/)?.[1] || 'Code'); pre.append(label);
    const button = document.createElement('button'); button.className = 'copy-code'; button.type = 'button'; button.textContent = '复制';
    button.addEventListener('click', async () => { await navigator.clipboard.writeText(code.textContent); button.textContent = '已复制'; setTimeout(() => button.textContent = '复制', 1200); }); pre.append(button);
  });

  const dialog = document.querySelector('#search-dialog'), input = document.querySelector('#search-input'), results = document.querySelector('#search-results');
  const searchable = [...document.querySelectorAll('main h1, main h2, main h3, main p, main td, main pre')].map(node => ({node, text: node.textContent.replace(/\s+/g, ' ').trim(), heading: node.matches('h1,h2,h3') ? node : node.closest('section')?.querySelector('h1,h2,h3') || [...headings].reverse().find(h => h.compareDocumentPosition(node) & Node.DOCUMENT_POSITION_FOLLOWING)}));
  const openSearch = () => { dialog.showModal(); input.value = ''; renderSearch(''); setTimeout(() => input.focus(), 0); };
  const renderSearch = query => {
    results.replaceChildren(); const needle = query.trim().toLocaleLowerCase();
    if (!needle) { results.innerHTML = '<li class="empty">输入关键词搜索语法、语义、Type、Rule 与示例</li>'; return; }
    const matches = searchable.filter(item => item.text.toLocaleLowerCase().includes(needle)).slice(0, 40);
    matches.forEach(item => { const li = document.createElement('li'), a = document.createElement('a'); const target = item.node.id ? item.node : item.heading; a.href = '#' + (target?.id || 'top'); const title = document.createElement('span'), snippet = document.createElement('span'); title.className = 'result-title'; snippet.className = 'result-text'; title.textContent = item.heading?.dataset?.label || 'Tapas Language Reference'; snippet.textContent = item.text.slice(0, 180); a.append(title, snippet); a.addEventListener('click', () => dialog.close()); li.append(a); results.append(li); });
    if (!matches.length) results.innerHTML = '<li class="empty">没有找到匹配内容</li>';
  };
  document.querySelector('#search-open').addEventListener('click', openSearch); input.addEventListener('input', () => renderSearch(input.value));
  document.addEventListener('keydown', event => { if ((event.key === '/' && !/input|textarea/i.test(document.activeElement.tagName)) || (event.key.toLowerCase() === 'k' && (event.ctrlKey || event.metaKey))) { event.preventDefault(); openSearch(); } });
  document.querySelector('#menu-toggle').addEventListener('click', () => document.body.classList.toggle('nav-open'));
})();
"""


def render_body(markdown: str) -> str:
    markdown = re.sub(
        r"^(#{1,6})\s+(.+?)\s+\{#([A-Za-z][A-Za-z0-9_-]*)\}\s*$",
        lambda match: (
            f'<div id="{match.group(3)}" class="source-anchor" aria-hidden="true"></div>'
            f"\n\n{match.group(1)} {match.group(2)}"
        ),
        markdown,
        flags=re.MULTILINE,
    )
    result = subprocess.run(
        [
            "pandoc",
            "--from=gfm+raw_html",
            "--to=html5",
            "--shift-heading-level-by=1",
            "--wrap=none",
        ],
        input=markdown,
        text=True,
        capture_output=True,
        check=True,
        cwd=ROOT,
    )
    return result.stdout


def build_html(body: str) -> str:
    metadata = {
        "@context": "https://schema.org",
        "@type": "TechArticle",
        "headline": "Tapas Language Reference",
        "inLanguage": "zh-CN",
        "description": "面向人类与 AI 的 Tapas 编程语言单页规范与学习参考。",
        "isBasedOn": [relative for _, relative, _ in SOURCES],
    }
    return f"""<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="description" content="面向人类与 AI 的 Tapas 编程语言单页规范与学习参考。">
<meta name="generator" content="tools/build_language_reference.py + pandoc">
<title>Tapas Language Reference</title>
<style>{CSS}</style>
<script type="application/ld+json">{json.dumps(metadata, ensure_ascii=False)}</script>
</head>
<body>
<a class="skip" href="#content">跳到正文</a>
<header class="topbar">
  <button id="menu-toggle" class="icon-button menu-button" type="button" aria-label="打开目录">☰</button>
  <a class="brand" href="#top">Tapas <small>Language Reference</small></a>
  <div class="top-actions">
    <button id="search-open" class="search-button" type="button"><span class="label">搜索参考手册</span><span class="key">/</span></button>
    <button id="theme-toggle" class="icon-button" type="button">◐</button>
  </div>
</header>
<div class="layout">
  <nav class="sidebar" aria-label="文档目录"><div class="sidebar-label">目录</div><ol id="toc" class="toc"></ol></nav>
  <main id="content">
    <header id="top" class="hero">
      <div class="eyebrow">Single-page · Offline-ready · Searchable</div>
      <h1>Tapas<br>Language Reference</h1>
      <p>一份同时面向人类学习与 AI 代码生成的语言参考：从第一个完整程序开始，逐步定义词法、值、表达式、函数、Type、Rule、模块、错误和完整语法。</p>
      <div class="hero-meta"><span>简体中文</span><span>语言规范</span><span>自包含 HTML</span><span>离线搜索</span></div>
    </header>
{body}
    <footer class="footer">本页由仓库文档生成。修改语言行为时，请先更新对应源文档，再运行 <code>python3 tools/build_language_reference.py</code>。</footer>
  </main>
</div>
<dialog id="search-dialog" class="search-dialog">
  <div class="search-head"><input id="search-input" type="search" placeholder="搜索，例如：RuleInstance、联合 Type、闭包……" autocomplete="off"><button class="icon-button" onclick="this.closest('dialog').close()" type="button">关闭</button></div>
  <ol id="search-results" class="search-results"></ol>
</dialog>
<script>{JS}</script>
</body>
</html>
"""


def main() -> None:
    markdown = assemble_markdown()
    body = render_body(markdown)
    output = build_html(body)
    OUTPUT.write_text(output, encoding="utf-8")
    print(f"wrote {OUTPUT.relative_to(ROOT)} ({len(output):,} bytes)")


if __name__ == "__main__":
    main()
