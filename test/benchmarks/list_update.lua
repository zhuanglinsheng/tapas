local values = {}
for index = 1, 1024 do
    values[index] = 0
end

local started = os.clock()
for index = 0, 499999 do
    local slot = index % 1024 + 1
    values[slot] = (values[slot] + index) % 1000003
end
local elapsed = os.clock() - started

local result = 0
for index = 1, 1024 do
    result = result + values[index]
end
print(result)
print(math.floor(elapsed * 1e9 + 0.5))
