local function lru_work(capacity, operations)
    local order = {}
    local values = {}
    local checksum = 0
    local seed = 12345
    for _ = 1, operations do
        seed = (seed * 1103515245 + 12345) % 2147483648
        local key = seed % 256
        if seed % 10 < 7 then
            if values[key] ~= nil then
                for i = 1, #order do
                    if order[i] == key then
                        table.remove(order, i)
                        break
                    end
                end
                order[#order + 1] = key
                checksum = checksum + values[key]
            end
        else
            if values[key] ~= nil then
                for i = 1, #order do
                    if order[i] == key then
                        table.remove(order, i)
                        break
                    end
                end
                order[#order + 1] = key
                values[key] = key * 3
            else
                if #order >= capacity then
                    local evicted = table.remove(order, 1)
                    values[evicted] = nil
                    checksum = checksum - evicted
                end
                order[#order + 1] = key
                values[key] = key * 3
            end
        end
    end
    return checksum
end

local capacity = 64
local operations = 300000

local started = os.clock()
local result = lru_work(capacity, operations)
local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)

print(result)
print(elapsed)
