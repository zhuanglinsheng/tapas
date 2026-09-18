local function dijkstra(graph, source, size)
    local dist = {}
    local visited = {}
    for i = 0, size - 1 do
        dist[i] = 1000000000000
        visited[i] = false
    end
    dist[source] = 0
    for step = 0, size - 1 do
        local best = -1
        local best_dist = 1000000000000
        for i = 0, size - 1 do
            if not visited[i] and dist[i] < best_dist then
                best = i
                best_dist = dist[i]
            end
        end
        if best < 0 then
            break
        end
        visited[best] = true
        if graph[best] ~= nil then
            for edge = 0, #graph[best] do
                local pair = graph[best][edge]
                local target = pair[0]
                local alt = dist[best] + pair[1]
                if alt < dist[target] then
                    dist[target] = alt
                end
            end
        end
    end
    return dist
end

local size = 250
local degree = 6
local graph = {}
for i = 0, size - 1 do
    local edges = {}
    edges[0] = {[0] = (i + 1) % size, 1}
    for k = 0, degree - 1 do
        local j = (i * 7 + k * 13 + 3) % size
        local w = (i * 5 + k * 11 + 7) % 19 + 1
        edges[k + 1] = {[0] = j, w}
    end
    graph[i] = edges
end
local sources = {[0] = 0, size // 4, size // 2, size * 3 // 4}
local rounds = 4

local started = os.clock()
local checksum = 0
for round = 0, rounds - 1 do
    for s = 0, #sources do
        local dist = dijkstra(graph, sources[s], size)
        for i = 0, size - 1 do
            checksum = checksum + dist[i]
        end
    end
end
local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)

print(checksum)
print(elapsed)
