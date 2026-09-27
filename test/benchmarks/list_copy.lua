local source = {}
for index = 0, 1023 do
    source[index + 1] = index
end

local started = os.clock()
local result = 0
for repetition = 0, 19999 do
    local copied = table.move(source, 1, 1024, 1, {})
    result = result + copied[repetition % 1024 + 1]
end
local elapsed = os.clock() - started

print(result)
print(math.floor(elapsed * 1e9 + 0.5))
