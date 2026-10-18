# Default trees use False leaves and fresh tuples; --matched keeps fresh Lists.
import sys
import time

def make_list_tree(depth):
    if depth == 0:
        return []
    return [make_list_tree(depth - 1), make_list_tree(depth - 1)]


def make_tuple_tree(depth):
    if depth == 0:
        return False
    return (make_tuple_tree(depth - 1), make_tuple_tree(depth - 1))


def tree_size(node):
    if not node:
        return 1
    return 1 + tree_size(node[0]) + tree_size(node[1])


def matched(n):
    result = 0
    for r in range(n):
        tree = make_list_tree(10)
        result += len(tree)
    return result + tree_size(tree)


def optimized(n):
    result = 0
    for r in range(n):
        tree = make_tuple_tree(10)
        result += len(tree)
    return result + tree_size(tree)


def bench(n):
    kernel = matched if "--matched" in sys.argv else optimized
    started = time.process_time_ns()
    result = kernel(n)
    elapsed = time.process_time_ns() - started
    print(result)
    print(elapsed)


if __name__ == "__main__":
    bench(300)
