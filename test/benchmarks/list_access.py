import time


values = [0, 1, 2, 3, 4, 5, 6, 7] * 62_500


def bench(n, values):
    started = time.process_time_ns()
    sink = 0

    for index in range(n):
        sink = values[index]

    elapsed = time.process_time_ns() - started
    print(sink + len(values))
    print(elapsed)


bench(500_000, values)
