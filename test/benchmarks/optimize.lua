local function optimize(f, grad, start, tolerance, max_iterations)
    local c1 = 0.0001
    local rho = 0.5
    local line_search = function(x, direction, fx)
        local x1, x2 = x[1], x[2]
        local d1, d2 = direction[1], direction[2]
        local step = 1.0
        local gnorm2 = d1 * d1 + d2 * d2
        local next = {}
        while true do
            next[1], next[2] = x1 + step * d1, x2 + step * d2
            if f(next) <= fx - c1 * step * gnorm2 then
                return next
            end
            step = step * rho
        end
        return x
    end
    local x = {start[1], start[2]}
    local iterations = 0
    while iterations < max_iterations do
        local g = grad(x)
        local gnorm2 = g[1] * g[1] + g[2] * g[2]
        if gnorm2 < tolerance then
            break
        end
        x = line_search(x, {0.0 - g[1], 0.0 - g[2]}, f(x))
        iterations = iterations + 1
    end
    return iterations
end

local rosenbrock = function(x)
    local a = 1.0 - x[1]
    local b = x[2] - x[1] * x[1]
    return a * a + 100.0 * b * b
end

local rosenbrock_grad = function(x)
    local a = 1.0 - x[1]
    local b = x[2] - x[1] * x[1]
    return {0.0 - 2.0 * a - 400.0 * x[1] * b, 200.0 * b}
end

local started = os.clock()
local result = optimize(rosenbrock, rosenbrock_grad, {-1.2, 1.0}, 0.00000001, 100000)
local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)

print(result)
print(elapsed)
