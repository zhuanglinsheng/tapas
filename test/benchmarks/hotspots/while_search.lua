local values = {}
for value = 8, 31 do
    values[#values + 1] = value
end
for _, value in ipairs({ 3, 1, 4, 0, 5, 7, 2, 6 }) do
    values[#values + 1] = value
end

local function bench(n, values)
    local started = os.clock()
    local result = 0
    for r = 0, n - 1 do
        local target = r % 8
        local j = 1
        while j <= 32 do
            if values[j] == target then
                break
            end
            j = j + 1
        end
        -- Return the zero-based offset (32 on a miss), matching Tapas/Python.
        result = result + j - 1
    end
    local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)
    print(result)
    print(elapsed)
end

bench(100000, values)
