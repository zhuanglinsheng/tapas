#!/usr/bin/env python3
import argparse
import datetime
import platform
import statistics
import subprocess
import sys
from pathlib import Path


HERE = Path(__file__).resolve().parent
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

HOT_PATH_BENCHMARKS = (
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
    "dict_insert",
    "dict_lookup",
    "dict_update",
    "dict_delete",
    "dict_literal",
    "dict_counting",
    "tail_recursion",
    "branch_logic",
)

BENCHMARKS = ALGORITHM_BENCHMARKS + HOT_PATH_BENCHMARKS


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
    rows: list[tuple[str, int, int, int, float, float]],
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
        lua_label = "Lua 解释器"
        runs_text = f"有效运行次数：每项 {runs} 次，另预热 1 次"
        results_title = "## 结果"
        timing_text = "时间为进程 CPU 时间的微秒中位数，不包含进程启动、源码加载和编译。"
        ratio_text = ("比值小于 1 表示 Tapas 更快（分别相对 Python 与 Lua）。")
        columns = "| 项目 | 源码 | Tapas | Python | Lua | Tapas/Python | Tapas/Lua |"
        algorithm_title = "综合算法"
        algorithm_description = "这些程序组合使用递归、分支、容器、索引和内存分配，更接近完整算法负载。"
        hot_path_title = "基础热路径"
        hot_path_description = ("这些程序把计时循环放在函数内，并让循环体只保留被测的那一种 VM 操作或容器访问模式，"
                                "用来定位解释器的基础开销；与目标操作无关的取模寻址、增长整数累加和模块级变量访问均已移除，"
                                "避免把其他语言的长短处混进单项结果。"
                                "`vm_hot_paths` 与 `float_arithmetic` 的循环体本身就是被测的混合算术负载；"
                                "`function_call_baseline` 与 `function_calls` 共享同一循环体，两者相减可估算直接调用净成本，"
                                "`function_callbacks` 在其上增加一层高阶回调。")
        group_mean = "本组几何平均数"
        usage_title = "## 用法"
        usage = (
            "测试应使用 Release 构建，并需要 `PATH` 中的 `lua` 可执行文件（可用 `--lua` 指定）。",
            "",
            "```sh",
            "python3 test/benchmarks/compare_script.py build-release/bin/tapas",
            "```",
            "",
            ("脚本先预热一次，再比较每项 3 次有效运行的中位数（可用 `--runs` "
             "调整），打印摘要并更新本目录的两份 README 文档。"),
            ("只查看控制台输出、不更新文档时，加 `--no-markdown`；"
             "用 `--markdown 路径` 可指定其他报告位置。"),
            ("重复 `--markdown` 会用同一组测量数据生成多份报告，"
             "避免数字因重复运行而不同。"),
            "要求每一项都快于 Python 时，使用 `--require-faster`。",
            ("性能门槛不属于常规测试，因为 CPU 负载、电源状态、编译器和 "
             "Python 版本都会影响结果；发布比较结果时应同时记录运行环境。"),
        )
        notes_title = "## 说明"
        notes = (
            "这些程序用于比较三种实现执行相同算法时的解释器开销，不代表大型应用的完整性能。",
            "各语言使用语义等价的惯用实现；内建批量操作和数据结构带来的优势属于比较结果的一部分。",
            "计时区域都位于函数内部，三种语言保持一致；比较脚本会校验三种实现的输出完全相同。",
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
        lua_label = "Lua interpreter"
        runs_text = f"Measured runs: {runs} per benchmark, after 1 warm-up run"
        results_title = "## Results"
        timing_text = "Times are median process CPU times in microseconds and exclude process startup, source loading, and compilation."
        ratio_text = ("A ratio below 1 means Tapas is faster (against Python and "
                      "Lua respectively).")
        columns = ("| Benchmark | Source | Tapas | Python | Lua | "
                   "Tapas/Python | Tapas/Lua |")
        algorithm_title = "Complete Algorithms"
        algorithm_description = "These programs combine recursion, branching, containers, indexing, and allocation to approximate complete algorithm workloads."
        hot_path_title = "VM Hot Paths"
        hot_path_description = ("Each program times a loop inside a function and keeps only the measured VM operation or container access pattern in the loop body, "
                                "to help isolate interpreter overhead. Modular index arithmetic, growing-integer accumulation, and module-scope variable access are kept out, "
                                "so one language's unrelated strengths or weaknesses cannot leak into a single-item result. "
                                "The loop bodies of `vm_hot_paths` and `float_arithmetic` are themselves the measured arithmetic workload; "
                                "`function_call_baseline` shares the loop body of `function_calls`, so subtracting the two estimates direct-call overhead, and `function_callbacks` adds a higher-order callback layer.")
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
            ("The script performs one warm-up and compares the median of "
             "three measured runs per benchmark (tune with `--runs`), prints "
             "a summary, and updates both README documents in this directory."),
            ("Add `--no-markdown` for console output only, or pass "
             "`--markdown PATH` to write a report elsewhere."),
            ("Repeating `--markdown` writes multiple reports from the same "
             "measurements, so their numbers cannot diverge because of a "
             "second run."),
            ("To require Tapas to be faster than Python in every benchmark, "
             "use `--require-faster`."),
            ("Performance thresholds are not part of the regular test suite "
             "because CPU load, power settings, compiler version, and Python "
             "version affect the results; published comparisons should "
             "always include the runtime environment."),
        )
        notes_title = "## Notes"
        notes = (
            "These programs compare interpreter overhead while all implementations execute the same algorithms; they do not represent complete application performance.",
            "Each language uses an idiomatic implementation with equivalent semantics; advantages from built-in bulk operations and data structures are part of the comparison.",
            "Timed regions live inside functions in all three languages, and the comparison script verifies that all implementations print identical results.",
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

    def append_group(title: str, names: tuple[str, ...], description: str) -> None:
        lines.extend(
            [
                f"### {title}",
                "",
                description,
                "",
                columns,
                "| --- | --- | ---: | ---: | ---: | ---: | ---: |",
            ]
        )
        python_ratios = []
        lua_ratios = []
        for name in names:
            _, tapas_ns, python_ns, lua_ns, ratio_py, ratio_lua = rows_by_name[
                name
            ]
            python_ratios.append(ratio_py)
            lua_ratios.append(ratio_lua)
            lines.append(
                f"| `{name}` | [Tapas]({name}.tap) · [Python]({name}.py) · "
                f"[Lua]({name}.lua) | {format_micros(tapas_ns)} | "
                f"{format_micros(python_ns)} | {format_micros(lua_ns)} | "
                f"{ratio_py:.3f}× | {ratio_lua:.3f}× |"
            )
        lines.extend(
            [
                "",
                (f"{group_mean}{separator}Tapas/Python "
                 f"**{statistics.geometric_mean(python_ratios):.3f}×**，"
                 f"Tapas/Lua **{statistics.geometric_mean(lua_ratios):.3f}×**"
                 f"{period}"),
                "",
            ]
        )

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
            command, check=True, capture_output=True, text=True
        )
        lines = result.stdout.splitlines()
        if len(lines) != 2:
            raise RuntimeError(
                f"unexpected output from {' '.join(command)}: {result.stdout!r}"
            )
        totals.append(int(lines[0]))
        elapsed.append(int(lines[1]))
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
        "--markdown",
        type=Path,
        action="append",
        help=("write the comparison and environment to a Markdown file; "
              "repeat for additional reports; by default both "
              "README.md and README_en.md next to this script are "
              "updated from one run"),
    )
    parser.add_argument(
        "--no-markdown",
        action="store_true",
        help="skip writing Markdown reports (console output only)",
    )
    args = parser.parse_args()
    if args.runs < 3:
        parser.error("--runs must be at least 3")

    lua_version = lua_version_text(args.lua)
    print(f"Lua interpreter: {lua_version}")

    failed = False
    python_ratios: list[float] = []
    lua_ratios: list[float] = []
    rows: list[tuple[str, int, int, int, float, float]] = []
    for name in BENCHMARKS:
        tapas_total, tapas_times = measure(
            [str(args.tapas), str(HERE / f"{name}.tap")], args.runs
        )
        python_total, python_times = measure(
            [sys.executable, str(HERE / f"{name}.py")], args.runs
        )
        lua_total, lua_times = measure(
            [args.lua, str(HERE / f"{name}.lua")], args.runs
        )
        if not (tapas_total == python_total == lua_total):
            raise RuntimeError(
                f"{name}: different results: "
                f"Tapas={tapas_total}, Python={python_total}, "
                f"Lua={lua_total}"
            )

        tapas_median = int(statistics.median(tapas_times))
        python_median = int(statistics.median(python_times))
        lua_median = int(statistics.median(lua_times))
        ratio_python = tapas_median / python_median
        ratio_lua = tapas_median / lua_median
        python_ratios.append(ratio_python)
        lua_ratios.append(ratio_lua)
        rows.append(
            (name, tapas_median, python_median, lua_median,
             ratio_python, ratio_lua)
        )
        print(name)
        print(f"  result:        {tapas_total}")
        print(f"  Tapas median:  {tapas_median:,} ns")
        print(f"  Python median: {python_median:,} ns")
        print(f"  Lua median:    {lua_median:,} ns")
        print(f"  Tapas/Python:  {ratio_python:.3f}x")
        print(f"  Tapas/Lua:     {ratio_lua:.3f}x")
        if args.require_faster and ratio_python >= 1.0:
            failed = True

    ratios_by_name = {row[0]: (row[4], row[5]) for row in rows}

    def family_mean(runtimes: "list[tuple[str, ...]]", index: int) -> float:
        return statistics.geometric_mean(
            ratios_by_name[name][index] for name in runtimes
        )

    print("algorithm geometric mean Tapas/Python: "
          f"{family_mean(list(ALGORITHM_BENCHMARKS), 0):.3f}x")
    print("hot-path geometric mean Tapas/Python: "
          f"{family_mean(list(HOT_PATH_BENCHMARKS), 0):.3f}x")
    print("algorithm geometric mean Tapas/Lua: "
          f"{family_mean(list(ALGORITHM_BENCHMARKS), 1):.3f}x")
    print("hot-path geometric mean Tapas/Lua: "
          f"{family_mean(list(HOT_PATH_BENCHMARKS), 1):.3f}x")

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
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
