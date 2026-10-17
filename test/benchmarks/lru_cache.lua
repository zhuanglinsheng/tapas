local function lru_work(capacity, operations)
    local values = {}
    local previous = { [-2] = -1 }
    local next_key = { [-1] = -2 }
    local count = 0
    local checksum = 0
    local seed = 12345
    for _ = 1, operations do
        seed = (seed * 1103515245 + 12345) % 2147483648
        local key = seed % 256
        local reading = seed % 10 < 7
        local present = values[key] ~= nil
        if present then
            local before, after = previous[key], next_key[key]
            next_key[before], previous[after] = after, before
        elseif not reading then
            if count >= capacity then
                local evicted = next_key[-1]
                local after = next_key[evicted]
                next_key[-1], previous[after] = after, -1
                values[evicted], previous[evicted], next_key[evicted] = nil, nil, nil
                checksum = checksum - evicted
                count = count - 1
            end
            count = count + 1
        end
        if present or not reading then
            local tail = previous[-2]
            previous[key], next_key[key] = tail, -2
            next_key[tail], previous[-2] = key, key
            if reading then
                checksum = checksum + values[key]
            else
                values[key] = key * 3
            end
        end
    end
    return checksum
end

local started = os.clock()
local result = lru_work(64, 300000)
local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)
print(result)
print(elapsed)
