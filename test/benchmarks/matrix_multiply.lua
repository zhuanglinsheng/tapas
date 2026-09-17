local function multiply_checksum(left, right, size)
    local result = {}
    for row = 1, size do
        local result_row = {}
        for column = 1, size do
            result_row[column] = 0
        end
        result[row] = result_row
    end

    for row = 1, size do
        for column = 1, size do
            local total = 0
            for inner = 1, size do
                total = total + left[row][inner] * right[inner][column]
            end
            result[row][column] = total
        end
    end

    local checksum = 0
    for row = 1, size do
        for column = 1, size do
            checksum = checksum + result[row][column] * (row + column - 1)
        end
    end
    return checksum
end

local size = 48
local left = {}
for row = 1, size do
    local left_row = {}
    for column = 1, size do
        left_row[column] = ((row - 1) * 17 + (column - 1) * 11 + 3) % 23
    end
    left[row] = left_row
end
local right = {}
for row = 1, size do
    local right_row = {}
    for column = 1, size do
        right_row[column] = ((row - 1) * 7 + (column - 1) * 19 + 5) % 29
    end
    right[row] = right_row
end

local started = os.clock()
local result = multiply_checksum(left, right, size)
local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)

print(result)
print(elapsed)
