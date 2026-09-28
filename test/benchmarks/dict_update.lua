local values = {}
for index = 1, 500000 do
    values[index] = 0
end

local function bench(n, values)
    local started = os.clock()

    for index = 1, n do
        values[index] = index - 1
    end

    local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)
    print(values[124] + values[500000] + #values)
    print(elapsed)
end

bench(500000, values)
