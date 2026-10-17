import time


def add(left, right):
    return left + right


def apply(callback, left, right):
    return callback(left, right)


def bench(n, add, apply):
    started = time.process_time_ns()
    total = 0

    for index in range(n):
        total = apply(add, total, index % 7)

    elapsed = time.process_time_ns() - started
    print(total)
    print(elapsed)


bench(200_000, add, apply)
