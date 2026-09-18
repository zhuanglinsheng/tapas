local function optimize(f, grad, start, tolerance, max_iterations)
    local c1 = 0.0001
    local rho = 0.5
    local line_search = function(x, direction, fx)
        local step = 1.0
        local gnorm2 = direction[0] * direction[0] + direction[1] * direction[1]
        while true do
            local next = {[0] = x[0] + step * direction[0], x[1] + step * direction[1]}
            if f(next) <= fx - c1 * step * gnorm2 then
                return next
            end
            step = step * rho
        end
        return x
    end
    local x = {[0] = start[0], start[1]}
    local iterations = 0
    while iterations < max_iterations do
        local g = grad(x)
        local gnorm2 = g[0] * g[0] + g[1] * g[1]
        if gnorm2 < tolerance then
            break
        end
        x = line_search(x, {[0] = 0.0 - g[0], 0.0 - g[1]}, f(x))
        iterations = iterations + 1
    end
    return iterations
end

local rosenbrock = function(x)
    local a = 1.0 - x[0]
    local b = x[1] - x[0] * x[0]
    return a * a + 100.0 * b * b
end

local rosenbrock_grad = function(x)
    local a = 1.0 - x[0]
    local b = x[1] - x[0] * x[0]
    return {[0] = 0.0 - 2.0 * a - 400.0 * x[0] * b, 200.0 * b}
end

local started = os.clock()
local result = optimize(rosenbrock, rosenbrock_grad, {[0] = -1.2, 1.0}, 0.00000001, 100000)
local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)

print(result)
print(elapsed)
