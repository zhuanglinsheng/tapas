import time


def bench(n):
    values = []
    started = time.process_time_ns()

    for index in range(n):
        values.append(index)

    elapsed = time.process_time_ns() - started
    print(len(values) + values[123] + values[499_999])
    print(elapsed)


bench(500_000)
