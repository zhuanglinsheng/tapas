local values = {}
for slot = 1, 1024 do
    values[slot] = 0
end

local function bench(rounds, values)
    local started = os.clock()

    for round = 1, rounds do
        for slot = 0, 1023 do
            values[slot + 1] = slot
        end
    end

    local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)
    print(values[124] + values[1024] + #values)
    print(elapsed)
end

bench(488, values)
