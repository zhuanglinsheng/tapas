local keys = {}
for index = 1, 500000 do
    keys[index] = (index - 1) % 4096
end

local function bench(n, keys)
    local started = os.clock()
    local counts = {}

    for index = 1, n do
        local key = keys[index]
        if counts[key] ~= nil then
            counts[key] = counts[key] + 1
        else
            counts[key] = 1
        end
    end

    local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)
    local result = 0
    for key = 0, 4095 do
        result = result + (counts[key] or 0)
    end
    print(result)
    print(elapsed)
end

bench(500000, keys)
