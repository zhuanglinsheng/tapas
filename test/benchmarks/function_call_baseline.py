import time


started = time.process_time_ns()
total = 0

for index in range(200_000):
    total = total + index % 7

print(total)
print(time.process_time_ns() - started)
