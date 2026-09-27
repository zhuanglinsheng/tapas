import time


def add(left, right):
    return left + right


def apply(callback, left, right):
    return callback(left, right)


started = time.process_time_ns()
total = 0

for index in range(200_000):
    total = apply(add, total, index % 7)

print(total)
print(time.process_time_ns() - started)
