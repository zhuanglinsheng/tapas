local function visit(depth, values)
    local total = 0
    for i = 1, #values do
        total = total + values[i]
    end
    if depth == 0 then
        return total
    end
    return total + visit(depth - 1, values) + visit(depth - 1, values)
end

local function bench()
    local values = { 1, 2, 3, 4 }
    local started = os.clock()
    local result = visit(16, values)
    local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)
    print(result)
    print(elapsed)
end

bench()
