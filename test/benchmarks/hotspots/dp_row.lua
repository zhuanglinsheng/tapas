-- One-row LCS update from the valid row for prefix 'ABCDAB'.
local byte = string.byte

local function bench(n, right)
    local started = os.clock()
    -- DP column zero is the boundary cell, as in the other implementations.
    local previous = { [0] = 0, 1, 1, 2, 2, 2, 3, 3, 4, 4, 4, 5, 5, 5, 6, 6, 6 }
    local current = {}
    for j = 0, 16 do
        current[j] = 0
    end
    local result = 0
    for r = 0, n - 1 do
        local ch = byte("ABCD", r % 4 + 1)
        local diagonal, preceding = 0, 0
        for j = 1, 16 do
            local above = previous[j]
            if ch == byte(right, j) then
                preceding = diagonal + 1
            elseif above > preceding then
                preceding = above
            end
            current[j] = preceding
            diagonal = above
        end
        result = result + current[16]
    end
    for j = 0, 16 do
        result = result + current[j] * j
    end
    local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)
    print(result)
    print(elapsed)
end

bench(40000, "DCBAABCDDCBAABCD")
