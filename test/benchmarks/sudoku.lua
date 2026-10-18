local function solve_from(grid, start)
    for slot = start, 81 do
        if grid[slot] == 0 then
            local index = slot - 1
            local row = (index // 9) * 9 + 1
            local column = index % 9 + 1
            local box = (index // 27) * 27 + ((column - 1) // 3) * 3 + 1
            for digit = 1, 9 do
                local valid = true
                for k = 0, 8 do
                    if grid[row + k] == digit or grid[column + k * 9] == digit then
                        valid = false
                        break
                    end
                end
                if valid then
                    for box_row = box, box + 18, 9 do
                        if grid[box_row] == digit or grid[box_row + 1] == digit or
                                grid[box_row + 2] == digit then
                            valid = false
                            break
                        end
                    end
                end
                if valid then
                    grid[slot] = digit
                    if solve_from(grid, slot + 1) then
                        return true
                    end
                end
            end
            grid[slot] = 0
            return false
        end
    end
    return true
end


local function solve_board(grid)
    return solve_from(grid, 1)
end

local puzzle = {
    1, 0, 0, 0, 0, 7, 0, 9, 0,
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
        board[index + 1] = puzzle[index + 1]
    end
    local solved = solve_board(board)
    if not solved then
        checksum = -1
        break
    end
    checksum = 0
    for index = 0, 80 do
        checksum = checksum + board[index + 1] * (index + 1)
    end
end
local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)

print(checksum)
print(elapsed)
