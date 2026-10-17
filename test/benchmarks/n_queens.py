import time


def count_queens(positions, row, size):
    if row == size:
        return 1

    count = 0
    for column in range(size):
        valid = True
        for previous in range(row):
            placed = positions[previous]
            distance = row - previous
            if placed == column or abs(placed - column) == distance:
                valid = False
                break
        if valid:
            positions[row] = column
            count += count_queens(positions, row + 1, size)
    return count


size = 10
positions = [-1] * size
def bench():
    started = time.process_time_ns()
    result = count_queens(positions, 0, size)
    elapsed = time.process_time_ns() - started

    print(result)
    print(elapsed)


if __name__ == "__main__":
    bench()
