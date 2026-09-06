# Testing retail exchanges with Tapas

[简体中文](README.md) | English

Yusuf has received a linear-switch keyboard and wants the unlit clicky-switch variant of the same product. The replacement costs 100 cents more. He confirms the target and chooses the payment method associated with his order.

The retail system checks his identity, order details, and confirmation, along with stock and payment capacity. An eligible request is accepted: the system charges the difference and updates the order. Otherwise, it rejects the request and leaves the order unchanged.

A simulator executes this business logic, and Tapas checks whether the result satisfies the rules. The tests start with Yusuf’s exchange, then vary the request and order information to check rejections and incorrect state changes. Later tests specify only some conditions and ask the solver to generate new inputs.

## 1. Business data

### Products and variants

The retailer’s catalog contains three keyboard variants and a thermostat. `product_id` identifies the product; `item_id` identifies its variant.

| Variant | Product | Price in cents | Available |
|---|---|---|---|
| `keyboard-linear-rgb` | Mechanical keyboard | 12000 | Yes |
| `keyboard-clicky-rgb` | Mechanical keyboard | 12500 | No |
| `keyboard-clicky-plain` | Mechanical keyboard | 12100 | Yes |
| `thermostat-google-home` | Smart thermostat | 12100 | Yes |

All amounts in code are integer cents. Moving from the first keyboard to the plain variant costs 100 cents. See the [catalog](data.tap) for the complete data.

### The initial order

Yusuf’s order `#W2378156` is delivered and contains `keyboard-linear-rgb`. The experiment supplies 100 cents of payment capacity.
Its `State` includes:

| Field | Meaning |
|---|---|
| `order_id` | Order identifier |
| `user_id` | Order owner |
| `status` | Current order stage |
| `item_id` | Current variant in the order |
| `payment_method_id` | Associated payment method |
| `payment_capacity` | Available capacity for the price difference |
| `authenticated_user_id` | Currently authenticated user |
| `confirmed_target_id` | Variant the user has confirmed |

Yusuf’s user identifier is `yusuf_rossi_9620`. The order’s `user_id` records its owner; `authenticated_user_id` records the operator identified by the system. They must match. An empty confirmed target means confirmation is missing. These structures are defined in the [business types](model_defs.tap).

### An exchange request

Yusuf’s request specifies the order, replacement variant, and payment method:

```text
{
    'order_id': '#W2378156',
    'target_item_id': 'keyboard-clicky-plain',
    'payment_method_id': 'credit_card_9513926',
}
```

`target_item_id` is the requested replacement; the state's `item_id` is the current item. The requested target must also match the user's confirmation.

## 2. Expressing business requirements as Rules

A Rule describes conditions that data must satisfy. It does not execute the exchange or modify the order.

### Eligibility

`ExchangeAllowed` requires the authenticated owner, confirmation matching the target, matching order and payment identifiers, a delivered order, a different variant of the same product, available stock, and enough payment capacity.

For example:

```text
previous_state::authenticated_user_id == previous_state::user_id
previous_state::payment_capacity >= replacement_item::price - original_item::price
```

The complete conditions are in the [exchange model](model.tap).

### Expected behavior

An eligible request must succeed, charge the difference, and update the order variant, stage, and remaining capacity.
An ineligible request must be rejected without charging or changing state.

`ExchangeTransition` uses `implies`: when the left side holds, the right side must hold.

```text
exchange_allowed implies {
    exchange_output::decision == '成功'
    exchange_output::charged_difference == price_difference
    next_state::status == '换货申请'
    next_state::item_id == action_parameters::target_item_id
    next_state::payment_capacity == previous_state::payment_capacity - price_difference
}
```

The rejection branch uses `not exchange_allowed`. Both branches preserve order identity, payment method, and authentication/confirmation context.

### A complete transition

Checking an exchange requires four values: **previous state, request, output, and next state**. Output contains the decision and charge; next state contains the resulting order.

```text
ExchangeModel
    ExchangeInput: valid previous state and request
    ExchangeResult: correct transition and valid next state
```

A valid input is not necessarily eligible. Out-of-stock and underfunded requests must still be accepted as test inputs so their rejection can be checked.

## 3. Checking simulated results

You need the current project build of Tapas, Python, and OR-Tools. See the [solve package](../../src/stdlib/solve/README.md) for installation. Run the commands below from the repository root. Business output is in Chinese.

The simulator stands in for the retail system: it takes the current state and customer request and returns `output` and `after`. Its business decisions are independent of the Rules that Tapas uses to check them.

### Test 1: a valid exchange

Run the [valid exchange experiment](test_valid_exchange.tap):

```sh
build/bin/tapas examples/retail/test_valid_exchange.tap
```

It prepares inputs and simulates execution:

```text
let previous_state = simulation::initial_state('keyboard-clicky-plain')
let exchange_parameters = simulation::exchange_parameters('keyboard-clicky-plain')
let exchange_result = simulation::exchange(previous_state.copy(), exchange_parameters.copy())
```

Copies preserve the original data for comparison. The exchange should charge 100 cents, request the exchange, and leave zero capacity.

The test fixes all four values onto the model:

```text
let restricted_model = rules::restrict(
    model::ExchangeModel,
    'previous_state': previous_state,
    'action_parameters': exchange_parameters,
    'exchange_output': exchange_result::output,
    'next_state': exchange_result::after,
)
let result = solve::hold(restricted_model)
```

`restrict` returns a new Rule with additional constraints. `hold` asks whether all conditions can hold together. With all values fixed, this checks one concrete transition. The result is `sat`.

### Test 2: correct rejection

Run the [rejection experiment](test_rejected_exchange.tap):

```sh
build/bin/tapas examples/retail/test_rejected_exchange.tap
```

It covers unavailable stock, missing authentication, missing confirmation, insufficient capacity, the original variant, and a different product. All six results are `sat`.

**A rejected exchange is not a failed test.** For these inputs, rejection with no charge and preserved state is exactly what the model requires.

### Test 3: an incorrect transition

Run the [invalid transition experiment](test_invalid_transition.tap):

```sh
build/bin/tapas examples/retail/test_invalid_transition.tap
```

The request now replaces Yusuf’s keyboard with a thermostat, with matching confirmation. Identity, confirmation, and capacity are sufficient, but the product differs. A deliberately defective simulation omits the same-product check, accepts the request, and changes the item.

The same query now returns `unsat`. The model requires rejection and preserved state, contradicting the simulated data. `result::conflicts` lists involved Rules and conditions; the conflict set is sufficient but not guaranteed minimal.

## 4. Generating inputs with partial restrictions

The first three tests start with complete inputs. Next, we fix the order and target but let the solver choose payment capacity.

### Test 4: enough capacity to exchange

Run the [accepted-input generation experiment](test_generate_valid_exchange.tap):

```sh
build/bin/tapas examples/retail/test_generate_valid_exchange.tap
```

`AcceptedCapacity(payment_capacity: Int)` constructs a state with unknown capacity and requires the existing `ExchangeInput` and `ExchangeAllowed` Rules. It does not duplicate the eligibility conditions.

```text
let input_model = rules::restrict(AcceptedCapacity,
    'payment_capacity': rules::range(0, 100),
)
let generated = solve::hold(input_model)
```

The range includes both endpoints. A 100-cent difference makes 100 the only solution. The returned `witness` is a satisfying Rule instance; retrieve its parameter with:

```text
let payment_capacity = arguments(generated::witness)[0]
```

The experiment simulates that input and checks the resulting transition: generation is `sat`, capacity is 100, simulation accepts, and the actual transition is `sat`.

It then tightens the range to 0–99 while still requiring eligibility. This returns `unsat`, with payment and range constraints. No simulation runs for that unsatisfiable query.

### Test 5: capacity that should be rejected

Run the [rejected-input generation experiment](test_generate_rejected_exchange.tap):

```sh
build/bin/tapas examples/retail/test_generate_rejected_exchange.tap
```

The experiment constructs a Rule that violates only the capacity condition:

```tapas
let RejectedByCapacity = rules::violate(
    model::ExchangeAllowed,
    '可支付额度必须足以支付差价',
)
```

`violate` negates the selected condition while retaining every other eligibility
condition. The experiment also requires `ExchangeInput` and limits capacity to
99–100, so the solver selects 99 cents. At 100 cents, the selected condition
would no longer be violated.

The input query is `sat`, simulation rejects, and the actual transition query is also `sat`. The first query finds an ineligible input; the second checks that simulation handles it correctly.

## 5. Solving capacity, authentication, and confirmation together

Run the [joint-input experiment](test_generate_joint_inputs.tap):

```sh
build/bin/tapas examples/retail/test_generate_joint_inputs.tap
```

Three values are now unknown. Candidate users and confirmation targets are explicit finite enums:

```text
let Authentication = types::enum('', 'yusuf_rossi_9620', 'another_user')
let Confirmation = types::enum('', 'keyboard-clicky-plain', 'thermostat-google-home')
```

The authentication candidates are no authenticated user, Yusuf, and another user. Confirmation candidates are no confirmation, the requested keyboard, and the thermostat. Yusuf remains the owner; the operator and confirmation context vary.
`ExchangeInputs` places these values into the state and reuses the original Rules. Its `allowed` parameter selects eligible inputs when fixed to true and ineligible inputs when fixed to false.

### Test 6: satisfy all three conditions

Require eligibility, constrain capacity to 0–100, and leave user and confirmation unknown.
The solver selects 100 cents, the order owner, and the requested keyboard variant. Simulation accepts; the actual transition is `sat`.

### Test 7: fix two conditions and find a problem in the third

The same entry runs three more restrictions, each requiring ineligibility:

| Correct conditions fixed | Remaining choice | Generated input |
|---|---|---|
| Authentication and confirmation | Capacity 99 or 100 | 99 cents |
| Authentication and capacity | Confirm keyboard or thermostat | Thermostat confirmation for a keyboard request |
| Confirmation and capacity | Authenticate owner or another user | Another user |

Discrete choices use `rules::points(Type, ...)`, for example:

```text
'confirmed_target_id': rules::points(Confirmation,
    'keyboard-clicky-plain', 'thermostat-google-home'),
```

All three simulations correctly reject, and all three transitions are `sat`. The restrictions change, not the business rules.

### Test 8: can other conditions compensate for wrong identity?

Finally, require eligibility but fix authentication to another user:

```text
let incompatible = rules::restrict(accepted,
    'authenticated_user_id': 'another_user',
)
let impossible = solve::hold(incompatible)
```

This is `unsat`: sufficient capacity and correct confirmation cannot satisfy the owner-authentication requirement. The failure occurs during input generation, before simulation.

## Interpreting the scope of these experiments

The amount ranges and candidate users restrict each experiment. A result is about that search space, not coverage of every possible business input.
Joint-input experiments explicitly construct records from scalar variables; they do not search arbitrary structural fields or symbolic catalog indices.

`unsupported`, `unknown`, and `error` mean unsupported, undecided, or failed queries. They are not business passes or violations. Configuration and supported features are documented in the solve package.

This example simulates a single exchange for a single-item order, without real payments or inventory updates. Its background draws on the [Retail Policy](https://github.com/sierra-research/tau2-bench/blob/main/data/tau2/domains/retail/policy.md) and [task 0](https://github.com/sierra-research/tau2-bench/blob/main/data/tau2/domains/retail/tasks.json), with simplified data and workflow.
