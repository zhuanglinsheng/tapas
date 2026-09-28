local values = {}
for index = 1, 500000 do
    values[index] = index - 1
end

local function bench(n, values)
    local started = os.clock()
    local sink = 0

    for index = 1, n do
        sink = values[index]
    end

    local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)
    print(sink + #values)
    print(elapsed)
end

bench(500000, values)
