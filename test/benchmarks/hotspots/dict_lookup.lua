local values = {}
for index = 0, 499999 do
    values[index] = index
end

local function bench(n, values)
    local started = os.clock()
    local sink = 0

    for index = 0, n - 1 do
        sink = values[index]
    end

    local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)
    local count = 0
    for _ in pairs(values) do
        count = count + 1
    end
    print(sink + count)
    print(elapsed)
end

bench(500000, values)
