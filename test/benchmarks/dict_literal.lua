local started = os.clock()
local result = 0
for index = 0, 299999 do
    local values = {[0] = index, [1] = index + 1}
    result = result + values[0] - values[1]
end
local elapsed = os.clock() - started

print(result)
print(math.floor(elapsed * 1e9 + 0.5))
