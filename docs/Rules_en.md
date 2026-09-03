# Tapas Rules

[简体中文](Rules_zh.md) | English | [Project Home](../README_en.md)

A Rule is a parameterized, composable, checkable first-class value. Rules
express function contracts, business policies, and object invariants, and can
also serve as input to test generators or other interpreters. This document
covers the currently implemented Rule syntax, Types, public IR, and evaluator
API. See the [Language Reference](Syntax_en.md), [Standard Library](Stdlib_en.md),
and [Type System](TypeSystem_en.md) for the surrounding language.

```tapas
let docs_positive = rule (value: Int) {
    "value must be positive":
        value > 0
}

let docs_bounded = rule (value: Int) {
    require docs_positive(value)
    "value must be below ten":
        value < 10
}

assert(docs_bounded(5))

let docs_failed = rules::check(docs_bounded(12))
print(docs_failed["passed"])
print(len(docs_failed["violations"]))
print(docs_failed["violations"][0]["description"])
```

<pre class='Tapas-Return'>
false
1
value must be below ten
</pre>

## 1. Concepts and evaluation time

| Name | Category | Purpose |
|---|---|---|
| `rule` | expression keyword | Creates a Rule literal |
| `require` | Rule-body statement | Composes another Rule application |
| `assert` | root built-in | Checks a Rule and raises a runtime error on failure |
| `Rule` | value and Type | Immutable rule with a signature, IR, and capture environment |
| `RuleInstance` | value and Type | Rule with bound arguments |
| `rules` | standard package | Checks, inspects, constructs, and serializes Rule IR |
| `evaluators` | standard package | Consumes Rule IR through custom interpreters |

Calling a Rule only binds arguments and produces a `RuleInstance`; it does not
check the Rule. Checking begins when `assert`, `rules::check`, or
`evaluators::eval` consumes the instance. Instances can therefore be stored,
passed, and composed while assertions and recoverable business validation
share the same rule.

There is no package named `rule`. Lowercase singular `rule` is a keyword,
plural `rules` is a package, and capitalized `Rule` denotes the value category
or Type.

## 2. Rule literals

### 2.1 Parameters, Conditions, and local computations

Every Rule parameter requires a Type annotation. A statically known argument
count or Type mismatch is a compile error; a dynamically obtained Rule checks
its signature at runtime.

```text
let Between = rule (
    value  : Int,
    minimum: Int,
    maximum: Int,
) {
    value >= minimum
    value <= maximum
}

let instance = Between(5, 0, 10)
```

A zero-parameter Rule may omit its parameter list. A top-level Bool expression
in the body forms a Condition. Every Condition must hold; an empty Rule passes.

The left side of a descriptive colon must be a String. Its right side may be
one Bool expression or a description block. Each expression in a block forms
a separate Condition with the same description. Descriptions affect
diagnostics, reflection, and documentation, not logic or Rule Types.

```text
let ValidQuantity = rule (quantity: Int, minimum: Int, maximum: Int) {
    "minimum exceeds maximum":
        minimum <= maximum

    "quantity is outside the range": {
        quantity >= minimum
        quantity <= maximum
    }
}
```

A body-level `let` defines a local computation without forming a Condition.
Rule items are limited to local `let`, Conditions, and `require`; assignment,
`var`, control flow, imports, and direct I/O cannot be items. A Condition may
still call an ordinary function and retains ordinary call, short-circuit, and
error semantics.

### 2.2 Values, identity, and captures

Rules are ordinary values: they can be stored in containers, passed to
functions, returned, and exported. A directly bound literal may use its binding
name for diagnostics, but that display name affects neither Type equality nor
identity. `identical` compares Rule identity; Rules do not define content-based
`==`.

A source Rule records referenced outer bindings. Parameters and captures are
read through the Rule environment when checking occurs, rather than copied
into a separate snapshot when a RuleInstance is created.

```text
function below(maximum: Int) -> Rule[Int]
{
    return rule (value: Int) {
        value < maximum
    }
}
```

The public IR exposes capture descriptions but not environment addresses,
slots, or reference counts. A dynamically constructed Rule uses explicit
Constant and Parameter Terms and does not implicitly capture its construction
site.

### 2.3 `require`

`require` is valid only at the top level of a Rule body and must be followed by
a Rule application:

```text
let Positive = rule (value: Int) {
    value > 0
}

let SmallPositive = rule (value: Int) {
    require Positive(value)
    value < 10
}
```

Items execute in source order. A Requirement uses the target Rule's own
signature, IR, and capture environment. Separate references execute
separately. The checker tracks the active Requirement path and reports a cycle
when the same RuleInstance reappears on that path.

## 3. Checking APIs

`assert(rule: RuleInstance | Rule) -> Nil` is a root built-in, not a keyword.
It accepts a RuleInstance or a Rule requiring no arguments. Success returns
`nil`; the first failed Condition raises an unrecoverable runtime error.
`assert` remains active in every build mode.

Use an anonymous Rule for a function contract that is not reused:

```text
function withdraw(account, amount)
{
    assert(rule {
        "amount must be positive":
            amount > 0

        "insufficient balance":
            account.balance >= amount
    })

    account.balance = account.balance - amount
}
```

`rules::check(rule: RuleInstance | Rule) -> CheckResult` applies the same Rule
semantics but returns failure as data and collects violations in order.

| Field | Meaning |
|---|---|
| `status` | `Success` for a completed check in the current implementation |
| `passed` | Whether no Condition failed |
| `violations` | Ordered failed Conditions |
| `diagnostics` | Evaluation, structure, or evaluator diagnostics |

A false Condition still means the check executed successfully, so `status` is
`Success` and `passed` is `false`. The current `rules::check` raises an ordinary
runtime error for invalid arguments, unavailable environments, Term evaluation
errors, and unsupported Extensions; it does not yet return `Unsupported` or
`Failed` records. Use `assert` for program contracts and `rules::check` when
callers need to display, record, or process Condition failure.

## 4. Rule Types

An exact Rule Type contains only the ordered parameter Types, not names,
Conditions, or a result Type:

| Form | Meaning |
|---|---|
| `Rule`, `RuleInstance` | Any Rule or instance |
| `Rule[T, ...]` | Rule with the specified parameter Types |
| `RuleInstance[T, ...]` | Bound instance with those parameter Types |
| `types::rule(Type...)` | Constructs an exact Rule Type value |
| `types::rule_instance(Type...)` | Constructs an exact instance Type value |

Zero-parameter exact Types are `Rule[]` and `RuleInstance[]`. Parameters are
invariant; names and descriptions do not affect Type equality. Every exact
Rule Type is assignable to broad `Rule`, and likewise for instances.

```text
let IntRule = types::rule(types::Int)
let Positive: IntRule = rule (value: Int) {
    value > 0
}
let instance: RuleInstance[Int] = Positive(1)
```

## 5. The `rules` package

### 5.1 Inspecting public IR

`rules::inspect(rule: Rule | RuleInstance) -> RuleIR` returns immutable IR.
Reader functions return independent Lists; modifying those Lists does not
change the IR.

| Type | Purpose |
|---|---|
| `RuleIR` | Complete rule representation |
| `Parameter`, `Capture` | Parameter and capture descriptions |
| `Item`, `Condition`, `Requirement` | Ordered Rule items |
| `Term`, `term(Type)` | Broad or result-typed Term |
| `Origin` | Source range |
| `CheckResult`, `Violation`, `Diagnostic` | Check results |

All names above use the `rules::` namespace. Common readers are:

| API | Result |
|---|---|
| `parameters(ir)` | Parameters in signature order |
| `items(ir)` | Items in execution order |
| `terms(ir)` | Term table |
| `origin(value)` | Origin for an IR value |
| `semantic_hash(value)` | Semantic hash excluding display data |
| `content_hash(value)` | Content hash including descriptions and origins |

RuleIR exposes `display_name`, `source`, `version`, `parameters`, `captures`,
`terms`, `items`, and `origins`. A Term exposes `id`, `kind`, `type`,
`arguments`, `payload`, `provider`, and `version`. An Item exposes `kind`,
`term`, `rule`, `arguments`, and `description`.

Base Term kinds are `Constant`, `Parameter`, `Capture`, `Intrinsic`, `Call`,
`Construct`, `Convert`, and `Extension`. Tools should depend on these public
fields rather than runtime pointers.

### 5.2 Dynamic construction and serialization

| API | Purpose |
|---|---|
| `parameter(name, type)` | Creates a Parameter |
| `constant(value)` | Creates a Constant Term |
| `call(function, arguments)` | Creates a Call Term |
| `condition(term[, description])` | Creates a Condition |
| `requirement(rule, arguments)` | Creates a Requirement |
| `extension(provider, kind, arguments, payload)` | Creates an extension Term |
| `make(display_name, parameters, items)` | Validates and creates a Rule |
| `serialize(rule_or_ir)` | Produces a canonical String |
| `deserialize(data)` | Validates and restores a Rule |

```text
let value = rules::parameter("value", types::Int)
let lower = rules::constant(0)
let upper = rules::constant(10)

let InRange = rules::make("InRange", [value], [
    rules::condition(value >= lower, "value must be non-negative"),
    rules::condition(value <= upper, "value must be at most ten"),
])
```

Each `parameter` call produces a distinct identity. `make` validates the
parameter list, Bool Conditions, Requirement signatures, Term references, and
cycles in the Term graph. Source and dynamic Rules share one RuleIR model and
can be consumed by the same checkers and evaluators.

A dynamic Rule serialization is self-contained. A source Rule depends on its
checker and captures, so serialization carries runtime reconnection data.
`deserialize` fails outside that runtime or when its closure cannot be
restored; it never returns a semantically incomplete Rule. Unstable call
targets and payloads are also rejected.

## 6. The `evaluators` package

An Evaluator gives the same RuleIR another interpretation, such as
documentation extraction, symbolic validation, test generation, or
optimization. It cannot change item order or base Tapas operation semantics.

| Type | Purpose |
|---|---|
| `Evaluator` | Evaluator value |
| `Context` | Read-only context for one evaluation |
| `Result` | Common result structure |
| `Diagnostic` | Evaluator diagnostic |

All names above use `evaluators::`.

| API | Purpose |
|---|---|
| `make(name, version, evaluate[, compile])` | Creates an evaluator |
| `eval(instance, evaluator)` | Builds a Context and calls evaluate |
| `compile(rule, evaluator)` | Calls the optional compile handler |
| `binding(context, parameter_or_index)` | Reads an argument binding |
| `capture(context, capture_or_index)` | Reads a captured value |
| `value(context, term)` | Evaluates a Term with ordinary Tapas semantics |
| `requirement(context, requirement)` | Creates a child RuleInstance |

The evaluate handler receives a Context and returns a value with `status`,
`value`, `violations`, and `diagnostics`. The compile handler may be omitted;
`evaluators::compile` then returns `Unsupported`.

```text
let observer = evaluators::make("observer", 1, (context)
{
    return {
        "status": "Success",
        "value": evaluators::binding(
            context, context["ir"]["parameters"][0]),
        "violations": [],
        "diagnostics": [],
    }
})
```

An Evaluator must return `Unsupported` for a valid Term or provider it cannot
interpret rather than silently changing meaning. Context is read-only and
cannot mutate IR, argument bindings, or captures.

## 7. Execution model

```text
Rule literal
    -> immutable RuleIR + checker
    -> bind capture environment
    -> Rule
    -> bind arguments
    -> RuleInstance
    -> assert / check / evaluator
```

A source Rule compiles to shared IR and a checker, then binds its capture
environment at runtime. A dynamic Rule has no source-specific checker and uses
the general IR path. Public semantics do not prescribe interpretation,
ahead-of-time compilation, or caching; optimizations must preserve item order,
capture reads, error locations, and cycle detection.

## 8. Grammar

```ebnf
rule-literal = "rule",
               [ "(", [ rule-parameters ], ")" ],
               rule-block ;

rule-parameters = rule-parameter,
                  { ",", rule-parameter }, [ "," ] ;

rule-parameter = IDENTIFIER, ":", type-expression ;

rule-block = "{", separators, [ rule-item-list ], separators, "}" ;

rule-item-list = rule-item, { separator-run, rule-item } ;

rule-item = let-declaration
          | condition-statement
          | described-condition-statement
          | require-statement ;

condition-statement = expression ;

described-condition-statement = STRING, ":", separators,
                                ( expression | condition-block ) ;

condition-block = "{", separators, condition-statement,
                  { separator-run, condition-statement },
                  separators, "}" ;

require-statement = "require", rule-application ;
rule-application = expression ;
```

The `require` expression must produce a RuleInstance. `rule` and `require` are
reserved words; `assert`, `rules::check`, and evaluator APIs use ordinary call
syntax.

## 9. Current limitations

- `assert` failure is unrecoverable; use `rules::check` for a normal result.
- Requirement cycles are detected on execution paths and need not all be found
  at compile time.
- An Extension Term requires a checker or evaluator that understands its
  provider and version.
- Cross-process source-Rule serialization depends on reconnecting its checker
  and capture environment.
- An evaluator compile handler is optional; automatic specialized compilation
  is not guaranteed.
- Rule IR is immutable; changing a rule requires constructing a new Rule.
