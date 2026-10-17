# One-row LCS update from the valid row for prefix 'ABCDAB'.
import sys
import time


def matched(n, right):
    previous = [0, 1, 1, 2, 2, 2, 3, 3, 4, 4, 4, 5, 5, 5, 6, 6, 6]
    current = [0] * 17
    result = 0
    for r in range(n):
        ch = 'ABCD'[r % 4]
        for j in range(1, 17):
            if ch == right[j - 1]:
                current[j] = previous[j - 1] + 1
            elif previous[j] >= current[j - 1]:
                current[j] = previous[j]
            else:
                current[j] = current[j - 1]
        result += current[16]
    return result + sum(value * j for j, value in enumerate(current))


def optimized(n, right):
    previous = [0, 1, 1, 2, 2, 2, 3, 3, 4, 4, 4, 5, 5, 5, 6, 6, 6]
    current = [0] * 17
    result = 0
    for r in range(n):
        ch = 'ABCD'[r % 4]
        for j, other in enumerate(right, 1):
            if ch == other:
                current[j] = previous[j - 1] + 1
            elif previous[j] >= current[j - 1]:
                current[j] = previous[j]
            else:
                current[j] = current[j - 1]
        result += current[16]
    return result + sum(value * j for j, value in enumerate(current))


def bench(n, right):
    kernel = matched if "--matched" in sys.argv else optimized
    started = time.process_time_ns()
    result = kernel(n, right)
    elapsed = time.process_time_ns() - started
    print(result)
    print(elapsed)


if __name__ == "__main__":
    bench(40_000, 'DCBAABCDDCBAABCD')
