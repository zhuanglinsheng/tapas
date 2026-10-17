local function fibonacci(n)
    if n < 2 then
        return n
    end
    return fibonacci(n - 1) + fibonacci(n - 2)
end

local started = os.clock()
local result = fibonacci(24)

local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)
print(result)
print(elapsed)
