# 用 Tapas 测试换货业务

简体中文 | [English](README_en.md)

顾客 Yusuf 收到了一把线性轴键盘，想换成同款的无背光段落轴键盘。新规格贵 1 元，他确认了目标规格，准备使用订单关联的支付方式补差价。

零售系统需要核对他的身份、订单和确认信息，还要检查目标规格是否有货、可支付额度是否足够。符合换货条件，就接受请求、收取差价并更新订单；不符合条件，就拒绝请求，保留原来的订单。

这里用模拟器执行这段业务逻辑，再用 Tapas 检查处理结果是否符合规则。测试从 Yusuf 的这次换货开始，随后改变请求和订单信息，检查拒绝处理和错误的状态变化，最后只给出部分条件，让求解器生成新的测试输入。

## 一、准备业务数据

### 商品和规格

零售系统的商品目录中有三种键盘规格和一款温控器。这里的“同一商品”指相同的 `product_id`，不同规格用 `item_id` 区分。

| 规格编号 | 商品 | 规格说明 | 价格 | 是否有货 |
|---|---|---|---|---|
| `keyboard-linear-rgb` | 机械键盘 | 线性轴、RGB 背光 | 120 元 | 有 |
| `keyboard-clicky-rgb` | 机械键盘 | 段落轴、RGB 背光 | 125 元 | 无 |
| `keyboard-clicky-plain` | 机械键盘 | 段落轴、无背光 | 121 元 | 有 |
| `thermostat-google-home` | 智能温控器 | 兼容 Google Home | 121 元 | 有 |

代码中的金额全部以“分”为单位。例如，120 元写成 `12000`，换成 121 元的规格需要补 `100` 分。完整目录见[商品数据](data.tap)。

### 换货前的订单

Yusuf 的订单 `#W2378156` 已经送达，包含一把 `keyboard-linear-rgb`。本例为这次操作设置的可支付额度是 `100` 分。
系统用 `State` 保存订单及处理请求所需的信息：

| 字段 | 含义 |
|---|---|
| `order_id` | 订单编号 |
| `user_id` | 订单属于谁，本例为 Yusuf |
| `status` | 订单当前阶段，例如“已送达” |
| `item_id` | 订单当前包含的商品规格 |
| `payment_method_id` | 订单关联的支付方式 |
| `payment_capacity` | 可用于支付差价的额度 |
| `authenticated_user_id` | 系统当前认证的操作者是谁 |
| `confirmed_target_id` | 顾客已经确认要换入的规格 |

Yusuf 的用户编号是 `yusuf_rossi_9620`。订单的 `user_id` 记录这个编号；系统识别出的操作者则记录在 `authenticated_user_id` 中。两者必须相同，系统才能允许这位操作者修改订单。
`confirmed_target_id` 记录顾客确认了什么；空字符串表示尚未确认。它与认证信息一起，构成本次操作的上下文。
这些字段的类型定义见[业务数据定义](model_defs.tap)。

### 一次换货请求

Yusuf 提交的请求需要说明：操作哪笔订单、换成哪个规格、用哪种支付方式。
他想换成无背光的段落轴键盘，请求如下：

```text
{
    'order_id': '#W2378156',
    'target_item_id': 'keyboard-clicky-plain',
    'payment_method_id': 'credit_card_9513926',
}
```

请求中的 `target_item_id` 是这次希望换入的规格，订单中的 `item_id` 是目前已有的规格。
请求目标还必须与用户已经确认的 `confirmed_target_id` 一致。

## 二、把业务要求写成 Rule

我们先把零售系统应遵守的要求写成 Rule。Rule 描述数据应满足的条件，不执行换货，也不修改订单。

先看模型中定义初始状态的一小段代码：

```text
let InitialState = rule (state: defs::State) {
    // 引用已有规则：它对状态的要求也必须满足。
    StateSpace(state)
    state::status == '已送达'
    state::item_id == 'keyboard-linear-rgb'
}
```

`let InitialState = rule (...) { ... }` 定义一个 Rule，并把它保存为 `InitialState`。参数 `state` 表示要检查的状态，`defs::State` 是它的类型；`::` 用来访问包成员或数据字段，例如 `state::status` 就是状态中的订单阶段。

这三条条件必须同时成立：状态满足 `StateSpace`，订单已送达，商品规格是原来的线性轴键盘。`StateSpace(state)` 引用了另一条 Rule，因此它的条件也要一起满足。Rule 中的这些表达式描述要求；模拟函数中的代码则实际计算输出和后状态。

`InitialState` 描述本例的起点。后面检查完整操作时使用的 `ExchangeModel` 不要求所有订单都处于这个起点，因此也可以检查其他订单阶段的处理。

### 什么请求可以换货？

`ExchangeAllowed` 集中了换货的允许条件：

1. 已认证用户是订单所属用户。
2. 用户确认的目标与请求目标一致。
3. 请求中的订单编号、支付方式与订单匹配。
4. 订单已经送达。
5. 换入的是同一商品的另一种规格。
6. 目标规格有货。
7. 可支付额度足以支付差价。

例如，身份与额度条件直接写成：

```text
previous_state::authenticated_user_id == previous_state::user_id
previous_state::payment_capacity >= replacement_item::price - original_item::price
```

完整条件见[换货模型](model.tap)中的 `ExchangeAllowed`。

### 系统应该怎样处理请求？

对于 Yusuf 这次符合条件的请求，系统应该返回“成功”，收取差价，并更新订单规格、订单阶段和剩余额度。
不允许换货时，应该返回“拒绝”，不收费，并保持订单状态不变。

`ExchangeTransition` 用 `implies` 表达这两个分支。`A implies B` 表示：当 A 成立时，B 必须成立。

```text
exchange_allowed implies {
    exchange_output::decision == '成功'
    exchange_output::charged_difference == price_difference
    next_state::status == '换货申请'
    next_state::item_id == action_parameters::target_item_id
    next_state::payment_capacity == previous_state::payment_capacity - price_difference
}
```

拒绝分支以 `not exchange_allowed` 为前提。无论走哪个分支，订单身份、支付方式和认证确认信息都不应被这次操作改写。

### 怎样检查完整的一次操作？

检查一次换货需要四项数据：**前状态、请求、输出、后状态**。
其中，“输出”包含成功或拒绝的决策，以及收取的差价；“后状态”是处理请求后的订单。

顶层 `ExchangeModel` 把相关规则组合起来：

```text
let ExchangeModel = rule (
        previous_state: defs::State,
        action_parameters: defs::ExchangeParameters,
        exchange_output: defs::ExchangeOutput,
        next_state: defs::State,
) {
    // 请求可以不符合换货条件，但必须是模型能够检查的输入。
    ExchangeInput(previous_state, action_parameters)
    // 检查实际输出和后状态，包括成功与拒绝两种处理。
    ExchangeResult(previous_state, action_parameters, exchange_output, next_state)
}
```

前两项参数交给 `ExchangeInput`，检查前状态和请求是否属于模型允许描述的输入；四项参数一起交给 `ExchangeResult`，检查系统的处理及后状态。它们引用的规则会继续参与检查，因此测试只需使用这个顶层模型。

这里的“合法输入”不等于“允许换货”。无货、额度不足的请求也可以提交，系统应该正确拒绝它们。
因此，`ExchangeAllowed` 用来决定应该走哪个分支，而不是要求所有被检查的请求都必须满足它。

## 三、检查模拟产生的结果

下面开始运行实验。需要当前项目构建的 Tapas、Python 和 OR-Tools；安装方法见 [solve 包说明](../../src/stdlib/solve/README.md)。所有命令都在仓库根目录执行。

在下面的实验中，模拟器扮演零售系统：接收前状态和顾客请求，返回业务输出 `output` 与后状态 `after`。模拟中的业务判断独立编写，Tapas 再用 Rule 检查它的处理是否正确。

### 测试 1：正常换货

运行[正常换货实验](test_valid_exchange.tap)：

```sh
build/bin/tapas examples/retail/test_valid_exchange.tap
```

测试文件开头先引入业务模型和模拟器：

```text
import model.tap as model
import simulation.tap as simulation
```

随后构造订单与请求，再调用模拟：

```text
let previous_state = simulation::initial_state('keyboard-clicky-plain')
let exchange_parameters = simulation::exchange_parameters('keyboard-clicky-plain')
let exchange_result = simulation::exchange(previous_state.copy(), exchange_parameters.copy())
```

传入副本，是为了保留原始数据供后面比较。
`initial_state('keyboard-clicky-plain')` 准备原键盘的订单，并记录顾客已确认目标规格；它不会提前把订单商品换掉。`exchange_parameters` 准备指向同一目标的请求。`exchange` 才执行模拟，返回包含 `output` 和 `after` 的记录。

这次模拟应该返回“成功”，收取 `100` 分。订单的变化应当是：

| 字段 | 前状态 | 后状态 |
|---|---|---|
| 商品规格 | `keyboard-linear-rgb` | `keyboard-clicky-plain` |
| 订单阶段 | 已送达 | 换货申请 |
| 可支付额度 | 100 分 | 0 分 |

订单编号、所属用户等其余信息应保持不变。

接下来，用 `rules::restrict` 把四项数据固定到模型的同名参数上：

```text
// 将模型的四个参数固定为这次模拟的真实记录。
let restricted_model = rules::restrict(
    model::ExchangeModel,
    'previous_state': previous_state,
    'action_parameters': exchange_parameters,
    'exchange_output': exchange_result::output,
    'next_state': exchange_result::after,
)
// 数据已全部固定：查询它们能否同时满足模型，而不是另生成一份结果。
let result = solve::hold(restricted_model)
```

每个冒号左侧的字符串对应 `ExchangeModel` 的一个参数名，右侧是本次执行得到的数据。例如，`'next_state': exchange_result::after` 把模型的后状态固定为模拟器实际产生的后状态。

`restrict` 返回附加了这些限制的新 Rule，原模型仍可用于下一次测试。`hold` 查询新 Rule 的所有条件能否同时成立。这里四项参数全部固定，求解器不能通过修改订单、请求或模拟结果来让检查通过，只能判断这份记录是否符合模型。

查询返回的 `result` 中，`status` 表示是否有解。测试打印 `result::status`，得到：

```text
合法换货：sat
```

`sat` 表示能够满足全部约束。在这次检查中，它说明模拟器产生的完整记录符合业务要求。它只说明这一次执行符合模型，还不能据此断言模拟器对所有输入都正确。

### 测试 2：正确拒绝

运行[拒绝换货实验](test_rejected_exchange.tap)：

```sh
build/bin/tapas examples/retail/test_rejected_exchange.tap
```

先看额度不足的一组：Yusuf 仍然请求换入那把贵 100 分的键盘，身份和确认信息都正确，但可支付额度改成了 99 分。请求可以提交，只是不满足 `ExchangeAllowed`。

此时，模型中的拒绝分支要求输出为“拒绝”、收费为零，并保留原来的商品、订单阶段和 99 分额度。模拟器确实这样处理后，测试仍然把前状态、请求、实际输出和实际后状态固定到 `ExchangeModel`，再调用 `hold`。得到 `sat`，说明拒绝处理符合要求。

这个文件还检查了另外五种情况：

| 改变的输入 | 应当拒绝的原因 |
|---|---|
| 请求无货的键盘规格 | 目标规格没有库存 |
| 认证用户为空 | 无法确认操作者是订单所属用户 |
| 确认目标为空 | 顾客尚未确认这次换入的规格 |
| 请求订单原有的规格 | 没有换成另一种规格 |
| 请求换成温控器 | 目标属于另一商品 |

六组完整转移查询都应得到 `sat`。这里检查的不只是有没有返回“拒绝”：如果模拟器拒绝后仍扣了钱，或者改了订单，同样会违背模型。

### 测试 3：发现错误的状态转移

运行[错误转移实验](test_invalid_transition.tap)：

```sh
build/bin/tapas examples/retail/test_invalid_transition.tap
```

这次把 Yusuf 的请求改为“将键盘换成温控器”，并让确认目标与请求一致。虽然身份、确认和额度都符合要求，目标仍然不是同一商品。实验使用故意遗漏“同一商品”检查的模拟函数，它会错误地返回成功，并更新订单商品。

查询方法不变，结果却是 `unsat`：模拟数据与模型无法同时成立。
模型要求跨商品请求被拒绝且状态不变，而模拟返回了相反的结果。

可以对照模型中的这条条件理解冲突：

```text
'不允许换货时必须拒绝且保持业务状态':
    // 请求不允许换货时，以下条件必须全部成立。
    (not exchange_allowed) implies {
        exchange_output::decision == '拒绝'
        exchange_output::charged_difference == 0
        next_state::status == previous_state::status
        next_state::item_id == previous_state::item_id
        next_state::payment_capacity == previous_state::payment_capacity
    }
```

跨商品使 `exchange_allowed` 不成立，因而这整个拒绝分支必须成立。但测试固定的实际输出是“成功”，商品也已经改成温控器，无法满足这些要求。

测试通过 `pprint(result::conflicts)` 打印涉及的 Rule 和条件。阅读时，将其中的业务条件与本次固定的数据对照，就能判断模拟行为违反了哪项要求；返回的冲突集合足以说明无解，但不保证最小。

这三项测试中的结果需要结合目的理解：

| 场景 | 模拟器的业务输出 | 完整转移查询 | 测试说明了什么 |
|---|---|---|---|
| 正常换货 | 成功 | `sat` | 本次成功处理符合模型 |
| 不符合换货条件 | 拒绝 | `sat` | 本次拒绝处理符合模型 |
| 故意遗漏同商品检查 | 成功 | `unsat` | 测试发现了预先放入的缺陷 |

因此，第三项的预期结果就是 `unsat`；它表示错误的执行记录被模型检出了。

## 四、让求解器生成测试输入

前三项测试先准备完整数据，再检查结果。接下来只固定一部分信息，把支付额度留给求解器决定。

### 测试 4：求出能够换货的额度

运行[生成合法输入的实验](test_generate_valid_exchange.tap)：

```sh
build/bin/tapas examples/retail/test_generate_valid_exchange.tap
```

订单、目标规格、认证和确认信息保持不变。实验定义 `AcceptedCapacity(payment_capacity: Int)`，用未知额度构造订单，再要求它满足已有的 `ExchangeInput` 和 `ExchangeAllowed`。
换货允许条件仍来自业务模型，不在实验中重新计算一套。这个实验 Rule 的完整定义如下：

```text
let initial_state = simulation::initial_state('keyboard-clicky-plain')
let exchange_parameters = simulation::exchange_parameters('keyboard-clicky-plain')

// 只将支付额度设为未知参数，订单的其他信息沿用已知数据。
let AcceptedCapacity = rule (payment_capacity: Int) {
    let previous_state = {
        'order_id': initial_state::order_id,
        'user_id': initial_state::user_id,
        'status': initial_state::status,
        'item_id': initial_state::item_id,
        'payment_method_id': initial_state::payment_method_id,
        // 把未知参数放入订单，使已有业务规则能够约束它。
        'payment_capacity': payment_capacity,
        'authenticated_user_id': initial_state::authenticated_user_id,
        'confirmed_target_id': initial_state::confirmed_target_id,
    }
    model::ExchangeInput(previous_state, exchange_parameters)
    // 生成的输入必须允许换货；具体允许条件仍由业务模型定义。
    model::ExchangeAllowed(previous_state, exchange_parameters)
}
```

前面准备的 `initial_state` 和 `exchange_parameters` 是已知数据。新构造的 `previous_state` 沿用原订单的其他字段，只有 `payment_capacity` 来自 Rule 参数，等待求解器赋值。

最后两行要求这份输入既满足 `ExchangeInput`，也满足 `ExchangeAllowed`。所以求解器寻找的是“能让这笔换货被允许的额度”。未知量是在这里显式声明并放入订单的；仅仅不限制一个已有订单的字段，并不等于自动把该字段变成未知参数。

本次只允许在 0–100 分内寻找额度：

```text
let input_model = rules::restrict(AcceptedCapacity,
    // 包含 0 和 100；额度在范围内由求解器选择。
    'payment_capacity': rules::range(0, 100),
)
let generated = solve::hold(input_model)
```

这里没有把额度固定成一个数，而是用 `range(0, 100)` 限制可选范围，两个端点都包含。模型要求额度至少达到 100 分，所以范围内唯一的解是 `100`。

`hold` 寻找一个满足条件的解，并不求最小值。如果把上限放宽到 200 分，100–200 分都满足要求，不能再假定它一定返回 100。
求解成功后，`witness` 保存找到的 Rule 实例。只有 `status` 为 `sat` 时才取解，再把求出的额度填入订单并执行模拟：

```text
if (generated::status == 'sat') {
    // 只有找到解才读取 witness；第一个参数就是支付额度。
    let payment_capacity = arguments(generated::witness)[0]
    let previous_state = initial_state.copy()
    previous_state['payment_capacity'] = payment_capacity
    let exchange_result = simulation::exchange(previous_state.copy(), exchange_parameters.copy())
    let transition_model = rules::restrict(model::ExchangeModel,
        'previous_state': previous_state,
        'action_parameters': exchange_parameters,
        'exchange_output': exchange_result::output,
        'next_state': exchange_result::after,
    )
    // 第二次查询检查实际执行；第一次查询只负责生成输入。
    let checked = solve::hold(transition_model)
}
```

`arguments` 按 Rule 参数声明的顺序返回值；`AcceptedCapacity` 只有一个参数，所以索引 `0` 就是求出的额度。随后，测试像正常换货时一样，将四项执行数据固定到 `ExchangeModel`，检查模拟器是否正确处理了生成的输入。

测试文件还会打印两次查询和模拟的结果：

```text
输入生成：sat
求出的支付额度（分）：100
模拟结果：成功
实际转移：sat
```

同一个实验随后把范围收紧为 0–99 分，但仍要求允许换货。这次得到 `unsat`，冲突涉及支付差价的条件和额度范围。因为没有生成输入，这次查询不会执行模拟。

### 测试 5：求出应该拒绝的额度

运行[生成拒绝输入的实验](test_generate_rejected_exchange.tap)：

```sh
build/bin/tapas examples/retail/test_generate_rejected_exchange.tap
```

实验先从 `ExchangeAllowed` 构造一个只违反额度条件的新 Rule：

```text
let RejectedByCapacity = rules::violate(
    model::ExchangeAllowed,
    '可支付额度必须足以支付差价',
)
```

`violate` 将指定条件取反，同时保留其余许可条件。因此，只要 `RejectedByCapacity` 成立，就可以确定拒绝原因是额度不足，不需要依赖身份、确认等数据碰巧正确。实验再要求输入满足 `ExchangeInput`，并将额度限制在 99–100 分。求解器得到 `99` 分；若选 100 分，额度条件就不会被违反。

```text
输入生成：sat
求出的支付额度（分）：99
模拟结果：拒绝
实际转移：sat
```

这里有两次不同的查询。第一次证明“存在一组应该被拒绝的输入”，第二次确认“模拟确实正确处理了这组输入”。

## 五、同时求解额度、认证和确认

运行[多变量实验](test_generate_joint_inputs.tap)：

```sh
build/bin/tapas examples/retail/test_generate_joint_inputs.tap
```

这次不只留下额度未知，还让求解器选择认证用户和确认目标。为了明确候选范围，实验使用两个枚举类型：

```text
// 空字符串表示未认证；其余值是本次实验允许选择的用户。
let Authentication = types::enum('', 'yusuf_rossi_9620', 'another_user')
let Confirmation = types::enum('', 'keyboard-clicky-plain', 'thermostat-google-home')
```

认证候选包括未认证、Yusuf 和另一用户；确认候选包括未确认、请求换入的键盘和温控器。订单始终属于 Yusuf，变化的是系统收到请求时识别出的操作者和记录的确认目标。

`ExchangeInputs` 将这三个未知量放入订单，仍然复用原有业务 Rule。
完整定义如下：

```text
let ExchangeInputs = rule (
        payment_capacity: Int,
        authenticated_user_id: Authentication,
        confirmed_target_id: Confirmation,
        // 指定要生成允许换货还是不允许换货的输入，不是模拟器的输出。
        allowed: Bool,
) {
    let previous_state = {
        'order_id': initial_state::order_id,
        'user_id': initial_state::user_id,
        'status': initial_state::status,
        'item_id': initial_state::item_id,
        'payment_method_id': initial_state::payment_method_id,
        'payment_capacity': payment_capacity,
        'authenticated_user_id': authenticated_user_id,
        'confirmed_target_id': confirmed_target_id,
    }
    model::ExchangeInput(previous_state, exchange_parameters)
    // true 要求满足允许条件；false 要求至少一项允许条件不满足。
    ((allowed and model::ExchangeAllowed(previous_state, exchange_parameters)) or
        (not allowed and not model::ExchangeAllowed(previous_state, exchange_parameters)))
}
```

构造订单的方法与测试 4 相同，只是现在有三个字段来自未知参数。`allowed` 是第四个参数，用来指定这次想寻找哪类输入，并不是模拟器已经返回的结果。

最后的逻辑表达式把它与业务规则关联起来：`allowed` 为 `true` 时，必须满足 `ExchangeAllowed`；为 `false` 时，必须不满足 `ExchangeAllowed`。`and` 要求两边同时成立，`or` 要求至少一边成立，`not` 表示取反。无论选择哪一类，都仍需满足前一行的 `ExchangeInput`。

### 测试 6：三个条件一起满足

先构造一个要求允许换货的实验：

```text
let accepted = rules::restrict(ExchangeInputs,
    'payment_capacity': rules::range(0, 100),
    // 认证用户和确认目标未固定，仍可从各自枚举中选择。
    'allowed': true,
)
```

这里将额度限制在 0–100 分，要求 `allowed: true`，不固定认证用户和确认目标；后二者仍可在各自枚举的候选中选择。
求解器必须同时满足三个关系，得到：

- 支付额度为 `100` 分。
- 认证用户是订单所属用户。
- 确认目标与请求换入的键盘规格一致。

这组输入模拟后成功，实际转移查询为 `sat`。

### 测试 7：固定两项，寻找第三项的问题

同一个入口接着运行三组限制，每次固定两项正确的信息，并要求 `allowed: false`：

| 已固定的正确条件 | 留给求解器选择的内容 | 求出的输入 |
|---|---|---|
| 认证、确认 | 额度为 99 或 100 分 | 99 分，差价不足 |
| 认证、额度 | 确认目标为请求规格或温控器 | 确认了温控器，与请求不符 |
| 确认、额度 | 认证用户为订单所属用户或另一用户 | 认证为另一用户 |

以第二组为例，完整限制是：

```text
let wrong_confirmation = rules::restrict(ExchangeInputs,
    'payment_capacity': 100,
    'authenticated_user_id': initial_state::user_id,
    // 排除“未确认”，只在正确目标和错误目标之间选择。
    'confirmed_target_id': rules::points(Confirmation,
        'keyboard-clicky-plain', 'thermostat-google-home'),
    // 其他条件已经正确，求解器只能通过选错确认目标来满足此要求。
    'allowed': false,
)
let generated = solve::hold(wrong_confirmation)
```

额度和身份已经正确，其他订单与请求信息也没有改变。确认目标只有两个候选：如果选择键盘，所有允许条件都会满足，与 `allowed: false` 矛盾；因此只能选择温控器，使确认目标与请求目标不一致。

`points(Confirmation, ...)` 表示只从列出的离散值中选择，且这些值属于 `Confirmation` 类型。它在原有枚举范围上进一步排除了空字符串，因此本组寻找的是“确认了错误目标”，而不是“尚未确认”。

求解成功后，`arguments(generated::witness)` 的前四项依次对应额度、认证用户、确认目标和 `allowed`。测试把前三项写入订单，再交给模拟器执行。`allowed` 只用于输入查询，不传给模拟器；模拟器需要自行判断这份请求应当被拒绝。

三组数据都会被模拟正确拒绝，实际转移也都为 `sat`。这里没有为三种错误分别编写判断规则，只是改变了同一个输入模型上的限制。

### 测试 8：其他条件能否补偿错误身份？

最后，实验在要求允许换货的 Rule 上，再固定认证用户为 `another_user`：

```text
// 保留 accepted 中的额度范围和 allowed: true，再追加错误身份。
let incompatible = rules::restrict(accepted,
    'authenticated_user_id': 'another_user',
)
let impossible = solve::hold(incompatible)
```

`accepted` 已经保留了测试 6 的额度范围和 `allowed: true`。再次 `restrict` 会追加身份限制，不会撤销先前的要求。

结果为 `unsat`。订单属于 Yusuf，另一用户即使有足够额度、确认了正确目标，也不满足“认证用户必须是订单所属用户”的条件。
这次无解发生在输入生成阶段，尚未执行模拟。

## 关于结果与范围

实验中的金额范围和用户候选是本次查询的限制。求解器只在这些范围内寻找解，不代表已经覆盖了所有可能的业务输入。
多变量实验显式地用标量参数构造订单，目前不涉及任意结构字段或商品目录索引的自动搜索。

如果出现 `unsupported`、`unknown` 或 `error`，分别表示查询尚不支持、未能判定或执行出错，不能当作业务通过或违规。具体配置与支持范围见 solve 包说明。

本例只模拟单商品订单的一步换货，不连接真实支付，也不扣减库存。业务背景参考 [Retail Policy](https://github.com/sierra-research/tau2-bench/blob/main/data/tau2/domains/retail/policy.md) 和 [task 0](https://github.com/sierra-research/tau2-bench/blob/main/data/tau2/domains/retail/tasks.json)，数据和流程均经过简化。

采样算法的可重复消融、置信区间和条件分布检查见 [Retail 概率采样数值实验](SamplingExperiment.md)。
