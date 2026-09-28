local function bench(n)
    local values = {}
    for index = 1, n do
        values[index] = index - 1
    end

    local started = os.clock()
    for index = 1, n do
        values[index] = nil
    end
    local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)

    local count = 0
    for _ in pairs(values) do
        count = count + 1
    end
    print(count)
    print(elapsed)
end

bench(200000)
