local pattern = {0, 1, 2, 3}

local function replicate(values, count)
    local result = {}
    for _ = 1, count do
        for index = 1, #values do
            result[#result + 1] = values[index]
        end
    end
    return result
end

local started = os.clock()
local result = 0
for repetition = 0, 19999 do
    local values = replicate(pattern, 256)
    result = result + values[repetition % 1024 + 1]
end
local elapsed = os.clock() - started

print(result)
print(math.floor(elapsed * 1e9 + 0.5))
