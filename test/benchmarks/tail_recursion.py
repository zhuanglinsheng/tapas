import sys
import time


sys.setrecursionlimit(100_000)


def count_down(remaining, count):
    if remaining == 0:
        return count
    return count_down(remaining - 1, count + 1)


started = time.process_time_ns()
result = count_down(50_000, 0)

print(result)
print(time.process_time_ns() - started)
