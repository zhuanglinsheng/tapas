import time


def multiply_checksum(left, right, size):
    result = [[0] * size for _ in range(size)]
    for row in range(size):
        for column in range(size):
            total = 0
            for inner in range(size):
                total += left[row][inner] * right[inner][column]
            result[row][column] = total

    checksum = 0
    for row in range(size):
        for column in range(size):
            checksum += result[row][column] * (row + column + 1)
    return checksum


size = 48
left = [
    [(row * 17 + column * 11 + 3) % 23 for column in range(size)]
    for row in range(size)
]
right = [
    [(row * 7 + column * 19 + 5) % 29 for column in range(size)]
    for row in range(size)
]

started = time.process_time_ns()
result = multiply_checksum(left, right, size)
elapsed = time.process_time_ns() - started

print(result)
print(elapsed)
