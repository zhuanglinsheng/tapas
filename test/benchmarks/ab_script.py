#!/usr/bin/env python3
"""Compare two Release runtimes on identical, unchanged benchmark sources."""
import argparse
import datetime
import hashlib
import json
import os
import platform
import random
import statistics
import subprocess
import tempfile
from pathlib import Path

from compare_script import (ALGORITHM_BENCHMARKS, BENCHMARKS, HERE,
                            benchmark_source, build_configuration)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def execute(binary: Path, source: Path) -> tuple[int, int]:
    # Automatic bytecode caches are rooted in the working directory. A fresh
    # directory ensures each runtime compiles its own source, even when both
    # executables have the same public version and cache timestamps overlap.
    # The benchmark's internal timer excludes compilation and startup.
    with tempfile.TemporaryDirectory(prefix='tapas-ab-') as working_directory:
        result = subprocess.run([str(binary.resolve()), str(source.resolve())],
                                cwd=working_directory, check=True,
                                capture_output=True, text=True, timeout=120)
    lines = result.stdout.splitlines()
    if len(lines) != 2:
        raise RuntimeError(f"unexpected output from {binary} {source}: {result.stdout!r}")
    checksum, elapsed = map(int, lines)
    if elapsed <= 0:
        raise RuntimeError(f"nonpositive CPU time from {binary} {source}: {elapsed}")
    return checksum, elapsed


def summarize(samples: dict) -> dict:
    ratios = [b / a for a, b in zip(samples['baseline'], samples['candidate'])]
    quartiles = statistics.quantiles(ratios, n=4)
    return {
        'samples_ns': samples,
        'baseline_median_ns': int(statistics.median(samples['baseline'])),
        'candidate_median_ns': int(statistics.median(samples['candidate'])),
        'paired_ratio_median': statistics.median(ratios),
        'paired_ratio_iqr': [quartiles[0], quartiles[2]],
    }


def measure(baseline: Path, candidate: Path, source: Path, runs: int,
            first: str = 'baseline') -> dict:
    samples = {'baseline': [], 'candidate': []}
    binaries = {'baseline': baseline, 'candidate': candidate}
    source_hash = digest(source)
    expected = None
    initial_order = (first, 'candidate' if first == 'baseline' else 'baseline')
    for repetition in range(runs + 1):
        order = initial_order if repetition == 0 or repetition % 2 else initial_order[::-1]
        for label in order:
            checksum, elapsed = execute(binaries[label], source)
            if expected is None:
                expected = checksum
            if checksum != expected:
                raise RuntimeError(f"{source}/{label}: expected {expected}, got {checksum}")
            if repetition:
                samples[label].append(elapsed)
    if digest(source) != source_hash:
        raise RuntimeError(f'benchmark source changed during the comparison: {source}')
    return {
        'checksum': expected,
        'source_sha256': source_hash,
        'first': first,
        **summarize(samples),
    }


def combine(rounds: list[dict]) -> dict:
    for row in rounds[1:]:
        if (row['checksum'], row['source_sha256']) != (
                rounds[0]['checksum'], rounds[0]['source_sha256']):
            raise RuntimeError('checksum or source changed between rounds')
    samples = {label: [value for row in rounds for value in row['samples_ns'][label]]
               for label in ('baseline', 'candidate')}
    return {
        'checksum': rounds[0]['checksum'],
        'source_sha256': rounds[0]['source_sha256'],
        **summarize(samples),
        'round_ratio_medians': [row['paired_ratio_median'] for row in rounds],
        'rounds': rounds,
    }


def binary_metadata(path: Path) -> dict:
    return {
        'path': str(path),
        'sha256': digest(path),
        'version': subprocess.run([str(path), '-v'], check=True, capture_output=True,
                                  text=True, env=dict(os.environ,
                                  LLVM_PROFILE_FILE=os.devnull)).stdout.splitlines()[0],
        'build_config': build_configuration(path),
    }


def write_markdown(path: Path, report: dict) -> None:
    lines = [
        '# Runtime A/B results', '',
        f"Measured at: {report['date']}", '',
        f"System: `{report['system']}`", '',
        f"A (baseline): `{report['baseline']['sha256']}`", '',
        f"B (candidate): `{report['candidate']['sha256']}`", '',
        f"Each runtime gets one discarded warm-up and {report['runs']} measured runs per case "
        f"in each of {report.get('rounds', 1)} rounds. "
        'Fresh processes run sequentially in alternating A/B order. Times are internal process CPU times; '
        'startup and compilation are excluded. Each execution uses a fresh working directory to isolate automatic bytecode caches. Every checksum must agree across both runtimes and all runs.', '',
        'Successive rounds reverse the first runtime and shuffle case order with a recorded seed. '
        'Round medians expose between-round variation. For an A/A noise control, pass the same binary twice.', '',
        'B/A is the median of paired time ratios; below 1 means the candidate is faster. '
        'The IQR describes the middle half of paired ratios, not a confidence interval. '
        'A near-1 ratio should be treated as noise unless repeated measurements establish otherwise.', '',
        '| Case | A median ms | B median ms | Paired B/A | Paired IQR | Round medians |',
        '| --- | ---: | ---: | ---: | --- | --- |',
    ]
    for row in report['rows']:
        low, high = row['paired_ratio_iqr']
        lines.append(f"| `{row['name']}` | {row['baseline_median_ns'] / 1e6:.3f} | "
                     f"{row['candidate_median_ns'] / 1e6:.3f} | "
                     f"{row['paired_ratio_median']:.3f} | {low:.3f}–{high:.3f} | "
                     f"{', '.join(f'{value:.3f}' for value in row.get('round_ratio_medians', []))} |")
    lines.append('')
    for family in ('algorithm', 'hotspot'):
        ratios = [row['paired_ratio_median'] for row in report['rows'] if row['kind'] == family]
        if ratios:
            lines.append(f"{family}: geometric mean B/A **{statistics.geometric_mean(ratios):.3f}** "
                         f"({len(ratios)} cases).")
    lines.extend(['', 'Sources are identical between A and B; no benchmark rewrite is used to obtain a speedup.', ''])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text('\n'.join(lines))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('baseline', type=Path)
    parser.add_argument('candidate', type=Path)
    parser.add_argument('--runs', type=int, default=7)
    parser.add_argument('--rounds', type=int, default=1,
                        help='independent rounds, each with a fresh warm-up')
    parser.add_argument('--first', choices=('baseline', 'candidate'), default='baseline')
    parser.add_argument('--seed', type=int, default=0, help='seed for shuffling cases between rounds')
    parser.add_argument('--case', action='append', choices=BENCHMARKS)
    parser.add_argument('--json', type=Path, default=HERE / 'hotspots/ab_results.json')
    parser.add_argument('--markdown', type=Path, default=HERE / 'hotspots/AB_RESULTS.md')
    args = parser.parse_args()
    if args.runs < 3:
        parser.error('--runs must be at least 3')
    if args.rounds < 1:
        parser.error('--rounds must be positive')
    baseline = args.baseline.resolve()
    candidate = args.candidate.resolve()
    metadata = {label: binary_metadata(binary)
                for label, binary in [('baseline', baseline), ('candidate', candidate)]}
    names = list(dict.fromkeys(args.case or BENCHMARKS))
    measurements = {name: [] for name in names}
    round_orders = []
    rng = random.Random(args.seed)
    for repetition in range(args.rounds):
        order = names.copy()
        rng.shuffle(order)
        round_orders.append(order)
        first = args.first if repetition % 2 == 0 else (
            'candidate' if args.first == 'baseline' else 'baseline')
        for name in order:
            row = measure(baseline, candidate, (HERE / benchmark_source(name)).with_suffix('.tap'),
                          args.runs, first)
            measurements[name].append(row)
            print(f"round {repetition + 1}/{args.rounds} {name}: "
                  f"B/A={row['paired_ratio_median']:.3f}", flush=True)
    rows = []
    for name in names:
        row = combine(measurements[name])
        row['name'] = name
        row['kind'] = 'algorithm' if name in ALGORITHM_BENCHMARKS else 'hotspot'
        rows.append(row)
        source = (HERE / benchmark_source(name)).with_suffix('.tap')
        if digest(source) != row['source_sha256']:
            raise RuntimeError(f'benchmark source changed between rounds: {source}')
    for label, binary in [('baseline', baseline), ('candidate', candidate)]:
        if digest(binary) != metadata[label]['sha256']:
            raise RuntimeError(f'{label} binary changed during the comparison')
    report = {
        'date': datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'cache_policy': 'fresh working directory for each execution',
        'system': platform.platform(), 'architecture': platform.machine(),
        'runs': args.runs, 'warmups': 1, 'rounds': args.rounds,
        'first': args.first, 'seed': args.seed, 'round_orders': round_orders,
        **metadata, 'rows': rows,
        'git_head': subprocess.check_output(
            ['git', '-C', str(HERE.parent.parent), 'rev-parse', 'HEAD'], text=True).strip(),
        'runtime_diff_sha256': hashlib.sha256(subprocess.check_output(
            ['git', '-C', str(HERE.parent.parent), 'diff', 'HEAD', '--',
             'src', 'include', 'CMakeLists.txt'])).hexdigest(),
    }
    args.json.parent.mkdir(parents=True, exist_ok=True)
    args.json.write_text(json.dumps(report, indent=2) + '\n')
    write_markdown(args.markdown, report)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
