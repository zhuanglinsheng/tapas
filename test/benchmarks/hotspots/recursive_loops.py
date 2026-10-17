import sys
import time


def visit(depth, values):
    total = 0
    for value in values:
        total += value
    if depth == 0:
        return total
    return total + visit(depth - 1, values) + visit(depth - 1, values)


def visit_sum(depth, values):
    total = sum(values)
    if depth == 0:
        return total
    return total + visit_sum(depth - 1, values) + visit_sum(depth - 1, values)


def bench():
    values = [1, 2, 3, 4]
    kernel = visit if '--matched' in sys.argv else visit_sum
    started = time.process_time_ns()
    result = kernel(16, values)
    elapsed = time.process_time_ns() - started
    print(result)
    print(elapsed)


if __name__ == '__main__':
    bench()
