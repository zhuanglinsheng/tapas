import time


keys = [index % 4096 for index in range(500_000)]


def bench(n, keys):
    started = time.process_time_ns()
    counts = {}

    for index in range(n):
        key = keys[index]
        counts[key] = counts.get(key, 0) + 1

    elapsed = time.process_time_ns() - started
    # The checksum must depend on the stored values: len(counts) alone cannot
    # detect a broken read-then-write-back. Summed after the timed region.
    print(sum(count * (key + 1) for key, count in counts.items()))
    print(elapsed)


bench(500_000, keys)
