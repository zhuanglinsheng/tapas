import sys
import time


def matched(n, values):
    result = 0
    for r in range(n):
        target = r % 8
        j = 0
        while j < 32:
            if values[j] == target:
                break
            j += 1
        result += j
    return result


def optimized(n, values):
    result = 0
    for r in range(n):
        try:
            j = values.index(r % 8)
        except ValueError:
            j = len(values)
        result += j
    return result


def bench(n, values):
    kernel = matched if "--matched" in sys.argv else optimized
    started = time.process_time_ns()
    result = kernel(n, values)
    elapsed = time.process_time_ns() - started
    print(result)
    print(elapsed)


if __name__ == "__main__":
    bench(100_000, list(range(8, 32)) + [3, 1, 4, 0, 5, 7, 2, 6])
