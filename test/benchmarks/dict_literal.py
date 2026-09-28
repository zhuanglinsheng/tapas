import time


def bench(n):
    started = time.process_time_ns()
    result = 0

    for index in range(n):
        values = {0: index % 97, 1: index % 89}
        result = result + values[0] - values[1]

    elapsed = time.process_time_ns() - started
    print(result)
    print(elapsed)


bench(300_000)
