# math：标量数学函数

`math` 提供 Int 与 Float 标量上的初等函数、指数与对数、浮点分解、取整、余数、特殊函数和浮点分类。除 `abs` 在 Int 输入时保留 Int 外，数值计算通常将输入转换为 Float，并遵循运行平台 C 数学库的结果语义。

## 支持范围

以下签名中的 `Number` 表示 `Int | Float`，仅用于简化本页展示，不是 `math` 导出的 Type。

### 绝对值、幂与根

```tap
math::abs(value: Number) -> Int | Float
math::fabs(value: Number) -> Float
math::sqrt(value: Number) -> Float
math::rsqrt(value: Number) -> Float
math::cbrt(value: Number) -> Float
math::pow(left: Number, right: Number) -> Float
math::hypot(left: Number, right: Number) -> Float
math::eleinv(value: Number) -> Float
```

`abs` 返回绝对值并保留 Int 输入的结果类型；`fabs` 始终返回 Float。`rsqrt` 计算 `1 / sqrt(value)`，`eleinv` 计算 `1 / value`。`hypot` 计算两个分量的欧几里得长度。

### 三角与双曲函数

```tap
math::sin(value: Number) -> Float
math::cos(value: Number) -> Float
math::tan(value: Number) -> Float
math::asin(value: Number) -> Float
math::acos(value: Number) -> Float
math::atan(value: Number) -> Float
math::atan2(left: Number, right: Number) -> Float
math::sinh(value: Number) -> Float
math::cosh(value: Number) -> Float
math::tanh(value: Number) -> Float
math::asinh(value: Number) -> Float
math::acosh(value: Number) -> Float
math::atanh(value: Number) -> Float
```

三角函数使用弧度。`atan2(left, right)` 将 left 作为纵坐标分量、right 作为横坐标分量，并保留平台数学库的象限与符号零语义。

### 指数、对数与缩放

```tap
math::exp(value: Number) -> Float
math::exp2(value: Number) -> Float
math::expm1(value: Number) -> Float
math::log(value: Number) -> Float
math::log2(value: Number) -> Float
math::log10(value: Number) -> Float
math::log1p(value: Number) -> Float
math::logb(value: Number) -> Float
math::ilogb(value: Number) -> Int
math::ldexp(value: Number, exponent: Int) -> Float
math::scalbn(value: Number, exponent: Int) -> Float
math::scalbln(value: Number, exponent: Int) -> Float
```

`expm1` 计算 `exp(value) - 1`，`log1p` 计算 `log(1 + value)`，用于改善接近零时的精度。`logb` 与 `ilogb` 返回平台浮点表示中的指数。`ldexp`、`scalbn` 和 `scalbln` 按二进制幂缩放 value。

### 浮点分解

```tap
math::frexp(value: Number) -> Pair
math::modf(value: Number) -> Pair
math::remquo(left: Number, right: Number) -> Pair
```

`frexp` 返回 `(mantissa, exponent)`，满足有限非零输入近似等于 `mantissa * 2 ^ exponent`。`modf` 返回 `(fractional_part, integer_part)`，两项均为 Float。`remquo` 返回 `(remainder, quotient_bits)`，第二项为 Int，其有效位数量由平台数学库决定。

### 特殊函数

```tap
math::erf(value: Number) -> Float
math::erfc(value: Number) -> Float
math::lgamma(value: Number) -> Float
math::tgamma(value: Number) -> Float
```

`erf` 与 `erfc` 分别计算误差函数及其补函数。`tgamma` 计算 Gamma 函数，`lgamma` 计算其绝对值的自然对数。

### 取整与余数

```tap
math::ceil(value: Number) -> Float
math::floor(value: Number) -> Float
math::nearbyint(value: Number) -> Float
math::rint(value: Number) -> Float
math::lrint(value: Number) -> Int
math::llrint(value: Number) -> Int
math::round(value: Number) -> Float
math::lround(value: Number) -> Int
math::llround(value: Number) -> Int
math::trunc(value: Number) -> Float
math::fmod(left: Number, right: Number) -> Float
math::remainder(left: Number, right: Number) -> Float
```

`nearbyint`、`rint`、`lrint` 与 `llrint` 使用当前浮点舍入模式。`round`、`lround` 与 `llround` 在中点处向远离零的方向取整。`trunc` 向零截断。`fmod` 的余数使用向零截断的商，`remainder` 使用最接近整数的商。

### 浮点组合与边界

```tap
math::copysign(left: Number, right: Number) -> Float
math::nextafter(left: Number, right: Number) -> Float
math::fdim(left: Number, right: Number) -> Float
math::fmax(left: Number, right: Number) -> Float
math::fmin(left: Number, right: Number) -> Float
math::fma(first: Number, second: Number, third: Number) -> Float
math::make_nan() -> Float
```

`copysign` 返回 left 的大小和 right 的符号。`nextafter` 返回从 left 朝 right 方向的下一个可表示 Float。`fdim` 返回正差 `max(left - right, 0)`。`fma` 以平台提供的融合乘加语义计算 `first * second + third`。`make_nan` 返回 NaN。

### 分类与比较

```tap
math::isfinite(value: Number) -> Bool
math::isinf(value: Number) -> Bool
math::isnan(value: Number) -> Bool
math::isnormal(value: Number) -> Bool
math::fpclassify(value: Number) -> Int
math::signbit(value: Number) -> Bool
math::isgreater(left: Number, right: Number) -> Bool
math::isgreaterequal(left: Number, right: Number) -> Bool
math::isless(left: Number, right: Number) -> Bool
math::islessequal(left: Number, right: Number) -> Bool
math::islessgreater(left: Number, right: Number) -> Bool
math::isunordered(left: Number, right: Number) -> Bool
```

分类与比较函数遵循平台浮点规则。`isunordered` 在任一参数为 NaN 时返回 `true`；其余有序比较在参数无序时返回 `false`。`fpclassify` 返回平台 C 数学库的分类整数，不保证不同平台使用相同数值编码。

### 限制

超出函数数学定义域、发生上溢或下溢，以及包含 NaN 或无穷时，结果遵循运行平台的浮点与 C 数学库行为，可能是 NaN、无穷或平台相关分类值；本包不将所有数学域错误转换为 Tapas 错误。

将 Float 结果转换为 Int 的函数要求结果可由 Tapas Int 表示。所有函数只接受签名明确允许的 Int 或 Float；类型或参数数量不合法，以及无法识别的输入，一律返回错误。
