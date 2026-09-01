import time


started = time.process_time_ns()
value = 0.5

for index in range(1_000_000):
    value = value * 1.00000001 + 0.25
    value = value - 0.125

print(int(value))
print(time.process_time_ns() - started)
