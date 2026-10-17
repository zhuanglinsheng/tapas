local pattern = {0, 1, 2, 3}

local function replicate(values, count)
    local result = {}
    local size = #values
    local total = size * count
    if total == 0 then
        return result
    end
    table.move(values, 1, size, 1, result)
    local filled = size
    while filled < total do
        local copied = math.min(filled, total - filled)
        table.move(result, 1, copied, filled + 1, result)
        filled = filled + copied
    end
    return result
end

local function bench(rounds, pattern)
    local started = os.clock()
    local result = 0

    for repetition = 1, rounds do
        local values = replicate(pattern, 256)
        result = values[1024]
    end

    local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)
    print(result)
    print(elapsed)
end

bench(20000, pattern)
