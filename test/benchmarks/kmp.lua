local function kmp_search(text, pattern)
    local pi = {}
    for i = 1, #pattern do
        pi[i] = 0
    end
    local j = 0
    for i = 2, #pattern do
        while j > 0 and string.sub(pattern, i, i) ~= string.sub(pattern, j + 1, j + 1) do
            j = pi[j]
        end
        if string.sub(pattern, i, i) == string.sub(pattern, j + 1, j + 1) then
            j = j + 1
        end
        pi[i] = j
    end

    local total = 0
    j = 0
    for i = 1, #text do
        while j > 0 and string.sub(text, i, i) ~= string.sub(pattern, j + 1, j + 1) do
            j = pi[j]
        end
        if string.sub(text, i, i) == string.sub(pattern, j + 1, j + 1) then
            j = j + 1
        end
        if j == #pattern then
            total = total + i - #pattern
            j = pi[j]
        end
    end
    return total
end

local text = string.rep("ABCABABCAB", 500)
local pattern = "ABCAB"
local rounds = 60

local started = os.clock()
local result = 0
for _ = 1, rounds do
    result = result + kmp_search(text, pattern)
end
local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)

print(result)
print(elapsed)
