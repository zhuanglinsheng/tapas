# 实验材料

所有补丁基于 `99ea72197b3e277c668a01124297b3ed7be1752e`，只用于重建对应运行时。先在独立检出中应用补丁，使用相同 Release/IPO 参数构建，再用 [ab_script.py](../../ab_script.py) 比较二进制。JSON 的二进制哈希区分不同候选；历史结果不能当成最终版本的结果。

| 数据 | 对应候选源码 |
| --- | --- |
| `field_fusion.json` | 同时应用 `field_fusion.patch`、`copy_cleanup.patch` |
| `combined.json` | `combined.patch` |
| `dispatch_initial.json` | `dispatch_only.patch` |
| `dispatch_control.json` | B 为 `dispatch_only.patch`；A 在此基础上仅删除 `VM_NEXT` 中的空汇编约束 |
| `distributed_every_exit.json` | `distributed_every_exit.patch` |
| `per_opcode_control.json` | A 为 `distributed_every_exit.patch`；B 为 `dispatch_fields_runtime.patch` |
| `aa_results.json` | A、B 均为同一基线二进制，无运行时修改 |

`codegen.json` 记录汇编站点数、指令数、序言及可用的二进制哈希。`profile_vm.txt` 是延长浮点负载后的 CPU 栈采样摘录，用于定位解释器中的执行位置；采样负载和时间不参与正式 A/B 汇总。

`field_incremental_v1.json` 是分散分派基础上的早期字段融合增量对照，后续又增加了不匹配缓存、错误路径保护和按操作码汇合出口；它不是最终源码补丁的结果。`copy_confirmation.json` 是未采用复制简化的定向复测，不能据此声称本次修改了值复制实现。

首轮两项优化的最终数据另存为 `dispatch_fields_original.json` / `dispatch_fields_original.md`，对应 `dispatch_fields_runtime.patch`。当前最终源码使用 `final_runtime.patch`，包含分派、字段融合、预解码、字节比较、只读哈希探测、标量覆盖、键比较、简单 PUSHX、短字符串共享及 syntax 值初始化修复；完整原基线对比见上一级 [AB_RESULTS.md](../AB_RESULTS.md) 和 [ab_results.json](../ab_results.json)。

## 综合算法 0.90 目标的后续实验

以下 JSON 均保存原始样本及覆盖案例，探索性筛选为两轮；正式验收为上级目录的三轮报告。部分筛选只覆盖相关算法，不应把它们的几何平均当成全部 16 项的结果。

| 数据 | 对应配置或补丁 |
| --- | --- |
| `target90_outline_arithmetic.json` | `target90_outline_arithmetic.patch`，整体外提算术融合函数；未采用 |
| `target90_string_comparison.json` | `target90_string_comparison.patch`，扩大通用比较融合；未采用 |
| `target90_dead_scalar_slots.json` | `target90_dead_scalar_slots.patch`，退出作用域省略标量清零；未采用 |
| `target90_frame_rebind.json` | `target90_frame_rebind.patch`，扩大调用帧快速复用；未采用 |
| `target90_complete_ipo.json` | `dispatch_fields_runtime.patch` 加 `target90_complete_ipo.patch`，扩大默认 ThinLTO 覆盖；未采用 |
| `target90_hotspot_pgo.json` | `dispatch_fields_runtime.patch`，ThinLTO + PGO；训练来源见 `target90_pgo_training.json` |
| `target90_hotspot_pgo_full.json` | 同上源码，完整 LTO + PGO；属于初始化修复前的探索结果 |
| `target90_native_control.json` | 同上完整 LTO + PGO，B 另加 `-mcpu=native`；未采用 |
| `target90_predecoded.json` | A 为 `dispatch_fields_runtime.patch`；B 为 `target90_predecoded.patch` |
| `target90_omit_control.json` | 两端均为 `target90_predecoded.patch` 加 `syntax_value_init.patch`、完整 LTO + PGO；B 加 `-fomit-frame-pointer`；未采用 |
| `target90_byte_comparison.json` | A 为预解码加初始化修复；B 为 `target90_pre_hash_runtime.patch`；两端普通 Release |

完整 LTO 探索中暴露的未初始化局部值已修复，原因及复现步骤见 [PGO 说明](../PGO.md)。早期完整 LTO 的性能数字不代表其正确性验收通过；最终修复后的构建使用上级目录的 [PGO A/B](../PGO_AB_RESULTS.md) 和 [训练记录](../pgo_training.json)。

## 哈希表、分派与短字符串的后续对照

下列补丁也基于原始提交，包含此前保留的修改。补丁中的注释整理可能晚于筛选样本；正式最终报告的 runtime_diff_sha256 应与 final_runtime.patch 完全一致。

| 原始数据 | 候选源码及判断 |
| --- | --- |
| `target90_pre_hash_runtime.json` | `target90_pre_hash_runtime.patch`，哈希优化前的完整普通 Release A/B |
| `target90_pre_hash_pgo.json` | 同上源码的普通 Release / 完整 LTO + PGO A/B |
| `target90_scalar_hash.json` | `target90_scalar_hash.patch`，标量覆盖及删除清理；普通 Release 删除未确认收益 |
| `target90_readonly_probe.json` | `target90_readonly_probe.patch`，只读探测；LRU 两轮约快 3% |
| `target90_key_equality.json` | `target90_key_equality.patch`，直接键比较；k_nucleotide 两轮约快 3.6%、7.2% |
| `target90_empty_list.json` | `target90_empty_list.patch`，空 List 延迟分配；整体无稳定收益，已恢复 |
| `target90_lazy_small_growth.json` | `target90_lazy_small_growth.patch`，再将首次增长改为 4；整体无收益，已恢复 |
| `target90_allocator_contract.json` | `target90_allocator_contract.patch`，分配器合约；不宣称独立显著收益 |
| `target90_pushx_address.json` | `target90_pushx_address.patch`，全部 PUSHX 地址类型拆分；回调和树回退，未采用 |
| `target90_pushx_plain_shared.json` | `target90_pushx_plain_shared.patch`，仅简单 PUSHX 小型入口，仍共用原出口 |
| `target90_pushx_plain_exit.json` | `target90_pushx_plain_exit.patch`，简单 PUSHX 使用自己的分派出口；相对前一项增量 |
| `target90_pair_string.json` | A 为 `target90_pushx_plain_exit.patch`，B 为 `final_runtime.patch`；两字节共享，k_nucleotide 两轮约快 13% |

`target90_hash_target_screen.txt` 的三次跨语言筛选结果为约 0.900214，严格判为失败；`target90_plain_target.txt` 的七次测量约 0.901，同样未达标。没有用显示精度将它们判为通过。最终两字节共享后的七次验收见上级两份 README，0.894 达标。

`target90_hash_training.json`、`target90_predecoded_pgo_training.json` 为相应中间阶段的训练记录，最终训练记录为上级 pgo_training.json。早期汇编证据仅用于解释当时的代码生成，不代表最终版本的精确指令数。

`final_codegen.json` 记录最终普通 Release / 完整 LTO + PGO 的 exec_tins 静态指令、调用和间接跳转站点、序言及二进制哈希。静态数量不是运行次数，不作为单独的速度结论。

## 跨语言语义与惯用写法审查

本轮在保留指定算法的前提下修订语言写法，合同见 [SEMANTICS.md](../../SEMANTICS.md)。此前 0.894 的跨语言达标结果仅对旧源码成立，不作为当前案例的结论。

- `pre_fairness_sources.json` 保存修订前全部 129 份基准源码及 SHA-256，可重建历史测量输入。
- `pre_fairness_README.md` / `pre_fairness_README_en.md`、`pre_fairness_OPTIMIZATION.md` 保存旧跨语言报告和实施结论。
- `pre_fairness_ab_results.json` / `pre_fairness_AB_RESULTS.md`、`pre_fairness_pgo_ab_results.json` / `pre_fairness_PGO_AB_RESULTS.md` 保存旧源码版本的两个正式运行时 A/B。
- `pre_fairness_pgo_training.json` 为旧训练记录；当前 profile 清单位于上一级 pgo_training.json。
- `fairness_initial_candidates.json` 与 `fairness_candidates_sources.json` 是首轮写法候选及其源码。
- `fairness_alternatives.json` 包含 Python map、原生查空格、LRU 及切片等替代实现的原始样本与完整候选源码。
- `fairness_candidates.json` 为惯用写法修订相对最初源码的两轮对照；后续 Dijkstra 堆选择另见 `fairness_heap_candidates.json`，后者包含对照两端源码。

最终运行时 A/B 使用修订后的同一份 Tapas 源码，每项三轮、每轮五次有效样本；跨语言验收为每项七次。被主动中断的测量没有写入最终 JSON。不同版本输入的数字不能直接当作同一次 A/B 的样本合并。

`fairness_selected_sources.json` 保存当前采用的全部 129 份源码及哈希，包括最终计时边界修正；历史候选的哈希与当前源码不同是预期现象。

## 基础热路径逐项审查

27 项用途及解释边界见 [AUDIT.md](../AUDIT.md)。`pre_hotspot_audit_sources.json` 保存修订前全部 81 份热点源码，`pre_hotspot_audit_README.md` / `_en.md` 保存旧测量表。dp_row 的非法前驱行已替换为真实 LCS 状态，dict_counting 已改为实际频率累加，旧校验和和旧时间不能当作当前输入的结果。

最新逐次样本、七组 Python 结构对照和完整源码哈希见上级 comparison_results.json；内核完整行/频数检查见上级 kernel_checks.json。`pre_hotspot_audit_codegen.json` 是修订前配置的汇编记录，final_codegen.json 与最新重新训练的 PGO 二进制对应。历史阶段的运行时 A/B 输入版本已在各报告页首注明。
