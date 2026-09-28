import time


values = {index: index for index in range(500_000)}


def bench(n, values):
    started = time.process_time_ns()
    sink = 0

    for index in range(n):
        sink = values[index]

    elapsed = time.process_time_ns() - started
    print(sink + len(values))
    print(elapsed)


bench(500_000, values)
