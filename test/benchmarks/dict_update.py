import time


values = {index: 0 for index in range(4096)}
started = time.process_time_ns()
for index in range(500_000):
    key = index % 4096
    values[key] += 1
elapsed = time.process_time_ns() - started

print(sum(values.values()))
print(elapsed)
