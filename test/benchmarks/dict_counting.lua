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
            counts[key] = counts[key]
        else
            counts[key] = key
        end
    end

    local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)
    -- The checksum must depend on the stored values: counting the pairs alone
    -- cannot detect a broken read-then-write-back. Summed after the timed region.
    local result = 0
    for _, value in pairs(counts) do
        result = result + value
    end
    print(result)
    print(elapsed)
end

bench(500000, keys)
