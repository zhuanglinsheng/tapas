local sub = string.sub
local parts = {}
local seed = 42
for i = 1, 120000 do
    seed = (seed * 1103515245 + 12345) % 2147483648
    -- Match the high-bit base selection; low bits would only cycle.
    local index = (seed // 65536) % 4 + 1
    parts[i] = sub("ACGT", index, index)
end
local sequence = table.concat(parts)

local function bench(sequence, k)
    local started = os.clock()
    local counts = {}
    for i = 1, #sequence - k + 1 do
        local key = sub(sequence, i, i + k - 1)
        counts[key] = (counts[key] or 0) + 1
    end
    local distinct = 0
    local result = 0
    for _, count in pairs(counts) do
        distinct = distinct + 1
        result = result + count * count
    end
    result = result + distinct
    local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)
    print(result)
    print(elapsed)
end

bench(sequence, 8)
