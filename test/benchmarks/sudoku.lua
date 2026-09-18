local function solve_board(grid)
    local index = 0
    while index < 81 do
        if grid[index] == 0 then
            local row = (index // 9) * 9
            local column = index % 9
            local box = (index // 27) * 27 + (column // 3) * 3
            local digit = 1
            while digit < 10 do
                local valid = true
                local k = 0
                while k < 9 do
                    if grid[row + k] == digit or grid[column + k * 9] == digit then
                        valid = false
                        break
                    end
                    if grid[box + (k // 3) * 9 + k % 3] == digit then
                        valid = false
                        break
                    end
                    k = k + 1
                end
                if valid then
                    grid[index] = digit
                    if solve_board(grid) then
                        return true
                    end
                end
                digit = digit + 1
            end
            grid[index] = 0
            return false
        end
        index = index + 1
    end
    return true
end

local puzzle = {
    [0] = 1, 0, 0, 0, 0, 7, 0, 9, 0,
    0, 3, 0, 0, 2, 0, 0, 0, 8,
    0, 0, 9, 6, 0, 0, 5, 0, 0,
    0, 0, 5, 3, 0, 0, 9, 0, 0,
    0, 1, 0, 0, 8, 0, 0, 0, 2,
    6, 0, 0, 0, 0, 4, 0, 0, 0,
    3, 0, 0, 0, 0, 0, 0, 1, 0,
    0, 4, 0, 0, 0, 0, 0, 0, 7,
    0, 0, 7, 0, 0, 0, 3, 0, 0,
}
local repetitions = 10

local started = os.clock()
local checksum = -1
for round = 0, repetitions - 1 do
    local board = {}
    for index = 0, 80 do
        board[index] = puzzle[index]
    end
    local solved = solve_board(board)
    if not solved then
        checksum = -1
        break
    end
    checksum = 0
    for index = 0, 80 do
        checksum = checksum + board[index] * (index + 1)
    end
end
local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)

print(checksum)
print(elapsed)
