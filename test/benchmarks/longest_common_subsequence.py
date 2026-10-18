import time


def lcs_length(left, right):
    previous = [0] * (len(right) + 1)
    current = [0] * (len(right) + 1)

    for ch in left:
        current[0] = 0
        diagonal = preceding = 0
        for j, other in enumerate(right, 1):
            above = previous[j]
            if ch == other:
                preceding = diagonal + 1
            elif above > preceding:
                preceding = above
            current[j] = preceding
            diagonal = above
        previous, current = current, previous
    return previous[len(right)]


left = "DYNAMICPROGRAMMINGALGORITHMSANDDATASTRUCTURESARECENTRALTOVIRTUALMACHINEBENCHMARKS"
right = "ALGORITHMSFORDYNAMICLANGUAGESUSEPROGRAMSTRUCTUREANDRUNTIMEDATATOMEASUREPERFORMANCE"

def bench():
    started = time.process_time_ns()
    result = 0
    for _ in range(40):
        result += lcs_length(left, right)
    elapsed = time.process_time_ns() - started

    print(result)
    print(elapsed)


if __name__ == "__main__":
    bench()
