import time


def bench(n):
    started = time.process_time_ns()
    state = 17

    for index in range(n):
        if state > 55:
            state = state - 23
        elif state > 40 and state <= 55:
            state = state + 21
        elif state < 20 or state == 20:
            state = state + 31
        else:
            state = state - 19

    elapsed = time.process_time_ns() - started
    print(state)
    print(elapsed)


bench(1_000_000)
