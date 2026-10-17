# os：运行环境与程序执行

面向 Linux/macOS；环境读取和程序执行属于本包，路径文本处理属于 path，文件系统操作属于 fs。
不修改调用进程的工作目录或环境变量。

## 环境读取

| 接口 | 结果 |
| --- | --- |
| `getenv(name: String) -> Dictionary` | `{found: Bool, value: String}`；未设置为 false/空串，设置为空为 true/空串。 |
| `cwd() -> String` | 当前工作目录；读取失败报错。 |
| `home() -> String` | 非空 HOME，否则查询当前真实用户的系统账户目录。 |
| `temp_dir() -> String` | 非空 TMPDIR，否则 `/tmp`；只返回位置，不创建或检查目录。 |
| `platform() -> String` | 当前支持平台为 linux 或 macos。 |

环境变量名必须非空且不含 NUL 或 `=`；不隐式展开变量。
返回的文本保留原始字节，不保证 UTF-8。参数错误和系统查询失败使用普通运行时错误。

## 同步执行

```tap
os::run(arguments: List[String], *,
    stdin = '', cwd, env = {}, inherit_env = true,
    timeout = 30.0, output_limit = 1048576) -> Dictionary
```

第一个实参是非空参数列表；执行文件名不得为空，所有参数、cwd 和环境名/值均不得含 NUL。
可选参数必须按名称传入，未知名称和重复选项报错。stdin 接受 String 或 bytes::Buffer，保留 NUL。
其他参数类型错误在启动前产生运行时错误。

- 默认直接执行程序，不经过 shell、不解释管道、重定向、变量或通配符。
- 可显式调用 `os::run(['/bin/sh', '-c', command])` 使用 shell；业务数据宜通过参数或 stdin 传递。
- cwd 只影响子进程，默认继承。env 是 String 键和值的 Dictionary，对继承环境进行覆盖；inherit_env=false 时从空环境开始。
- 不含 `/` 的执行文件名使用子进程环境 PATH 搜索；PATH 未设置时使用 `/usr/bin:/bin`，空路径段表示子进程工作目录。
- timeout 是大于 0、不超过 1e9 的有限秒数，覆盖启动后的等待与管道排空；默认 30 秒。
- output_limit 是每个输出流最多保留的字节数，默认 1 MiB，可为 0。超过后继续读取并丢弃，防止管道堵塞；正常退出状态仍保留。

## 执行结果

| 字段 | 合同 |
| --- | --- |
| `status: String` | exited、signaled、timeout、cancelled、spawn_error、io_error |
| `exit_code: Int` | 仅 exited 时有效，包括非零退出码；其他状态为 -1。 |
| `signal: Int` | 已观察到的子进程终止信号；未观察到为 0。 |
| `cancel_signal: Int` | 触发取消的 SIGINT/SIGTERM 编号，其他情况为 0。 |
| `stdout`, `stderr` | 各自的 String 原始字节，保留换行、NUL，不执行编码转换。 |
| `elapsed: Float` | 单调时钟测得的启动、等待和捕获耗时，单位秒。 |
| `errno: Int`, `error: String` | 启动或捕获的系统错误；没有错误时为 0/空串。诊断文本不稳定。 |
| `stdout_truncated`, `stderr_truncated` | 是否因输出限制或清理期限而未完整捕获。 |
| `output_truncated` | 任一路输出截断。 |

必须先检查 status，再解释其他字段。非零退出码是程序结果，不是 Tapas 异常。
启动错误通过独立的 close-on-exec 管道报告，不能用退出码 127 推断启动失败。
若要解析 stdout 中的 JSON，还应检查 output_truncated，避免解析不完整的结果。

## 生命周期

内部同时写 stdin、读取两路输出，处理短读写和 EINTR；输入结束关闭管道以发送 EOF。
父进程中无关的已打开描述符不会有意传给子进程。子进程在独立进程组中执行；超时、取消或捕获失败时
先发送 SIGTERM，100 ms 后对仍存在的组发送 SIGKILL。清理开始 300 ms 后关闭仍未结束的捕获管道，
随后回收直接子进程。内核中不可中断的等待可能延迟回收；本接口不提供硬实时保证或进程隔离沙箱。
主动脱离进程组的后代不保证被终止。

调用线程在等待期间临时阻塞 SIGINT/SIGTERM；原本未阻塞的信号转为 cancelled，调用前已阻塞的信号保持待处理，结束后恢复原信号掩码；
不安装进程全局信号处理器。SIGPIPE 在调用线程内处理，避免子进程提前关闭 stdin 导致 Tapas 被终止。
多线程宿主应自行协调进程信号路由，并同步其他线程对进程全局环境/工作目录的修改。

本包暂不提供异步进程对象、交互终端或持续 shell 会话。

```tapas
let os_example = os::run(['/bin/sh', '-c', 'printf hello; printf log >&2; exit 7'])
print(os_example::status)
print(os_example::exit_code)
print(os_example::stdout)
print(os_example::stderr)
```

<pre class='Tapas-Return'>
exited
7
hello
log
</pre>
