# False marks a leaf; every internal node is a fresh tuple.
import time


def make_tree(depth):
    if depth == 0:
        return False
    return (make_tree(depth - 1), make_tree(depth - 1))


def checksum(node):
    if not node:
        return 1
    return 1 + checksum(node[0]) + checksum(node[1])


max_depth = 16

def bench():
    started = time.process_time_ns()
    result = 0
    for depth in range(4, max_depth + 1):
        iterations = 2 ** (max_depth - depth + 2)
        for _ in range(iterations):
            tree = make_tree(depth)
            result += checksum(tree)
    elapsed = time.process_time_ns() - started

    print(result)
    print(elapsed)


if __name__ == "__main__":
    bench()
