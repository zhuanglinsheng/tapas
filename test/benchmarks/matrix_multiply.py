import time
from operator import mul


def multiply_checksum(left, right, size):
    result = [[0] * size for _ in range(size)]
    columns = list(zip(*right))
    for row, left_row in enumerate(left):
        for column, right_column in enumerate(columns):
            result[row][column] = sum(map(mul, left_row, right_column))

    return sum(value * (row + column + 1)
               for row, result_row in enumerate(result)
               for column, value in enumerate(result_row))


size = 48
left = [
    [(row * 17 + column * 11 + 3) % 23 for column in range(size)]
    for row in range(size)
]
right = [
    [(row * 7 + column * 19 + 5) % 29 for column in range(size)]
    for row in range(size)
]

def bench():
    started = time.process_time_ns()
    result = multiply_checksum(left, right, size)
    elapsed = time.process_time_ns() - started

    print(result)
    print(elapsed)


if __name__ == "__main__":
    bench()
