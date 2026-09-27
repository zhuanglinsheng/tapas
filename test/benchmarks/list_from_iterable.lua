local function list_from_range(first, last)
    local result = {}
    for value = first, last - 1 do
        result[#result + 1] = value
    end
    return result
end

local started = os.clock()
local result = 0
for repetition = 0, 19999 do
    local values = list_from_range(0, 1024)
    result = result + values[repetition % 1024 + 1]
end
local elapsed = os.clock() - started

print(result)
print(math.floor(elapsed * 1e9 + 0.5))
