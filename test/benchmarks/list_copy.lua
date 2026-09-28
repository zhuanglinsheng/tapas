local source = {}
for index = 1, 1024 do
    source[index] = index - 1
end

local function bench(rounds, source)
    local started = os.clock()
    local result = 0

    for repetition = 1, rounds do
        local copied = table.move(source, 1, 1024, 1, {})
        result = result + copied[1024]
    end

    local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)
    print(result)
    print(elapsed)
end

bench(20000, source)
