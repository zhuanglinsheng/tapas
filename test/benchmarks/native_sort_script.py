#!/usr/bin/env python3
"""Measure the builtin against libc qsort within one native C process."""
import argparse
import datetime
import hashlib
import json
import platform
import statistics
import subprocess
from pathlib import Path

from ab_script import binary_metadata, summarize
from sort_ab_script import PATTERNS


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('driver', type=Path)
    parser.add_argument('tapas', type=Path)
    parser.add_argument('--size', type=int, action='append')
    parser.add_argument('--runs', type=int, default=11)
    parser.add_argument('--rounds', type=int, default=3)
    parser.add_argument('--json', type=Path, required=True)
    args = parser.parse_args()
    if args.runs < 3 or args.rounds < 1:
        parser.error('at least three runs and one round required')
    metadata = binary_metadata(args.tapas.resolve())
    driver_hash = hashlib.sha256(args.driver.read_bytes()).hexdigest()
    rows = []
    for kind in ('int', 'float'):
        for pattern, name in enumerate(PATTERNS):
            # The native generator shares the distribution order of PATTERNS.
            for size in args.size or [512, 8192, 65536]:
                if not 8 <= size <= 1000000:
                    parser.error('native sizes must be between 8 and 1000000')
                rounds = []
                combined = {'baseline': [], 'candidate': []}
                for _ in range(args.rounds):
                    output = subprocess.check_output([
                        str(args.driver.resolve()), str(int(kind == 'float')),
                        str(pattern), str(size), str(args.runs)], text=True, timeout=120)
                    result = json.loads(output)
                    for key, samples in [('baseline', result['qsort_ns']),
                                         ('candidate', result['tapas_ns'])]:
                        if len(samples) != args.runs or min(samples) <= 0:
                            raise RuntimeError('invalid native CPU samples')
                        combined[key].extend(samples)
                    rounds.append(result)
                row = {'type': kind, 'pattern': name, 'size': size,
                       'rounds': rounds, **summarize(combined)}
                rows.append(row)
                print(f'{kind}/{name}/{size}: B/A={row["paired_ratio_median"]:.3f}', flush=True)
    if hashlib.sha256(args.driver.read_bytes()).hexdigest() != driver_hash:
        raise RuntimeError('native benchmark binary changed during measurement')
    if hashlib.sha256(args.tapas.read_bytes()).hexdigest() != metadata['sha256']:
        raise RuntimeError('Tapas binary changed during measurement')
    args.json.parent.mkdir(parents=True, exist_ok=True)
    args.json.write_text(json.dumps({
        'date': datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'platform': platform.platform(), 'runtime': metadata,
        'driver_sha256': driver_hash, 'runs': args.runs, 'rounds': args.rounds,
        'note': 'Same tobj arrays; both include memcpy; builtin includes validation, libc qsort does not. One warm-up per round. Additional scalar validation and validated-qsort controls are diagnostic; counters are separate untimed template instantiations.',
        'sources_sha256': {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in
                          [Path(__file__), Path('test/benchmarks/native_sort.c'),
                           Path('src/stdlib/builtins/sort.c'), Path('src/stdlib/builtins/sort_impl.h')]},
        'rows': rows,
    }, indent=2) + '\n')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
