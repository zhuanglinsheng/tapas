#!/usr/bin/env python3
"""A/B test two equivalent Tapas sources using the same runtime."""
import argparse
import datetime
import json
from pathlib import Path

from ab_script import binary_metadata, digest, execute, summarize


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('tapas', type=Path)
    parser.add_argument('baseline', type=Path)
    parser.add_argument('candidate', type=Path)
    parser.add_argument('--runs', type=int, default=9)
    parser.add_argument('--rounds', type=int, default=3)
    parser.add_argument('--json', type=Path, required=True)
    args = parser.parse_args()
    if args.runs < 3 or args.rounds < 1:
        parser.error('at least 3 runs and 1 round required')
    binary = args.tapas.resolve()
    metadata = binary_metadata(binary)
    sources = {'baseline': args.baseline.resolve(),
               'candidate': args.candidate.resolve()}
    snapshots = {label: {'path': str(path), 'sha256': digest(path),
                         'source': path.read_text()}
                 for label, path in sources.items()}
    checksum = None
    rounds = []
    for round_index in range(args.rounds):
        samples = {'baseline': [], 'candidate': []}
        for repetition in range(args.runs + 1):
            order = ('baseline', 'candidate')
            if (round_index + repetition) % 2:
                order = order[::-1]
            for label in order:
                result, elapsed = execute(binary, sources[label])
                if checksum is None:
                    checksum = result
                if result != checksum:
                    raise RuntimeError(f'{label}: expected {checksum}, got {result}')
                if repetition:
                    samples[label].append(elapsed)
        row = summarize(samples)
        rounds.append(row)
        print(f'round {round_index + 1}: '
              f'B/A={row["paired_ratio_median"]:.3f}', flush=True)
    for label, source in sources.items():
        if digest(source) != snapshots[label]['sha256']:
            raise RuntimeError(f'{label} changed during measurement')
    if digest(binary) != metadata['sha256']:
        raise RuntimeError('runtime changed during measurement')
    combined = {label: [sample for row in rounds
                        for sample in row['samples_ns'][label]]
                for label in sources}
    report = {
        'date': datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'runtime': metadata, 'sources': snapshots, 'checksum': checksum,
        'runs': args.runs, 'rounds': rounds, 'warmups_per_round': 1,
        'round_ratio_medians': [row['paired_ratio_median'] for row in rounds],
        **summarize(combined),
    }
    args.json.parent.mkdir(parents=True, exist_ok=True)
    args.json.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    print(f'median B/A={report["paired_ratio_median"]:.3f}; '
          f'IQR={report["paired_ratio_iqr"]}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
