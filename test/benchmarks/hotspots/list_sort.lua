local function bench(repetitions, values)
    local started = os.clock()
    local checksum = 0
    for _ = 1, repetitions do
        local ordered = table.move(values, 1, #values, 1, {})
        table.sort(ordered)
        for position = 1, #ordered do checksum = checksum + ordered[position] * position end
    end
    print(checksum)
    print(math.floor((os.clock() - started) * 1e9 + 0.5))
end

local values = {}
local seed = 42
for _ = 1, 4096 do
    seed = (seed * 1103515245 + 12345) % 2147483648
    values[#values + 1] = seed % 2000001 - 1000000
end
bench(16, values)
