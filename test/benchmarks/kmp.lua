local byte = string.byte

local function kmp_search(text, pattern)
    local length = #pattern
    local pattern_bytes = {}
    for index = 1, length do pattern_bytes[index] = byte(pattern, index) end
    local pi = {}
    for i = 1, length do
        pi[i] = 0
    end
    local j = 0
    for i = 2, length do
        while j > 0 and pattern_bytes[i] ~= pattern_bytes[j + 1] do
            j = pi[j]
        end
        if pattern_bytes[i] == pattern_bytes[j + 1] then
            j = j + 1
        end
        pi[i] = j
    end

    local total = 0
    j = 0
    for i = 1, #text do
        local ch = byte(text, i)
        while j > 0 and ch ~= pattern_bytes[j + 1] do
            j = pi[j]
        end
        if ch == pattern_bytes[j + 1] then
            j = j + 1
        end
        if j == length then
            total = total + i - length
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
