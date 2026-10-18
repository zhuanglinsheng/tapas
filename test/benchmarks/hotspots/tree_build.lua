-- false marks a leaf; every internal node is a fresh table.
local function tree_size(node)
    if not node then
        return 1
    end
    return 1 + tree_size(node[1]) + tree_size(node[2])
end

local function make_tree(depth)
    if depth == 0 then
        return false
    end
    return { make_tree(depth - 1), make_tree(depth - 1) }
end

local function bench(n)
    local started = os.clock()
    local result = 0
    local tree = false
    for r = 1, n do
        tree = make_tree(10)
        result = result + #tree
    end
    result = result + tree_size(tree)
    local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)
    print(result)
    print(elapsed)
end

bench(300)
