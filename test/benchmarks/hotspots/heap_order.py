import heapq
import time


def bench(repetitions, values):
    started = time.process_time_ns()
    checksum = 0
    for _ in range(repetitions):
        queue = values.copy()
        heapq.heapify(queue)
        for position in range(1, len(values) + 1):
            checksum += heapq.heappop(queue)[0] * position
    elapsed = time.process_time_ns() - started
    print(checksum)
    print(elapsed)


if __name__ == '__main__':
    bench(64, [((i * 73 + 19) % 31 - 15, i) for i in range(256)])
