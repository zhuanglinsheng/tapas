local function bench(n)
    local started = os.clock()
    local result = 0

    for index = 0, n - 1 do
        local values = {[0] = index, [1] = index}
        result = values[0]
    end

    local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)
    print(result)
    print(elapsed)
end

bench(300000)
