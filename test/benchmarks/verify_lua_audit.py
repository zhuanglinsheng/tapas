#!/usr/bin/env python3
"""Differentially verify archived Lua source candidates on varied small inputs.

Complements verify_semantics.py (independent cross-language expected results).
No benchmark files are modified. Every archived candidate is checked, including
ones rejected for performance. Floating body state is compared at 17 digits.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile


PROBES = {
    'recursive_fibonacci': 'for n = 0, 18 do print(fibonacci(n)) end',
    'sieve': 'for n = 0, 300 do print(primes_up_to(n)) end',
    'merge_sort': '''
for n = 0, 100 do
    local input = {}
    for i = 1, n do input[i] = (i * 37 + n * 11) % 19 - 9 end
    local output = merge_sort(input)
    print(#output)
    for i = 1, n do
        assert(input[i] == (i * 37 + n * 11) % 19 - 9)
        print(output[i])
    end
end''',
    'n_queens': 'for n = 0, 8 do print(count_queens({}, 0, n)) end',
    'longest_common_subsequence': '''
local texts = {"", "A", "AAAA", "ABABABA", "BACAB", "!ABC09"}
for _, a in ipairs(texts) do
    for _, b in ipairs(texts) do print(lcs_length(a, b)) end
end''',
    'matrix_multiply': '''
for n = 0, 8 do
    local a, b = {}, {}
    for r = 1, n do
        a[r], b[r] = {}, {}
        for c = 1, n do
            a[r][c] = (r * 7 + c * 3) % 11 - 5
            b[r][c] = (r * 3 + c * 7) % 13 - 6
        end
    end
    print(multiply_checksum(a, b, n))
end''',
    'sudoku': '''
for stride = 3, 7 do
    local board = {}
    for r = 0, 8 do
        for c = 0, 8 do board[r * 9 + c + 1] = (r * 3 + r // 3 + c) % 9 + 1 end
    end
    for slot = 1, 81, stride do board[slot] = 0 end
    print(solve_board(board) and 1 or 0)
    for i = 1, 81 do print(board[i]) end
end''',
    'dijkstra': '''
for n = 1, 14 do
    local graph = {}
    for i = 1, n do
        if i % 3 ~= 0 then
            local edges = {}
            graph[i] = edges
            for j = 1, n do
                if (i * 7 + j * 3) % 5 == 0 then
                    edges[#edges + 1] = {j, (i * 3 + j) % 7}
                end
            end
        end
    end
    for source = 0, n - 1 do
        for _, d in ipairs(dijkstra(graph, source, n)) do print(d) end
    end
end''',
    'optimize': '''
local function observed(x)
    print(string.format("%.17g %.17g %.17g", x[1], x[2], rosenbrock(x)))
    return rosenbrock(x)
end
for _, start in ipairs({{-1.2, 1.0}, {0.0, 0.0}, {1.0, 1.0}}) do
    print(optimize(observed, rosenbrock_grad, start, 0.00000001, 30))
end''',
    'kmp': '''
for _, t in ipairs({"", "A", "AAAAAA", "ABABABA", "ABCAB!ABCAB"}) do
    for _, p in ipairs({"A", "AAA", "ABA", "ABCAB", "Z", "!"}) do
        print(kmp_search(t, p))
    end
end''',
    'k_nucleotide': '''
for _, text in ipairs({"", "A", "ABABA", "ACGTACGT", "GGGGGG"}) do
    for k = 1, 4 do
        local counts = count_kmers(text, k)
        local keys = {}
        for key in pairs(counts) do keys[#keys + 1] = key end
        table.sort(keys)
        for _, key in ipairs(keys) do print(key, counts[key]) end
        print("end")
    end
end''',
    'binary_trees': '''
for depth = 0, 8 do
    local tree = make_tree(depth)
    print(checksum(tree))
    if depth >= 2 then assert(tree[1] ~= tree[2]) end
end''',
    'lru_cache': '''
for _, capacity in ipairs({1, 2, 5, 64, 128}) do
    for _, count in ipairs({0, 1, 37, 257, 1000}) do
        print(lru_work(capacity, count))
    end
end''',
    'huffman': '''
for _, text in ipairs({"a", "ab", "aaaabb", "abcdefgh", "hhggffeeddccbbaa",
                      "abcdaaaabbbbcccddeefghh"}) do
    print(huffman_total(text))
end''',
    'fannkuch': 'for n = 1, 7 do print(fannkuch(n)) end',
    'n_body': '''
for _, steps in ipairs({1, 2, 4, 7}) do
    for step = 1, steps do advance(bodies, 0.01) end
    print(string.format("%.17g", energy(bodies)))
    for _, b in ipairs(bodies) do
        for _, field in ipairs({"x", "y", "z", "vx", "vy", "vz", "mass"}) do
            print(string.format("%.17g", b[field]))
        end
    end
end''',
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('reports', type=Path, nargs='+')
    parser.add_argument('--lua', default='lua')
    parser.add_argument('--json', type=Path, required=True)
    args = parser.parse_args()
    checked = []
    with tempfile.TemporaryDirectory(prefix='tapas-lua-probes-') as folder:
        for report_path in args.reports:
            report = json.loads(report_path.read_text())
            for row in report['rows']:
                outputs = {}
                for label, source in row['sources'].items():
                    prefix = re.split(r'^local started = ', source, maxsplit=1, flags=re.M)[0]
                    path = Path(folder) / (label + '.lua')
                    path.write_text(prefix + '\n' + PROBES[row['case']] + '\n')
                    outputs[label] = subprocess.check_output([args.lua, str(path)], text=True, timeout=120)
                if outputs['baseline'] != outputs['candidate']:
                    raise RuntimeError(f'{row["case"]}/{row["variant"]}: differential probe failed')
                checked.append({'case': row['case'], 'variant': row['variant'],
                                'sources_sha256': row['sources_sha256'],
                                'probe_sha256': hashlib.sha256(PROBES[row['case']].encode()).hexdigest(),
                                'output_sha256': hashlib.sha256(outputs['candidate'].encode()).hexdigest()})
    args.json.write_text(json.dumps({'checked': checked}, indent=2) + '\n')
    print(f'{len(checked)} archived candidates passed varied-input differential probes')


if __name__ == '__main__':
    main()
