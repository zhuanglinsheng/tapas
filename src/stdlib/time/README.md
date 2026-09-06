# time：系统时间值与 Unix 时间转换

`time` 提供 Time 与 Unix 秒数之间的转换，以及按本地时区格式化 Time。当前时间通过根作用域的 `now()` 获取；本包负责已有 Time 值的转换和展示。

## 支持范围

### 通用操作

```tap
time::from_unix(seconds: Int) -> Time
time::unix(value: Time) -> Int
time::format(value: Time, pattern: String) -> String
```

`from_unix` 将 Unix epoch 起的秒数转换为 Time。`unix` 返回 Time 对应的 Unix 秒数；在平台 Time 精度高于秒时，结果仍以整秒表示。

`format` 使用运行平台的本地时区和 `strftime` pattern 格式化 Time。空 pattern 返回空 String；其他 pattern 的指令与本地化结果遵循平台 C 运行库。

### 限制

Unix 时间的可表示范围同时受 Tapas Int、平台 `time_t` 与本地时间转换能力限制；某些平台不支持负时间戳。格式化结果最大为 65535 字节，无法产生非空结果或超出限制时返回错误。

本包不提供时区对象、UTC 专用格式化、日期算术或亚秒级 Unix 转换。所有函数只接受签名及约束明确允许的输入；类型、时间范围或 pattern 不合法，以及无法识别的输入，一律返回错误。
