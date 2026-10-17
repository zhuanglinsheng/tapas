import sys
import time

class Body:
    def __init__(self):
        self.x = 0.0
        self.vx = 0.25


class SlottedBody:
    __slots__ = ('x', 'vx')

    def __init__(self):
        self.x = 0.0
        self.vx = 0.25


def matched(n, body):
    for i in range(n):
        body.x += body.vx
    return int(body.x)


def optimized(n, body):
    for i in range(n):
        body.x += body.vx
    return int(body.x)


def bench(n, body):
    kernel = matched if "--matched" in sys.argv else optimized
    started = time.process_time_ns()
    result = kernel(n, body)
    elapsed = time.process_time_ns() - started
    print(result)
    print(elapsed)


if __name__ == "__main__":
    bench(500_000, Body() if "--matched" in sys.argv else SlottedBody())
