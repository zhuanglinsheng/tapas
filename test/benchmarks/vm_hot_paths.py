import time


started = time.process_time_ns()
total = 0

for index in range(2_000_000):
    if index % 2 == 0:
        continue
    total = total + index % 7
    total = total + index % 5
    total = total - index % 3
    total = total + index % 11

print(total)
print(time.process_time_ns() - started)
