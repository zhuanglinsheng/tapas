import time


def bench(n):
    started = time.process_time_ns()
    value = 0.5

    for index in range(n):
        value = value * 1.00000001 + 0.25
        value = value - 0.125

    elapsed = time.process_time_ns() - started
    print(int(value))
    print(elapsed)


bench(1_000_000)
