import sys
import time
from collections import Counter


def matched(sequence, k):
    counts = {}
    for i in range(len(sequence) - k + 1):
        key = sequence[i:i + k]
        counts[key] = counts.get(key, 0) + 1
    return len(counts) + sum(count * count for count in counts.values())


def optimized(sequence, k):
    counts = Counter(sequence[i:i + k] for i in range(len(sequence) - k + 1))
    return len(counts) + sum(count * count for count in counts.values())


def bench(sequence, k):
    kernel = matched if '--matched' in sys.argv else optimized
    started = time.process_time_ns()
    result = kernel(sequence, k)
    elapsed = time.process_time_ns() - started
    print(result)
    print(elapsed)


if __name__ == '__main__':
    parts = []
    seed = 42
    for i in range(120_000):
        seed = (seed * 1103515245 + 12345) % 2147483648
        parts.append('ACGT'[(seed // 65536) % 4])
    bench(''.join(parts), 8)
