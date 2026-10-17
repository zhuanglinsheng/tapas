import time


def bench(n):
    started = time.process_time_ns()
    total = 0

    for index in range(n):
        total = total + index % 7

    elapsed = time.process_time_ns() - started
    print(total)
    print(elapsed)


bench(200_000)
