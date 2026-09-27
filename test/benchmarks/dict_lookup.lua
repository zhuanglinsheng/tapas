local values = {}
for index = 0, 4095 do
    values[index] = index % 97
end

local started = os.clock()
local result = 0
for index = 0, 999999 do
    result = result + values[index % 4096]
end
local elapsed = os.clock() - started

print(result)
print(math.floor(elapsed * 1e9 + 0.5))
