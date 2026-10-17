import time


def bench(n):
    values = {}
    for index in range(n):
        values[index] = index

    started = time.process_time_ns()
    for index in range(n):
        del values[index]
    elapsed = time.process_time_ns() - started

    print(len(values))
    print(elapsed)


bench(200_000)
