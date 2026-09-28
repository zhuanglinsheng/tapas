import time


def bench(n):
    started = time.process_time_ns()
    values = {}

    for index in range(n):
        values[index] = index

    elapsed = time.process_time_ns() - started
    print(len(values) + values[123] + values[199_999])
    print(elapsed)


bench(200_000)
