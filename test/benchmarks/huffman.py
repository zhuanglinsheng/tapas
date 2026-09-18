import time

SYMBOLS = "abcdefgh"


def huffman_total(text):
    freq = {}
    for ch in text:
        freq[ch] = freq.get(ch, 0) + 1
    # Fixed symbol order makes ties in the minimum scan deterministic.
    nodes = [(freq[ch], ch) for ch in SYMBOLS if ch in freq]

    def find_min():
        best = -1
        for i, node in enumerate(nodes):
            if best < 0 or node[0] < nodes[best][0]:
                best = i
        return best

    while len(nodes) > 1:
        first = find_min()
        left = nodes.pop(first)
        second = find_min()
        right = nodes.pop(second)
        nodes.append((left[0] + right[0], (left, right)))

    def total_length(node, depth):
        if isinstance(node[1], str):
            return node[0] * depth
        return (total_length(node[1][0], depth + 1) +
                total_length(node[1][1], depth + 1))

    return total_length(nodes[0], 0)


seed = 42
chars = []
for _ in range(40000):
    seed = (seed * 1103515245 + 12345) % 2147483648
    chars.append(SYMBOLS[seed % 8])
text = "".join(chars)

started = time.process_time_ns()
result = huffman_total(text)
elapsed = time.process_time_ns() - started

print(result)
print(elapsed)
