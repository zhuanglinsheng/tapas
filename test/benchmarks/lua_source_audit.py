#!/usr/bin/env python3
"""Reproduce Lua algorithm candidates; keep sources and alternating CPU samples.

Screen original sources: --baseline-report ARCHIVE --json PATH --runs 5 --rounds 2
Replay archived candidates: --from-report ARCHIVE --replay-all --json PATH
Confirm: --from-report PATH --json OTHER_PATH --runs 11 --rounds 3
Confirmation selects the fastest screening candidate per algorithm only when
every screening round improved and the combined paired ratio is below 0.97.
This tool never installs a candidate or changes the benchmark reports.
"""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import platform
import shutil
import subprocess
import tempfile

from ab_script import summarize
from compare_script import ALGORITHM_BENCHMARKS, HERE


def candidates(name, source):
    def replace(old, new):
        if old not in source:
            raise ValueError(f'{name}: missing candidate anchor {old!r}')
        return source.replace(old, new)

    if name == 'recursive_fibonacci':
        yield 'integer_base_case', replace('n < 2', 'n <= 1')
    elif name == 'sieve':
        yield 'numeric_for', replace('''            local multiple = candidate * candidate
            while multiple <= limit do
                is_prime[multiple] = false
                multiple = multiple + candidate
            end''', '''            for multiple = candidate * candidate, limit, candidate do
                is_prime[multiple] = false
            end''')
        yield 'for_and_integer_bound', replace('''    local candidate = 3
    while candidate * candidate <= limit do''', '''    for candidate = 3, math.floor(math.sqrt(limit)), 2 do''').replace('''            local multiple = candidate * candidate
            while multiple <= limit do
                is_prime[multiple] = false
                multiple = multiple + candidate
            end''', '''            for multiple = candidate * candidate, limit, candidate do
                is_prime[multiple] = false
            end''').replace('        candidate = candidate + 2\n', '')
    elif name == 'merge_sort':
        yield 'local_table_move', 'local move = table.move\n' + source.replace('table.move(', 'move(')
        helper = '''local move = table.move

local function merge_range(values, scratch, first, last)
    if first >= last then return end
    local middle = (first + last) // 2
    merge_range(values, scratch, first, middle)
    merge_range(values, scratch, middle + 1, last)
    move(values, first, last, first, scratch)
    local i, j, out = first, middle + 1, first
    while i <= middle and j <= last do
        if scratch[i] <= scratch[j] then
            values[out] = scratch[i]
            i = i + 1
        else
            values[out] = scratch[j]
            j = j + 1
        end
        out = out + 1
    end
    if i <= middle then move(scratch, i, middle, out, values) end
    if j <= last then move(scratch, j, last, out, values) end
end

local function merge_sort(values)
    local n = #values
    if n <= 1 then return values end
    local result = move(values, 1, n, 1, {})
    merge_range(result, {}, 1, n)
    return result
end

'''
        yield 'shared_scratch', helper + source[source.index('local values = {}'):]
    elif name == 'n_queens':
        yield 'local_abs', 'local abs = math.abs\n' + source.replace('math.abs(', 'abs(')
        yield 'diagonal_arithmetic', replace('math.abs(placed - column) == distance', 'placed - column == distance or column - placed == distance')
    elif name == 'longest_common_subsequence':
        yield 'decode_right', replace('    local previous = {}', '''    local right_bytes = {}
    for index = 1, right_length do right_bytes[index] = byte(right, index) end
    local previous = {}''').replace('byte(right, j)', 'right_bytes[j]')
        yield 'decode_both', replace('    local previous = {}', '''    local left_bytes = {}
    for index = 1, left_length do left_bytes[index] = byte(left, index) end
    local right_bytes = {}
    for index = 1, right_length do right_bytes[index] = byte(right, index) end
    local previous = {}''').replace('byte(right, j)', 'right_bytes[j]').replace('byte(left, i)', 'left_bytes[i]')
    elif name == 'matrix_multiply':
        yield 'omit_overwritten_zeroes', replace('''        for column = 1, size do
            result_row[column] = 0
        end
''', '')
    elif name == 'sudoku':
        yield 'separate_box_scan', replace('''                    if grid[row + k] == digit or grid[column + k * 9] == digit or
                            grid[box + (k // 3) * 9 + k % 3] == digit then''', '''                    if grid[row + k] == digit or grid[column + k * 9] == digit then''').replace('''                if valid then
                    grid[slot] = digit''', '''                if valid then
                    for box_row = box, box + 18, 9 do
                        if grid[box_row] == digit or grid[box_row + 1] == digit or
                                grid[box_row + 2] == digit then
                            valid = false
                            break
                        end
                    end
                end
                if valid then
                    grid[slot] = digit''')
        yield 'bulk_board_copy', replace('''    local board = {}
    for index = 0, 80 do
        board[index + 1] = puzzle[index + 1]
    end''', '    local board = table.move(puzzle, 1, 81, 1, {})')
    elif name == 'dijkstra':
        numeric = replace('''            for _, edge in ipairs(graph[vertex] or {}) do
                local target = edge[1]''', '''            local edges = graph[vertex]
            for edge_index = 1, edges and #edges or 0 do
                local edge = edges[edge_index]
                local target = edge[1]''')
        yield 'numeric_edges', numeric
        yield 'numeric_edges_manual_pop', numeric.replace('''    local last = table.remove(queue)
    local size = #queue''', '''    local size = #queue
    local last = queue[size]
    queue[size] = nil
    size = size - 1''')
    elif name == 'optimize':
        cached = replace('''        local step = 1.0''', '''        local x1, x2 = x[1], x[2]
        local d1, d2 = direction[1], direction[2]
        local step = 1.0''').replace('direction[1] * direction[1] + direction[2] * direction[2]', 'd1 * d1 + d2 * d2').replace('x[1] + step * direction[1], x[2] + step * direction[2]', 'x1 + step * d1, x2 + step * d2')
        yield 'cache_line_search_coordinates', cached
        yield 'reuse_line_search_trial', cached.replace('''        while true do
            local next = {x1 + step * d1, x2 + step * d2}''', '''        local next = {}
        while true do
            next[1], next[2] = x1 + step * d1, x2 + step * d2''')
    elif name == 'kmp':
        yield 'decode_pattern', replace('    local pi = {}', '''    local pattern_bytes = {}
    for index = 1, length do pattern_bytes[index] = byte(pattern, index) end
    local pi = {}''').replace('byte(pattern, i)', 'pattern_bytes[i]').replace('byte(pattern, j + 1)', 'pattern_bytes[j + 1]')
    elif name == 'k_nucleotide':
        yield 'cached_window_end', replace('''        for i = 1, #sequence - k + 1 do
            local key = sub(sequence, i, i + k - 1)''', '''        local offset = k - 1
        for i = 1, #sequence - offset do
            local key = sub(sequence, i, i + offset)''')
    elif name == 'binary_trees':
        yield 'fixed_arity_children', replace('    return { make_tree(depth - 1), make_tree(depth - 1) }', '''    local left = make_tree(depth - 1)
    local right = make_tree(depth - 1)
    return { left, right }''')
    elif name == 'lru_cache':
        yield 'cache_value', replace('        local present = values[key] ~= nil', '''        local value = values[key]
        local present = value ~= nil''').replace('checksum = checksum + values[key]', 'checksum = checksum + value')
    elif name == 'huffman':
        yield 'gmatch_count', replace('''    for i = 1, #text do
        local ch = sub(text, i, i)''', '''    for ch in text:gmatch(".") do''')
        yield 'gsub_count', replace('''    for i = 1, #text do
        local ch = sub(text, i, i)
        freq[ch] = (freq[ch] or 0) + 1
    end''', '''    for i = 1, #SYMBOLS do
        local ch = sub(SYMBOLS, i, i)
        local _, count = text:gsub(ch, "")
        if count > 0 then freq[ch] = count end
    end''')
        yield 'byte_count', ('local byte = string.byte\n' + replace('local ch = sub(text, i, i)', 'local ch = byte(text, i)').replace('local ch = sub(SYMBOLS, i, i)', 'local ch = byte(SYMBOLS, i)').replace('type(node[2]) == "string"', 'type(node[2]) == "number"'))
    elif name == 'fannkuch':
        yield 'bulk_copy', replace('''        local perm = {}
        for i = 1, n do
            perm[i] = perm1[i]
        end''', '        local perm = table.move(perm1, 1, n, 1, {})')
        yield 'reuse_scratch', replace('    local checksum = 0', '    local perm = {}\n    local checksum = 0').replace('        local perm = {}\n', '')
        yield 'reuse_bulk_scratch', replace('    local checksum = 0', '    local perm = {}\n    local checksum = 0').replace('''        local perm = {}
        for i = 1, n do
            perm[i] = perm1[i]
        end''', '        table.move(perm1, 1, n, 1, perm)')
    elif name == 'n_body':
        yield 'cache_pair_coordinates', replace('''        local bi = bodies[i]
        for j''', '''        local bi = bodies[i]
        local x, y, z, mass = bi.x, bi.y, bi.z, bi.mass
        for j''').replace('local dx = bi.x - bj.x', 'local dx = x - bj.x', 1).replace('local dy = bi.y - bj.y', 'local dy = y - bj.y', 1).replace('local dz = bi.z - bj.z', 'local dz = z - bj.z', 1).replace('local mi = bi.mass * mag', 'local mi = mass * mag')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lua', default='lua')
    parser.add_argument('--runs', type=int, default=5)
    parser.add_argument('--rounds', type=int, default=2)
    parser.add_argument('--from-report', type=Path)
    parser.add_argument('--baseline-report', type=Path,
                        help='generate candidates from archived baseline sources')
    parser.add_argument('--replay-all', action='store_true',
                        help='replay all archived rows rather than select winners')
    parser.add_argument('--cases', nargs='+', choices=ALGORITHM_BENCHMARKS)
    parser.add_argument('--json', type=Path, required=True)
    args = parser.parse_args()
    if args.runs < 3 or args.rounds < 2:
        parser.error('at least 3 runs and 2 rounds required')
    if args.from_report and args.baseline_report:
        parser.error('--from-report and --baseline-report are mutually exclusive')
    if args.replay_all and not args.from_report:
        parser.error('--replay-all requires --from-report')
    runtime = Path(shutil.which(args.lua) or args.lua).resolve()
    digest = lambda data: hashlib.sha256(data).hexdigest()
    report = {'date': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'system': platform.platform(), 'runtime': str(runtime),
              'runtime_sha256': digest(runtime.read_bytes()),
              'lua': subprocess.check_output([str(runtime), '-v'], text=True).strip(),
              'runs': args.runs, 'rounds': args.rounds, 'warmups_per_round': 1,
              'selection': 'all candidates' if not args.from_report or args.replay_all else
              'fastest per algorithm with screening B/A < 0.97 and every round < 1',
              'rows': []}
    work = []
    if args.from_report:
        prior = json.loads(args.from_report.read_text())
        for name in ALGORITHM_BENCHMARKS:
            rows = [r for r in prior['rows'] if r['case'] == name]
            if not rows:
                continue
            if args.replay_all:
                work.extend((name, r['variant'], r['sources']['baseline'],
                             r['sources']['candidate']) for r in rows)
                continue
            best = min(rows, key=lambda r: r['paired_ratio_median'])
            if best['paired_ratio_median'] < 0.97 and all(r < 1 for r in best['round_ratio_medians']):
                work.append((name, best['variant'], best['sources']['baseline'], best['sources']['candidate']))
        report['screening_report_sha256'] = digest(args.from_report.read_bytes())
    else:
        archived = {}
        if args.baseline_report:
            prior = json.loads(args.baseline_report.read_text())
            archived = {row['case']: row['sources']['baseline'] for row in prior['rows']}
            report['baseline_report_sha256'] = digest(args.baseline_report.read_bytes())
        for name in ALGORITHM_BENCHMARKS:
            if args.cases and name not in args.cases:
                continue
            source = archived[name] if args.baseline_report else (HERE / (name + '.lua')).read_text()
            for variant, candidate in candidates(name, source):
                work.append((name, variant, source, candidate))
    if args.cases:
        work = [row for row in work if row[0] in args.cases]
    manifest = json.loads((HERE / 'semantics.json').read_text())
    with tempfile.TemporaryDirectory(prefix='tapas-lua-audit-') as folder:
        paths = {key: Path(folder) / (key + '.lua') for key in ('baseline', 'candidate')}
        for name, variant, baseline, candidate in work:
            sources = {'baseline': baseline, 'candidate': candidate}
            for key, source in sources.items():
                paths[key].write_text(source)
            rounds = []
            for round_index in range(args.rounds):
                samples = {key: [] for key in paths}
                for repetition in range(args.runs + 1):
                    order = list(paths)
                    if (round_index + repetition) % 2:
                        order.reverse()
                    for key in order:
                        lines = subprocess.check_output([str(runtime), str(paths[key])], text=True, timeout=120).splitlines()
                        if len(lines) != 2 or int(lines[0]) != manifest[name]['checksum'] or int(lines[1]) <= 0:
                            raise RuntimeError(f'{name}/{variant}/{key}: invalid output {lines!r}')
                        if repetition:
                            samples[key].append(int(lines[1]))
                rounds.append(summarize(samples))
            combined = {key: [x for r in rounds for x in r['samples_ns'][key]] for key in paths}
            row = {'case': name, 'variant': variant, 'sources': sources,
                   'sources_sha256': {k: digest(v.encode()) for k, v in sources.items()},
                   'checksum': manifest[name]['checksum'], 'rounds': rounds,
                   'round_ratio_medians': [r['paired_ratio_median'] for r in rounds],
                   **summarize(combined)}
            report['rows'].append(row)
            args.json.parent.mkdir(parents=True, exist_ok=True)
            args.json.write_text(json.dumps(report, indent=2) + '\n')
            print(f'{name}/{variant}: {row["paired_ratio_median"]:.3f}; rounds ' +
                  ', '.join(f'{r:.3f}' for r in row['round_ratio_medians']), flush=True)
    if digest(runtime.read_bytes()) != report['runtime_sha256']:
        raise RuntimeError('Lua runtime changed during audit')


if __name__ == '__main__':
    main()
