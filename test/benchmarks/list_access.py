import time


values = [3, 1, 4, 1, 5, 9, 2, 6]
started = time.process_time_ns()
total = 0

for index in range(500_000):
    total = total + values[index % 8]

print(total)
print(time.process_time_ns() - started)
