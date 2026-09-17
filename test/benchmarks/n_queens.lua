local function count_queens(positions, row, size)
    if row == size then
        return 1
    end

    local count = 0
    for column = 0, size - 1 do
        local valid = true
        for previous = 0, row - 1 do
            local placed = positions[previous]
            local distance = row - previous
            if placed == column then
                valid = false
            end
            if placed == column - distance then
                valid = false
            end
            if placed == column + distance then
                valid = false
            end
        end
        if valid then
            positions[row] = column
            count = count + count_queens(positions, row + 1, size)
        end
    end
    return count
end

local size = 10
local positions = {}
for index = 0, size - 1 do
    positions[index] = -1
end

local started = os.clock()
local result = count_queens(positions, 0, size)
local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)

print(result)
print(elapsed)
