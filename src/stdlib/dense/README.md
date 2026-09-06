# dense：二维稠密数组与线性代数操作

`dense` 提供 RealArray 与 BoolArray 的二维 shape 查询、转置，以及 RealArray 的向量和矩阵运算。返回新数组的操作不修改输入；名称带有 `inplace` 或接收显式输出数组的操作会修改目标数组。

## 支持范围

### Shape 与构造

```tap
dense::rows(value: RealArray | BoolArray) -> Int
dense::cols(value: RealArray | BoolArray) -> Int
dense::transpose(value: RealArray | BoolArray) -> RealArray | BoolArray
dense::identity(size: Int) -> RealArray
```

`rows` 和 `cols` 返回二维数组的行数与列数。`transpose` 返回行列互换的新数组，并保留 RealArray 或 BoolArray 的元素类型。`identity` 返回 `size × size` 的 RealArray 单位矩阵；`size` 必须是非负整数。

### 向量与矩阵运算

```tap
dense::trace(value: RealArray) -> Float
dense::inner(left: RealArray, right: RealArray) -> Float
dense::outer(left: RealArray, right: RealArray) -> RealArray
dense::norm(value: RealArray) -> Float
dense::normalize(value: RealArray) -> RealArray
```

`trace` 返回主对角线元素之和；非方阵使用 `min(rows, cols)` 个对角元素。`inner` 将 shape 相同的数组按扁平元素序列计算内积。`outer` 接受行向量或列向量，并返回以两个向量长度为 shape 的外积矩阵。

`norm` 返回全部元素的欧几里得范数。`normalize` 返回除以该范数的新数组；零数组不能归一化。

### 原地操作

```tap
dense::copy_into(source: RealArray, target: RealArray) -> Nil
dense::scale_inplace(value: RealArray, factor: Int | Float) -> Nil
dense::add_scaled_inplace(target: RealArray, source: RealArray, factor: Int | Float) -> Nil
dense::gemm(alpha: Int | Float, left: RealArray, right: RealArray, beta: Int | Float, output: RealArray) -> Nil
```

`copy_into` 将 source 的全部元素复制到 shape 相同的 target。`scale_inplace` 将 value 的每个元素乘以 factor。`add_scaled_inplace` 执行 `target += factor * source`，两个数组的 shape 必须相同。

`gemm` 执行 `output = alpha * left * right + beta * output`。必须满足 `left.cols == right.rows`，output 的 shape 必须为 `(left.rows, right.cols)`，并且 output 不得与 left 或 right 是同一数组。

### 限制

除 shape 查询与转置外，线性代数操作只接受 RealArray。需要相同 shape、向量或矩阵乘法相容 shape 的操作不会执行隐式广播或重排。

所有函数只接受签名及约束明确允许的输入；类型、shape、别名关系或参数不合法，以及无法识别的输入，一律返回错误。
