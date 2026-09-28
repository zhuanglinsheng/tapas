import time


keys = [index % 4096 for index in range(500_000)]


def bench(n, keys):
    started = time.process_time_ns()
    counts = {}

    for index in range(n):
        key = keys[index]
        if key in counts:
            counts[key] = counts[key] + 1
        else:
            counts[key] = 1

    elapsed = time.process_time_ns() - started
    print(sum(counts.values()))
    print(elapsed)


bench(500_000, keys)
