import time


def merge_sort(values):
    if len(values) <= 1:
        return values

    middle = len(values) // 2
    left = merge_sort(values[:middle])
    right = merge_sort(values[middle:])
    merged = []
    i = 0
    j = 0

    left_size = len(left)
    right_size = len(right)
    while i < left_size and j < right_size:
        if left[i] <= right[j]:
            merged.append(left[i])
            i += 1
        else:
            merged.append(right[j])
            j += 1
    merged.extend(left[i:])
    merged.extend(right[j:])
    return merged


values = [(index * 73 + 19) % 1009 for index in range(4096)]
def bench():
    started = time.process_time_ns()
    sorted_values = merge_sort(values)
    checksum = sum(value * index for index, value in enumerate(sorted_values, 1))
    elapsed = time.process_time_ns() - started

    print(checksum)
    print(elapsed)


if __name__ == "__main__":
    bench()
