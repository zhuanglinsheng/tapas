import time


values = [0] * 1024
started = time.process_time_ns()
for index in range(500_000):
    slot = index % 1024
    values[slot] = (values[slot] + index) % 1_000_003
elapsed = time.process_time_ns() - started

print(sum(values))
print(elapsed)
