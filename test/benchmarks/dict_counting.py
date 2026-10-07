import time


keys = [index % 4096 for index in range(500_000)]


def bench(n, keys):
    started = time.process_time_ns()
    counts = {}

    for index in range(n):
        key = keys[index]
        if key in counts:
            counts[key] = counts[key]
        else:
            counts[key] = key

    elapsed = time.process_time_ns() - started
    print(len(counts))
    print(elapsed)


bench(500_000, keys)
