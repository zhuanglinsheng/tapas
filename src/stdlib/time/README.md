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

本包不提供时区对象、日期算术或亚秒精度 Time；UTC 和当前纳秒时间戳接口见下节。所有函数只接受签名及约束明确允许的输入；类型、时间范围或 pattern 不合法，以及无法识别的输入，一律返回错误。

## 计时与 UTC

| 接口 | 合同 |
| --- | --- |
| `monotonic() -> Float` | 单调时钟秒数，仅用于同一次运行中的差值，不是 Unix 时间。 |
| `monotonic_ns() -> Int` | 同一单调时钟的整数纳秒数，检查 Int 溢出。 |
| `unix_ns() -> Int` | 调用时刻的 Unix 纳秒时间戳；检查 Int 范围。 |
| `format_utc(value: Time, pattern: String) -> String` | 使用 UTC 和宿主 C strftime 模式输出，不依赖本地时区。 |
| `parse_utc(text: String) -> Time` | 严格接受 `YYYY-MM-DDTHH:MM:SSZ`，年份 0001..9999；非法日期、闰秒、时区偏移和小数秒报错。 |
| `try_parse_utc(text: AnyType) -> Dictionary` | 返回 ok/value/code/error/errno；失败 value 为 Unix epoch Time，code 为 invalid_type、invalid_time、out_of_range。 |

新的纳秒查询不改变现有 Time 的秒精度，也不提供纳秒 Time 构造。
C 格式符及年份补零行为仍依赖平台；用于机器交换时，应选择明确格式并检查可表示范围。
两个格式化接口均拒绝模式中的内嵌 NUL。单调时钟无法跨机器或进程持久化比较；只保存差值。
