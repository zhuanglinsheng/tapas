import time


started = time.process_time_ns()
state = 17
total = 0

for index in range(1_000_000):
    if index % 3 == 0 and state % 2 == 1:
        state = (state * 5 + index) % 97
        total = total + state
    elif index % 5 == 0 or state < 20:
        state = (state + index % 11 + 7) % 97
        total = total - state
    else:
        state = (state * 3 + 1) % 97
        if state > 60:
            total = total + 3
        else:
            total = total - 2

print(total + state)
print(time.process_time_ns() - started)
