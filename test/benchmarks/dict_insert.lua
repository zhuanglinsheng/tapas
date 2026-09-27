local started = os.clock()
local values = {}
for index = 0, 199999 do
    values[index] = index % 97
end
local elapsed = os.clock() - started

local result = 200000 + values[123] + values[199999]
print(result)
print(math.floor(elapsed * 1e9 + 0.5))
