import time


values = [0] * 1024


def bench(rounds, values):
    started = time.process_time_ns()

    for round in range(rounds):
        for slot in range(1024):
            values[slot] = slot

    elapsed = time.process_time_ns() - started
    print(values[123] + values[1023] + len(values))
    print(elapsed)


bench(488, values)
