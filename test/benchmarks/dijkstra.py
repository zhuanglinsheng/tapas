import time
from heapq import heappop, heappush


def dijkstra(graph, source, size):
    dist = [1000000000000] * size
    dist[source] = 0
    queue = [(0, source)]
    while queue:
        distance, vertex = heappop(queue)
        if distance != dist[vertex]:
            continue
        for target, weight in graph.get(vertex, ()):
            alternative = distance + weight
            if alternative < dist[target]:
                dist[target] = alternative
                heappush(queue, (alternative, target))
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

def bench():
    started = time.process_time_ns()
    checksum = 0
    for round in range(rounds):
        for s in range(len(sources)):
            dist = dijkstra(graph, sources[s], size)
            checksum += sum(dist)
    elapsed = time.process_time_ns() - started

    print(checksum)
    print(elapsed)


if __name__ == "__main__":
    bench()
