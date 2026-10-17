#!/usr/bin/env python3
"""Algorithm-derived hotspots against optimized and matched CPython references.

Run sequentially, rotate language order, retain every measured sample, and check
both an expected checksum and agreement between all implementations. See
hotspots/ANALYSIS.md for the scope of each comparison and causal limitations.
"""
import argparse
import datetime
import hashlib
import json
import platform
import statistics
import subprocess
import sys
import tempfile
from pathlib import Path

from compare_script import ALGORITHM_BENCHMARKS, HOT_PATH_CATALOG

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
# Fixed oracles also catch a regression shared by all implementations.
HOTSPOTS = {
    "while_search": 2750000,
    "dp_row": 260659,
    "recursive_loops": 1310710,
    "tree_build": 2647,
    "record_update": 125000,
    "string_count": 1375000,
}
CONTROLS = {"substring_count": 441866, "constraint_probe": 390117}
ALGORITHM_RESULTS = dict(zip(ALGORITHM_BENCHMARKS, (
    46368, 2262, 5638402144, 724, 1480, 817318963, 16605, 40796,
    8058, 149850000, 1439994, 6782980, -11217876, 120000, 180282,
    199830979999,
)))


def execute(command):
    result = subprocess.run(command, check=True, capture_output=True,
                            text=True, timeout=120)
    lines = result.stdout.splitlines()
    if len(lines) != 2:
        raise RuntimeError(f"unexpected output from {command}: {result.stdout!r}")
    checksum, elapsed = map(int, lines)
    if elapsed <= 0:
        raise RuntimeError(f"nonpositive CPU time from {command}: {elapsed}")
    return checksum, elapsed


def measure(name, commands, expected, runs, kind="hotspot"):
    samples = {language: [] for language in commands}
    languages = list(commands)
    # Every implementation gets a discarded fresh-process warm-up, like the
    # existing suite. This does not warm CPython's separate measured processes.
    for repetition in range(runs + 1):
        offset = repetition % len(languages)
        order = languages[offset:] + languages[:offset]
        for language in order:
            checksum, elapsed = execute(commands[language])
            if checksum != expected:
                raise RuntimeError(
                    f"{name}/{language}: expected {expected}, got {checksum}")
            if repetition:
                samples[language].append(elapsed)
    medians = {key: int(statistics.median(values))
               for key, values in samples.items()}
    ratios = {key: medians['tapas'] / value
              for key, value in medians.items() if key != 'tapas'}
    row = dict(name=name, kind=kind, checksum=expected, samples_ns=samples,
               median_ns=medians, tapas_over=ratios)
    print(f"{name}: " + ', '.join(
        f"Tapas/{key}={ratio:.3f}x" for key, ratio in ratios.items()), flush=True)
    return row


def replace_once(source, old, new):
    if source.count(old) != 1:
        raise RuntimeError(f"ablation source drift: expected one {old!r}")
    return source.replace(old, new, 1)


def ablations(directory):
    """Only one factor changes per language; inputs and checksums stay fixed."""
    variants = []

    def add(label, name, transforms, base=HERE):
        for extension in ('tap', 'py'):
            source = (base / f'{name}.{extension}').read_text()
            for old, new in transforms.get(extension, ()):
                source = replace_once(source, old, new)
            (directory / f'{label}.{extension}').write_text(source)
        variants.append((label, name))

    add('lcs_character_lists', 'longest_common_subsequence', {
        'tap': [('let started = clock_ns()',
                 'left = list(left)\nright = list(right)\nlet started = clock_ns()')],
        'py': [('def bench():',
                'left = list(left)\nright = list(right)\n\ndef bench():')],
    })
    add('binary_fresh_lists', 'binary_trees', {'py': [
        ('return ()', 'return []'),
        ('return (make_tree(depth - 1), make_tree(depth - 1))',
         'return [make_tree(depth - 1), make_tree(depth - 1)]'),
    ]})
    add('queens_reuse_locals', 'n_queens', {'tap': [
        ('    let count = 0', '    let count = 0\n    let placed = 0\n    let distance = 0'),
        ('let placed = positions[previous]', 'placed = positions[previous]'),
        ('let distance = row - previous', 'distance = row - previous'),
    ]})
    add('sudoku_for', 'sudoku', {
        'tap': [('let k = 0\n                while (k < 9)', 'for (let k in 0 to 9)'),
                ('                    k = k + 1\n', '')],
        # Python already uses a for loop; its unchanged source is the control.
        'py': [],
    })
    add('merge_cached_lengths', 'merge_sort', {
        'tap': [('    while (i < left.len() and j < right.len()) {',
                 '    let nl = left.len()\n    let nr = right.len()\n'
                 '    while (i < nl and j < nr) {'),
                ('while (i < left.len())', 'while (i < nl)'),
                ('while (j < right.len())', 'while (j < nr)')],
        'py': [('    while i < len(left) and j < len(right):',
                '    nl = len(left)\n    nr = len(right)\n'
                '    while i < nl and j < nr:')],
    })
    add('record_list_storage', 'record_update', {'tap': [
        ("let Body = types::make_type('x': types::Float, 'vx': types::Float)\n"
         "let body: Body = {x = 0.0, vx = 0.25}", "let body = [0.0, 0.25]"),
        ("body['x'] = body::x + body::vx", "body[0] = body[0] + body[1]"),
        ("int(body::x)", "int(body[0])"),
    ]}, HERE / 'hotspots')
    return variants


def git_text(*args):
    return subprocess.run(['git', '-C', str(ROOT), *args], check=True,
                          capture_output=True, text=True).stdout.strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('tapas', type=Path)
    parser.add_argument('--runs', type=int, default=7)
    parser.add_argument('--case', action='append', choices=HOTSPOTS)
    parser.add_argument('--algorithms', action='store_true',
                        help='also measure the existing complete algorithms')
    parser.add_argument('--controls', action='store_true',
                        help='also measure sliced-key and Sudoku-predicate negative controls')
    parser.add_argument('--ablations', action='store_true',
                        help='also measure controlled algorithm and storage variants and their baselines')
    parser.add_argument('--json', type=Path, help='save all samples and environment')
    parser.add_argument('--require-slower', action='store_true',
                        help='fail if a selected hotspot does not lose to optimized Python')
    args = parser.parse_args()
    if args.runs < 3:
        parser.error('--runs must be at least 3')
    if sys.version_info[:2] != (3, 14):
        parser.error('run with Python 3.14; its executable is the Python under test')
    tapas = str(args.tapas.resolve())
    selected = list(dict.fromkeys(args.case or HOTSPOTS))
    rows = []
    for name in selected + (list(CONTROLS) if args.controls else []):
        source = HERE / 'hotspots' / name
        commands = {
            'tapas': [tapas, str(source.with_suffix('.tap'))],
            'python_optimized': [sys.executable, str(source.with_suffix('.py'))],
        }
        if HOT_PATH_CATALOG[name]['python_matched']:
            commands['python_matched'] = [sys.executable, str(source.with_suffix('.py')), '--matched']
        rows.append(measure(name, commands, (HOTSPOTS | CONTROLS)[name], args.runs,
                            "hotspot" if name in HOTSPOTS else "control"))
    algorithm_names = list(ALGORITHM_BENCHMARKS) if args.algorithms else []
    with tempfile.TemporaryDirectory(prefix='tapas-ablation-') as scratch:
        directory = Path(scratch)
        variants = ablations(directory) if args.ablations else []
        for _, name in variants:
            if name in ALGORITHM_RESULTS and name not in algorithm_names:
                algorithm_names.append(name)
        for name in algorithm_names:
            rows.append(measure(name, {
                'tapas': [tapas, str(HERE / f'{name}.tap')],
                'python_matched': [sys.executable, str(HERE / f'{name}.py')],
            }, ALGORITHM_RESULTS[name], args.runs, "algorithm"))
        for label, original in variants:
            rows.append(measure(label, {
                'tapas': [tapas, str(directory / f'{label}.tap')],
                ('python_optimized' if original in HOTSPOTS else 'python_matched'):
                    [sys.executable, str(directory / f'{label}.py')],
            }, (ALGORITHM_RESULTS | HOTSPOTS)[original], args.runs, "ablation"))
    # Record the binary itself: HEAD alone misses an uncommitted runtime build.
    report = {
        'date': datetime.datetime.now(datetime.timezone(datetime.timedelta(hours=8))).isoformat(),
        'system': platform.platform(), 'architecture': platform.machine(),
        'python': sys.version, 'python_executable': sys.executable,
        'tapas': tapas,
        'tapas_version': subprocess.run([tapas, '-v'], check=True, capture_output=True,
                                        text=True).stdout.splitlines()[0],
        'tapas_sha256': hashlib.sha256(Path(tapas).read_bytes()).hexdigest(),
        'git_head': git_text('rev-parse', 'HEAD'),
        'source_diff_sha256': hashlib.sha256(git_text('diff', 'HEAD', '--', 'src', 'include',
                                                     'CMakeLists.txt').encode()).hexdigest(),
        'git_status': git_text('status', '--short'),
        'build_config': [line for line in
                         (Path(tapas).parent.parent / 'CMakeCache.txt').read_text().splitlines()
                         if line.startswith(('CMAKE_BUILD_TYPE:', 'CMAKE_C_FLAGS_RELEASE:',
                                             'CMAKE_C_COMPILER:', 'TAPAS_ENABLE_IPO:'))]
                         if (Path(tapas).parent.parent / 'CMakeCache.txt').exists() else None,
        'runs': args.runs, 'warmups': 1, 'rows': rows,
    }
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    return int(args.require_slower and any(
        row['tapas_over']['python_optimized'] <= 1 for row in rows[:len(selected)]))


if __name__ == '__main__':
    raise SystemExit(main())
