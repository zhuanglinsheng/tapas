local function merge_sort(values)
    local n = #values
    if n <= 1 then
        local copy = {}
        for index = 1, n do
            copy[index] = values[index]
        end
        return copy
    end

    local middle = n // 2
    local left_slice = {}
    for index = 1, middle do
        left_slice[index] = values[index]
    end
    local right_slice = {}
    for index = middle + 1, n do
        right_slice[index - middle] = values[index]
    end
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
    while i <= #left do
        k = k + 1
        merged[k] = left[i]
        i = i + 1
    end
    while j <= #right do
        k = k + 1
        merged[k] = right[j]
        j = j + 1
    end

    local copy = {}
    for index = 1, k do
        copy[index] = merged[index]
    end
    return copy
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
