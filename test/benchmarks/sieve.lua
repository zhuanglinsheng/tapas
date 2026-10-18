local function primes_up_to(limit)
    local is_prime = {}
    for value = 0, limit do
        is_prime[value] = value >= 2
    end

    for multiple = 4, limit, 2 do is_prime[multiple] = false end
    for candidate = 3, math.floor(math.sqrt(limit)), 2 do
        if is_prime[candidate] then
            for multiple = candidate * candidate, limit, candidate do
                is_prime[multiple] = false
            end
        end
    end

    local count = 0
    for value = 2, limit do
        if is_prime[value] then
            count = count + 1
        end
    end
    return count
end

local started = os.clock()
local result = primes_up_to(20000)

local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)
print(result)
print(elapsed)
