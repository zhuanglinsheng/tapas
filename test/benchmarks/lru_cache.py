import time


def lru_work(capacity, operations):
    order = []
    values = {}
    checksum = 0
    seed = 12345
    for _ in range(operations):
        seed = (seed * 1103515245 + 12345) % 2147483648
        key = seed % 256
        if seed % 10 < 7:
            if key in values:
                order.remove(key)
                order.append(key)
                checksum += values[key]
        else:
            if key in values:
                order.remove(key)
                order.append(key)
                values[key] = key * 3
            else:
                if len(order) >= capacity:
                    evicted = order.pop(0)
                    del values[evicted]
                    checksum -= evicted
                order.append(key)
                values[key] = key * 3
    return checksum


capacity = 64
operations = 300000

started = time.process_time_ns()
result = lru_work(capacity, operations)
elapsed = time.process_time_ns() - started

print(result)
print(elapsed)
