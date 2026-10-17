local function bench(n, body)
    local started = os.clock()
    for i = 1, n do
        body.x = body.x + body.vx
    end
    local result = math.floor(body.x)
    local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)
    print(result)
    print(elapsed)
end

bench(500000, { x = 0.0, vx = 0.25 })
