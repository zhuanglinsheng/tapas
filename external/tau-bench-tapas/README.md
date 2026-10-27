# 原始 τ-bench Retail：Tapas 可执行约束复现

来源是 Sierra 的 **τ-bench**，不是 τ²-bench，也不是本目录 `simplified/` 的单订单简化示例。原始仓库保存在项目根目录 `external/tau-bench`，固定提交 `59a200c6d575d595120f1cb70fea53cef0632f6b`。`SOURCE.json` 保存政策、工具、任务、数据文件的 SHA-256；执行复现前校验这些文件，防止同一 HEAD 下未提交修改污染结果。原始 MIT 许可保存在 `UPSTREAM_LICENSE`。

这里复现所有 16 个工具的结构化运行契约，并把 Retail 政策逐条映射到会话约束或明确的证据要求。**不把工具检查通过等同于完整会话政策通过，也不声称已自动理解所有自然语言。** 逐条范围、差异和边界见 [COVERAGE.md](COVERAGE.md)。

## 审阅状态

进一步审阅结论为 **需要修改**。`review_retail.py` 与 `review_results.json` 复现了原回归集之外的漏检：相邻工具状态未接续验证、请求覆盖转人工义务、普通计算器表达式差异、领域规则缺少必填邮箱检查，另有混合大小写 yes 的兼容问题。已有通过数量是固定回归集的结果，不是对所有输入的完整性证明。

政策测试的 `REVIEW=True` 是测试前提，且正例没有实际助手自然语言文本，不能用来证明“不编造、无主观推荐、退款与邮件承诺正确”已完成逐句审阅。原始任务文件也不是完整对话记录；缺少的消息证据保持未核实。

这里的“完整 JSON”仅指保留并比较作用域内嵌套对象的全部字段；更准确的名称是“基于原始嵌套 JSON 状态的运行时契约”。它不表示完整 JSON Schema 校验、全库状态证明、跨调用轨迹证明、政策语义已审阅或求解器已支持整体抽样。

复现审阅发现：

```sh
python3 external/tau-bench-tapas/review_retail.py
```

## 执行

从 Tapas 项目根目录执行；只需要 Python 3 标准库和已编译的 Tapas，不需要模型 API、Pydantic 或安装原项目的 LLM 依赖。

```sh
python3 external/tau-bench-tapas/fetch_upstream.py
cmake -S . -B build
cmake --build build --target tapas -j 4
python3 external/tau-bench-tapas/reproduce.py
python3 external/tau-bench-tapas/test_policy.py
```

下载脚本不会覆盖有未提交修改的仓库。原始仓库被父项目 `.gitignore` 排除，用脚本与提交清单复现，避免意外加入嵌套 Git 仓库。

`reproduce.py --source /path/to/tau-bench --tapas /path/to/tapas --report /tmp/results.json` 可指定位置。政策测试也支持 `--source`、`--tapas`、`--report`。

运行报告默认写入仓库根目录的 `tmp/external/tau-bench-tapas/`，实验脚本默认写入其中的 `experiments/sampling/results/`。这些中间产物仅在本地保存，不纳入版本控制。

## 实现

| 文件 | 作用 |
|---|---|
| `domain.tap` | 原始全量数据库的目录、ID、支付类型、引用、状态和地址约束；`relations(db)` 返回用户、商品规格及支付账户 Table 快照 |
| `model.tap` | 订单、用户、商品完整 JSON 状态上的独立转移模型，以及 `ToolContract(case)` 和命名政策 Rule |
| `calculator.tap` | 不调用 Python eval 的独立算术解析器；错误信息按类别核对 |
| `policy.tap` | 从有角色和轮次的事件中推导认证、归属、确认、状态查询、提醒、串行调用等事实，再检查政策 Rule |
| `reproduce.py` | 直接载入并执行原仓库工具，回放正式三个划分全部参考动作，与独立 Tapas 模型对照 |
| `test_policy.py` | 在真实工具记录上构造会话政策的正反例和缺失证据用例 |
| `results.json`、`policy_results.json` | 运行时生成的报告，保存在 `tmp/external/tau-bench-tapas/` |

Python 负责加载原始数据、调用原始工具和记录结果，业务转移预测与政策判定在 Tapas 中实现。每个参考任务重新开始独立状态；同一任务内的动作连续执行，不会每一步重置订单。任务指令只按 AST 解析为数据，不执行其中的文本。

工具状态使用引用闭包：操作涉及的用户、订单、商品，身份查找的所有匹配用户，以及列目录时的整个目录。对闭包内所有字段做前后比较，保留多商品、商品选项、支付账户、历史价格、付款退款历史、履约字段等原始结构。没有将其他不相关用户也复制进每一步状态；因此这不是对原始工具任意修改整个全库的全局 frame 证明。全量原始数据另外通过 `domain.tap` 检查。

## Table 关系与符号生成

`domain::relations(db)` 从具体数据库快照生成用户表、商品规格引用表和支付账户归属表。`Domain` 用完整商品对检查引用，状态和支付来源也使用 Table；不改变历史订单价格，不将浮点金额直接放入整数求解模型。表格是快照，数据库变化后需要重新构造。

```sh
python3 external/tau-bench-tapas/test_tables.py build/bin/tapas
build/bin/tapas external/tau-bench-tapas/simplified/test_generate_table_inputs.tap
```

第一项测试覆盖引用错误及按用户和来源联合选择支付账户，需要 OR-Tools。第二项在单商品换货模型中联合生成目标规格、额度、输出和后状态；说明见 [Table 换货建模](simplified/README.md#六用-table-联合选择规格与生成状态)。这些关系可供有限投影使用，完整 `ToolContract` 中的循环、更新和自然语言政策边界仍按下文说明处理。

## 当前结果

正式划分共 **635 个任务**：train 500、dev 20、test 115；`tasks.py` 是旧版任务表示，不计入正式划分。连同补充边界与连续操作，共 **1,476 次调用**，16 个工具全部覆盖，转移模型差异为 0。输出篡改检测 16/16，状态越界篡改检测 16/16。会话政策测试 41/41。

原始参考动作有 **44 次返回 Error**；其中包括余额不足和不存在的查询目标，既可能是预期拒绝，也可能暴露数据或参考动作的问题。结果保存在 `reference_tool_errors`，不能当作 44 个模型差异，或直接推断为 44 个错误标注。补充用例还保留原工具在空付款历史下抛出的 `IndexError`。

正式任务只给参考工具动作，缺少完整的用户 yes、助手说明、全部商品提醒与消息轮次，因此不对这 635 个任务报告“政策合规率”。41 个政策用例是专门构造的、有显式证据的测试轨迹，不能冒充原始真实会话。

## 约束能力与边界

`rules::check` 可在完整嵌套 JSON 上执行这些契约，保留具名约束诊断。模型使用循环、查表、深拷贝和字符串处理；这些辅助函数不是当前 `solve::hold/sample` 能整体翻译的 CP-SAT 表达式。**没有把全量 Retail 报告成已实现的符号抽样模型。** 若论文实验需要从全量模型生成新输入，仍需建立可求解的有限投影并验证它与运行契约的一致性，或扩展求解器支持。

政策中的事实真实性、主观推荐、行动描述是否忠实、业务范围，以及退款时效/邮件/不需新订单等自然语言承诺，须有外部审阅证据。没有该证据就列为 unknown，不视为通过。该实现不是自动语言裁判，也不会真的寄邮件或等待 5–7 个工作日；原工具本身也没有实现这些外部行为。

工具契约针对原始数据结构及符合工具参数类型的调用；不是对任意损坏数据库或任意 Python 输入的等价性证明。金额保留原始浮点计算和求和顺序，比较容差为 `1e-7`（不会容忍一分钱差异），两位小数舍入通过 FMA 修正二进制中点情况。查询大小写处理针对本数据库的 ASCII 姓名和邮箱。计算器覆盖原任务表达式及普通有限浮点算术、幂、整除、括号；不保证复刻任意大整数、所有 Python 数值极端情况或 CPython 错误文案。JSON 对象键顺序、数字输出格式按结构语义比较。
