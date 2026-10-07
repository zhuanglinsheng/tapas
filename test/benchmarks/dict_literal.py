import time


def bench(n):
    started = time.process_time_ns()
    result = 0

    for index in range(n):
        values = {0: index, 1: index}
        result = values[0]

    elapsed = time.process_time_ns() - started
    print(result)
    print(elapsed)


bench(300_000)
