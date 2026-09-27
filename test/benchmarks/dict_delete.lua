local values = {}
for index = 0, 199999 do
    values[index] = index
end

local started = os.clock()
for index = 0, 199999 do
    values[index] = nil
end
local elapsed = os.clock() - started

print(0)
print(math.floor(elapsed * 1e9 + 0.5))
