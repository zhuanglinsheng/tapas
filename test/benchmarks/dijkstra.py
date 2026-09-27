import time


def dijkstra(graph, source, size):
    dist = [1000000000000] * size
    visited = [False] * size
    dist[source] = 0
    for step in range(size):
        best = -1
        best_dist = 1000000000000
        for i in range(size):
            if not visited[i] and dist[i] < best_dist:
                best = i
                best_dist = dist[i]
        if best < 0:
            break
        visited[best] = True
        if best in graph:
            for edge in graph[best]:
                target = edge[0]
                alt = dist[best] + edge[1]
                if alt < dist[target]:
                    dist[target] = alt
    return dist


size = 250
degree = 6
graph = {}
for i in range(size):
    edges = []
    edges.append([(i + 1) % size, 1])
    for k in range(degree):
        j = (i * 7 + k * 13 + 3) % size
        w = (i * 5 + k * 11 + 7) % 19 + 1
        edges.append([j, w])
    graph[i] = edges
sources = [0, size // 4, size // 2, size * 3 // 4]
rounds = 4

started = time.process_time_ns()
checksum = 0
for round in range(rounds):
    for s in range(len(sources)):
        dist = dijkstra(graph, sources[s], size)
        for i in range(size):
            checksum += dist[i]
elapsed = time.process_time_ns() - started

print(checksum)
print(elapsed)
