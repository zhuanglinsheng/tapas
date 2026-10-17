local function bench(text)
    local started = os.clock()
    local counts = {}
    for ch in text:gmatch(".") do
        counts[ch] = (counts[ch] or 0) + 1
    end
    local result = counts.a + counts.b * 3 + counts.c * 5 + counts.d * 7
    local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)
    print(result)
    print(elapsed)
end

bench(string.rep("abacabad", 62500))
