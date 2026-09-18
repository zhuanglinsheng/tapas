import time


def optimize(f, grad, start, tolerance, max_iterations):
    c1 = 0.0001
    rho = 0.5

    def line_search(x, direction, fx):
        step = 1.0
        gnorm2 = direction[0] * direction[0] + direction[1] * direction[1]
        while True:
            next = [x[0] + step * direction[0], x[1] + step * direction[1]]
            if f(next) <= fx - c1 * step * gnorm2:
                return next
            step = step * rho
        return x

    x = [start[0], start[1]]
    iterations = 0
    while iterations < max_iterations:
        g = grad(x)
        gnorm2 = g[0] * g[0] + g[1] * g[1]
        if gnorm2 < tolerance:
            break
        x = line_search(x, [0.0 - g[0], 0.0 - g[1]], f(x))
        iterations += 1
    return iterations


def rosenbrock(x):
    a = 1.0 - x[0]
    b = x[1] - x[0] * x[0]
    return a * a + 100.0 * b * b


def rosenbrock_grad(x):
    a = 1.0 - x[0]
    b = x[1] - x[0] * x[0]
    return [0.0 - 2.0 * a - 400.0 * x[0] * b, 200.0 * b]


started = time.process_time_ns()
result = optimize(rosenbrock, rosenbrock_grad, [-1.2, 1.0], 0.00000001, 100000)
elapsed = time.process_time_ns() - started

print(result)
print(elapsed)
