import time


def solve_board(grid):
    index = 0
    while index < 81:
        if grid[index] == 0:
            row = (index // 9) * 9
            column = index % 9
            box = (index // 27) * 27 + (column // 3) * 3
            digit = 1
            while digit < 10:
                valid = True
                k = 0
                while k < 9:
                    if grid[row + k] == digit or grid[column + k * 9] == digit:
                        valid = False
                        break
                    if grid[box + (k // 3) * 9 + k % 3] == digit:
                        valid = False
                        break
                    k += 1
                if valid:
                    grid[index] = digit
                    if solve_board(grid):
                        return True
                digit += 1
            grid[index] = 0
            return False
        index += 1
    return True


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

started = time.process_time_ns()
checksum = -1
for round in range(repetitions):
    board = puzzle.copy()
    solved = solve_board(board)
    if not solved:
        checksum = -1
        break
    checksum = 0
    for index in range(81):
        checksum += board[index] * (index + 1)
elapsed = time.process_time_ns() - started

print(checksum)
print(elapsed)
