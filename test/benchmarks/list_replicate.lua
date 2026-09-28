local pattern = {0, 1, 2, 3}

local function replicate(values, count)
    local result = {}
    for _ = 1, count do
        for index = 1, #values do
            result[#result + 1] = values[index]
        end
    end
    return result
end

local function bench(rounds, pattern)
    local started = os.clock()
    local result = 0

    for repetition = 1, rounds do
        local values = replicate(pattern, 256)
        result = result + values[1024]
    end

    local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)
    print(result)
    print(elapsed)
end

bench(20000, pattern)
