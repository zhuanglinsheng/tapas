import time


def bench(n):
    started = time.process_time_ns()
    total = 0

    for index in range(n):
        if index % 2 == 0:
            continue
        total = total + index % 7
        total = total + index % 5
        total = total - index % 3
        total = total + index % 11

    elapsed = time.process_time_ns() - started
    print(total)
    print(elapsed)


bench(2_000_000)
