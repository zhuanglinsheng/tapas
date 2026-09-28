local function bench(n)
    local started = os.clock()
    local result = 0

    for index = 0, n - 1 do
        local values = {[0] = index % 97, [1] = index % 89}
        result = result + values[0] - values[1]
    end

    local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)
    print(result)
    print(elapsed)
end

bench(300000)
