import time


values = {index: index % 97 for index in range(4096)}
started = time.process_time_ns()
result = 0
for index in range(1_000_000):
    result += values[index % 4096]
elapsed = time.process_time_ns() - started

print(result)
print(elapsed)
