# finite：有限集上的概率分布

`finite` 提供有限索引空间上的概率分布、分布组合、概率查询和独立采样。Index 是由 Int 组成的不可变 Tuple；Distribution 保存各维大小组成的 shape，全部概率操作只处理与 shape 匹配的 Index。外部集合不存储在分布中，调用方通过 `Indexable` 的 `index -> value` 映射解释各维坐标。

## 支持范围

### 类型

```tap
finite::Index: Type
finite::Distribution: Type
```

`Index` 表示 `(i0, ..., ik)`，是由非负 Int 组成的非空、不可变 Tuple，并支持 `Indexable`。`Distribution` 是由 `finite` 包创建的封闭抽象类型，不提供用户构造接口。每个 Distribution 保存一个 shape `(n0, ..., nk)`；shape 的每个分量必须大于零。合法 Index 的维数必须等于 shape 的维数，并满足每一维的 `index[i] < shape[i]`。

### 通用操作

```tap
finite::index(coords: List[Int]) -> Index
finite::shape(dist: Distribution) -> Index
finite::prob(dist: Distribution, index: Index) -> Float
finite::mass(dist: Distribution, indices: List[Index]) -> Float
finite::samples(dist: Distribution, count: Int, rng: random::Generator) -> List[Index]
```

`index` 将非空的 `coords` 复制为不可变 Index。所有坐标必须是非负整数。

`shape` 返回 Distribution 的 shape。

`prob` 返回单个 Index 的概率。

`mass` 返回 `indices` 中全部 Index 的概率总和。重复 Index 只计算一次；任何 Index 与分布的 shape 不匹配时返回错误。

`samples` 使用传入的 `random::Generator` 独立采样 `count` 次并返回 Index，同时推进 generator 的状态。`count` 必须是非负整数，结果允许包含重复 Index。由相同随机源和 seed 构造、处于相同状态的 generator 必须产生相同结果。

### 基础分布表示

```tap
finite::uniform(n: Int) -> Distribution
finite::categorical(weights: List[Float]) -> Distribution
finite::piecewise_constant(n: Int, steps: List[Pair[Int, Float]]) -> Distribution
finite::piecewise_linear(knots: List[Pair[Int, Float]]) -> Distribution
```

`uniform` 构造 shape 为 `(n,)` 的分布，并为 Index `(0,)` 到 `(n - 1,)` 分配相同概率。`n` 必须大于零。

`categorical` 使用 `weights` 的位置作为一维坐标，Index `(i,)` 的概率为 `weights[i] / sum(weights)`，结果的 shape 为 `(len(weights),)`。

`piecewise_constant` 构造 shape 为 `(n,)` 的分布，并在一维坐标上使用 `(start, weight)` steps。每个 weight 应用于从当前 start 到下一个 start 的半开区间，最后一段延伸到 `n`。`n` 必须大于零，steps 必须非空，第一个 start 必须为 `0`，其余 start 必须严格递增且小于 `n`。

`piecewise_linear` 在一维坐标上使用 `(coordinate, weight)` knots，相邻 knots 之间线性插值，再将全部权重统一归一化。knots 必须非空，第一个 coordinate 必须为 `0`，其余 coordinate 必须严格递增；最后一个 coordinate 加一是结果 shape 的唯一分量。

所有权重必须是有限的非负数，总权重必须大于零。

### 分布组合

```tap
finite::product(dists: List[Distribution]) -> Distribution
finite::mixture(dists: List[Distribution], weights: List[Float]) -> Distribution
finite::conditional(dist: Distribution, indices: List[Index]) -> Distribution
finite::map(dist: Distribution, shape: Index, mapping: Function[Index] -> Index) -> Distribution
```

`product` 从非空的 `dists` 构造独立联合分布，结果的 shape 是各分布 shape 的依次拼接。

`mixture` 按 `weights` 混合多个 shape 相同的分布。分布数量必须与权重数量相同且大于零。

`conditional` 将分布限制在 `indices` 指定的事件上并重新归一化，返回的分布保留原 shape。重复 Index 只计算一次，Index 不合法或事件质量为零时返回错误。

`map` 使用 `mapping(source_index)` 将源 Index 映射到目标 shape，映射到同一目标 Index 的概率相加。shape 的每一维必须大于零，mapping 返回的所有 Index 都必须与目标 shape 匹配。

所有组合权重必须是有限的非负数，总权重必须大于零。

### 具名有限分布

```tap
finite::bernoulli(p: Float) -> Distribution
finite::binomial(trials: Int, p: Float) -> Distribution
finite::multinomial(trials: Int, weights: List[Float]) -> Distribution
finite::hypergeometric(population: Int, successes: Int, draws: Int) -> Distribution
finite::zipf(n: Int, exponent: Float) -> Distribution
```

`bernoulli` 返回 shape 为 `(2,)` 的分布，Index `(0,)` 表示失败，Index `(1,)` 表示成功；`p` 是成功概率。

`binomial` 返回 `trials` 次独立 Bernoulli 试验中成功次数的分布，shape 为 `(trials + 1,)`，Index `(k,)` 表示成功 `k` 次。

`multinomial` 返回 `trials` 次 categorical 试验的类别计数分布。`weights` 决定类别概率；shape 包含 `len(weights)` 个值为 `trials + 1` 的分量。Index 的每个分量是对应类别的计数，各分量非负且总和为 `trials`，其余 Index 的概率为零。

`hypergeometric` 返回从大小为 `population`、其中包含 `successes` 个成功元素的总体中无放回抽取 `draws` 次时的成功次数分布，shape 为 `(draws + 1,)`，Index `(k,)` 表示成功 `k` 次，不可能的次数概率为零。

`zipf` 返回 shape 为 `(n,)` 的有界 Zipf 分布，Index `(i,)` 的权重为 `(i + 1) ^ (-exponent)`。

`p` 必须位于 `[0, 1]`。试验次数和总体参数必须满足各分布的有限总体约束。`weights` 必须非空，所有权重必须是有限的非负数且总权重大于零。`n` 和 `exponent` 必须大于零。

### 限制

原始支持集无限的分布不以未截断形式包含在 `finite` 中。若支持其有限截断版本，名称必须明确带有 `truncated`。

所有函数只接受签名及约束明确允许的输入；类型、范围、shape 或参数关系不合法，以及无法识别的输入，一律返回错误。
