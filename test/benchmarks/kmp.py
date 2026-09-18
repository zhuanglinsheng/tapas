import time


def kmp_search(text, pattern):
    pi = [0] * len(pattern)
    j = 0
    for i in range(1, len(pattern)):
        while j > 0 and pattern[i] != pattern[j]:
            j = pi[j - 1]
        if pattern[i] == pattern[j]:
            j += 1
        pi[i] = j

    total = 0
    j = 0
    for i in range(len(text)):
        while j > 0 and text[i] != pattern[j]:
            j = pi[j - 1]
        if text[i] == pattern[j]:
            j += 1
        if j == len(pattern):
            total += i - len(pattern) + 1
            j = pi[j - 1]
    return total


text = "ABCABABCAB" * 500
pattern = "ABCAB"
rounds = 60

started = time.process_time_ns()
result = 0
for _ in range(rounds):
    result += kmp_search(text, pattern)
elapsed = time.process_time_ns() - started

print(result)
print(elapsed)
