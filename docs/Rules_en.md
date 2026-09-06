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
    docs_positive(value)
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
| Bare expression | Rule item | Requires a true Bool or a satisfied RuleInstance |
| `and`, `or`, `not` | Logical expression | Combines or negates Bool/RuleInstance satisfaction and returns Bool |
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
in the body forms a Condition; a bare instance forms a child obligation. Every item must hold; an empty Rule passes.

The left side of a descriptive colon must be a String. Its right side may be
one Bool/RuleInstance expression or a description block. Each expression in a block forms
a separate condition or child obligation with the same description. Descriptions affect
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
Rule items are limited to local `let`, Bool/RuleInstance items, and `implies`; assignment,
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

### 2.3 Bare instances and composition

Each top-level Rule expression is an obligation: a Bool result must be true, and a RuleInstance result must hold. Write the instance directly to compose a child rule:

```text
let Positive = rule (value: Int) { value > 0 }
let SmallPositive = rule (value: Int) {
    Positive(value)
    value < 10
}
```

`Positive(value)` retains a Requirement relationship and uses the child's signature, IR, and capture environment. Child violations retain their `requirement_path`. Separate items are checked separately; cyclic dependencies remain errors. Source IR records Requirement for statically known instance items. Dynamic expressions such as AnyType acquire a Requirement relationship in the violation path when they evaluate to an instance.

The `require` keyword has been removed: migrate `require R(x)` to `R(x)`. It is now available as an ordinary identifier. Internal Requirement nodes, the `rules::requirement` builder, and violation paths remain supported.

### 2.4 Implication items

`implies` is a language-provided Rule item (`Implication` in the IR), not a general Bool expression operator. Like a Condition, it expresses a constraint, but also preserves its antecedent, consequents, and trigger information.

```text
let Exchange = rule (approved: Bool, stock: Int, quantity: Int) {
    'Approved exchanges require sufficient stock':
        approved implies {
            quantity > 0
            stock >= quantity
        }
}
```

The antecedent may be Bool or RuleInstance, including their union; parentheses are optional. Consequents remain Bool. Multiple consequents require braces and form a conjunction. The formatter recommends bare antecedents only for a single variable; calls use parentheses. Bare consequents are recommended only for a single Bool variable.

The checker evaluates the antecedent once. False skips all consequents and passes logically without triggering. True checks all consequents in order and records each failure with its source location. Evaluation errors remain runtime errors, not skipped branches.

`rules::check` adds an `implications` list, including checks reached through bare instance items. Each entry has `item`, `instance`, `triggered`, `passed`, and `consequents_checked`. A skipped item has `passed: true` and `consequents_checked: 0`; this does not establish branch coverage.

The first version only supports body-level implication items: no chains, nesting, empty consequent blocks, declarations, control flow, or bare instance obligations inside consequent blocks. Existing expression newline boundaries apply.

Dynamic construction uses `rules::implication(antecedent, consequents[, description]) -> rules::Item`: the antecedent Term may have Bool, RuleInstance, or their union Type; consequents remain a nonempty List of Bool Terms. `antecedent` preserves the original Term and its Type. RuleInstance antecedents require IR version 3 / `TPIR3`; Bool-only implications retain version 2, and readers accept `TPIR1/2`. Source Rule serialization retains its runtime-reconnection limitation. Dynamic IR declaring instance parameters remains portable: the caller binds fresh instances after restoration. Custom evaluators must distinguish Bool values from RuleInstance satisfaction checks.

RuleInstance antecedents need no explicit checker call:

```tapas
let docs_positive_premise = rule (value: Int) { value > 0 }
let docs_guarded = rule (value: Int) {
    (docs_positive_premise(value)) implies { value < 10 }
}
assert(docs_guarded(-1))
assert(docs_guarded(5))
```

This means $Holds(R(p)) \\Rightarrow Q$. The antecedent expression is evaluated once and its instance checked once. Failure skips consequents without merging antecedent violations into the enclosing result, unlike the standalone item `R(p)`. Evaluation errors remain runtime errors; recursive antecedents have a depth limit.

The original instance remains accessible through source IR. The default checker reuses the truth recorded for that check, rather than evaluating twice for reporting. Explicit evaluator reads of source Terms may rerun the source checker; they are not cache queries.

A bare Rule is invalid, including a zero-parameter Rule: write `R()`. Consequents remain Bool. There is no global RuleInstance-to-Bool conversion. Rule-local logical expressions check satisfaction as described below. Instance negation uses `not`, described below; explicitly retain a Bool result when sharing a check across expressions.

### 2.5 Logical negation `not`

Inside a Rule's own expressions, `not` accepts Bool, RuleInstance, or their union and produces Bool. In ordinary functions it still accepts only Bool; defining or calling a function inside a Rule does not change that function's semantics. A bare Rule, even with no parameters, must first be instantiated as `R()`.

```tapas
let Positive = rule (value: Int) { value > 0 }
let NonPositive = rule (value: Int) { not Positive(value) }
let Contract = rule (value: Int) {
    'lower bound for non-positive values': not Positive(value) implies { value >= -10 }
}
assert(NonPositive(-1))
assert(Contract(1))
```

`not R(x)` negates the complete check result. For a conjunction of conditions, at least one violation makes its negation hold; it does not negate each condition and conjoin the results. An empty Rule holds, so its negation fails.

Binding the outer RuleInstance does not check anything. When the checker reaches `not`, it evaluates the operand once, checks an instance once, and negates its `passed` result. `not not R(x)` checks R only once: the outer negation consumes Bool. Short-circuiting still applies: `true or not R(x)` neither constructs nor checks R(x).

Evaluation errors, cyclic or excessively deep check dependencies, and unsupported Terms/Extensions remain errors, not logical false. Child violations do not become outer violations. A failed standalone negation reports its outer Condition; a false negated antecedent leaves an implication untriggered. The default checker checks all child conditions, so an earlier false does not hide later evaluation errors.

The Bool result can appear as a Condition, an implication antecedent or consequent, an `and`/`or` operand, or a Rule-local `let` initializer. Repeated occurrences run separately. To reuse one result, bind `let rejected = not R(x)` inside the Rule. Ordinary functions and `if` still reject implicit instance-to-Bool conversion. Rule-local `and` and `or` are described below.

IR represents negation with a `Not` Term: Bool `type`, exactly one original operand in `arguments`, and nil `payload`. Instance operands retain their Type. Source negations preserve this structure; other source expressions may remain `Construct` Terms with contained negations in `arguments`. Source-local bindings retain the existing checker/capture mechanism rather than promising a complete data-flow expansion.

Use `rules::negation(operand)` for dynamic construction. The operand is a Bool/RuleInstance Term; an AnyType Term is checked at runtime. The resulting Bool Term works with `rules::condition` and `rules::implication`:

```tapas
let premise = rules::parameter('premise', types::RuleInstance)
let rejected = rules::negation(premise)
let Reject = rules::make('Reject', [premise], [rules::condition(rejected)])
assert(Reject(Positive(-1)))
```

IR containing `Not` uses version 4 / `TPIR4`; readers retain `TPIR1/2/3` compatibility. Other IR retains its previous version rules. Dynamic IR can be restored in another process and bound to fresh instances. Source Rules still require their originating runtime closure.

Custom evaluators must interpret `Not` as Bool negation or negated instance satisfaction. Neither Unsupported nor Failed means logical false; evaluators lacking support should return Unsupported. For source Not Terms, `evaluators::value` returns the recorded checker result without a second child check; reading the operand returns the original instance. Each API call may still rerun the source checker: there is no cross-call cache. Requesting a source negation subterm skipped by short-circuiting is an error.


### 2.6 Logical composition `and` / `or`

Within a Rule's own expressions, `and` and `or` accept Bool, RuleInstance, or their union and return Bool. AnyType operands are checked at runtime. Ordinary functions remain Bool-only even when called by a Rule. Elementwise `&` and `|` do not acquire instance semantics.

```tapas
let docs_in_range = rule (x: Int) { x >= 0; x <= 10 }
let docs_override = rule (x: Int) { x == 42 }
let docs_allowed = rule (x: Int) {
    docs_in_range(x) or docs_override(x)
    not (x < 0)
    (docs_in_range(x) and not docs_override(x)) implies { x <= 10 }
}
assert(docs_allowed(5))
assert(docs_allowed(42))
```

Operands evaluate left to right. A false left operand skips the right of `and`; a true left operand skips the right of `or`, including instance construction. Each reached instance is checked once. Errors propagate rather than becoming false and triggering another alternative. Evaluation occurs when the outer checker reaches the expression. Precedence remains `not > and > or`, with comparisons binding more tightly than not.

Two bare instance items are independent obligations and collect child violations separately. `R(x) and S(x)` is one short-circuit Condition: its failure reports the outer condition, not all child violations. A successful `or` has no violation from its unsuccessful branch. Logical expressions also work in implication antecedents/consequents, local `let` initializers, and larger Bool expressions. Consequents themselves still require Bool.

IR uses `And` and `Or` Terms with Bool Type, nil payload, and exactly two ordered operands. Do not turn them into unordered sets or eagerly evaluated legacy Intrinsics. Source IR retains logical nodes and original instance operands; local bindings keep the existing checker mechanism. `evaluators::value` reads the recorded source result without a repeated child check. Reading a skipped source operand is an error, not a request to execute that branch.

Dynamic construction uses `rules::conjunction(left, right)` and `rules::disjunction(left, right)` with Term operands. IR containing And/Or uses version 5 / TPIR5; readers accept TPIR1–4. Other IR retains its previous version rules. Custom evaluators must preserve order, short-circuiting, instance checks, and error propagation, or return Unsupported. Source bytecode requires its matching new runtime.


## 3. Checking APIs

### 3.1 `assert`

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

### 3.2 `rules::check`

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
| `InstanceOf[R]` | Instance bound by the same runtime Rule R; infers its parameters without asserting satisfaction |
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
| `item(rule, target)` | Direct Item selected by integer position, description, or Item identity |
| `terms(ir)` | Term table |
| `origin(value)` | Origin for an IR value |
| `semantic_hash(value)` | Semantic hash excluding display data |
| `content_hash(value)` | Content hash including descriptions and origins |

RuleIR exposes `display_name`, `source`, `version`, `parameters`, `captures`,
`terms`, `items`, and `origins`. A Term exposes `id`, `kind`, `type`,
`arguments`, `payload`, `provider`, and `version`. An Item exposes `kind`,
`term`, `rule`, `arguments`, and `description`.

Base Term kinds are `Constant`, `Parameter`, `Capture`, `Intrinsic`, `Call`,
`Construct`, `Convert`, `Extension`, `Not`, `And`, and `Or`. Tools should depend on these public
fields rather than runtime pointers.

### 5.2 Dynamic construction and serialization

| API | Purpose |
|---|---|
| `parameter(name, type)` | Creates a Parameter |
| `constant(value)` | Creates a Constant Term |
| `call(function, arguments)` | Creates a Call Term |
| `conjunction(left, right)` | Creates an ordered, short-circuiting And Term |
| `disjunction(left, right)` | Creates an ordered, short-circuiting Or Term |
| `negation(operand)` | Creates a Not Term for Bool or RuleInstance satisfaction |
| `condition(term[, description])` | Creates a Condition |
| `implication(antecedent, consequents[, description])` | Bool/RuleInstance antecedent Term and nonempty Bool consequent Terms |
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

Root `parameters(value)` reads declaration-order name/Type pairs from a Function, Rule,
RuleInstance, or RuleIR. Root `arguments(instance)` reads declaration-order bound
values from a RuleInstance. Neither query runs or checks the Rule; a failing
instance can still be inspected. Zero parameters produce an empty list, whereas
unsupported objects raise an error. Named/anonymous functions, closures, and function copies retain declaration metadata.
Unannotated parameters use available contextual Types, otherwise AnyType. Standard-library
functions use their registered signatures. Old bytecode functions must be recompiled;
third-party native functions without metadata raise an explicit error.

This is declaration reflection, not a complete arity API: `(...)` has no named
parameters and returns an empty list despite being variadic. Pairs do not encode
optional/default/variadic flags.

Both return fresh lists. Replacing/removing list entries cannot change the original
bindings or declarations, but mutable argument objects retain shared references:
this is not a deep snapshot. Types are actual Type values, not strings.
The existing `rules::parameters(ir) -> List[Parameter]` IR API remains unchanged.

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
          | implication-statement ;

condition-statement = expression ;
implication-statement = [ STRING, ":", separators ],
                        expression, "implies",
                        ( expression | condition-block ) ;

described-condition-statement = STRING, ":", separators,
                                ( expression | condition-block ) ;

condition-block = "{", separators, condition-statement,
                  { separator-run, condition-statement },
                  separators, "}" ;

rule-application = expression ;
```

Standalone items produce Bool or RuleInstance; bare Rules are not automatically instantiated. `rule` is a
reserved word; `assert`, `rules::check`, and evaluator APIs use ordinary call
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

## 10. Points, integer ranges, and membership

`rules::points(element_type, ...values)` creates `PointsOf[T]`, preserving the supplied element Type. It accepts any existing Type, including enums, records, containers, functions, and rules. Every member is checked against T; duplicates are removed using `identical`. Empty points are allowed. In value expressions use existing Type values such as `types::Int`; annotations can use `PointsOf[Int]`.

`rules::range(start, end)` creates `RangeOf[Int]`, a closed interval including both endpoints. This first version only accepts Int endpoints. Float endpoints, mixed types, and reversed bounds are rejected. Equal bounds form a singleton. Intervals are never expanded into lists.

```tapas
let domain_points: PointsOf[Int] = rules::points(types::Int, 1, 4, 9)
let domain_range: RangeOf[Int] = rules::range(0, 5)
let domain_member = rule (x: Int) { x in domain_points and x in domain_range }
assert(domain_member(4))
print(rules::check(domain_member(9))::passed)
```
<pre class='Tapas-Return'>
false
</pre>

Membership works inside and outside Rules. A candidate with an incompatible Type is not a member. The collection has no mutation interface, but composite members retain normal Tapas reference and `identical` semantics; this is not a deep snapshot. Construction order is retained for display, not as a sampling preference.

Points expose `type` and `values`; ranges expose `type`, `start`, and `end`. `values` is a fresh list with shared member references. `len(points)` returns the stored point count after deduplication; `len(range)` is unsupported. `types::of` preserves the parameterized Type, and `types::parameters` exposes `item`. Annotations from dynamic sources are checked at runtime.

RuleIR represents membership as a Bool `In` Term with two ordered operands: value, then domain. Source IR retains both operands and source origins. Evaluation follows existing Tapas membership order (right operand before left), once each, and respects surrounding short-circuit expressions. Reading a source Term or operand through `evaluators::value` runs the checker and reads its recorded value without separately reevaluating the operand. Reading a skipped operand fails.

Dynamic IR can use `rules::membership(value_term, domain_term)`, or `in` with either operand being a Term. The result is an In Term suitable for `rules::condition`.

In, domain constants, and domain parameter Types use TPIR6, with TPIR1–5 read compatibility. Integer intervals and points containing transportable scalars/Types support cross-process serialization. Arbitrary object graphs, closures, or rule identities remain subject to existing serialization limits and fail explicitly when unsupported. Source Rules still require their original checker and closure environment.

## 11. Restricting an existing Rule

`rules::restrict(base, ...restrictions) -> Rule` accepts a Rule and zero or more `Pair[String : AnyType]` arguments. Keys name parameters. Concrete values add `==` conditions; `RangeOf[Int]` and `PointsOf[T]` add `in` conditions.

```tapas
let restrict_base = rule (quantity: Int, approved: Bool) {
    quantity >= 0
    approved
}
let restrict_trial = rules::restrict(restrict_base,
    'quantity': rules::range(1, 10),
    'approved': true,
)
assert(restrict_trial(4, true))
```

The result is an ordinary Rule with a new identity and the original parameter names, types, and order. Calls still supply every argument. The original Rule is unchanged and retained through a Requirement, preserving its checker, closure, and diagnostics. Added conditions use the parameter name as their description. No restrictions creates an equivalent new Rule. Repeated restrictions intersect; contradictory restrictions and empty points are valid constructions with no satisfying instances. Construction does not run a solver.

Unknown parameter names, non-string keys, and incompatible concrete values or point members are rejected. Range restrictions initially require Int or AnyType parameters. Empty points denote an empty domain. Names refer only to top-level parameters, not dotted field paths. Cross-process serialization remains subject to the existing limitations for Rule constants because the result retains the original Rule.

## 12. Selecting and transforming Rule Items

`rules::item(rule, target)`, `rules::drop(rule, target)`,
`rules::violate(rule, target)`, `rules::drop_if(rule, target, premise)`, and
`rules::violate_if(rule, target, premise)` share one target convention:

- an Int is a zero-based position in `rules::items(rule)`;
- a String exactly matches a direct Item's `description`, and missing or
  ambiguous matches fail explicitly;
- an Item must belong directly to the input Rule and is matched by identity.

```tapas
let transform_bounds = rule (value: Int) {
    'lower bound': value >= 0
    'upper bound': value <= 10
}

let lower_item = rules::item(transform_bounds, 'lower bound')
let without_lower = rules::drop(transform_bounds, lower_item)
let below_lower = rules::violate(transform_bounds, 0)

assert(without_lower(-1))
assert(below_lower(-1))
assert(rule { not below_lower(0); not below_lower(11) })
```

`drop` removes the selected Item. `violate` retains every other direct Item and
replaces the target with the negation of its satisfaction. A Condition `C`
becomes `not C`; a Requirement `Child(...)` becomes `not Child(...)`; violating
`A implies B` is equivalent to `A and not B`. Multiple implication consequents
are negated as one conjunction.

`drop_if` and `violate_if` take a premise Rule with the same parameter
signature as the source Rule and apply it to the same arguments. Let `P` be
the premise's truth and `t` the target Item's satisfaction:

- `drop_if` replaces the target with `P or t`: when `P` holds, `t` is no
  longer constrained; otherwise `t` must hold;
- `violate_if` replaces it with `(P or t) and (not P or not t)`: when `P`
  holds, `t` must fail; otherwise `t` must hold.

```tapas
let negative = rule (value: Int) {
    value < 0
}
let conditional_bounds = rules::violate_if(
    transform_bounds,
    'lower bound',
    negative,
)

assert(conditional_bounds(-1))
assert(conditional_bounds(0))
```

A different parameter count or corresponding parameter Type fails during
construction. The premise retains its own closure, whose current captures are
read whenever the derived Rule is checked or solved.

The result is an ordinary Rule with the same parameter signature and a new
identity. The original Rule is unchanged. A derived source Rule retains its
checker and closure, so ordinary checking and solving read captures at use
time. Transformations address direct Items only and do not recursively search
Requirements. To target a child precisely, transform that child Rule. Because
descriptions can change or repeat, programmatic code should select an Item once
by integer position and then pass the Item object.

## 13. Structured source expression IR

Source Rules now produce an expression graph alongside their checker. Parameter references reuse declaration identities; Capture references identify closure slots. Defining a Rule does not read or freeze its captured values.

```tapas
var ir_minimum = 3
let ir_example = rule(x: Int) {
    let y = x + 1
    y >= ir_minimum
}
let ir_comparison = rules::inspect(ir_example)::items[0]::term
print(ir_comparison::kind)
print(ir_comparison::payload)
assert(ir_example(2))
ir_minimum = 5
assert(rule { not ir_example(2) })
```
<pre class='Tapas-Return'>
Intrinsic
>=
</pre>

The condition is `Intrinsic(">=", Local(y), Capture(ir_minimum))`. The local initializer is `Intrinsic("+", Parameter(x), Constant(1))`. A local reference is a Construct with payload `local:<binding number>:<name>` and one initializer operand. Repeated references share the initializer graph rather than expanding or executing it repeatedly.

| Expression | Representation |
|---|---|
| Scalar literal | Constant containing its actual Int, Float, Bool, String, or Nil value |
| Parameter / outer variable | Parameter / Capture with binding identity |
| Arithmetic, comparison, Pair | Intrinsic with operator payload and operands in source left/right order |
| Unary signs | Intrinsic `pos` / `neg` |
| `and`, `or`, `not`, `in` | And, Or, Not, In |
| `value::field` | Intrinsic `member`, receiver and field-name Constant |
| `values[index]` | Intrinsic `index`, receiver followed by index expressions |
| List / dictionary | Construct `list` / `dictionary`; dictionary operands alternate keys and values |
| Slice | Construct `slice:...`, recording omitted endpoints and existing endpoint expressions |
| Function or Rule application | Call with a target Term in payload and argument expressions in arguments |
| Tunnel call `x.f(...)` | Call target is Construct `tunnel:f`, containing the actual function binding and receiver, applied using tunnel semantics |
| `to` conversion | Convert retaining both operands |

Function and Rule literals introduce separate lexical scopes and currently remain explicit Extension `opaque:...` nodes. Unresolved special references such as `this` use Extension `reference:...`. These are explicit analysis boundaries, not expressions a solver may guess. Applications of captured Rules retain their target bindings and argument graphs, allowing later query preparation to read and recursively analyze those Rules. Structured IR does not imply solver support for arbitrary functions.

`evaluators::capture(context, capture)` and `evaluators::value(context, capture)` read the current Rule closure without executing its checker. The C API `trule_read_capture` provides the same identity-checked access for future query preparation; it does not create a snapshot.

For recorded source operation expressions, `evaluators::value` executes the checker once and reads the expression record without separately replaying the expression. Evaluation order and call counts are preserved. Reading a short-circuited expression fails because it has no record. Each `evaluators::value` call is a separate evaluation request, without cross-request caching. Parameters and captures read their bindings directly, and synthetic field-name Constants return their values directly. Synthetic slice descriptions and tunnel targets have no independent evaluation records.

New source graphs use TPIR7, with backward reading support for TPIR1–6. Source Rule serialization retains its existing same-process closure reconnection limitation. This layer supplies analyzable structure and capture access; the `solve` package handles query preparation, backend translation, and solving.

## 14. Feasibility queries

`solve::hold(Rule | RuleInstance)` now has an executable CP-SAT backend. It reads the current context at call time and returns a status with a checker-verified witness when feasible. Solve declares a default closed domain of ±10¹² for input Int parameters, configurable through `TAPAS_SOLVE_INT_MIN/MAX`. The domain is applied before presolve, including to bound Int arguments. Results use `scope: configured`: `unsat` means no solution in the configured domain, without changing the language Int type. See the [solve example](../examples/solve/README.md) for the interface, dependencies, supported subset, and runnable program.
