#!/usr/bin/env python3
"""Isolate List bulk operations and compact sieve representations."""
import argparse
import datetime
import json
from pathlib import Path
import tempfile

from ab_script import binary_metadata, execute, summarize

HERE = Path(__file__).resolve().parent


def pair(binary_a, binary_b, source_a, source_b, runs, first):
    samples = {'baseline': [], 'candidate': []}
    expected = None
    binaries = {'baseline': binary_a, 'candidate': binary_b}
    sources = {'baseline': source_a, 'candidate': source_b}
    for repetition in range(runs + 1):
        order = ['baseline', 'candidate']
        if (repetition + first) % 2:
            order.reverse()
        for label in order:
            checksum, elapsed = execute(binaries[label], sources[label])
            if expected is None:
                expected = checksum
            if checksum != expected:
                raise RuntimeError('source or runtime checksum mismatch')
            if repetition:
                samples[label].append(elapsed)
    return {'checksum': expected, **summarize(samples)}


def sieve_sources(size):
    # Keep the List reference explicit rather than depending on the timed case.
    source = '''function primes_up_to(limit)
{
    let is_prime = replicate([true], limit + 1)
    is_prime[0] = false
    is_prime[1] = false
    let candidate = 2
    while (candidate * candidate <= limit) {
        if (is_prime[candidate]) {
            fill(is_prime, false, iter(candidate * candidate, candidate, limit + 1))
        }
        candidate = candidate + 1
    }
    return count(is_prime, true)
}
let started = clock_ns()
let result = primes_up_to(SIZE)
print(result)
print(clock_ns() - started)
'''.replace('SIZE', str(size))
    buffer = source.replace('replicate([true], limit + 1)', 'bytes::buffer(limit + 1, 1)')
    buffer = buffer.replace(' = false', ' = 0').replace('if (is_prime[candidate])', 'if (is_prime[candidate] != 0)')
    buffer = buffer.replace('fill(is_prime, false,', 'fill(is_prime, 0,').replace('count(is_prime, true)', 'count(is_prime, 1)')
    view = source.replace('replicate([true], limit + 1)', 'dense::flat(array(1, limit + 1, true))')
    return {'list': source, 'buffer': buffer, 'view': view}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('baseline', type=Path)
    parser.add_argument('candidate', type=Path)
    parser.add_argument('--runs', type=int, default=9)
    parser.add_argument('--rounds', type=int, default=3)
    parser.add_argument('--json', type=Path, required=True)
    args = parser.parse_args()
    if args.runs < 3 or args.rounds < 1:
        parser.error('at least three runs and one round required')
    binaries = {name: path.resolve() for name, path in
                [('baseline', args.baseline), ('candidate', args.candidate)]}
    metadata = {name: binary_metadata(path) for name, path in binaries.items()}
    rows = []
    cases = []
    for size in [20000, 200000]:
        sources = sieve_sources(size)
        cases.extend([
            (f'sieve_list_runtime_{size}', 'baseline', sources['list'], sources['list']),
            (f'sieve_buffer_{size}', 'candidate', sources['list'], sources['buffer']),
            (f'sieve_view_{size}', 'candidate', sources['list'], sources['view']),
        ])
    for size in [512, 8192, 65536]:
        for step in [1, 3, -3]:
            first, end = (0, size) if step > 0 else (size - 1, -1)
            source = f'''let values = replicate([true], {size})
let started = clock_ns()
for (let repetition in 0 to 64) {{ fill(values, false, iter({first}, {step}, {end})) }}
let elapsed = clock_ns() - started
print(count(values, false))
print(elapsed)
'''
            cases.append((f'list_fill_{size}_{step}', 'baseline', source, source))
    with tempfile.TemporaryDirectory(prefix='tapas-sequence-ab-') as directory:
        for name, baseline_label, source_a, source_b in cases:
            paths = [Path(directory) / (name + suffix) for suffix in ['_a.tap', '_b.tap']]
            for path, source in zip(paths, [source_a, source_b]):
                path.write_text(source)
            rounds = [pair(binaries[baseline_label], binaries['candidate'], *paths,
                           args.runs, round_index) for round_index in range(args.rounds)]
            samples = {label: [v for r in rounds for v in r['samples_ns'][label]]
                       for label in ['baseline', 'candidate']}
            row = {'name': name, 'baseline_runtime': baseline_label,
                   'sources': {'baseline': source_a, 'candidate': source_b},
                   'checksum': rounds[0]['checksum'], 'rounds': rounds, **summarize(samples)}
            rows.append(row)
            print(name, f'B/A={row["paired_ratio_median"]:.3f}', flush=True)
    if any(binary_metadata(path)['sha256'] != metadata[name]['sha256']
           for name, path in binaries.items()):
        raise RuntimeError('binary changed during measurement')
    args.json.write_text(json.dumps({
        'date': datetime.datetime.now(datetime.timezone.utc).isoformat(),
        **metadata, 'runs': args.runs, 'rounds': args.rounds, 'rows': rows,
    }, indent=2) + '\n')


if __name__ == '__main__':
    main()
