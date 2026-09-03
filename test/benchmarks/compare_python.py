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
)

HOT_PATH_BENCHMARKS = (
    "vm_hot_paths",
    "float_arithmetic",
    "function_calls",
    "list_access",
    "tail_recursion",
    "branch_logic",
)

BENCHMARKS = ALGORITHM_BENCHMARKS + HOT_PATH_BENCHMARKS


def write_markdown(
    path: Path,
    tapas: Path,
    runs: int,
    rows: list[tuple[str, int, int, int, float]],
    geometric_mean: float,
    language: str,
) -> None:
    tapas_version = subprocess.run(
        [str(tapas), "-v"], check=True, capture_output=True, text=True
    ).stdout.splitlines()[0]
    if language == "zh":
        separator = "："
        period = "。"
        title = "# Tapas 与 Python 性能比较"
        navigation = "简体中文 | [English](Results_en.md) | [项目主页](../../README.md)"
        date_label = "测试日期"
        environment_title = "## 测试环境"
        system_label = "系统"
        architecture_label = "处理器架构"
        executable_label = "Tapas 可执行文件"
        runs_text = f"有效运行次数：每项 {runs} 次，另预热 1 次"
        results_title = "## 结果"
        timing_text = "时间为进程 CPU 时间的中位数，不包含进程启动、源码加载和编译。"
        ratio_text = "`Tapas/Python` 小于 1 表示 Tapas 更快。"
        columns = "| 项目 | 源码 | 计算结果 | Tapas | Python | Tapas/Python |"
        algorithm_title = "综合算法"
        algorithm_description = "这些程序组合使用递归、分支、容器、索引和内存分配，更接近完整算法负载。"
        hot_path_title = "基础热路径"
        hot_path_description = "这些程序分别放大某一种常见 VM 操作，用来定位解释器的基础开销。"
        group_mean = "本组比值的几何平均数为"
        total_mean = "全部项目的几何平均数为"
        notes_title = "## 说明"
        notes = (
            "这些程序用于比较两种实现执行相同算法时的解释器开销，不代表大型应用的完整性能。",
            "结果会受系统负载、电源状态、编译器版本和 Python 版本影响。",
            "更新 VM 或运行环境后，应使用本目录 README 中的命令重新生成。",
        )
    else:
        separator = ": "
        period = "."
        title = "# Tapas and Python Performance Comparison"
        navigation = "[简体中文](Results_zh.md) | English | [Project Home](../../README_en.md)"
        date_label = "Test date"
        environment_title = "## Test Environment"
        system_label = "System"
        architecture_label = "Architecture"
        executable_label = "Tapas executable"
        runs_text = f"Measured runs: {runs} per benchmark, after 1 warm-up run"
        results_title = "## Results"
        timing_text = "Times are median process CPU times and exclude process startup, source loading, and compilation."
        ratio_text = "A `Tapas/Python` ratio below 1 means Tapas is faster."
        columns = "| Benchmark | Source | Result | Tapas | Python | Tapas/Python |"
        algorithm_title = "Complete Algorithms"
        algorithm_description = "These programs combine recursion, branching, containers, indexing, and allocation to approximate complete algorithm workloads."
        hot_path_title = "VM Hot Paths"
        hot_path_description = "Each program amplifies one common VM operation to help isolate interpreter overhead."
        group_mean = "The geometric mean ratio for this group is"
        total_mean = "The geometric mean ratio across all benchmarks is"
        notes_title = "## Notes"
        notes = (
            "These programs compare interpreter overhead while both implementations execute the same algorithms; they do not represent complete application performance.",
            "Results depend on system load, power settings, compiler version, and Python version.",
            "Regenerate the reports with the commands in this directory's README after changing the VM or runtime environment.",
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
                "| --- | --- | ---: | ---: | ---: | ---: |",
            ]
        )
        ratios = []
        for name in names:
            _, result, tapas_ns, python_ns, ratio = rows_by_name[name]
            ratios.append(ratio)
            lines.append(
                f"| `{name}` | [Tapas]({name}.tap) · [Python]({name}.py) | "
                f"{result} | {tapas_ns:,} ns | "
                f"{python_ns:,} ns | {ratio:.3f}× |"
            )
        lines.extend(
            [
                "",
                f"{group_mean} **{statistics.geometric_mean(ratios):.3f}×**{period}",
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
            f"{total_mean} **{geometric_mean:.3f}×**{period}",
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
        description="Compare matching Tapas and Python benchmark programs."
    )
    parser.add_argument("tapas", type=Path, help="Release Tapas executable")
    parser.add_argument("--runs", type=int, default=7)
    parser.add_argument(
        "--require-faster",
        action="store_true",
        help="fail when the Tapas median is not lower than Python's",
    )
    parser.add_argument(
        "--markdown",
        type=Path,
        help="write the comparison and environment to a Markdown file",
    )
    args = parser.parse_args()
    if args.runs < 3:
        parser.error("--runs must be at least 3")

    failed = False
    ratios: list[float] = []
    rows: list[tuple[str, int, int, int, float]] = []
    for name in BENCHMARKS:
        tapas_total, tapas_times = measure(
            [str(args.tapas), str(HERE / f"{name}.tap")], args.runs
        )
        python_total, python_times = measure(
            [sys.executable, str(HERE / f"{name}.py")], args.runs
        )
        if tapas_total != python_total:
            raise RuntimeError(
                f"{name}: different results: "
                f"Tapas={tapas_total}, Python={python_total}"
            )

        tapas_median = int(statistics.median(tapas_times))
        python_median = int(statistics.median(python_times))
        ratio = tapas_median / python_median
        ratios.append(ratio)
        rows.append(
            (name, tapas_total, tapas_median, python_median, ratio)
        )
        print(name)
        print(f"  result:        {tapas_total}")
        print(f"  Tapas median:  {tapas_median:,} ns")
        print(f"  Python median: {python_median:,} ns")
        print(f"  Tapas/Python:  {ratio:.3f}x")
        if args.require_faster and ratio >= 1.0:
            failed = True

    geometric_mean = statistics.geometric_mean(ratios)
    ratios_by_name = {
        name: ratio for name, _, _, _, ratio in rows
    }
    algorithm_mean = statistics.geometric_mean(
        ratios_by_name[name] for name in ALGORITHM_BENCHMARKS
    )
    hot_path_mean = statistics.geometric_mean(
        ratios_by_name[name] for name in HOT_PATH_BENCHMARKS
    )
    print(f"algorithm geometric mean Tapas/Python: {algorithm_mean:.3f}x")
    print(f"hot-path geometric mean Tapas/Python: {hot_path_mean:.3f}x")
    print(f"geometric mean Tapas/Python: {geometric_mean:.3f}x")
    if args.markdown:
        markdown_language = (
            "en" if args.markdown.name.endswith("_en.md") else "zh"
        )
        write_markdown(
            args.markdown,
            args.tapas,
            args.runs,
            rows,
            geometric_mean,
            markdown_language,
        )
        print(f"wrote Markdown report: {args.markdown}")
    if failed:
        print(
            "Tapas is not faster than Python in every benchmark.",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
