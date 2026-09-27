import time


source = list(range(1024))
started = time.process_time_ns()
result = 0
for repetition in range(20_000):
    copied = source.copy()
    result += copied[repetition % 1024]
elapsed = time.process_time_ns() - started

print(result)
print(elapsed)
