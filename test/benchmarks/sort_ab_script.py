#!/usr/bin/env python3
"""Compare sort implementations on identical lists across types and distributions."""
import argparse
import datetime
import json
import tempfile
from pathlib import Path

from ab_script import binary_metadata, combine, measure

TYPES = ('int', 'float', 'mixed', 'string', 'record', 'nested')
PATTERNS = ('random', 'sorted', 'reverse', 'equal', 'few_unique', 'nearly_sorted', 'organ_pipe', 'shuffled_few', 'skewed', 'period_7', 'period_31')


def source(kind, pattern, size):
    expression = {'random': 'seed % 2000001 - 1000000', 'sorted': 'i',
                  'reverse': 'i', 'equal': '7', 'few_unique': 'i % 8',
                  'nearly_sorted': 'i', 'organ_pipe': 'i',
                  'shuffled_few': '(seed / 65536) % 8',
                  'skewed': '0', 'period_7': 'i % 7', 'period_31': 'i % 31'}[pattern]
    value = {'int': 'value', 'float': 'value * 0.5', 'mixed': 'value',
             'string': "join(['key_', str(value + 1000000)], '')",
             'record': '[value, i]', 'nested': '[[value % 17], value, i]'}[kind]
    if pattern in ('equal', 'few_unique', 'shuffled_few', 'skewed', 'period_7', 'period_31') and kind in ('record', 'nested'):
        value = value.replace(', i]', ', 0]' if pattern == 'equal' else ', i % 8]')
    append = f'values.append({value})'
    if kind == 'mixed':
        append = 'if (i % 2 == 0) { values.append(value) } else { values.append(value * 0.5) }'
    modify = 'if (i >= size / 2) { value = size - i }' if pattern == 'organ_pipe' else ''
    if pattern == 'skewed':
        modify = 'if ((seed / 65536) % 100 >= 95) { value = (seed / 65536) % 8 + 1 }'
    prepare = ''
    if pattern in ('sorted', 'reverse', 'nearly_sorted'):
        prepare = 'sort(values)\n'
    if pattern == 'reverse':
        prepare += 'for (let i in 0 to (size / 2)) { let value = values[i]\n values[i] = values[size - i - 1]\n values[size - i - 1] = value }'
    elif pattern == 'nearly_sorted':
        prepare += 'if (size > 1) { let value = values[size / 3]\n values[size / 3] = values[size * 2 / 3]\n values[size * 2 / 3] = value }'
    score = {'int': 'item', 'float': 'int(item * 2)', 'mixed': 'int(item * 2)',
             'string': 'len(item)', 'record': 'item[0] * 17 + item[1]',
             'nested': 'item[1] * 17 + item[2]'}[kind]
    repetitions = max(4, 16384 // max(size, 1))
    return f'''let size = {size}
let values = []
let seed = 42
for (let i in 0 to size) {{
 seed = (seed * 1103515245 + 12345) % 2147483648
 let value = {expression}
 {modify}
 {append}
}}
{prepare}
let started = clock_ns()
var checksum = 0
for (let repetition in 0 to {repetitions}) {{
 let queue = copy(values)
 sort(queue)
 for (let i in 0 to len(queue)) {{
  let item = queue[i]
  checksum = checksum + ({score}) * (i % 7 + 1)
 }}
}}
print(checksum)
print(clock_ns() - started)
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('baseline', type=Path)
    parser.add_argument('candidate', type=Path)
    parser.add_argument('--type', action='append', choices=TYPES)
    parser.add_argument('--pattern', action='append', choices=PATTERNS)
    parser.add_argument('--size', type=int, action='append')
    parser.add_argument('--runs', type=int, default=7)
    parser.add_argument('--rounds', type=int, default=3)
    parser.add_argument('--json', type=Path, required=True)
    args = parser.parse_args()
    sizes = args.size or [0, 1, 8, 16, 32, 512, 8192]
    if args.runs < 3 or args.rounds < 1 or any(n < 0 or n > 1000000 for n in sizes):
        parser.error('at least three runs, one round and sizes between 0 and 1000000 required')
    binaries = {name: binary_metadata(path.resolve()) for name, path in
                [('baseline', args.baseline), ('candidate', args.candidate)]}
    rows = []
    with tempfile.TemporaryDirectory(prefix='tapas-sort-matrix-') as directory:
        for kind in args.type or TYPES:
            for pattern in args.pattern or PATTERNS:
                for size in sizes:
                    text = source(kind, pattern, size)
                    name = f'{kind}_{pattern}_{size}'
                    path = Path(directory) / (name + '.tap')
                    path.write_text(text)
                    row = {'name': name, 'type': kind, 'pattern': pattern, 'size': size,
                           'source': text, **combine([
                               measure(args.baseline.resolve(), args.candidate.resolve(), path,
                                       args.runs, 'candidate' if i % 2 else 'baseline')
                               for i in range(args.rounds)])}
                    rows.append(row)
                    print(f'{name}: B/A={row["paired_ratio_median"]:.3f}', flush=True)
    for label, path in [('baseline', args.baseline), ('candidate', args.candidate)]:
        if binary_metadata(path.resolve())['sha256'] != binaries[label]['sha256']:
            raise RuntimeError(f'{label} runtime changed during measurement')
    args.json.parent.mkdir(parents=True, exist_ok=True)
    args.json.write_text(json.dumps({
        'date': datetime.datetime.now(datetime.timezone.utc).isoformat(),
        **binaries, 'runs': args.runs, 'rounds': args.rounds,
        'warmups_per_round': 1, 'rows': rows,
    }, ensure_ascii=False, indent=2) + '\n')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
