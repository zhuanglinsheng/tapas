local function count_down(remaining, count)
    if remaining == 0 then
        return count
    end
    return count_down(remaining - 1, count + 1)
end

local started = os.clock()
local result = count_down(50000, 0)

print(result)
print(math.floor((os.clock() - started) * 1e9 + 0.5))
