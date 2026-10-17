local function bench(n)
    local started = os.clock()
    local values = {}

    for index = 0, n - 1 do
        values[index] = index
    end

    local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)
    local count = 0
    for _ in pairs(values) do
        count = count + 1
    end
    print(count + values[123] + values[199999])
    print(elapsed)
end

bench(200000)
