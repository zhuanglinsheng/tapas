import time


def fibonacci(n):
    if n < 2:
        return n
    return fibonacci(n - 1) + fibonacci(n - 2)


started = time.process_time_ns()
result = fibonacci(24)

print(result)
print(time.process_time_ns() - started)
