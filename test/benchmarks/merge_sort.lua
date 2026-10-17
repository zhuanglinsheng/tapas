local function merge_sort(values)
    local n = #values
    if n <= 1 then
        return table.move(values, 1, n, 1, {})
    end

    local middle = n // 2
    local left_slice = table.move(values, 1, middle, 1, {})
    local right_slice = table.move(values, middle + 1, n, 1, {})
    local left = merge_sort(left_slice)
    local right = merge_sort(right_slice)
    local merged = {}
    local i = 1
    local j = 1
    local k = 0

    while i <= #left and j <= #right do
        k = k + 1
        if left[i] <= right[j] then
            merged[k] = left[i]
            i = i + 1
        else
            merged[k] = right[j]
            j = j + 1
        end
    end
    table.move(left, i, #left, k + 1, merged)
    k = k + #left - i + 1
    table.move(right, j, #right, k + 1, merged)
    return merged
end

local values = {}
for index = 0, 4095 do
    values[index + 1] = (index * 73 + 19) % 1009
end

local started = os.clock()
local sorted_values = merge_sort(values)
local checksum = 0
for index = 1, #sorted_values do
    checksum = checksum + sorted_values[index] * index
end
local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)

print(checksum)
print(elapsed)
