# random：可复现的伪随机数生成

`random` 提供带有明确随机源和 seed 的伪随机数生成器。Generator 保存并推进随机状态；相同的 Source、seed 和初始状态必须产生相同的随机序列。

## 支持范围

### 类型

```tap
random::Source: Type
random::Generator: Type
```

`Source` 是由 `random` 包提供的伪随机算法标识。`Generator` 是由 Source 和 seed 初始化的有状态随机数生成器，不提供用户构造接口。

### 随机源

```tap
random::pcg32_xsh_rr: Source
```

`pcg32_xsh_rr` 表示 PCG-XSH-RR 64/32 算法。Source 同时固定算法变体和初始化规则：seed 作为 PCG 的初始状态，序列常量固定为 54。其输出序列不依赖运行平台；相同 seed 必须产生相同序列。该随机源用于测试、采样和模拟，不提供密码学安全性。

### 通用操作

```tap
random::generator(source: Source, seed: Int) -> Generator
random::next_int(rng: Generator, bound: Int) -> Int
random::next_float(rng: Generator) -> Float
random::next_bool(rng: Generator) -> Bool
random::advance(rng: Generator, steps: Int) -> Nil
```

`generator` 使用 `source` 和 `seed` 创建处于初始状态的 Generator。Generator 被采样操作使用后推进状态；使用相同 Source 和 seed 重新构造 Generator，可以重现相同的采样序列。

`next_int` 等概率返回区间 `[0, bound)` 中的一个 Int；`bound` 必须大于零。

`next_float` 使用 53 bit 随机精度返回区间 `[0, 1)` 中的一个 Float。

`next_bool` 等概率返回 `true` 或 `false`。

`advance` 将 Generator 的状态移动 `steps` 个 32-bit 输出位置。正数向前移动，负数向后移动，零保持当前状态。

### 限制

从 Generator 生成任意有界整数时必须保证各结果等概率，不允许使用会产生取模偏差的直接取模。生成 Float 概率阈值时必须使用至少 53 bit 的随机精度。

`pcg32_xsh_rr` 不是密码学安全随机源，不得用于密钥、凭证、访问令牌或其他安全敏感值。

所有函数只接受签名及约束明确允许的输入；类型或参数不合法，以及无法识别的输入，一律返回错误。

## 参考资料

- Melissa E. O'Neill, [PCG: A Family of Simple Fast Space-Efficient Statistically Good Algorithms for Random Number Generation](https://www.pcg-random.org/pdf/hmc-cs-2014-0905.pdf), 2014.
- PCG Project, [C implementation](https://github.com/imneme/pcg-c), commit `83252d9c23df9c82ecb42210afed61a7b42402d7`.
- PCG Project, [Predictability of the PCG family](https://www.pcg-random.org/predictability.html).
