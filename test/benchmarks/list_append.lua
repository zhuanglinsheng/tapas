local started = os.clock()
local values = {}

for index = 0, 499999 do
    values[#values + 1] = index % 97
end

local elapsed = os.clock() - started
local result = #values + values[124] + values[500000]
print(result)
print(math.floor(elapsed * 1e9 + 0.5))
