import time


started = time.process_time_ns()
values = []

for index in range(500_000):
    values.append(index % 97)

elapsed = time.process_time_ns() - started
result = len(values) + values[123] + values[499_999]
print(result)
print(elapsed)
