import time


values = {index: index for index in range(200_000)}
started = time.process_time_ns()
for index in range(200_000):
    del values[index]
elapsed = time.process_time_ns() - started

print(len(values))
print(elapsed)
