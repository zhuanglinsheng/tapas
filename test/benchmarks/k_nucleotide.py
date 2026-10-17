import time
from collections import Counter

BASES = "ACGT"
DIMERS = [a + b for a in BASES for b in BASES]
PROBES = list(BASES) + DIMERS


def count_kmers(sequence, k):
    if k == 1:
        return Counter(sequence)
    return Counter(sequence[i:i + k] for i in range(len(sequence) - k + 1))


seed = 42
parts = []
for _ in range(120000):
    seed = (seed * 1103515245 + 12345) % 2147483648
    parts.append(BASES[seed % 4])
sequence = "".join(parts)

def bench():
    started = time.process_time_ns()
    result = 0
    for _ in range(6):
        counts = count_kmers(sequence, 1)
        for probe in PROBES[:4]:
            result += counts.get(probe, 0)
        counts = count_kmers(sequence, 2)
        for probe in PROBES[4:]:
            result += counts.get(probe, 0)
    elapsed = time.process_time_ns() - started

    print(result)
    print(elapsed)


if __name__ == "__main__":
    bench()
