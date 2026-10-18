import heapq
import time


class PriorityRecord:
    __slots__ = ('priority', 'payload')

    def __init__(self, record):
        self.priority, self.payload = record

    def __lt__(self, other):
        return self.priority < other.priority


def bench(repetitions, values):
    # Records are input setup, outside the timed copy/build/pop task.
    values = list(map(PriorityRecord, values))
    started = time.process_time_ns()
    checksum = 0
    for _ in range(repetitions):
        queue = values.copy()
        heapq.heapify(queue)
        for position in range(1, len(values) + 1):
            checksum += heapq.heappop(queue).priority * position
    elapsed = time.process_time_ns() - started
    print(checksum)
    print(elapsed)


if __name__ == '__main__':
    bench(64, [((i * 73 + 19) % 1009 - 500, i) for i in range(256)])
