import time


def kmp_search(text, pattern):
    length = len(pattern)
    pi = [0] * length
    j = 0
    for i in range(1, len(pattern)):
        while j > 0 and pattern[i] != pattern[j]:
            j = pi[j - 1]
        if pattern[i] == pattern[j]:
            j += 1
        pi[i] = j

    total = 0
    j = 0
    for i, ch in enumerate(text):
        while j > 0 and ch != pattern[j]:
            j = pi[j - 1]
        if ch == pattern[j]:
            j += 1
        if j == length:
            total += i - length + 1
            j = pi[j - 1]
    return total


text = "ABCABABCAB" * 500
pattern = "ABCAB"
rounds = 60

def bench():
    started = time.process_time_ns()
    result = 0
    for _ in range(rounds):
        result += kmp_search(text, pattern)
    elapsed = time.process_time_ns() - started

    print(result)
    print(elapsed)


if __name__ == "__main__":
    bench()
