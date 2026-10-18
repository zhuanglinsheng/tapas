local function heap_push(queue, item)
    local index = #queue + 1
    while index > 1 do
        local parent = index // 2
        local above = queue[parent]
        if above[1] < item[1] or (above[1] == item[1] and above[2] <= item[2]) then break end
        queue[index] = above
        index = parent
    end
    queue[index] = item
end

local function heap_pop(queue)
    local root = queue[1]
    local size = #queue
    local last = queue[size]
    queue[size] = nil
    size = size - 1
    if size > 0 then
        local index = 1
        while index * 2 <= size do
            local child = index * 2
            if child < size then
                local a, b = queue[child], queue[child + 1]
                if b[1] < a[1] or (b[1] == a[1] and b[2] < a[2]) then child = child + 1 end
            end
            local below = queue[child]
            if last[1] < below[1] or (last[1] == below[1] and last[2] <= below[2]) then break end
            queue[index] = below
            index = child
        end
        queue[index] = last
    end
    return root
end

local function dijkstra(graph, source, size)
    local dist = {}
    for i = 1, size do dist[i] = 1000000000000 end
    dist[source + 1] = 0
    local queue = {{0, source + 1}}
    while #queue > 0 do
        local entry = heap_pop(queue)
        local distance, vertex = entry[1], entry[2]
        if distance == dist[vertex] then
            local edges = graph[vertex]
            for edge_index = 1, edges and #edges or 0 do
                local edge = edges[edge_index]
                local target = edge[1]
                local alternative = distance + edge[2]
                if alternative < dist[target] then
                    dist[target] = alternative
                    heap_push(queue, {alternative, target})
                end
            end
        end
    end
    return dist
end

local size, degree = 250, 6
local graph = {}
for i = 0, size - 1 do
    local edges = { { (i + 1) % size + 1, 1 } }
    for k = 0, degree - 1 do
        local j = (i * 7 + k * 13 + 3) % size
        local w = (i * 5 + k * 11 + 7) % 19 + 1
        edges[#edges + 1] = { j + 1, w }
    end
    graph[i + 1] = edges
end
local sources = { 0, size // 4, size // 2, size * 3 // 4 }
local rounds = 4
local started = os.clock()
local checksum = 0
for _ = 1, rounds do
    for _, source in ipairs(sources) do
        local dist = dijkstra(graph, source, size)
        for i = 1, size do
            checksum = checksum + dist[i]
        end
    end
end
local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)
print(checksum)
print(elapsed)
