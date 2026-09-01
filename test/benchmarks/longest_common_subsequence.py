import time


def lcs_length(left, right):
    previous = [0] * (len(right) + 1)
    current = [0] * (len(right) + 1)

    for i in range(1, len(left) + 1):
        current[0] = 0
        for j in range(1, len(right) + 1):
            if left[i - 1] == right[j - 1]:
                current[j] = previous[j - 1] + 1
            elif previous[j] >= current[j - 1]:
                current[j] = previous[j]
            else:
                current[j] = current[j - 1]
        previous, current = current, previous
    return previous[len(right)]


left = "DYNAMICPROGRAMMINGALGORITHMSANDDATASTRUCTURESARECENTRALTOVIRTUALMACHINEBENCHMARKS"
right = "ALGORITHMSFORDYNAMICLANGUAGESUSEPROGRAMSTRUCTUREANDRUNTIMEDATATOMEASUREPERFORMANCE"

started = time.process_time_ns()
result = 0
for _ in range(40):
    result += lcs_length(left, right)
elapsed = time.process_time_ns() - started

print(result)
print(elapsed)
