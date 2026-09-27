import time


started = time.process_time_ns()
counts = {}
for index in range(500_000):
    key = (index * 17 + 3) % 4096
    counts[key] = counts.get(key, 0) + 1
elapsed = time.process_time_ns() - started

print(sum(counts.values()))
print(elapsed)
