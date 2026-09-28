local function add(left, right)
    return left + right
end

local function bench(n, add)
    local started = os.clock()
    local total = 0

    for index = 0, n - 1 do
        total = add(total, index % 7)
    end

    local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)
    print(total)
    print(elapsed)
end

bench(200000, add)
