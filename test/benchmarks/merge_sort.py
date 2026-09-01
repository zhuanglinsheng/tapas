import time


def merge_sort(values):
    if len(values) <= 1:
        return values.copy()

    middle = len(values) // 2
    left = merge_sort(values[:middle])
    right = merge_sort(values[middle:])
    merged = []
    i = 0
    j = 0

    while i < len(left) and j < len(right):
        if left[i] <= right[j]:
            merged.append(left[i])
            i += 1
        else:
            merged.append(right[j])
            j += 1
    while i < len(left):
        merged.append(left[i])
        i += 1
    while j < len(right):
        merged.append(right[j])
        j += 1
    return merged.copy()


values = [(index * 73 + 19) % 1009 for index in range(4096)]
started = time.process_time_ns()
sorted_values = merge_sort(values)
checksum = 0
for index, value in enumerate(sorted_values):
    checksum += value * (index + 1)
elapsed = time.process_time_ns() - started

print(checksum)
print(elapsed)
