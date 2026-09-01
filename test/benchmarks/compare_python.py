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
) -> None:
    tapas_version = subprocess.run(
        [str(tapas), "-v"], check=True, capture_output=True, text=True
    ).stdout.splitlines()[0]
    lines = [
        "# Tapas 与 Python 性能比较",
        "",
        f"测试日期：{datetime.date.today().isoformat()}",
        "",
        "## 测试环境",
        "",
        f"- 系统：`{platform.platform()}`",
        f"- 处理器架构：`{platform.machine()}`",
        f"- Python：`{platform.python_version()}`",
        f"- Tapas：`{tapas_version}`",
        f"- Tapas 可执行文件：`{tapas}`",
        f"- 有效运行次数：每项 {runs} 次，另预热 1 次",
        "",
        "## 结果",
        "",
        "时间为进程 CPU 时间的中位数，不包含进程启动、源码加载和编译。"
        "`Tapas/Python` 小于 1 表示 Tapas 更快。",
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
                "| 项目 | 源码 | 计算结果 | Tapas | Python | Tapas/Python |",
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
                f"本组比值的几何平均数为 **{statistics.geometric_mean(ratios):.3f}×**。",
                "",
            ]
        )

    append_group(
        "综合算法",
        ALGORITHM_BENCHMARKS,
        "这些程序组合使用递归、分支、容器、索引和内存分配，更接近完整算法负载。",
    )
    append_group(
        "基础热路径",
        HOT_PATH_BENCHMARKS,
        "这些程序分别放大某一种常见 VM 操作，用来定位解释器的基础开销。",
    )
    lines.extend(
        [
            f"全部项目的几何平均数为 **{geometric_mean:.3f}×**。",
            "",
            "## 说明",
            "",
            "这些程序用于比较两种实现执行相同算法时的解释器开销，不代表大型应用的"
            "完整性能。结果会受系统负载、电源状态、编译器版本和 Python 版本"
            "影响。更新 VM 或运行环境后，应使用本目录 README 中的命令重新生成。",
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
        write_markdown(
            args.markdown,
            args.tapas,
            args.runs,
            rows,
            geometric_mean,
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
