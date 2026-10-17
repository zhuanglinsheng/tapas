import sys
import time
from collections import Counter


def matched(text):
    counts = {}
    for ch in text:
        counts[ch] = counts.get(ch, 0) + 1
    return counts['a'] + counts['b'] * 3 + counts['c'] * 5 + counts['d'] * 7


def optimized(text):
    counts = Counter(text)
    return counts['a'] + counts['b'] * 3 + counts['c'] * 5 + counts['d'] * 7


def bench(text):
    kernel = matched if "--matched" in sys.argv else optimized
    started = time.process_time_ns()
    result = kernel(text)
    elapsed = time.process_time_ns() - started
    print(result)
    print(elapsed)


if __name__ == "__main__":
    bench('abacabad' * 62_500)
