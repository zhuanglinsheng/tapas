import time


started = time.process_time_ns()
values = {}
for index in range(200_000):
    values[index] = index % 97
elapsed = time.process_time_ns() - started

result = len(values) + values[123] + values[199_999]
print(result)
print(elapsed)
