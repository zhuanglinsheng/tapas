local function primes_up_to(limit)
    local is_prime = {}
    for value = 0, limit do
        is_prime[value] = value >= 2
    end

    local candidate = 2
    while candidate * candidate <= limit do
        if is_prime[candidate] then
            local multiple = candidate * candidate
            while multiple <= limit do
                is_prime[multiple] = false
                multiple = multiple + candidate
            end
        end
        candidate = candidate + 1
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

print(result)
print(math.floor((os.clock() - started) * 1e9 + 0.5))
