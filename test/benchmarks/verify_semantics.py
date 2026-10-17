#!/usr/bin/env python3
"""Check full benchmark checksums and small cross-language kernel results."""
import argparse
import hashlib
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

from compare_script import BENCHMARKS, HERE, benchmark_source


def run(command, source):
    return subprocess.check_output([*command, str(source)], text=True,
                                   timeout=120).splitlines()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('tapas', type=Path)
    parser.add_argument('--lua', default='lua')
    parser.add_argument('--json', type=Path, default=HERE / 'hotspots/semantics_results.json')
    args = parser.parse_args()
    commands = {'tap': [str(args.tapas.resolve())], 'py': [sys.executable],
                'lua': [args.lua]}
    manifest = json.loads((HERE / 'semantics.json').read_text())
    hashes = {}
    checked = []
    for name in BENCHMARKS:
        for extension, command in commands.items():
            source = (HERE / benchmark_source(name)).with_suffix('.' + extension)
            relative = str(source.relative_to(HERE))
            hashes[relative] = hashlib.sha256(source.read_bytes()).hexdigest()
            lines = run(command, source)
            if len(lines) != 2 or int(lines[0]) != manifest[name]['checksum'] or int(lines[1]) <= 0:
                raise RuntimeError(f'{relative}: invalid benchmark result {lines!r}')
        checked.append(name)
    # Each trailer emits the complete result rather than its timed checksum.
    probes = []
    def add(name, expected, py, tap, lua):
        probes.append((name, list(map(str, expected)), {'py': py, 'tap': tap, 'lua': lua}))
    add('recursive_fibonacci', [0, 1, 1, 21, 144],
        'for n in [0, 1, 2, 8, 12]: print(fibonacci(n))',
        'for (let n in [0, 1, 2, 8, 12]) { print(fibonacci(n)) }',
        'for _, n in ipairs({0, 1, 2, 8, 12}) do print(fibonacci(n)) end')
    add('sieve', [1, 2, 4, 25],
        'for n in [2, 3, 10, 97]: print(primes_up_to(n))',
        'for (let n in [2, 3, 10, 97]) { print(primes_up_to(n)) }',
        'for _, n in ipairs({2, 3, 10, 97}) do print(primes_up_to(n)) end')
    add('merge_sort', [0, 1, 7, 5, -2, 0, 3, 3, 8],
        'for xs in [[], [7], [3, -2, 3, 0, 8]]:\n ys = merge_sort(xs)\n print(len(ys))\n for y in ys: print(y)',
        'for (let xs in [[], [7], [3, -2, 3, 0, 8]]) { let ys = merge_sort(xs)\n print(len(ys))\n for (let y in ys) { print(y) } }',
        'for _, xs in ipairs({{}, {7}, {3, -2, 3, 0, 8}}) do local ys = merge_sort(xs)\n print(#ys)\n for _, y in ipairs(ys) do print(y) end end')
    add('n_queens', [1, 1, 0, 2, 10],
        'for n in [0, 1, 2, 4, 5]: print(count_queens([-1] * n, 0, n))',
        'for (let n in [0, 1, 2, 4, 5]) { print(count_queens(replicate([-1], n), 0, n)) }',
        'for _, n in ipairs({0, 1, 2, 4, 5}) do print(count_queens({}, 0, n)) end')
    add('longest_common_subsequence', [0, 0, 3, 2],
        'for a, b in [("", "ABC"), ("ABC", ""), ("ABABA", "BBA"), ("ABC", "AC")]: print(lcs_length(a, b))',
        "for (let pair in [['', 'ABC'], ['ABC', ''], ['ABABA', 'BBA'], ['ABC', 'AC']]) { print(lcs_length(pair[0], pair[1])) }",
        'for _, p in ipairs({{"", "ABC"}, {"ABC", ""}, {"ABABA", "BBA"}, {"ABC", "AC"}}) do print(lcs_length(p[1], p[2])) end')
    add('kmp', [3, 0, 6, 0],
        'for t, p in [("AAAAA", "AAA"), ("ABC", "Z"), ("AAAA", "A"), ("", "A")]: print(kmp_search(t, p))',
        "for (let pair in [['AAAAA', 'AAA'], ['ABC', 'Z'], ['AAAA', 'A'], ['', 'A']]) { print(kmp_search(pair[0], pair[1])) }",
        'for _, p in ipairs({{"AAAAA", "AAA"}, {"ABC", "Z"}, {"AAAA", "A"}, {"", "A"}}) do print(kmp_search(p[1], p[2])) end')
    add('k_nucleotide', [3, 2, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 2],
        'for k in [1, 2, 3]:\n counts = count_kmers("ABABA", k)\n for key in ["A", "B", "AB", "BA", "ABA"]: print(counts.get(key, 0))',
        "for (let k in [1, 2, 3]) { let counts = count_kmers('ABABA', k)\n for (let key in ['A', 'B', 'AB', 'BA', 'ABA']) { print(idx_or(counts, key, 0)) } }",
        'for _, k in ipairs({1, 2, 3}) do local counts = count_kmers("ABABA", k)\n for _, key in ipairs({"A", "B", "AB", "BA", "ABA"}) do print(counts[key] or 0) end end')
    add('binary_trees', [1, 3, 7, 15, 31, 63],
        'for d in range(6): print(checksum(make_tree(d)))',
        'for (let d in 0 to 6) { print(checksum(make_tree(d))) }',
        'for d = 0, 5 do print(checksum(make_tree(d))) end')
    add('huffman', [2, 5, 8],
        'for text in ["ab", "aaabb", "abcd"]: print(huffman_total(text))',
        "for (let text in ['ab', 'aaabb', 'abcd']) { print(huffman_total(text)) }",
        'for _, text in ipairs({"ab", "aaabb", "abcd"}) do print(huffman_total(text)) end')
    add('fannkuch', [0, 2, 8],
        'for n in [1, 2, 3]: print(fannkuch(n))',
        'for (let n in [1, 2, 3]) { print(fannkuch(n)) }',
        'for _, n in ipairs({1, 2, 3}) do print(fannkuch(n)) end')
    # Independent slow LRU reference also covers capacities unlike the timed case.
    def reference(capacity, operations):
        order, values, total, seed = [], {}, 0, 12345
        for _ in range(operations):
            seed = (seed * 1103515245 + 12345) % 2147483648
            key = seed % 256
            if seed % 10 < 7:
                if key in values:
                    order.remove(key)
                    order.append(key)
                    total += values[key]
            else:
                if key in values:
                    order.remove(key)
                elif len(values) >= capacity:
                    evicted = order.pop(0)
                    del values[evicted]
                    total -= evicted
                order.append(key)
                values[key] = key * 3
        return total
    inputs = [(1, 0), (1, 37), (4, 1000), (64, 1000), (128, 1000)]
    add('lru_cache', [reference(*p) for p in inputs],
        '\n'.join(f'print(lru_work({c}, {n}))' for c, n in inputs),
        '\n'.join(f'print(lru_work({c}, {n}))' for c, n in inputs),
        '\n'.join(f'print(lru_work({c}, {n}))' for c, n in inputs))
    add('matrix_multiply', [299],
        'print(multiply_checksum([[1, 2], [3, 4]], [[5, 6], [7, 8]], 2))',
        'let a = array(2, 2, 0)\n let b = array(2, 2, 0)\n for (let r in 0 to 2) { for (let c in 0 to 2) { a[r, c] = r * 2 + c + 1\n b[r, c] = r * 2 + c + 5 } }\n print(multiply_checksum(a, b, 2))',
        'print(multiply_checksum({{1, 2}, {3, 4}}, {{5, 6}, {7, 8}}, 2))')
    add('dijkstra', [0, 2, 5, 1000000000000, 0, 1000000000000,
                     1000000000000, 0, 0, 1, 1],
        'for graph, size in [({0: [(1, 2), (2, 10)], 1: [(2, 3)]}, 4), '
        '({}, 3), ({0: [(1, 0), (2, 1), (3, 8)], 1: [(0, 0), (3, 1)], '
        '2: [(3, 0)]}, 4)]:\n for d in dijkstra(graph, 0, size): print(d)',
        'let ds = dijkstra({0: [[1, 2], [2, 10]], 1: [[2, 3]]}, 0, 4)\n'
        'for (let d in ds) { print(d) }\n'
        'let isolated = dijkstra({}, 0, 3)\nfor (let d in isolated) { print(d) }\n'
        'let ties = dijkstra({0: [[1, 0], [2, 1], [3, 8]], '
        '1: [[0, 0], [3, 1]], 2: [[3, 0]]}, 0, 4)\n'
        'for (let d in ties) { print(d) }',
        'for _, spec in ipairs({{{{{2, 2}, {3, 10}}, {{3, 3}}}, 4}, '
        '{{}, 3}, {{{{2, 0}, {3, 1}, {4, 8}}, {{1, 0}, {4, 1}}, '
        '{{4, 0}}}, 4}}) do\n'
        'for _, d in ipairs(dijkstra(spec[1], 0, spec[2])) do print(d) end end')
    solved = [(r * 3 + r // 3 + c) % 9 + 1 for r in range(9) for c in range(9)]
    board = solved.copy()
    for index in [0, 40, 80]:
        board[index] = 0
    add('sudoku', [1, *solved],
        f'board = {board!r}\nprint(int(solve_board(board)))\nfor value in board: print(value)',
        f'let board = {board!r}\nif (solve_board(board)) {{ print(1) }} else {{ print(0) }}\nfor (let value in board) {{ print(value) }}',
        'local board = {' + ', '.join(map(str, board)) + '}\nprint(solve_board(board) and 1 or 0)\nfor _, value in ipairs(board) do print(value) end')
    with tempfile.TemporaryDirectory(prefix='tapas-semantics-') as directory:
        for name, expected, trailers in probes:
            for extension, command in commands.items():
                source = (HERE / name).with_suffix('.' + extension)
                prefix = re.split(r'^def bench\(\):|^(?:let |local )?started = ', source.read_text(), maxsplit=1, flags=re.M)[0]
                path = Path(directory) / (name + '.' + extension)
                trailer = trailers[extension]
                if extension == 'tap':
                    trailer = 'function semantic_probe() {\n' + trailer + '\n}\nsemantic_probe()'
                path.write_text(prefix + '\n' + trailer + '\n')
                actual = run(command, path)
                if actual != expected:
                    raise RuntimeError(f'{name}/{extension}: {actual!r} != {expected!r}')
    for relative, digest in hashes.items():
        if hashlib.sha256((HERE / relative).read_bytes()).hexdigest() != digest:
            raise RuntimeError(f'source changed during verification: {relative}')
    report = {'verifier_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'contracts_sha256': hashlib.sha256((HERE / 'semantics.json').read_bytes()).hexdigest(),
              'benchmarks': checked, 'small_kernel_probes': [p[0] for p in probes],
              'sources_sha256': hashes, 'tapas_sha256': hashlib.sha256(args.tapas.read_bytes()).hexdigest(),
              'python': sys.version, 'lua': subprocess.check_output([args.lua, '-v'], text=True).strip()}
    args.json.parent.mkdir(parents=True, exist_ok=True)
    args.json.write_text(json.dumps(report, indent=2) + '\n')
    print(f'{len(checked)} benchmark triples and {len(probes)} kernel probe groups passed')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
