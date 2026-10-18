-- false marks a leaf; every internal node is a fresh table.
local function make_tree(depth)
    if depth == 0 then
        return false
    end
    local left = make_tree(depth - 1)
    local right = make_tree(depth - 1)
    return { left, right }
end

local function checksum(node)
    if not node then
        return 1
    end
    return 1 + checksum(node[1]) + checksum(node[2])
end

local max_depth = 16

local started = os.clock()
local result = 0
for depth = 4, max_depth do
    local iterations = 2 ^ (max_depth - depth + 2)
    for _ = 1, iterations do
        local tree = make_tree(depth)
        result = result + checksum(tree)
    end
end
local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)

print(result)
print(elapsed)
