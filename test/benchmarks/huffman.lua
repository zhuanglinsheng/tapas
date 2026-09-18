local SYMBOLS = "abcdefgh"

local function huffman_total(text)
    local freq = {}
    for i = 1, #text do
        local ch = string.sub(text, i, i)
        if freq[ch] then
            freq[ch] = freq[ch] + 1
        else
            freq[ch] = 1
        end
    end

    -- Fixed symbol order makes ties in the minimum scan deterministic.
    local nodes = {}
    for i = 1, 8 do
        local ch = string.sub(SYMBOLS, i, i)
        if freq[ch] then
            nodes[#nodes + 1] = { freq[ch], ch }
        end
    end

    local function find_min()
        local best = -1
        for i = 1, #nodes do
            if best < 0 or nodes[i][1] < nodes[best][1] then
                best = i
            end
        end
        return best
    end

    while #nodes > 1 do
        local first = find_min()
        local left = table.remove(nodes, first)
        local second = find_min()
        local right = table.remove(nodes, second)
        nodes[#nodes + 1] = { left[1] + right[1], { left, right } }
    end

    local function total_length(node, depth)
        if type(node[2]) == "string" then
            return node[1] * depth
        end
        return total_length(node[2][1], depth + 1) +
            total_length(node[2][2], depth + 1)
    end

    return total_length(nodes[1], 0)
end

local seed = 42
local chars = {}
for _ = 1, 40000 do
    seed = (seed * 1103515245 + 12345) % 2147483648
    chars[#chars + 1] = string.sub(SYMBOLS, seed % 8 + 1, seed % 8 + 1)
end
local text = table.concat(chars)

local started = os.clock()
local result = huffman_total(text)
local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)

print(result)
print(elapsed)
