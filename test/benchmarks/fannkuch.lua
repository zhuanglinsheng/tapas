local function fannkuch(n)
    local perm1 = {}
    local count = {}
    for i = 1, n do
        perm1[i] = i - 1
        count[i] = 0
    end
    local checksum = 0
    local maxflips = 0
    local r = n
    while true do
        while r ~= 1 do
            count[r] = r
            r = r - 1
        end

        local perm = {}
        for i = 1, n do
            perm[i] = perm1[i]
        end
        local flips = 0
        local k = perm[1]
        while k ~= 0 do
            local i = 1
            local j = k + 1
            while i < j do
                perm[i], perm[j] = perm[j], perm[i]
                i = i + 1
                j = j - 1
            end
            flips = flips + 1
            k = perm[1]
        end
        checksum = checksum + flips
        if flips > maxflips then
            maxflips = flips
        end

        while true do
            if r == n then
                return checksum + maxflips
            end
            local first = perm1[1]
            for i = 1, r do
                perm1[i] = perm1[i + 1]
            end
            perm1[r + 1] = first
            count[r + 1] = count[r + 1] - 1
            if count[r + 1] > 0 then
                break
            end
            r = r + 1
        end
    end
end

local n = 8

local started = os.clock()
local result = fannkuch(n)
local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)

print(result)
print(elapsed)
