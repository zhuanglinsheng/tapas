#!/usr/bin/env python3
"""Pair two native sorting drivers on the same generated inputs."""
import argparse
import datetime
import hashlib
import json
import statistics
from pathlib import Path
import subprocess

from ab_script import summarize
from sort_ab_script import PATTERNS


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('baseline', type=Path)
    parser.add_argument('candidate', type=Path)
    parser.add_argument('--runs', type=int, default=9)
    parser.add_argument('--rounds', type=int, default=3)
    parser.add_argument('--size', type=int, action='append')
    parser.add_argument('--json', type=Path, required=True)
    args = parser.parse_args()
    if args.runs < 3 or args.rounds < 1:
        parser.error('at least three runs and one round required')
    drivers = {name: path.resolve() for name, path in
               [('baseline', args.baseline), ('candidate', args.candidate)]}
    hashes = {name: digest(path) for name, path in drivers.items()}
    rows = []
    for kind in ('int', 'float'):
        for pattern, name in enumerate(PATTERNS):
            for size in args.size or [512, 8192, 65521, 65536]:
                samples = {'baseline': [], 'candidate': []}
                rounds = []
                for round_index in range(args.rounds):
                    result = {}
                    order = ['baseline', 'candidate']
                    if round_index % 2:
                        order.reverse()
                    for label in order:
                        output = subprocess.check_output([
                            str(drivers[label]), str(int(kind == 'float')),
                            str(pattern), str(size), str(args.runs)], text=True, timeout=120)
                        result[label] = json.loads(output)
                        samples[label].extend(result[label]['tapas_ns'])
                    rounds.append(result)
                row = {'type': kind, 'pattern': name, 'size': size,
                       'rounds': rounds, **summarize(samples)}
                # Both drivers also time unchanged libc qsort on identical
                # values. Retain it as a common-mode timing control; do not
                # replace the actual builtin B/A result with a normalization.
                controls = {label: [v for result in rounds
                                   for v in result[label]['qsort_ns']]
                            for label in drivers}
                normalized = [b / qb / (a / qa)
                              for a, b, qa, qb in zip(
                                  samples['baseline'], samples['candidate'],
                                  controls['baseline'], controls['candidate'])]
                row['qsort_control'] = summarize(controls)
                row['normalized_ratio_median'] = statistics.median(normalized)
                row['normalized_ratio_iqr'] = statistics.quantiles(normalized, n=4)[::2]
                rows.append(row)
                print(f'{kind}/{name}/{size}: B/A={row["paired_ratio_median"]:.3f}', flush=True)
    for name, path in drivers.items():
        if digest(path) != hashes[name]:
            raise RuntimeError('driver changed during measurement')
    args.json.write_text(json.dumps({
        'date': datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'drivers': {name: {'path': str(path), 'sha256': hashes[name]}
                    for name, path in drivers.items()},
        'note': 'Compare builtin times reported by two native drivers. Each driver alternates builtin/qsort internally; outer driver order reverses per round. No VM. Input copies and validation included.',
        'runs': args.runs, 'rounds': args.rounds, 'rows': rows,
    }, indent=2) + '\n')


if __name__ == '__main__':
    main()
