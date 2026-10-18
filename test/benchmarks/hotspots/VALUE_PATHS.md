# 字符比较与局部变量路径优化

本轮针对字符缓存、动态局部初始化和作用域退出的通用成本，不依赖 LCS 输入、字符内容或指定容器尺寸。没有新增语法、公开类型或扩展 ABI。

## 保留的实现

- 内部索引字节与普通 String 比较时，直接比较长度与字节，不再先物化为 String。覆盖 `==`、`!=` 和两个操作数顺序；其他类型仍走原有运算符分派。单字节 String 原本已有缓存，本次不是消除逐字符堆分配。
- 未被捕获的局部变量可以在动态下标、调用、对象等初始化表达式求值后，直接通过已有 `VCRT-init` 接收结果。函数构造保留绑定先行的布局，未知类型算术和简单命名下标保留已有的直接写槽融合；可变、被捕获及无初始化表达式的绑定保留原有处理。

标量省略清零、清理函数拆分和单值守卫均做过实验，最终未保留，原因见下文。

这些调整保留词法作用域和原有作用域清理，不推迟对象生命周期，也不改变闭包绑定。尚未实现固定局部帧、按活跃区间分配槽位、字符表示跨变量传播或自动循环不变量外提；本轮没有用不完整的类型推断省略对象清理。

## 测量方法

先按改动分别筛选，再对最终构建测量全部 16 项综合算法和 30 项基础热路径。使用相同 Release 配置、同一份源码，进程顺序执行，交替 A/B；每轮独立预热，按固定种子改变案例顺序。测量期间没有并行构建、测试或其他性能实验。B/A 小于 1 表示候选更快，配对四分位区间不等于置信区间。

编译器 A/B 必须隔离自动字节码缓存。原脚本在同一工作目录执行两个版本，可能把一侧生成的字节码交给另一侧。本轮修正 `ab_script.execute`，每次执行使用独立工作目录，并验证目录和缓存隔离。保持普通源码执行模式，程序内部 CPU 时钟排除编译、启动和目录准备成本。

未隔离缓存的 [早期初始化实验](experiments/value_paths_initializer_ab.json)已明确标为无效，不用于性能结论。仅改变 VM、编译输出相同的早期字符实验仍可用于分析；最终结果均使用隔离缓存的工具复测。

## 最终结果

最终二进制 SHA-256：`fa7b0ec89ed71d51d49d4ae9bd03746af7b646491026849b2f8266569ffd2ce1`。构建配置、源码哈希和检查清单见 [manifest](experiments/value_paths_manifest.json)。

| 对照 | 配对 B/A |
| --- | ---: |
| 16 项综合算法几何平均 | 0.989 |
| 30 项基础热路径几何平均 | 0.993 |
| LCS：相同最终源码 | 0.879 |
| dp_row：相同源码 | 0.892 |
| 缓存字符在左侧 | 0.591 |
| 缓存字符在右侧 | 0.618 |
| 字符不等比较 | 0.598 |
| 比较结果保存为 Bool | 0.840 |
| 直接索引字符对照 | 0.996 |
| 动态下标局部初始化 | 0.903 |
| 对象及别名局部初始化 | 0.913 |
| 多算术局部变量对照 | 0.996 |
| Huffman 放大负载控制 | 0.998 |
| heap 放大负载控制 | 0.996 |
| 浮点对照 | 0.998 |

完整案例为 3 轮 × 7 次，诊断内核为 3 轮 × 9 次，每轮各版本先预热一次。所有跨版本校验和一致。完整案例中没有总体配对中位回退超过 1.5% 的项目；不把约 1% 的差异都解释为确定收益或退化。

[完整运行时 A/B 表](experiments/value_paths_full_ab.md)、[全部样本](experiments/value_paths_full_ab.json)、[内核源码快照与样本](experiments/value_paths_final_sources_ab.json)。

最终源码 A/B 使用同一个运行时：相对原先直接索引、外移 above 的写法，现在缓存每行字符并恢复内层 above，五轮 × 11 次配对 B/A 为 **0.988**，耗时约降低 1.2%；五轮中位数均小于 1。见 [源码 A/B](experiments/value_paths_lcs_source_ab.json)。

最终构建的六项 A/A 控制中位数范围为 0.999–1.001，各项 3 轮 × 9 次。配对样本仍有波动，不能把中位数范围当成每个样本的误差上限。见 [A/A 原始数据](experiments/value_paths_aa_control.json)。

## 正确性验证

- Release：56 项 CTest 全部通过，串行执行，避免文档案例共享字节码临时文件的竞争。
- ASan、UBSan：各通过 compile_frontend 和 language_rules；后者包含本轮新增的源码与显式字节码回归。ASan 使用 `detect_leaks=0`，不将其表述为泄漏检查。
- 13 项 benchmark 脚本测试通过，包括自动缓存隔离与目录清理。
- 46 组三语言 benchmark 校验、14 组小输入探针，以及 94 组完整 DP 行、直方图、堆输出核验通过。
- 88 组 LCS 输入通过独立穷举子序列 oracle，覆盖空输入、重复字符、无公共字符和不等长输入。[原始结果](experiments/value_paths_lcs_correctness.json)。
- 字符回归覆盖双操作数顺序、相等/不等、空串、多字节内容、String 与其他类型交替、保存 Bool 及临时 String。局部初始化回归覆盖对象别名、类型交替、break/continue、提前返回和延迟声明；既有闭包与递归测试同时保留。


## 回退原因与处理

### 字符比较融合

最初同时扩展栈比较与命名比较融合。混合字符比较变快，但直接索引 LCS 回退约 7%；隔离混合处理并保留字节分支后仍回退约 4%。扩展给高频双字节路径增加了探测，并扩大 VM 内联代码；进一步保留原有融合形态，仅在慢路径补齐混合比较，直接索引 LCS 恢复到基线附近，同时保留主要的混合比较收益。

最终不把混合类型处理复制到每个 VM 融合入口。相关原始样本：[首版](experiments/value_paths_string_ab.json)、[隔离处理](experiments/value_paths_string_refined_ab.json)、[命名融合版本](experiments/value_paths_string_named_ab.json)、[仅补齐比较](experiments/value_paths_string_leaf_ab.json)。

### 动态初始化

将所有未捕获初始化一律改成 `VCRT-init`，动态下标与对象内核快约 9–10%，但多算术局部变量内核回退约 26%。字节码对照确认，未知类型的 `VCRT → PUSH → named arithmetic → POPCOV` 原本能直接融合；一律改写后失去该路径。简单命名下标也已有直接写槽路径。

最终保留这两类既有融合，只推广其他动态初始化。不是由于小幅数据变化撤回整个优化。原始数据：[隔离缓存后的宽泛版本](experiments/value_paths_initializer_isolated_ab.json)、[保留算术融合](experiments/value_paths_initializer_refined_ab.json)。

### 函数绑定布局

动态初始化最初也覆盖函数构造，改变了 `PUSHF` 与函数体在共享字节码数组中的位置。浮点内核没有新增循环工作，但五轮对照仍慢约 1.5%。函数构造并非本轮动态局部收益所需，因此恢复它原有的绑定先行形式。随后浮点字节码与基线逐项一致，五轮 B/A 恢复到 0.997，fannkuch 为 0.993，LCS 为 0.875。

`tfunc_new` 使用共享指令数组中的起止位置，因而仅移动函数构造也可能影响执行布局。实验支持布局相关解释，但没有进一步用硬件计数器区分取指与缓存效应。数据见 [函数绑定控制](experiments/value_paths_function_binding_ab.json)。

### 标量清理与 VM 布局

首版还内联重写了槽位初始化与函数帧清理，收益不稳定。收窄到作用域标量清理后，多局部变量内核快约 2.7%，但浮点内核五轮均有约 1.2% 回退。浮点计时循环没有局部声明或作用域删除，字节码循环没有额外工作，因而不能解释为标量删除算法变慢；证据指向大型 VM 函数的代码布局扰动。

把清理函数移出 VM 内联主体后，浮点五轮 B/A 回到 1.000，优化器为 1.001，多局部变量约 0.991。解决了浮点回退，但完整案例又显示 fannkuch 回退约 2.3%、尾递归约 1.8%。清理函数拆分使空清理和单标量路径也承担函数调用；加入空值/单标量守卫后，尾递归恢复，fannkuch 仍在五轮中稳定回退约 1.7%。去掉清理改动后，fannkuch 五轮 B/A 为 0.995。

最终恢复原有清理逻辑。省略清零本身语义正确，但局部收益被 VM 布局和新增调用抵消；三种实现都未同时改善通用负载。继续围绕单个内核调节布局容易把成本转移给其他指令，当前不值得保留这组零散补丁。后续需要结合固定槽位整体设计，而非把变量移出作用域。

原始数据：[宽泛清理候选](experiments/value_paths_scalar_cleanup_ab.json)、[只改变标量清理](experiments/value_paths_cleanup_isolated_ab.json)、[独立清理函数](experiments/value_paths_cleanup_outlined_ab.json)、[空值/单标量守卫](experiments/value_paths_cleanup_guarded_ab.json)、[恢复原清理的控制](experiments/value_paths_without_cleanup_controls.json)。

## LCS 写法与比较边界

正式源码在每行缓存 `character`，在内层循环声明 `let above = previous[j]`。保留两行 DP、行交换和 `diagonal`、`preceding` 缓存，算法仍为 O(mn) 时间、O(n) 额外空间。Python 和 Lua 参考算法不变。

运行时 A/B 对两侧使用相同的最终源码；源码 A/B 则使用同一个运行时比较两种写法。前者的 LCS 收益不能直接当成“上一版最佳源码到新版源码”的收益，也不能把两批次的中位耗时相除替代配对测量。[历史 LCS 外移实验](LCS.md)保留旧数据，当前三语言结果以主 README 为准。

## 当前三语言表

使用最终构建完整刷新中英文 README，每项每种语言预热一次、正式测量 7 次。当前 LCS 中位耗时为 Tapas 12,328 µs、Python 3.14 9,328 µs、Lua 11,212 µs，Tapas/Python 为 1.322×。本批次与源码/运行时 A/B 分开测量，不直接用历史批次的跨语言比值判断本轮改动是否退化。

完整三语言结果见 [中文表](../README.md)、[英文表](../README_en.md)和[原始样本](comparison_results.json)。

## 复现与后续范围

完整案例使用 `ab_script.py BASELINE CANDIDATE --rounds 3 --runs 7`。显式路径的诊断内核使用 `runtime_source_ab_script.py BASELINE CANDIDATE SOURCE... --json RESULT`，报告保存源码快照、哈希、各轮顺序、校验和及配对样本；从报告恢复内核源码即可复测。源码选择使用 `source_ab_script.py RUNTIME BASELINE_SOURCE CANDIDATE_SOURCE --rounds 5 --runs 11 --json RESULT`。

[最终运行时代码补丁](experiments/value_paths_final.patch)与[失败候选补丁](experiments/value_paths_discarded_patches.json)相对于本轮开始的工作区，不代表此前所有未提交改动。二进制哈希与构建信息保存在对应 JSON 中。

后续固定局部帧需要统一处理嵌套作用域、提前退出、闭包和无初始化声明，再考虑活跃区间共享槽位。字符值流和自动不变量外提同样需要可靠的用途、副作用及逃逸分析，不能仅以源代码看起来不变为依据。
