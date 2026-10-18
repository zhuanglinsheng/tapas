import time


def solve_from(grid, start):
    try:
        index = grid.index(0, start)
    except ValueError:
        return True
    row = (index // 9) * 9
    column = index % 9
    box = (index // 27) * 27 + (column // 3) * 3
    for digit in range(1, 10):
        for k in range(9):
            if (grid[row + k] == digit or grid[column + k * 9] == digit or
                    grid[box + (k // 3) * 9 + k % 3] == digit):
                break
        else:
            grid[index] = digit
            if solve_from(grid, index + 1):
                return True
    grid[index] = 0
    return False


def solve_board(grid):
    return solve_from(grid, 0)

puzzle = [
    1, 0, 0, 0, 0, 7, 0, 9, 0,
    0, 3, 0, 0, 2, 0, 0, 0, 8,
    0, 0, 9, 6, 0, 0, 5, 0, 0,
    0, 0, 5, 3, 0, 0, 9, 0, 0,
    0, 1, 0, 0, 8, 0, 0, 0, 2,
    6, 0, 0, 0, 0, 4, 0, 0, 0,
    3, 0, 0, 0, 0, 0, 0, 1, 0,
    0, 4, 0, 0, 0, 0, 0, 0, 7,
    0, 0, 7, 0, 0, 0, 3, 0, 0,
]
repetitions = 10

def bench():
    started = time.process_time_ns()
    checksum = -1
    for round in range(repetitions):
        board = puzzle.copy()
        solved = solve_board(board)
        if not solved:
            checksum = -1
            break
        checksum = sum(value * index for index, value in enumerate(board, 1))
    elapsed = time.process_time_ns() - started

    print(checksum)
    print(elapsed)


if __name__ == "__main__":
    bench()
