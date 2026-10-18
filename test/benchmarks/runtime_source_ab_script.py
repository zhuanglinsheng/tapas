#!/usr/bin/env python3
"""Compare two runtimes on explicit source paths, preserving source snapshots."""
import argparse
import datetime
import json
import random
from pathlib import Path

from ab_script import binary_metadata, combine, digest, measure


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('baseline', type=Path)
    parser.add_argument('candidate', type=Path)
    parser.add_argument('sources', nargs='+', type=Path)
    parser.add_argument('--runs', type=int, default=9)
    parser.add_argument('--rounds', type=int, default=3)
    parser.add_argument('--seed', type=int, default=83)
    parser.add_argument('--json', type=Path, required=True)
    args = parser.parse_args()
    if args.runs < 3 or args.rounds < 1:
        parser.error('at least 3 runs and 1 round required')
    binaries = {label: getattr(args, label).resolve()
                for label in ('baseline', 'candidate')}
    metadata = {label: binary_metadata(binary)
                for label, binary in binaries.items()}
    sources = list(dict.fromkeys(source.resolve() for source in args.sources))
    snapshots = {source: {'source': source.read_text(), 'sha256': digest(source)}
                 for source in sources}
    measurements = {source: [] for source in sources}
    rng = random.Random(args.seed)
    orders = []
    for round_index in range(args.rounds):
        order = sources.copy()
        rng.shuffle(order)
        orders.append([str(source) for source in order])
        for source in order:
            row = measure(binaries['baseline'], binaries['candidate'], source,
                          args.runs, 'candidate' if round_index % 2 else 'baseline')
            measurements[source].append(row)
            print(f'round {round_index + 1}: {source.name} '
                  f'B/A={row["paired_ratio_median"]:.3f}', flush=True)
    for source in sources:
        if digest(source) != snapshots[source]['sha256']:
            raise RuntimeError(f'source changed during measurement: {source}')
    for label, binary in binaries.items():
        if digest(binary) != metadata[label]['sha256']:
            raise RuntimeError(f'{label} runtime changed during measurement')
    report = {
        'date': datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'binaries': metadata, 'runs': args.runs, 'rounds': args.rounds,
        'seed': args.seed, 'round_orders': orders, 'warmups_per_round': 1,
        'cache_policy': 'fresh working directory for each execution',
        'rows': [{'name': source.stem, 'path': str(source),
                  **snapshots[source], **combine(measurements[source])}
                 for source in sources],
    }
    args.json.parent.mkdir(parents=True, exist_ok=True)
    args.json.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    for row in report['rows']:
        print(f'{row["name"]}: B/A={row["paired_ratio_median"]:.3f}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
