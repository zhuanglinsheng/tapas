local function bench(n)
    local started = os.clock()
    local value = 0.5

    for _ = 1, n do
        value = value * 1.00000001 + 0.25
        value = value - 0.125
    end

    local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)
    print(math.floor(value))
    print(elapsed)
end

bench(1000000)
