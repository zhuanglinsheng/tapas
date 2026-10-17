import time


def probe(n, grid):
    result = 0
    started = time.process_time_ns()
    for r in range(n):
        index = r % 81
        row = (index // 9) * 9
        column = index % 9
        box = (index // 27) * 27 + (column // 3) * 3
        digit = (r // 81) % 9 + 1
        k = 0
        while k < 9:
            if grid[row + k] == digit or grid[column + k * 9] == digit:
                break
            if grid[box + (k // 3) * 9 + k % 3] == digit:
                break
            k += 1
        result += k
    elapsed = time.process_time_ns() - started
    print(result)
    print(elapsed)


if __name__ == '__main__':
    grid = [(row * 3 + row // 3 + column) % 9 + 1
            for row in range(9) for column in range(9)]
    probe(200_000, grid)
