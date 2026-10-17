# 用热点训练 PGO，以综合算法验收

验收指标固定为 16 项综合算法的 Tapas/Python 3.14 耗时比几何平均 ≤ **0.90**，各案例等权。基础热路径单独汇总。普通 Release 与 PGO 构建分别测量；报告的「构建参数」标明所用配置。

PGO 仅在构建时使用采样计数，最终解释器不带计数器。训练输入为 `hotspots/` 的 27 项案例，完整的 16 项综合算法用于验证，不参与训练。运行时没有算法名称、字段名称、校验和或输入规模特判。

## 构建和复现

以下流程适用于 Clang，本机验证使用 Apple Clang 21、arm64。`llvm-profdata` 必须与编译器兼容；脚本优先查找 PATH，其次使用 macOS 的 xcrun，也可通过 `--llvm-profdata` 显式指定。

```sh
cmake -S . -B build-profile-generate \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
  -DCMAKE_C_FLAGS=-fprofile-instr-generate \
  -DCMAKE_EXE_LINKER_FLAGS=-fprofile-instr-generate
cmake --build build-profile-generate -j4

python3 test/benchmarks/profile_script.py \
  build-profile-generate/bin/tapas build-pgo/profile/hotspots.profdata

cmake -S . -B build-pgo \
  -DCMAKE_BUILD_TYPE=Release -DTAPAS_ENABLE_IPO=OFF \
  "-DCMAKE_C_FLAGS=-flto=full -fprofile-instr-use=$PWD/build-pgo/profile/hotspots.profdata"
cmake --build build-pgo --clean-first -j4
ctest --test-dir build-pgo --output-on-failure

python3 test/benchmarks/ab_script.py \
  build-release/bin/tapas build-pgo/bin/tapas --rounds 3 --runs 8 \
  --json test/benchmarks/hotspots/pgo_ab_results.json \
  --markdown test/benchmarks/hotspots/PGO_AB_RESULTS.md
python3.14 test/benchmarks/compare_script.py \
  build-pgo/bin/tapas --runs 7 --require-algorithm-target
```

`TAPAS_ENABLE_IPO=OFF` 关闭 CMake 自动选择的 IPO 参数；这里的 LTO 由 `-flto=full` 显式启用，以免后加的 ThinLTO 参数覆盖它。普通 Release 的现有配置保持不变。没有采用 CPU 专用参数、浮点语义放宽或省略帧指针参数。

源码或编译器变化后应重新生成 profile，并清理重建使用它的二进制。`profile_script.py` 输出同名 JSON，记录训练二进制、构建参数、每份训练源码、校验和、profile 哈希及工具版本；训练中源码或二进制变化会报错。原始 profile 位于忽略的构建目录中。

`--require-algorithm-target` 要求执行脚本的 Python 为 3.14；用未四舍五入的比值判定目标，失败时仍生成两份完整 README，然后返回非零状态。常规测试不包含依赖机器负载的性能门槛。

## 对照与正确性

[源码优化 A/B](AB_RESULTS.md) 使用相同的普通 Release 参数。[构建优化 A/B](PGO_AB_RESULTS.md) 比较相同源码的普通 Release 与完整 LTO + PGO，两组结果应分别阅读。每组保留逐次样本、三轮结果、校验和及二进制哈希。

完整 LTO 曾使格式化测试失败。对照表明，未加入预解码分派的版本同样复现；关闭严格别名优化仍失败。根因是 `syntax` 包的 `dictionary_set_int` 将未初始化的局部值传给会先清理旧值的 setter。按项目惯例先初始化为 nil 后，格式化测试恢复；新增回归反复验证 token 字典的 String/Int 字段，并覆盖源码和字节码入口。这项修复独立于性能优化。

PGO 会改变编译器的内联和布局选择，收益取决于训练分布、编译器和处理器。最终验收使用完整算法实测，不把分项优化的百分比相乘。启动、源码加载和编译仍不在基准计时内；预解码增加的首次缓存建立成本也应在关注启动性能时另行测量。

语言写法审查阶段的 16 项几何平均为 **1.266**，未达到目标；后续热路径修订及最新测量见 [AUDIT.md](AUDIT.md)。旧 0.894 结果对应不同的语言实现版本，不能当作当前达标证据；合同及变化见 [SEMANTICS.md](../SEMANTICS.md)。
