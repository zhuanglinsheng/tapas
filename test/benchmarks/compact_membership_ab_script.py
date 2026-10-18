#!/usr/bin/env python3
"""Compare compact membership paths with identical sources and checked results."""
import argparse
import datetime
import json
import platform
import tempfile
from pathlib import Path

from ab_script import binary_metadata, combine, measure


def source(kind, size, position):
    constructors = {
        'byte': f'bytes::buffer({size}, 0)',
        'bool': f'dense::flat(array(1, {size}, false))',
        'real': f'dense::flat(array(1, {size}, 0.0))',
    }
    keys = {'byte': '1', 'bool': 'true', 'real': '1.0'}
    key = keys[kind]
    assignment = ''
    if position in ('first', 'last'):
        index = 0 if position == 'first' else size - 1
        assignment = f'values[{index}] = {key}'
    if position == 'wrong_type':
        key = 'true' if kind == 'byte' else '1'
    repetitions = 8192 if size <= 64 or position == 'wrong_type' else 512
    expected = repetitions if position in ('first', 'last') else 0
    return f'''let values = {constructors[kind]}
{assignment}
let found = 0
let started = clock_ns()
for (let repetition in 0 to {repetitions}) {{
    if ({key} in values) {{ found = found + 1 }}
}}
let elapsed = clock_ns() - started
print(found)
print(elapsed)
''', expected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('baseline', type=Path)
    parser.add_argument('candidate', type=Path)
    parser.add_argument('--runs', type=int, default=7)
    parser.add_argument('--rounds', type=int, default=3)
    parser.add_argument('--json', type=Path, required=True)
    parser.add_argument('--markdown', type=Path, required=True)
    args = parser.parse_args()
    if args.runs < 3 or args.rounds < 1:
        parser.error('at least three runs and one round required')
    binaries = [args.baseline.resolve(), args.candidate.resolve()]
    metadata = [binary_metadata(binary) for binary in binaries]
    rows = []
    with tempfile.TemporaryDirectory(prefix='tapas-membership-ab-') as directory:
        for kind in ('byte', 'bool', 'real'):
            for size in (0, 64, 65536):
                positions = ('absent',) if size == 0 else (
                    'first', 'last', 'absent', 'wrong_type')
                for position in positions:
                    name = f'{kind}_{size}_{position}'
                    text, expected = source(kind, size, position)
                    path = Path(directory) / f'{name}.tap'
                    path.write_text(text)
                    rounds = [measure(*binaries, path, args.runs,
                                      'candidate' if r % 2 else 'baseline')
                              for r in range(args.rounds)]
                    row = {'name': name, 'kind': 'hotspot', 'source': text,
                           **combine(rounds)}
                    if row['checksum'] != expected:
                        raise RuntimeError(f'{name}: incorrect membership result')
                    rows.append(row)
                    print(name, f'B/A={row["paired_ratio_median"]:.3f}', flush=True)
    if metadata != [binary_metadata(binary) for binary in binaries]:
        raise RuntimeError('binary changed during measurement')
    report = {
        'date': datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'system': platform.platform(), 'baseline': metadata[0],
        'candidate': metadata[1], 'runs': args.runs, 'rounds': args.rounds,
        'rows': rows,
    }
    args.json.parent.mkdir(parents=True, exist_ok=True)
    args.json.write_text(json.dumps(report, indent=2) + '\n')
    lines = [
        '# Compact membership A/B', '',
        f"A SHA256: `{metadata[0]['sha256']}`", '',
        f"B SHA256: `{metadata[1]['sha256']}`", '',
        f'{args.rounds} rounds of {args.runs} samples per runtime; '
        'one discarded warm-up per round. Identical sources, alternating A/B order, '
        'reversed starting runtime each round. Each case completes all rounds before '
        'the next case. All results checked against expected checksums.', '',
        'B/A below 1 means faster; IQR describes paired variation, not confidence.', '',
        '| Case | A ns | B ns | B/A | IQR | Round medians |',
        '| --- | ---: | ---: | ---: | --- | --- |',
    ]
    for row in rows:
        low, high = row['paired_ratio_iqr']
        medians = ', '.join(f'{value:.3f}' for value in row['round_ratio_medians'])
        lines.append(f"| {row['name']} | {row['baseline_median_ns']} | "
                     f"{row['candidate_median_ns']} | {row['paired_ratio_median']:.3f} | "
                     f"{low:.3f}–{high:.3f} | {medians} |")
    args.markdown.parent.mkdir(parents=True, exist_ok=True)
    args.markdown.write_text('\n'.join(lines) + '\n')


if __name__ == '__main__':
    main()
