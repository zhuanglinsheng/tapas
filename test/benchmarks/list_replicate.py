import time


pattern = [0, 1, 2, 3]
started = time.process_time_ns()
result = 0
for repetition in range(20_000):
    values = pattern * 256
    result += values[repetition % 1024]
elapsed = time.process_time_ns() - started

print(result)
print(elapsed)
