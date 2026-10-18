#!/usr/bin/env python3
"""Check complete DP rows, histograms and heap output against independent oracles."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
RIGHT = 'DCBAABCDDCBAABCD'
PREFIX = 'ABCDAB'


def lcs_row(left, right):
    # A full 2-D DP oracle differs from the benchmark's fixed single-row update.
    table = [[0] * (len(right) + 1) for _ in range(len(left) + 1)]
    for i, ch in enumerate(left, 1):
        for j, other in enumerate(right, 1):
            table[i][j] = (table[i - 1][j - 1] + 1 if ch == other else
                           max(table[i - 1][j], table[i][j - 1]))
    return table[-1]


def verify(tapas, lua='lua'):
    commands = {'tap': [str(Path(tapas).resolve())], 'py': [sys.executable], 'lua': [lua]}
    results = []
    with tempfile.TemporaryDirectory(prefix='tapas-hotspot-check-') as scratch:
        for name, repetitions in [('dp_row', (1, 2, 3, 4, 9)),
                                  ('dict_counting', (0, 5, 4096, 4101))]:
            for n in repetitions:
                if name == 'dp_row':
                    row = lcs_row(PREFIX + 'ABCD'[(n - 1) % 4], RIGHT)
                    total = sum(lcs_row(PREFIX + 'ABCD'[r % 4], RIGHT)[-1] for r in range(n))
                    expected = row + [total + sum(j * value for j, value in enumerate(row))]
                else:
                    counts = Counter(i % 4096 for i in range(n))
                    expected = [len(counts), *(counts[i] for i in range(4096))]
                    expected.append(sum(count * (key + 1) for key, count in counts.items()))
                for extension, command in commands.items():
                    original = HERE / 'hotspots' / (name + '.' + extension)
                    source = original.read_text()
                    if name == 'dp_row':
                        if extension == 'tap':
                            source = source.replace('bench(40000, right)', f'bench({n}, right)')
                            source = source.replace('    print(result)',
                                '    for (let j in 0 to 17) { print(current[j]) }\n    print(result)')
                        elif extension == 'py':
                            source = source.replace("bench(40_000,", f'bench({n},')
                            source = source.replace('    return result + sum',
                                '    for value in current:\n        print(value)\n    return result + sum')
                        else:
                            source = source.replace('bench(40000,', f'bench({n},')
                            source = source.replace('    print(result)',
                                '    for j = 0, 16 do print(current[j]) end\n    print(result)')
                    else:
                        source = source.replace('bench(500000, keys)', f'bench({n}, keys)')
                        source = source.replace('bench(500000, input_keys)', f'bench({n}, input_keys)')
                        source = source.replace('bench(500_000, keys)', f'bench({n}, keys)')
                        if extension == 'tap':
                            source = source.replace('    var total = 0',
                                '    print(counts.len())\n    for (let key in 0 to 4096) { print(idx_or(counts, key, 0)) }\n    var total = 0')
                        elif extension == 'py':
                            source = source.replace('    print(sum(',
                                '    print(len(counts))\n    for key in range(4096):\n        print(counts.get(key, 0))\n    print(sum(')
                        else:
                            source = source.replace('    local result = 0',
                                '    local size = 0\n    for _ in pairs(counts) do size = size + 1 end\n    print(size)\n    for key = 0, 4095 do print(counts[key] or 0) end\n    local result = 0')
                    if source == original.read_text():
                        raise RuntimeError(f'probe instrumentation did not match: {original}')
                    # Distinct paths avoid reusing an automatic bytecode cache between inputs.
                    path = Path(scratch) / f'{name}_{n}.{extension}'
                    path.write_text(source)
                    modes = [[], ['--matched']] if extension == 'py' and name == 'dp_row' else [[]]
                    for flags in modes:
                        lines = subprocess.check_output([*command, str(path), *flags], text=True,
                                                        timeout=120).splitlines()
                        values = list(map(int, lines))
                        if values[:-1] != expected or values[-1] < 0:
                            raise RuntimeError(f'{name}/{extension}/{flags}, n={n}: wrong row or histogram: {values[:20]!r} != {expected[:20]!r}')
                        results.append({'case': name, 'language': extension, 'n': n,
                                        'matched': bool(flags), 'values_checked': len(expected),
                                        'source_sha256': hashlib.sha256(original.read_bytes()).hexdigest()})
        # Include symbols outside the timed fixture's original four-letter alphabet.
        for index, text in enumerate(('abacabad', 'abcdexxyzabcd')):
            counts = Counter(text)
            alphabet = 'abcdexyz'
            weighted = sum(counts[ch] * weight for ch, weight in zip('abcd', (1, 3, 5, 7)))
            expected = [len(counts), *(counts[ch] for ch in alphabet), weighted]
            for extension, command in commands.items():
                original = HERE / 'hotspots' / ('string_count.' + extension)
                source = original.read_text()
                if extension == 'tap':
                    first_line = source.splitlines()[0]
                    source = source.replace(first_line, f'let text = {text!r}', 1)
                    source = source.replace('    let result =',
                        "    print(counts.len())\n    for (let ch in 'abcdexyz') { print(idx_or(counts, ch, 0)) }\n    let result =")
                elif extension == 'py':
                    source = source.replace("bench('abacabad' * 62_500)", f'bench({text!r})')
                    source = source.replace("    return counts['a']",
                        "    print(len(counts))\n    for ch in 'abcdexyz':\n        print(counts.get(ch, 0))\n    return counts['a']")
                else:
                    source = source.replace('bench(string.rep("abacabad", 62500))', f'bench({text!r})')
                    source = source.replace('    local result =',
                        '    local size = 0\n    for _ in pairs(counts) do size = size + 1 end\n    print(size)\n    for ch in ("abcdexyz"):gmatch(".") do print(counts[ch] or 0) end\n    local result =')
                path = Path(scratch) / f'string_count_{index}.{extension}'
                path.write_text(source)
                modes = [[], ['--matched']] if extension == 'py' else [[]]
                for flags in modes:
                    actual = list(map(int, subprocess.check_output(
                        [*command, str(path), *flags], text=True, timeout=120).splitlines()))
                    if actual[:-1] != expected or actual[-1] < 0:
                        raise RuntimeError(f'string_count/{extension}/{flags}: wrong complete histogram')
                    results.append({'case': 'string_count', 'language': extension,
                                    'input': text, 'matched': bool(flags),
                                    'values_checked': len(expected),
                                    'source_sha256': hashlib.sha256(original.read_bytes()).hexdigest()})
    return results


def verify_heap(tapas, lua='lua'):
    commands = {'tap': [str(Path(tapas).resolve())], 'py': [sys.executable], 'lua': [lua]}
    results = []
    inputs = ([], [1], [4, -3, 4, 0], list(range(17)), list(range(17, 0, -1)), [7] * 32)
    with tempfile.TemporaryDirectory(prefix='tapas-heap-check-') as scratch:
        for index, values in enumerate(inputs):
            expected = sorted(values)
            expected = expected + [sum(position * value for position, value in enumerate(expected, 1))]
            for extension, command in commands.items():
                original = HERE / 'hotspots' / ('heap_callback.' + extension)
                source = original.read_text()
                if extension == 'tap':
                    source = source.replace('bench(64, values)', f'bench(1, {[[v, i] for i, v in enumerate(values)]})')
                    source = source.replace(
                        'checksum = checksum + heap::pop(queue, priority_less)[0] * (position + 1)',
                        'let value = heap::pop(queue, priority_less)[0]\n'
                        '            print(value)\n'
                        '            checksum = checksum + value * (position + 1)')
                elif extension == 'py':
                    source = source.replace(
                        'bench(64, [((i * 73 + 19) % 1009 - 500, i) for i in range(256)])',
                        f'bench(1, {[(v, i) for i, v in enumerate(values)]})')
                    source = source.replace('checksum += heapq.heappop(queue).priority * position',
                        'value = heapq.heappop(queue).priority\n'
                        '            print(value)\n'
                        '            checksum += value * position')
                else:
                    literal = '{' + ', '.join('{' + f'{v}, {i}' + '}' for i, v in enumerate(values)) + '}'
                    source = source.replace('bench(64, values)', f'bench(1, {literal})')
                    source = source.replace('checksum = checksum + pop(queue)[1] * position',
                        'local value = pop(queue)[1]; print(value); checksum = checksum + value * position')
                path = Path(scratch) / f'heap_{index}.{extension}'
                path.write_text(source)
                actual = list(map(int, subprocess.check_output(
                    [*command, str(path)], text=True, timeout=120).splitlines()))
                if actual[:-1] != expected or actual[-1] < 0:
                    raise RuntimeError(f'heap_callback/{extension}: wrong sorted output')
                results.append({'case': 'heap_callback', 'language': extension,
                                'input': values, 'matched': False,
                                'values_checked': len(expected),
                                'source_sha256': hashlib.sha256(original.read_bytes()).hexdigest()})
    return results


def verify_heap_payloads(tapas, lua='lua'):
    """Tied priorities must not compare opaque payloads or lose records."""
    commands = {'tap': [str(Path(tapas).resolve())], 'py': [sys.executable], 'lua': [lua]}
    results = []
    inputs = ([], [0], [2, -1, 2, 0, -1], [7] * 8)
    with tempfile.TemporaryDirectory(prefix='tapas-heap-payload-check-') as scratch:
        for index, priorities in enumerate(inputs):
            expected = sorted(priorities)
            checksum = sum(position * value for position, value in enumerate(expected, 1))
            records = [[value, {'id': i}] for i, value in enumerate(priorities)]
            expected_records = Counter((value, i) for i, value in enumerate(priorities))
            for extension, command in commands.items():
                original = HERE / 'hotspots' / ('heap_callback.' + extension)
                source = original.read_text()
                if extension == 'tap':
                    source = source.replace('bench(64, values)', f'bench(1, {records!r})')
                    source = source.replace(
                        'checksum = checksum + heap::pop(queue, priority_less)[0] * (position + 1)',
                        'let item = heap::pop(queue, priority_less)\n'
                        '            print(item[0])\n'
                        '            print(item[1]::id)\n'
                        '            checksum = checksum + item[0] * (position + 1)')
                elif extension == 'py':
                    source = source.replace(
                        'bench(64, [((i * 73 + 19) % 1009 - 500, i) for i in range(256)])',
                        f'bench(1, {records!r})')
                    source = source.replace(
                        'checksum += heapq.heappop(queue).priority * position',
                        'item = heapq.heappop(queue)\n'
                        '            print(item.priority)\n'
                        "            print(item.payload['id'])\n"
                        '            checksum += item.priority * position')
                else:
                    literal = '{' + ', '.join(
                        '{' + f'{value}, {{id = {i}}}' + '}'
                        for i, value in enumerate(priorities)) + '}'
                    source = source.replace('bench(64, values)', f'bench(1, {literal})')
                    source = source.replace(
                        'checksum = checksum + pop(queue)[1] * position',
                        'local item = pop(queue); print(item[1]); print(item[2].id); '
                        'checksum = checksum + item[1] * position')
                path = Path(scratch) / f'heap_payload_{index}.{extension}'
                path.write_text(source)
                actual = list(map(int, subprocess.check_output(
                    [*command, str(path)], text=True, timeout=120).splitlines()))
                pairs = list(zip(actual[:-2:2], actual[1:-2:2]))
                if (len(actual) != 2 * len(priorities) + 2 or
                        [value for value, _ in pairs] != expected or
                        Counter(pairs) != expected_records or
                        actual[-2] != checksum or actual[-1] < 0):
                    raise RuntimeError(f'heap_callback/{extension}: wrong priority/payload output')
                results.append({'case': 'heap_callback_payload', 'language': extension,
                                'input': priorities, 'values_checked': len(actual) - 1,
                                'source_sha256': hashlib.sha256(original.read_bytes()).hexdigest()})
    return results


def verify_heap_order(tapas, lua='lua'):
    """Check entire records, including deliberately reversed equal-priority ties."""
    commands = {'tap': [str(Path(tapas).resolve())], 'py': [sys.executable], 'lua': [lua]}
    inputs = ([], [(1, 4)], [(7, 3), (7, 1), (7, 2)],
              [(2, 9), (-1, 5), (2, 0), (-1, 2), (2, 0)],
              [(i % 3, 31 - i) for i in range(32)])
    results = []
    with tempfile.TemporaryDirectory(prefix='tapas-heap-order-check-') as scratch:
        for index, records in enumerate(inputs):
            ordered = sorted(records)
            expected = [item for record in ordered for item in record]
            expected.append(sum(position * record[0] for position, record in enumerate(ordered, 1)))
            for extension, command in commands.items():
                original = HERE / 'hotspots' / ('heap_order.' + extension)
                source = original.read_text()
                if extension == 'tap':
                    source = source.replace('bench(64, values)', f'bench(1, {[list(r) for r in records]})')
                    source = source.replace(
                        'checksum = checksum + heap::pop(queue)[0] * (position + 1)',
                        'let value = heap::pop(queue)\n'
                        '            print(value[0])\n            print(value[1])\n'
                        '            checksum = checksum + value[0] * (position + 1)')
                elif extension == 'py':
                    source = source.replace(
                        'bench(64, [((i * 73 + 19) % 31 - 15, i) for i in range(256)])',
                        f'bench(1, {records!r})')
                    source = source.replace('checksum += heapq.heappop(queue)[0] * position',
                        'value = heapq.heappop(queue)\n'
                        '            print(value[0])\n            print(value[1])\n'
                        '            checksum += value[0] * position')
                else:
                    literal = '{' + ', '.join('{' + f'{a}, {b}' + '}' for a, b in records) + '}'
                    source = source.replace('bench(64, values)', f'bench(1, {literal})')
                    source = source.replace('checksum = checksum + pop(queue)[1] * position',
                        'local value = pop(queue); print(value[1]); print(value[2]); '
                        'checksum = checksum + value[1] * position')
                path = Path(scratch) / f'heap_order_{index}.{extension}'
                path.write_text(source)
                actual = list(map(int, subprocess.check_output(
                    [*command, str(path)], text=True, timeout=120).splitlines()))
                if actual[:-1] != expected or actual[-1] < 0:
                    raise RuntimeError(f'heap_order/{extension}: wrong complete records: {actual}')
                results.append({'case': 'heap_order', 'language': extension,
                                'input': records, 'values_checked': len(expected),
                                'source_sha256': hashlib.sha256(original.read_bytes()).hexdigest()})
    return results


def verify_list_sort(tapas, lua='lua'):
    commands = {'tap': [str(Path(tapas).resolve())], 'py': [sys.executable], 'lua': [lua]}
    inputs = ([], [3], [4, -3, 4, 0], list(range(17)), list(range(17, 0, -1)))
    results = []
    with tempfile.TemporaryDirectory(prefix='tapas-sort-check-') as scratch:
        for index, values in enumerate(inputs):
            ordered = sorted(values)
            expected = ordered + [sum(i * value for i, value in enumerate(ordered, 1))]
            for extension, command in commands.items():
                original = HERE / 'hotspots' / ('list_sort.' + extension)
                source = original.read_text()
                if extension == 'tap':
                    source = source.replace('bench(16, values)', f'bench(1, {values})')
                    source = source.replace('        sort(ordered)',
                        '        sort(ordered)\n        for (let value in ordered) { print(value) }')
                elif extension == 'py':
                    source = source.replace('bench(16, values)', f'bench(1, {values})')
                    source = source.replace('        ordered.sort()',
                        '        ordered.sort()\n        for value in ordered:\n            print(value)')
                else:
                    literal = '{' + ', '.join(map(str, values)) + '}'
                    source = source.replace('bench(16, values)', f'bench(1, {literal})')
                    source = source.replace('        table.sort(ordered)',
                        '        table.sort(ordered)\n        for _, value in ipairs(ordered) do print(value) end')
                path = Path(scratch) / f'sort_{index}.{extension}'
                path.write_text(source)
                actual = list(map(int, subprocess.check_output(
                    [*command, str(path)], text=True, timeout=120).splitlines()))
                if actual[:-1] != expected or actual[-1] < 0:
                    raise RuntimeError(f'list_sort/{extension}: wrong complete sorted output')
                results.append({'case': 'list_sort', 'language': extension,
                                'input': values, 'values_checked': len(expected),
                                'source_sha256': hashlib.sha256(original.read_bytes()).hexdigest()})
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('tapas', type=Path)
    parser.add_argument('--lua', default='lua')
    parser.add_argument('--json', type=Path, default=HERE / 'hotspots/kernel_checks.json')
    args = parser.parse_args()
    checks = verify(args.tapas, args.lua)
    checks.extend(verify_heap(args.tapas, args.lua))
    checks.extend(verify_heap_payloads(args.tapas, args.lua))
    checks.extend(verify_heap_order(args.tapas, args.lua))
    checks.extend(verify_list_sort(args.tapas, args.lua))
    args.json.parent.mkdir(parents=True, exist_ok=True)
    args.json.write_text(json.dumps({
        'tapas_sha256': hashlib.sha256(args.tapas.read_bytes()).hexdigest(),
        'verifier_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'checks': checks,
    }, indent=2) + '\n')
    print(f'{len(checks)} complete-row/histogram/heap probes passed')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
