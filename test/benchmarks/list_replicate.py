import time


pattern = [0, 1, 2, 3]


def bench(rounds, pattern):
    started = time.process_time_ns()
    result = 0

    for repetition in range(rounds):
        values = pattern * 256
        result = result + values[1023]

    elapsed = time.process_time_ns() - started
    print(result)
    print(elapsed)


bench(20_000, pattern)
