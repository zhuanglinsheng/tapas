import time


def bench(repetitions, values):
    started = time.process_time_ns()
    checksum = 0
    for _ in range(repetitions):
        ordered = values.copy()
        ordered.sort()
        checksum += sum(value * position for position, value in enumerate(ordered, 1))
    print(checksum)
    print(time.process_time_ns() - started)


if __name__ == '__main__':
    values = []
    seed = 42
    for _ in range(4096):
        seed = (seed * 1103515245 + 12345) % 2147483648
        values.append(seed % 2000001 - 1000000)
    bench(16, values)
