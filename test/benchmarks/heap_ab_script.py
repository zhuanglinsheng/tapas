#!/usr/bin/env python3
"""Compare heap implementations across orderings, payloads and callback kinds."""
import argparse
import datetime
import json
import tempfile
from pathlib import Path

from ab_script import binary_metadata, combine, digest, measure


def cases():
    for name, value, comparator, mode in (
        ('numeric_random', '(i * 73 + 19) % 1009 - 500', None, 'drain'),
        ('numeric_ascending', 'i - 256', None, 'drain'),
        ('numeric_descending', '256 - i', None, 'drain'),
        ('numeric_equal', '7', None, 'drain'),
        ('callback_random', '(i * 73 + 19) % 1009 - 500', 'return a < b', 'drain'),
        ('callback_equal', '7', 'return a < b', 'drain'),
        ('callback_duplicates', 'i % 8', 'return a < b', 'drain'),
        ('callback_maximum', '(i * 73 + 19) % 1009 - 500', 'return a > b', 'drain'),
        ('callback_records', '[(i * 73 + 19) % 1009 - 500, i]', 'return a[0] < b[0]', 'drain'),
        ('callback_captured', '(i * 73 + 19) % 1009 - 500', 'return a + offset < b + offset', 'drain'),
        ('callback_expensive', '(i * 73 + 19) % 1009 - 500',
         'var total = 0\n for (let k in 0 to 8) { total = total + a + k }\n return a + total < b + total', 'drain'),
        ('native_comparator', '(i * 73 + 19) % 1009 - 500', 'native', 'drain'),
        ('incremental_build', '(i * 73 + 19) % 1009 - 500', 'return a < b', 'push'),
        ('interleaved', '(i * 73 + 19) % 1009 - 500', 'return a < b', 'interleaved'),
        ('heapify_ordered', 'i - 256', 'return a < b', 'heapify'),
    ):
        declaration = 'var offset = 7\n' if name == 'callback_captured' else ''
        if comparator == 'native':
            declaration += 'let less = math::isless\n'
        elif comparator:
            declaration += f'let less = (a, b) {{ {comparator} }}\n'
        suffix = ', less' if comparator else ''
        build = f'let queue = copy(values)\n heap::heapify(queue{suffix})'
        if mode == 'push':
            build = f'let queue = []\n for (let value in values) {{ heap::push(queue, value{suffix}) }}'
        element = 'value[0]' if name == 'callback_records' else 'value'
        consume = f'''for (let position in 0 to len(values)) {{
            let value = heap::pop(queue{suffix})
            checksum = checksum + {element} * (position + 1)
        }}'''
        if mode == 'interleaved':
            consume = f'''for (let position in 0 to (len(values) * 2)) {{
                checksum = checksum + heap::pop(queue{suffix}) * (position + 1)
                heap::push(queue, (position * 91 + 17) % 1009 - 500{suffix})
            }}'''
        elif mode == 'heapify':
            consume = 'checksum = checksum + heap::peek(queue) + queue[len(queue) - 1]'
        source = f'''function run_heap(values)
{{
    {declaration}
    var checksum = 0
    for (let repetition in 0 to 16) {{
        {build}
        {consume}
    }}
    return checksum + 0
}}
let values = []
for (let i in 0 to 512) {{ values.append({value}) }}
let started = clock_ns()
let result = run_heap(values)
let elapsed = clock_ns() - started
print(result)
print(elapsed)
'''
        yield name, source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('baseline', type=Path)
    parser.add_argument('candidate', type=Path)
    parser.add_argument('--runs', type=int, default=7)
    parser.add_argument('--rounds', type=int, default=3)
    parser.add_argument('--case', action='append', choices=[name for name, _ in cases()])
    parser.add_argument('--json', type=Path, required=True)
    args = parser.parse_args()
    if args.runs < 3 or args.rounds < 1:
        parser.error('at least three runs and one round required')
    binaries = {key: binary_metadata(path.resolve()) for key, path in
                (('baseline', args.baseline), ('candidate', args.candidate))}
    rows = []
    with tempfile.TemporaryDirectory(prefix='tapas-heap-matrix-') as directory:
        for name, source in cases():
            if args.case and name not in args.case:
                continue
            path = Path(directory) / (name + '.tap')
            path.write_text(source)
            rounds = [measure(args.baseline.resolve(), args.candidate.resolve(), path,
                              args.runs, 'candidate' if i % 2 else 'baseline')
                      for i in range(args.rounds)]
            row = {'name': name, 'source': source, **combine(rounds)}
            rows.append(row)
            print(f'{name}: B/A={row["paired_ratio_median"]:.3f}; '
                  f'rounds={[round(v, 3) for v in row["round_ratio_medians"]]}', flush=True)
    if not rows:
        parser.error('no matching cases')
    for key, path in (('baseline', args.baseline), ('candidate', args.candidate)):
        if digest(path) != binaries[key]['sha256']:
            raise RuntimeError(f'{key} changed during measurement')
    report = {'date': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              **binaries, 'runs': args.runs, 'rounds': args.rounds,
              'warmups_per_round': 1, 'rows': rows}
    args.json.parent.mkdir(parents=True, exist_ok=True)
    args.json.write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
