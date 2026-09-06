"""Framed worker entry point; all query analysis lives in reusable modules."""
import json
import sys
from lowering import lower
from presolve import presolve
from cp_sat import compile_model, hold
from model_ir import Unsupported
from diagnostics import explain_unsat


def solve(request):
    if request.get('version') != 3:
        return 'error', 'configured', 'unsupported solver protocol version', [], []
    try:
        query = lower(request)
        analysis = presolve(query)
        if analysis.infeasible:
            reason, conflicts = explain_unsat(query)
            return 'unsat', 'configured', reason, [], conflicts
        from ortools.sat.python import cp_model
        compiled = compile_model(query, analysis, cp_model)
        result = hold(compiled)
        if result[0] == 'unsat':
            reason, conflicts = explain_unsat(query, cp_model)
            return result[0], result[1], reason, result[3], conflicts
        return (*result, [])
    except ImportError:
        return 'error', 'configured', 'OR-Tools is unavailable: install src/stdlib/solve/requirements.txt into the selected Python environment', [], []
    except Unsupported as exc:
        return 'unsupported', 'configured', str(exc), [], []
    except Exception as exc:
        return 'error', 'configured', f'{type(exc).__name__}: {exc}', [], []


def read_exact(stream, size):
    data = bytearray()
    while len(data) < size:
        chunk = stream.read(size - len(data))
        if not chunk:
            raise EOFError('truncated solver frame')
        data.extend(chunk)
    return bytes(data)


def main():
    source, target = sys.stdin.buffer, sys.stdout.buffer
    while True:
        first = source.read(1)
        if not first:
            return
        size = int.from_bytes(first + read_exact(source, 3), 'big')
        if not 0 < size <= 16 * 1024 * 1024:
            raise ValueError('solver request exceeds size limit')
        request = json.loads(read_exact(source, size))
        status, scope, reason, values, conflicts = solve(request)
        reason = reason.replace('\n', ' ').replace('\r', ' ')
        if len(reason) > 1000:
            reason = reason[:970] + ' ... (diagnostic truncated)'
        lines = ['TAPAS_SOLVE_3', status, scope, reason, str(len(values))]
        for value in values:
            lines.append('b' + str(int(value)) if type(value) is bool else 'i' + str(value) if type(value) is int else 's' + value.encode('utf-8').hex())
        lines.append(str(len(conflicts)))
        for conflict in conflicts:
            lines.extend([str(conflict['rule']), 's' + conflict['condition'].encode('utf-8').hex()])
        reply = ('\n'.join(lines) + '\n').encode('utf-8')
        target.write(len(reply).to_bytes(4, 'big') + reply)
        target.flush()


if __name__ == '__main__':
    main()
