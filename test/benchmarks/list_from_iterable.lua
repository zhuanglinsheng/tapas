local function list_from_range(first, last)
    local result = {}
    for value = first, last - 1 do
        result[#result + 1] = value
    end
    return result
end

local function bench(rounds)
    local started = os.clock()
    local result = 0

    for repetition = 1, rounds do
        local values = list_from_range(0, 1024)
        result = result + values[1024]
    end

    local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)
    print(result)
    print(elapsed)
end

bench(20000)
