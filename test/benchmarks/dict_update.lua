local values = {}
for index = 0, 4095 do
    values[index] = 0
end

local started = os.clock()
for index = 0, 499999 do
    local key = index % 4096
    values[key] = values[key] + 1
end
local elapsed = os.clock() - started

local result = 0
for index = 0, 4095 do
    result = result + values[index]
end
print(result)
print(math.floor(elapsed * 1e9 + 0.5))
