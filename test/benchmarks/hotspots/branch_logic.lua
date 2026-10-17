local function bench(n)
    local started = os.clock()
    local state = 17

    for _ = 1, n do
        if state > 55 then
            state = state - 23
        elseif state > 40 and state <= 55 then
            state = state + 21
        elseif state < 20 or state == 20 then
            state = state + 31
        else
            state = state - 19
        end
    end

    local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)
    print(state)
    print(elapsed)
end

bench(1000000)
