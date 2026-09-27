import time


started = time.process_time_ns()
result = 0
for index in range(300_000):
    values = {0: index, 1: index + 1}
    result += values[0] - values[1]
elapsed = time.process_time_ns() - started

print(result)
print(elapsed)
