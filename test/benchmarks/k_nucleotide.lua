local BASES = "ACGT"
local probes = {}
for i = 1, 4 do
    probes[#probes + 1] = string.sub(BASES, i, i)
end
for a = 1, 4 do
    for b = 1, 4 do
        probes[#probes + 1] =
            string.sub(BASES, a, a) .. string.sub(BASES, b, b)
    end
end

local function count_kmers(sequence, k)
    local counts = {}
    for i = 1, #sequence - k + 1 do
        local key = string.sub(sequence, i, i + k - 1)
        if counts[key] then
            counts[key] = counts[key] + 1
        else
            counts[key] = 1
        end
    end
    return counts
end

local seed = 42
local parts = {}
for _ = 1, 120000 do
    seed = (seed * 1103515245 + 12345) % 2147483648
    parts[#parts + 1] = string.sub(BASES, seed % 4 + 1, seed % 4 + 1)
end
local sequence = table.concat(parts)

local started = os.clock()
local result = 0
for _ = 1, 6 do
    local single = count_kmers(sequence, 1)
    for i = 1, 4 do
        result = result + (single[probes[i]] or 0)
    end
    local pairs = count_kmers(sequence, 2)
    for i = 5, 20 do
        result = result + (pairs[probes[i]] or 0)
    end
end
local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)

print(result)
print(elapsed)
