import time


def add(left, right):
    return left + right


def bench(n, add):
    started = time.process_time_ns()
    total = 0

    for index in range(n):
        total = add(total, index % 7)

    elapsed = time.process_time_ns() - started
    print(total)
    print(elapsed)


bench(200_000, add)
