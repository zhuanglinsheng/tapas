local move = table.move

local function merge_range(values, scratch, first, last)
    if first >= last then return end
    local middle = (first + last) // 2
    merge_range(values, scratch, first, middle)
    merge_range(values, scratch, middle + 1, last)
    move(values, first, last, first, scratch)
    local i, j, out = first, middle + 1, first
    while i <= middle and j <= last do
        if scratch[i] <= scratch[j] then
            values[out] = scratch[i]
            i = i + 1
        else
            values[out] = scratch[j]
            j = j + 1
        end
        out = out + 1
    end
    if i <= middle then move(scratch, i, middle, out, values) end
    if j <= last then move(scratch, j, last, out, values) end
end

local function merge_sort(values)
    local n = #values
    if n <= 1 then return values end
    local result = move(values, 1, n, 1, {})
    merge_range(result, {}, 1, n)
    return result
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
