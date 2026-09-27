local started = os.clock()
local counts = {}
for index = 0, 499999 do
    local key = (index * 17 + 3) % 4096
    counts[key] = (counts[key] or 0) + 1
end
local elapsed = os.clock() - started

local result = 0
for key = 0, 4095 do
    result = result + counts[key]
end
print(result)
print(math.floor(elapsed * 1e9 + 0.5))
