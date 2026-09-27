local function add(left, right)
    return left + right
end

local function apply(callback, left, right)
    return callback(left, right)
end

local started = os.clock()
local total = 0

for index = 0, 199999 do
    total = apply(add, total, index % 7)
end

print(total)
print(math.floor((os.clock() - started) * 1e9 + 0.5))
