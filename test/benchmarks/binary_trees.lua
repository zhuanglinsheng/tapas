local function make_tree(depth)
    if depth == 0 then
        return {}
    end
    return { make_tree(depth - 1), make_tree(depth - 1) }
end

local function checksum(node)
    if #node == 0 then
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
