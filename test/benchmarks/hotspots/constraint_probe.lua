local grid = {}
for row = 0, 8 do
    for column = 0, 8 do
        grid[#grid + 1] = (row * 3 + row // 3 + column) % 9 + 1
    end
end

local function bench(n, grid)
    local result = 0
    local started = os.clock()
    for r = 0, n - 1 do
        local index = r % 81
        local row = (index // 9) * 9
        local column = index % 9
        local box = (index // 27) * 27 + (column // 3) * 3
        local digit = (r // 81) % 9 + 1
        local k = 0
        while k < 9 do
            -- Logical board offsets are zero-based; Lua arrays start at one.
            if grid[row + k + 1] == digit or grid[column + k * 9 + 1] == digit then
                break
            end
            if grid[box + (k // 3) * 9 + k % 3 + 1] == digit then
                break
            end
            k = k + 1
        end
        result = result + k
    end
    local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)
    print(result)
    print(elapsed)
end

bench(200000, grid)
