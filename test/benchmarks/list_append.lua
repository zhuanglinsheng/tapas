local function bench(n)
    local values = {}
    local started = os.clock()

    for index = 1, n do
        values[#values + 1] = index - 1
    end

    local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)
    print(#values + values[124] + values[500000])
    print(elapsed)
end

bench(500000)
