import time


source = list(range(1024))


def bench(rounds, source):
    started = time.process_time_ns()
    result = 0

    for repetition in range(rounds):
        copied = source.copy()
        result = copied[1023]

    elapsed = time.process_time_ns() - started
    print(result)
    print(elapsed)


bench(20_000, source)
