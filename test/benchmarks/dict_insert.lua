local function bench(n)
    local started = os.clock()
    local values = {}

    for index = 1, n do
        values[index] = index - 1
    end

    local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)
    print(#values + values[124] + values[200000])
    print(elapsed)
end

bench(200000)
