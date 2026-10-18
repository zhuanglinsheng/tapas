# 成员检查与排序性能恢复

本轮恢复上一轮撤回的字节/布尔成员检查优化，并处理它引出的排序回退。基线为上一轮整理后的普通 Release 构建；候选保留类型比较与 C 扩展能力校验修复。没有修改算法 benchmark 源码来获取收益。

## 最终结果

B/A 表示新版本耗时除以基线耗时，小于 1 更快。性能测量顺序执行，期间没有并行构建或运行测试。

| 检查 | B/A | 范围 |
| --- | ---: | --- |
| 原生 int 排序几何平均 | 0.947 | 11 种分布 × 4 种规模，3 轮 × 11 次 |
| 原生 float 排序几何平均 | 0.939 | 11 种分布 × 4 种规模，3 轮 × 11 次 |
| Tapas int 排序几何平均 | 0.995 | 11 种分布，8,192 项，3 轮 × 7 次 |
| Tapas float 排序几何平均 | 0.986 | 11 种分布，8,192 项，3 轮 × 7 次 |
| Tapas mixed 排序几何平均 | 1.001 | 11 种分布，8,192 项，3 轮 × 7 次 |
| list_sort 控制项 | 0.976 | 相同源码，3 轮 × 9 次 |
| heap_order 控制项 | 1.004 | 相同源码，3 轮 × 9 次 |
| heap_callback 控制项 | 1.002 | 相同源码，3 轮 × 9 次 |
| 固定 16 项综合算法几何平均 | 1.001 | 等权，3 轮 × 9 次 |

综合算法整体持平。本轮的主要收益在成员检查和排序内核；不把约 0.1% 的几何平均差异解释为确定的回退或提升。

| 成员检查 | B/A |
| --- | ---: |
| byte_64_first | 0.665 |
| byte_64_absent | 0.727 |
| byte_65536_last | 0.133 |
| byte_65536_absent | 0.133 |
| bool_64_first | 0.688 |
| bool_65536_absent | 0.133 |
| real_64_absent | 1.000 |
| real_65536_absent | 0.994 |

成员检查共覆盖 27 项：byte/bool/real、空容器、小型及大型输入、首项命中、末项命中、未命中和错误 Type。每项 3 轮，每轮预热一次、正式测量 11 次。所有结果校验。

## 保留的实现

- 字节与布尔存储复用字节键校验，成员检查使用 memchr。类型不匹配、空容器与 identical 语义不变。
- libc 查找隔离到禁止内联的小函数，使实数成员检查继续使用原有无调用的向量化扫描，避免 ARM64 的额外栈帧。
- 同构 Int/Float 在样本互异时使用更简单的双向二路分区。重复样本继续使用三路分区，混合 Number 保留原有三路路径。
- 二路分区后，跳过分界处连续等于枢轴的值，再递归剩余区间；仅递归较小侧，保留深度预算和 heapsort 保底。
- Float 重复值路径按样本中的枢轴频率选择分支顺序，只有占多数时优先检查相等；Int 和混合 Number 保留常规顺序。三路分区的常规循环保持直接表达，避免将额外策略带入 Int 热循环。

## 回退的分析与处理

### 实数成员检查

最初将 memchr 放进公共成员检查函数，导致实数分支也增加保存/恢复栈帧的指令。隔离 libc 调用后恢复原来的叶函数扫描，最终 real 对照接近基线。

### 排序布局与取样尝试

单纯固定函数对齐没有稳定改善。直接排序样本指针、改用完整样本中位数，也没有稳定补回运行时耗时，并改变了重复值分区的选择。因此最终均未保留。最终实现没有插入布局填充，也没有依赖特定 benchmark 名称或数据规模分派。

### 二路分区的边界

第一版二路分区在 512 项山形输入上慢约 28%，比较次数从基线 6,404 增至 8,307。跳过边界相等值后，比较次数降至 7,168，槽位写入从基线 4,359 降至 3,726；专项复测耗时比约 0.688。该修正减少重复递归，同时保持值及其身份。

### 重复值的分支顺序

重复样本不意味着枢轴占多数。一律优先判断相等，会让均匀低基数输入回退；频率选择也不值得无条件施加到 Int 路径。最终只在 Float 的多数枢轴情况下启用该路径。之前回退的 Int 随机八值与 Float 低基数控制分别做了五轮复测，回到基线附近。

### 计时波动的对照

原生驱动还测量未改动的 libc qsort。native_sort_ab_script.py 同时保存 qsort 控制项、辅助归一化比率与原始耗时；主结果始终采用实际耗时比，不用归一化掩盖回退。冻结矩阵最大原始比率约 1.030，对应 qsort 控制约 1.036；另一些对照也有共同波动，未再重现先前稳定的 4%–8% 回退。

综合算法中 sieve 单次计时约几十微秒，主表约 1 微秒的差异对应约 2.5%。补充对照在同一计时窗口重复执行原算法 128 次，5 轮 × 15 次，B/A 为 1.005，各轮为 1.004, 1.010, 0.993, 1.015, 1.007。未重现 2.5% 的持续损失，不据单次计时下结论。

## 验证与复现

- Release 全部 56 项通过。
- ASan、UBSan 的 sort/compact 各 2 项通过。排序单元测试覆盖 6 种值域、13 种分布和 23 种规模，校验顺序、完整值身份与引用计数。
- benchmark 脚本单元测试 12 项通过；git diff --check 通过。

基线运行时 SHA256：`e122256812628f8fb3001a34988ecc8947d1def236ecb201e514141611ee89c0`

最终运行时 SHA256：`17cd65cd3faf59131d8f126ee339aec8c3e1e32aa78fcd141e53fb91fb30752d`

最终数据使用 recovery_frozen 前缀，其他 recovery 文件是探索过程，不应混作最终结果：

- `experiments/recovery_frozen_native_ab.json`：88 项原生矩阵；由 native_sort_ab_script.py BASELINE_DRIVER CANDIDATE_DRIVER --runs 11 --rounds 3 生成。
- [成员检查 A/B](experiments/recovery_frozen_membership_ab.md)：同名 JSON 保存样本和源码；由 compact_membership_ab_script.py BASELINE CANDIDATE --runs 11 --rounds 3 生成。
- `experiments/recovery_frozen_sort_runtime_ab.json`：sort_ab_script.py 的 Int/Float/mixed × 11 分布，--size 8192 --runs 7 --rounds 3。
- [综合算法与控制 A/B](experiments/recovery_frozen_runtime_ab.md)：同名 JSON 保存样本、源码哈希与二进制哈希；ab_script.py 固定 16 算法加 list_sort/heap_order/heap_callback，--runs 9 --rounds 3 --seed 71。
- `experiments/recovery_inline_screen.json`：之前回退项的五轮复测，保留逐轮原始驱动输出。
- `experiments/recovery_sieve_repeat.json`：sieve 放大计时的完整源码与样本。
- `experiments/recovery_baseline_sources.json` 与 recovery_final_sources.json：相关源码快照；清理注释后重建，运行时与原生驱动哈希均与冻结测量相同。
