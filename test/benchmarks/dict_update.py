import time


values = {index: 0 for index in range(500_000)}


def bench(n, values):
    started = time.process_time_ns()

    for index in range(n):
        values[index] = index

    elapsed = time.process_time_ns() - started
    print(values[123] + values[499_999] + len(values))
    print(elapsed)


bench(500_000, values)
