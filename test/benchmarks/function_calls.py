import time


def add(left, right):
    return left + right


started = time.process_time_ns()
total = 0

for index in range(200_000):
    total = add(total, index % 7)

print(total)
print(time.process_time_ns() - started)
