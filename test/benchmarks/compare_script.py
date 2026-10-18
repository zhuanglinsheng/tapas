#!/usr/bin/env python3
import argparse
import datetime
import hashlib
import json
import os
import platform
import statistics
import subprocess
import sys
from pathlib import Path


HERE = Path(__file__).resolve().parent
HOT_PATH_CATALOG = json.loads((HERE / "hotspots/catalog.json").read_text())
ALGORITHM_BENCHMARKS = (
    "recursive_fibonacci",
    "sieve",
    "merge_sort",
    "n_queens",
    "longest_common_subsequence",
    "matrix_multiply",
    "sudoku",
    "dijkstra",
    "optimize",
    "kmp",
    "k_nucleotide",
    "binary_trees",
    "lru_cache",
    "huffman",
    "fannkuch",
    "n_body",
)
ALGORITHM_TARGET_RATIO = 0.90

BASIC_HOT_PATH_BENCHMARKS = (
    "vm_hot_paths",
    "float_arithmetic",
    "function_call_baseline",
    "function_calls",
    "function_callbacks",
    "list_access",
    "list_append",
    "list_update",
    "list_copy",
    "list_from_iterable",
    "list_replicate",
    "list_sort",
    "dict_insert",
    "dict_lookup",
    "dict_update",
    "dict_delete",
    "dict_literal",
    "dict_counting",
    "tail_recursion",
    "branch_logic",
)


def discover_hot_paths(directory: Path = HERE / "hotspots") -> tuple[str, ...]:
    names = []
    for source in sorted(directory.glob("*.tap")):
        if not source.with_suffix(".py").is_file():
            raise RuntimeError(f"missing Python reference for {source}")
        if source.stem in ALGORITHM_BENCHMARKS:
            raise RuntimeError(f"duplicate algorithm/hotspot name: {source.stem}")
        names.append(source.stem)
    return tuple(names)


# Preserve the established order while including every new .tap/.py pair.
_discovered_hot_paths = discover_hot_paths()
HOT_PATH_BENCHMARKS = (
    tuple(name for name in BASIC_HOT_PATH_BENCHMARKS if name in _discovered_hot_paths)
    + tuple(name for name in _discovered_hot_paths if name not in BASIC_HOT_PATH_BENCHMARKS)
)
BENCHMARKS = ALGORITHM_BENCHMARKS + HOT_PATH_BENCHMARKS
BenchmarkRow = tuple[str, int, int, int | None, float, float | None]


def algorithm_ratio(rows: list[BenchmarkRow]) -> float:
    ratios = {row[0]: row[4] for row in rows}
    return statistics.geometric_mean(ratios[name] for name in ALGORITHM_BENCHMARKS)


def build_configuration(tapas: Path) -> list[str] | None:
    cache = tapas.resolve().parent.parent / "CMakeCache.txt"
    if not cache.is_file():
        return None
    keys = ("CMAKE_BUILD_TYPE:", "CMAKE_C_COMPILER:", "CMAKE_C_FLAGS:",
            "CMAKE_C_FLAGS_RELEASE:", "CMAKE_EXE_LINKER_FLAGS:", "TAPAS_ENABLE_IPO:")
    return [line for line in cache.read_text().splitlines() if line.startswith(keys)]


def benchmark_source(name: str) -> Path:
    """Source stem relative to this directory, shared by execution and reports."""
    if name in HOT_PATH_BENCHMARKS:
        return Path("hotspots") / name
    if name in ALGORITHM_BENCHMARKS:
        return Path(name)
    raise ValueError(f"unknown benchmark: {name}")


def lua_version_text(lua: str) -> str:
    # Lua versions differ in where `-v` writes: 5.4 prints to stderr,
    # newer builds to stdout. Try both so the report records one line.
    probe = subprocess.run(
        [lua, "-v"], check=True, capture_output=True, text=True
    )
    text = probe.stdout or probe.stderr
    first = text.splitlines()
    if not first:
        raise RuntimeError(f"{lua} -v printed no version")
    return first[0]


def format_micros(nanoseconds: int) -> str:
    return f"{nanoseconds / 1000:,.1f} µs"


def write_markdown(
    path: Path,
    tapas: Path,
    lua_version: str,
    runs: int,
    rows: list[BenchmarkRow],
    language: str,
) -> None:
    tapas_version = subprocess.run(
        [str(tapas), "-v"], check=True, capture_output=True, text=True
    ).stdout.splitlines()[0]
    if language == "zh":
        separator = "："
        period = "。"
        title = "# Tapas 与 Python、Lua 性能比较"
        navigation = "简体中文 | [English](README_en.md) | [项目主页](../../README.md)"
        date_label = "测试日期"
        environment_title = "## 测试环境"
        system_label = "系统"
        architecture_label = "处理器架构"
        executable_label = "Tapas 可执行文件"
        build_label = "构建参数"
        lua_label = "Lua 解释器"
        runs_text = f"有效运行次数：每项 {runs} 次，另预热 1 次"
        results_title = "## 结果"
        timing_text = "时间为进程 CPU 时间的微秒中位数，不包含进程启动、源码加载和编译。"
        ratio_text = ("比值小于 1 表示 Tapas 更快（分别相对 Python 与 Lua）。")
        columns = "| 项目 | 源码 | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |"
        algorithm_title = "综合算法"
        algorithm_description = "保留指定算法，允许各语言使用惯用容器和标准库批量操作；不要求逐行翻译。输入、重复次数、输出及计时边界保持等价。规则见 SEMANTICS.md。"
        hot_path_title = "基础热路径"
        hot_path_description = (
            "用于定位性能瓶颈：保留被测关键操作，在此约束内采用合理写法。"
            "任务型结果提示能力缺口，同机制对照和运行时 A/B 用于归因；"
            "例如 `heap_callback` 三语言均执行语言级比较回调，原生搜索与显式 while 的比值不能直接解释为循环成本。"
            "诊断合同见 [SEMANTICS.md](SEMANTICS.md#热点合同)。"
            "缺少 Lua 实现时标为 —，不计入 Lua 几何平均。"
        )
        group_mean = "本组几何平均数"
        usage_title = "## 用法"
        usage = (
            "测试应使用 Release 构建，并需要 `PATH` 中的 `lua` 可执行文件（可用 `--lua` 指定）。",
            "",
            "```sh",
            "python3 test/benchmarks/compare_script.py build-release/bin/tapas",
            "```",
            "",
            ("脚本会重新测量全部综合算法和 `hotspots/` 案例，先预热一次，再比较每项 3 次有效运行的中位数（可用 `--runs` "
             "调整），打印摘要并更新本目录的两份 README 文档。"),
            ("只查看控制台输出、不更新文档时，加 `--no-markdown`；"
             "用 `--markdown 路径` 可指定其他报告位置。"),
            ("重复 `--markdown` 会用同一组测量数据生成多份报告，"
             "避免数字因重复运行而不同。"),
            "要求每一项都快于 Python 时，使用 `--require-faster`。",
            "验收综合性能目标时，使用 Python 3.14 运行并加 `--require-algorithm-target`：固定 16 项综合算法的 Tapas/Python 耗时比几何平均必须 ≤ 0.90，热点不计入。",
            "",
            "综合算法源码保留在本目录；基础热点与算法退化热点统一放在 `hotspots/`，运行入口仍在本目录。",
            "算法退化热点另见 [诊断分析](hotspots/ANALYSIS.md)，包含优化 Python 写法、同结构对照及单因素实验。",
            "运行：`python3.14 test/benchmarks/diagnose_script.py build-release/bin/tapas --algorithms --ablations --controls`。",
            "运行时优化另见 [实施说明](hotspots/OPTIMIZATION.md) 与 [A/B 结果](hotspots/AB_RESULTS.md)；使用 `ab_script.py BASELINE CANDIDATE --rounds 3 --runs 8` 对相同源码多轮交替测量，传入同一二进制可检查 A/A 噪声。",
            "PGO 的热点训练与构建步骤见实施说明；`profile_script.py` 会记录训练输入、二进制和 profile 的哈希。",
            "List 字典序和默认堆的写法 A/B 见 [字典序实验](hotspots/LIST_ORDER.md)。",
            "原生 heap 包与 Dijkstra、Huffman 的写法 A/B 见 [heap 实验](hotspots/HEAP.md)；`source_ab_script.py` 比较同一运行时上的两份等价 Tapas 源码。",
            "通用 fill/count 的 sieve 源码 A/B 与 Python count 写法选择见 [批量操作实验](hotspots/BULK.md)。",
            "sort 与 libc qsort 的原生及运行时对照见 [排序优化](hotspots/SORT.md)。本轮逐项优化见 [紧凑序列与排序](hotspots/COMPACT_AND_SORT.md)。",
            "代码结构整理、扩展能力校验与性能保留决策见 [代码整理验证](hotspots/CODE_CLEANUP.md)。",
            "成员检查收益恢复与排序回退处理见 [性能恢复验证](hotspots/PERFORMANCE_RECOVERY.md)。",
            "LCS 的当前写法、历史外移实验与源码 A/B 见 [LCS 写法优化](hotspots/LCS.md)。",
            "字符比较、局部变量初始化优化及作用域清理实验见 [值路径优化](hotspots/VALUE_PATHS.md)。",
            "通用堆策略与回调边界的独立 A/B 见 [通用堆优化](hotspots/HEAP_OPTIMIZATION.md)。",
            "堆回调的测量合同、Python 写法选择与解释限制见 [公平性审查](hotspots/HEAP_CALLBACK.md)。",
            "六项综合算法的剩余差距与语法、原生操作及对象表示的区分见 [剩余差距分析](hotspots/REMAINING_GAPS.md)。",
            "现有语法与标准库下的候选写法及三语言源码 A/B 见 [惯用写法优化](hotspots/IDIOMATIC.md)。",
            "Lua 的 16 项综合算法复核、候选源码与独立复测见 [Lua 写法审查](hotspots/LUA_AUDIT.md)；当前结果采用经验证保留的实现，不声称全局最优。",
            "保留运行时或基准源码优化后，应使用最终构建同步重测并更新中英文 README、原始样本及校验记录。",
            ("性能门槛不属于常规测试，因为 CPU 负载、电源状态、编译器和 "
             "Python 版本都会影响结果；发布比较结果时应同时记录运行环境。"),
        )
        notes_title = "## 说明"
        notes = (
            "这些程序用于比较各实现执行语义等价任务时的性能，不代表大型应用的完整性能。",
            "各语言使用语义等价的惯用实现；内建批量操作和数据结构带来的优势属于比较结果的一部分。",
            "比较脚本核对固定负载输出；verify_semantics.py 额外检查小输入与完整结果序列。",
            "结果会受系统负载、电源状态、编译器版本、Python 版本和 Lua 版本影响。",
            "更新 VM 或运行环境后，应使用上文「用法」中的命令重新生成。",
        )
    else:
        separator = ": "
        period = "."
        title = "# Tapas, Python, and Lua Performance Comparison"
        navigation = "[简体中文](README.md) | English | [Project Home](../../README_en.md)"
        date_label = "Test date"
        environment_title = "## Test Environment"
        system_label = "System"
        architecture_label = "Architecture"
        executable_label = "Tapas executable"
        build_label = "Build options"
        lua_label = "Lua interpreter"
        runs_text = f"Measured runs: {runs} per benchmark, after 1 warm-up run"
        results_title = "## Results"
        timing_text = "Times are median process CPU times in microseconds and exclude process startup, source loading, and compilation."
        ratio_text = ("A ratio below 1 means Tapas is faster (against Python and "
                      "Lua respectively).")
        columns = ("| Benchmark | Source | Tapas | Python | Lua | "
                   "Tapas/Python | Tapas/Lua |")
        algorithm_title = "Complete Algorithms"
        algorithm_description = "Keep the specified algorithm while allowing idiomatic containers and standard-library bulk operations. Inputs, repetitions, outputs, and timing boundaries are equivalent; line-by-line translations are not required. See SEMANTICS.md for the contracts."
        hot_path_title = "VM Hot Paths"
        hot_path_description = (
            "Diagnose bottlenecks by retaining the operations being measured and using reasonable implementations within that constraint. "
            "Task results expose capability gaps; matched-mechanism controls and runtime A/B experiments support attribution. "
            "For example, all heap_callback implementations execute language-level comparators; a native-search versus explicit-while ratio does not isolate loop cost. "
            "See [SEMANTICS.md](SEMANTICS.md#热点合同) for the diagnostic contract. "
            "Missing Lua implementations are shown as — and excluded from the Lua geometric mean."
        )
        group_mean = "The geometric mean ratio for this group"
        usage_title = "## Usage"
        usage = (
            ("Use a Release build and a `lua` executable on `PATH` "
             "(override with `--lua`)."),
            "",
            "```sh",
            "python3 test/benchmarks/compare_script.py build-release/bin/tapas",
            "```",
            "",
            ("The script remeasures all complete algorithms and `hotspots/` cases, performs one warm-up, and compares the median of "
             "three measured runs per benchmark (tune with `--runs`), prints "
             "a summary, and updates both README documents in this directory."),
            ("Add `--no-markdown` for console output only, or pass "
             "`--markdown PATH` to write a report elsewhere."),
            ("Repeating `--markdown` writes multiple reports from the same "
             "measurements, so their numbers cannot diverge because of a "
             "second run."),
            ("To require Tapas to be faster than Python in every benchmark, "
             "use `--require-faster`."),
            "Run under Python 3.14 with `--require-algorithm-target` to require a geometric mean Tapas/Python time ratio ≤ 0.90 across the fixed 16 complete algorithms; hotspots are excluded.",
            "Complete algorithm sources live here; basic and algorithm-derived hotspots live in `hotspots/`. Runner scripts remain here.",
            "Algorithm-derived deficits are covered in the [diagnostic analysis](hotspots/ANALYSIS.md), with optimized Python references, matched controls, and ablations.",
            "Run `python3.14 test/benchmarks/diagnose_script.py build-release/bin/tapas --algorithms --ablations --controls`.",
            "See the [implementation notes](hotspots/OPTIMIZATION.md) and [A/B results](hotspots/AB_RESULTS.md). Use `ab_script.py BASELINE CANDIDATE --rounds 3 --runs 8` for alternating rounds on identical sources; pass the same binary twice for an A/A noise control.",
            "See the implementation notes for hotspot-only PGO training and build commands; `profile_script.py` records training inputs, binary hashes, and the profile hash.",
            "See [List ordering experiments](hotspots/LIST_ORDER.md) for native lexicographic heaps and source A/B results.",
            "See the [heap experiments](hotspots/HEAP.md) for Dijkstra and Huffman source A/B tests. `source_ab_script.py` compares two equivalent Tapas programs on the same runtime.",
            "See the [bulk-operation experiments](hotspots/BULK.md) for sieve fill/count source A/B tests and the Python count reference choice.",
            "See [sort experiments](hotspots/SORT.md) for native and runtime comparisons against libc qsort, and [compact sequences and sorting](hotspots/COMPACT_AND_SORT.md) for the latest optimization round.",
            "See [code cleanup validation](hotspots/CODE_CLEANUP.md) for Type comparison, extension capability validation, and performance retention decisions.",
            "See [performance recovery validation](hotspots/PERFORMANCE_RECOVERY.md) for restored membership gains and sorting regression analysis.",
            "See [LCS source optimization](hotspots/LCS.md) for the current implementation, historical hoisting experiments, and source A/B results.",
            "See [value-path optimization](hotspots/VALUE_PATHS.md) for generic character comparison and local initialization improvements, plus scope-cleanup experiments.",
            "See [generic heap optimization](hotspots/HEAP_OPTIMIZATION.md) for heap strategy and callback-entry A/B experiments.",
            "See [heap callback fairness](hotspots/HEAP_CALLBACK.md) for the contract, Python representation screening, and interpretation limits.",
            "See [remaining algorithm gaps](hotspots/REMAINING_GAPS.md) for source A/B evidence separating syntax, native operations, and object representation in six complete algorithms.",
            "See [idiomatic source optimization](hotspots/IDIOMATIC.md) for candidate implementations and source A/B tests across all three languages.",
            "See the [Lua source audit](hotspots/LUA_AUDIT.md) for the 16-algorithm review, archived candidates, and independent confirmation; retained implementations are measured choices, not a claim of global optimality.",
            "After retaining a runtime or benchmark-source optimization, remeasure the final build and update both README reports, raw samples, and validation records together.",
            ("Performance thresholds are not part of the regular test suite "
             "because CPU load, power settings, compiler version, and Python "
             "version affect the results; published comparisons should "
             "always include the runtime environment."),
        )
        notes_title = "## Notes"
        notes = (
            "These programs compare semantically equivalent tasks across the available implementations; they do not represent complete application performance.",
            "Each language uses an idiomatic implementation with equivalent semantics; advantages from built-in bulk operations and data structures are part of the comparison.",
            "The comparison checks fixed-workload results; verify_semantics.py additionally checks small kernel inputs and complete output sequences.",
            "Results depend on system load, power settings, compiler version, Python version, and Lua version.",
            "Regenerate the reports with the commands in the Usage section after changing the VM or runtime environment.",
        )
    lines = [
        title,
        "",
        navigation,
        "",
        f"{date_label}{separator}{datetime.date.today().isoformat()}",
        "",
        environment_title,
        "",
        f"- {system_label}{separator}`{platform.platform()}`",
        f"- {architecture_label}{separator}`{platform.machine()}`",
        f"- Python{separator}`{platform.python_version()}`",
        f"- Tapas{separator}`{tapas_version}`",
        f"- {lua_label}{separator}`{lua_version}`",
        f"- {executable_label}{separator}`{tapas}`",
        f"- {runs_text}",
        "",
        results_title,
        "",
        timing_text,
        ratio_text,
        "",
    ]
    rows_by_name = {row[0]: row for row in rows}
    configuration = build_configuration(tapas)
    if configuration:
        environment_end = lines.index(f"- {runs_text}") + 1
        lines[environment_end:environment_end] = [
            f"- {build_label}{separator}`{entry}`" for entry in configuration
        ]

    def append_group(title: str, names: tuple[str, ...], description: str) -> None:
        separator_row = "| --- | --- | ---: | ---: | ---: | ---: | ---: |"
        lines.extend(
            [
                f"### {title}",
                "",
                description,
                "",
                columns,
                separator_row,
            ]
        )
        python_ratios = []
        lua_ratios = []
        for name in names:
            _, tapas_ns, python_ns, lua_ns, ratio_py, ratio_lua = rows_by_name[
                name
            ]
            python_ratios.append(ratio_py)
            if ratio_lua is not None:
                lua_ratios.append(ratio_lua)
            source = Path(os.path.relpath(HERE / benchmark_source(name), path.parent.resolve())).as_posix()
            sources = f"[Tapas]({source}.tap) · [Python]({source}.py)"
            if lua_ns is not None:
                sources += f" · [Lua]({source}.lua)"
            lua_time = format_micros(lua_ns) if lua_ns is not None else "—"
            lua_ratio = f"{ratio_lua:.3f}×" if ratio_lua is not None else "—"
            lines.append(
                f"| `{name}` | {sources} | {format_micros(tapas_ns)} | "
                f"{format_micros(python_ns)} | {lua_time} | "
                f"{ratio_py:.3f}× | {lua_ratio} |"
            )

        def mean_text(label: str, ratios: list[float]) -> str:
            value = f"**{statistics.geometric_mean(ratios):.3f}×**" if ratios else "—"
            count = f"（{len(ratios)} 项）" if language == "zh" else f" ({len(ratios)} cases)"
            return f"{label} {value}{count}"

        lines.extend([
            "",
            f"{group_mean}{separator}{mean_text('Tapas/Python', python_ratios)}, "
            f"{mean_text('Tapas/Lua', lua_ratios)}{period}",
            "",
        ])

    append_group(
        algorithm_title,
        ALGORITHM_BENCHMARKS,
        algorithm_description,
    )
    append_group(
        hot_path_title,
        HOT_PATH_BENCHMARKS,
        hot_path_description,
    )
    audit_link = Path(os.path.relpath(HERE / "hotspots/AUDIT.md", path.parent.resolve())).as_posix()
    lines.extend([
        (f"内核、任务、表示与语义案例测量的层面不同，不能把本组均值解释成纯 VM 性能。逐项结论见[热路径审查]({audit_link})。" if language == "zh" else
         f"Kernels, tasks, representations and semantic cases measure different layers; this group mean is not a pure VM score. See the [case audit]({audit_link})."),
        "",
    ])
    lines.extend(
        [
            usage_title,
            "",
            *usage,
            "",
            notes_title,
            "",
            *notes,
            "",
        ]
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines), encoding="utf-8")


def measure(command: list[str], runs: int) -> tuple[int, list[int]]:
    totals: list[int] = []
    elapsed: list[int] = []
    for _ in range(runs + 1):
        result = subprocess.run(
            command, check=True, capture_output=True, text=True, timeout=120
        )
        lines = result.stdout.splitlines()
        if len(lines) != 2:
            raise RuntimeError(
                f"unexpected output from {' '.join(command)}: {result.stdout!r}"
            )
        totals.append(int(lines[0]))
        elapsed.append(int(lines[1]))
        if elapsed[-1] <= 0:
            raise RuntimeError(f"nonpositive CPU time from {command}: {elapsed[-1]}")
    if len(set(totals)) != 1:
        raise RuntimeError(f"result changed between runs: {totals}")
    return totals[0], elapsed[1:]


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare matching Tapas, Python, and Lua benchmark programs."
    )
    parser.add_argument("tapas", type=Path, help="Release Tapas executable")
    parser.add_argument(
        "--lua",
        type=str,
        default="lua",
        help="Lua interpreter executable (default: lua from PATH)",
    )
    parser.add_argument(
        "--runs",
        type=int,
        default=3,
        help="valid runs per benchmark (default: 3)",
    )
    parser.add_argument(
        "--require-faster",
        action="store_true",
        help="fail when the Tapas median is not lower than Python's",
    )
    parser.add_argument(
        "--require-algorithm-target",
        action="store_true",
        help="fail unless the 16-algorithm geometric mean Tapas/Python time ratio is <= 0.90",
    )
    parser.add_argument(
        "--markdown",
        type=Path,
        action="append",
        help=("write the comparison and environment to a Markdown file; "
              "repeat for additional reports; by default both "
              "README.md and README_en.md next to this script are "
              "updated from one run"),
    )
    parser.add_argument(
        "--json", type=Path, help="save samples, checksums, controls and source hashes"
    )
    parser.add_argument(
        "--no-markdown",
        action="store_true",
        help="skip writing Markdown reports (console output only)",
    )
    args = parser.parse_args()
    if args.runs < 3:
        parser.error("--runs must be at least 3")
    if args.require_algorithm_target and sys.version_info[:2] != (3, 14):
        parser.error("--require-algorithm-target must run under Python 3.14")

    lua_version = lua_version_text(args.lua)
    print(f"Lua interpreter: {lua_version}")

    failed = False
    python_ratios: list[float] = []
    lua_ratios: list[float] = []
    rows: list[BenchmarkRow] = []
    samples = {}
    sources = [HERE / benchmark_source(name).with_suffix(extension)
               for name in BENCHMARKS for extension in (".tap", ".py", ".lua")
               if (HERE / benchmark_source(name).with_suffix(extension)).is_file()]
    hashes = {str(p.relative_to(HERE)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}
    binary_hash = hashlib.sha256(args.tapas.read_bytes()).hexdigest() if args.json else None
    for name in BENCHMARKS:
        source = HERE / benchmark_source(name)
        tapas_total, tapas_times = measure(
            [str(args.tapas), str(source.with_suffix(".tap"))], args.runs
        )
        python_total, python_times = measure(
            [sys.executable, str(source.with_suffix(".py"))], args.runs
        )
        lua_source = source.with_suffix(".lua")
        lua_times = None
        if lua_source.is_file():
            lua_total, lua_times = measure([args.lua, str(lua_source)], args.runs)
            if lua_total != tapas_total:
                raise RuntimeError(
                    f"{name}: different results: Tapas={tapas_total}, Lua={lua_total}"
                )
        if tapas_total != python_total:
            raise RuntimeError(
                f"{name}: different results: Tapas={tapas_total}, Python={python_total}"
            )

        samples[name] = {"checksum": tapas_total, "tapas": tapas_times,
                         "python": python_times, "lua": lua_times}
        if HOT_PATH_CATALOG.get(name, {}).get("python_matched"):
            control_total, control_times = measure(
                [sys.executable, str(source.with_suffix(".py")), "--matched"], args.runs
            )
            if control_total != tapas_total:
                raise RuntimeError(f"{name}: structural control has a different result")
            samples[name]["python_matched"] = control_times

        tapas_median = int(statistics.median(tapas_times))
        python_median = int(statistics.median(python_times))
        lua_median = int(statistics.median(lua_times)) if lua_times is not None else None
        ratio_python = tapas_median / python_median
        ratio_lua = tapas_median / lua_median if lua_median is not None else None
        python_ratios.append(ratio_python)
        if ratio_lua is not None:
            lua_ratios.append(ratio_lua)
        rows.append(
            (name, tapas_median, python_median, lua_median,
             ratio_python, ratio_lua)
        )
        print(name)
        print(f"  result:        {tapas_total}")
        print(f"  Tapas median:  {tapas_median:,} ns")
        print(f"  Python median: {python_median:,} ns")
        print(f"  Lua median:    {lua_median:,} ns" if lua_median is not None
              else "  Lua median:    — (no Lua implementation)")
        print(f"  Tapas/Python:  {ratio_python:.3f}x")
        print(f"  Tapas/Lua:     {ratio_lua:.3f}x" if ratio_lua is not None
              else "  Tapas/Lua:     —")
        if args.require_faster and ratio_python >= 1.0:
            failed = True

    ratios_by_name = {row[0]: (row[4], row[5]) for row in rows}

    for family, names in (("algorithm", ALGORITHM_BENCHMARKS),
                          ("hot-path", HOT_PATH_BENCHMARKS)):
        for index, language in enumerate(("Python", "Lua")):
            ratios = [ratios_by_name[name][index] for name in names
                      if ratios_by_name[name][index] is not None]
            mean = f"{statistics.geometric_mean(ratios):.3f}x" if ratios else "—"
            print(f"{family} geometric mean Tapas/{language}: {mean} ({len(ratios)} cases)")

    target_met = algorithm_ratio(rows) <= ALGORITHM_TARGET_RATIO
    print(f"algorithm target <= {ALGORITHM_TARGET_RATIO:.2f}: {'PASS' if target_met else 'FAIL'}")
    for source in sources:
        if hashlib.sha256(source.read_bytes()).hexdigest() != hashes[str(source.relative_to(HERE))]:
            raise RuntimeError(f"benchmark changed during measurement: {source}")
    if args.json:
        if hashlib.sha256(args.tapas.read_bytes()).hexdigest() != binary_hash:
            raise RuntimeError("Tapas binary changed during measurement")
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps({
            "date": datetime.datetime.now(datetime.timezone.utc).isoformat(),
            "system": platform.platform(), "python": sys.version, "lua": lua_version,
            "tapas": str(args.tapas.resolve()),
            "tapas_sha256": binary_hash,
            "catalog_sha256": hashlib.sha256((HERE / "hotspots/catalog.json").read_bytes()).hexdigest(),
            "build_config": build_configuration(args.tapas),
            "runs": args.runs, "warmups": 1, "samples_ns": samples,
            "sources_sha256": hashes,
        }, indent=2) + "\n")
    markdowns = args.markdown
    if markdowns is None and not args.no_markdown:
        markdowns = [HERE / "README.md", HERE / "README_en.md"]
    for markdown in markdowns or ():
        markdown_language = "en" if markdown.name.endswith("_en.md") else "zh"
        write_markdown(
            markdown,
            args.tapas,
            lua_version,
            args.runs,
            rows,
            markdown_language,
        )
        print(f"wrote Markdown report: {markdown}")
    if failed:
        print(
            "Tapas is not faster than Python in every benchmark.",
            file=sys.stderr,
        )
        return 1
    if args.require_algorithm_target and not target_met:
        print("The 16-algorithm performance target has not been reached.", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
