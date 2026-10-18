#!/usr/bin/env python3
"""Pair equivalent sieve loops without changing the storage implementation."""
import argparse
import datetime
import json
from pathlib import Path
import sys
import tempfile

from ab_script import binary_metadata, summarize
from sequence_ab_script import HERE, pair, sieve_sources


def loop_sources(kind, size, repetitions):
    original = sieve_sources(size)[kind].split('let started = clock_ns()')[0]
    ranged = original.replace('    let candidate = 2\n    while (candidate * candidate <= limit) {',
                              '    for (let candidate in 2 to (math::isqrt(limit) + 1)) {')
    ranged = ranged.replace('        candidate = candidate + 1\n', '')
    value = '0' if kind == 'buffer' else 'false'
    odd = ranged.replace('    for (let candidate in 2 to (math::isqrt(limit) + 1)) {',
                         f'    fill(is_prime, {value}, iter(4, 2, limit + 1))\n'
                         '    for (let candidate in iter(3, 2, math::isqrt(limit) + 1)) {')
    footer = f'''let started = clock_ns()
var result = 0
for (let repetition in 0 to {repetitions}) {{ result = result + primes_up_to({size}) }}
print(result)
print(clock_ns() - started)
'''
    return {name: source + footer for name, source in
            [('while', original), ('for', ranged), ('odd', odd)]}


def python_sources(size):
    original = (HERE / 'sieve.py').read_text().split('def bench():')[0]
    odd = original.replace('    for candidate in range(2, math.isqrt(limit) + 1):',
                           '    is_prime[4::2] = bytes(max(0, (limit - 4) // 2 + 1))\n'
                           '    for candidate in range(3, math.isqrt(limit) + 1, 2):')
    footer = f'''started = time.process_time_ns()
result = 0
for repetition in range(100):
    result += primes_up_to({size})
print(result)
print(time.process_time_ns() - started)
'''
    return original + footer, odd + footer


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('tapas', type=Path)
    parser.add_argument('--json', type=Path, required=True)
    args = parser.parse_args()
    binary = args.tapas.resolve()
    metadata = binary_metadata(binary)
    cases = []
    for size in [20000, 200000]:
        for kind in ['buffer', 'view']:
            for repetitions in [1, 64]:
                sources = loop_sources(kind, size, repetitions)
                for first, second in [('while', 'for'), ('for', 'odd')]:
                    cases.append((f'{kind}_{size}_{repetitions}_{first}_{second}',
                                  binary, sources[first], sources[second]))
        cases.append((f'python_odd_{size}', Path(sys.executable), *python_sources(size)))
    rows = []
    with tempfile.TemporaryDirectory(prefix='tapas-sieve-loop-') as directory:
        for name, runtime, source_a, source_b in cases:
            suffix = '.py' if name.startswith('python') else '.tap'
            paths = [Path(directory) / (name + letter + suffix) for letter in ['_a', '_b']]
            for path, source in zip(paths, [source_a, source_b]):
                path.write_text(source)
            rounds = [pair(runtime, runtime, *paths, 9, i) for i in range(3)]
            samples = {label: [v for r in rounds for v in r['samples_ns'][label]]
                       for label in ['baseline', 'candidate']}
            row = {'name': name, 'sources': {'baseline': source_a, 'candidate': source_b},
                   'rounds': rounds, 'checksum': rounds[0]['checksum'], **summarize(samples)}
            rows.append(row)
            print(name, f'B/A={row["paired_ratio_median"]:.3f}', flush=True)
    if binary_metadata(binary)['sha256'] != metadata['sha256']:
        raise RuntimeError('binary changed during measurement')
    args.json.write_text(json.dumps({
        'date': datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'runtime': metadata, 'python': sys.version, 'runs': 9, 'rounds': 3,
        'rows': rows,
    }, indent=2) + '\n')


if __name__ == '__main__':
    main()
