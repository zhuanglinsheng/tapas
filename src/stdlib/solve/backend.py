"""Framed worker entry point; all query analysis lives in reusable modules."""
import json
import sys
from lowering import lower
from presolve import presolve
from cp_sat import compile_model, hold
from model_ir import Unsupported
from diagnostics import explain_unsat


def hold_with_binding_core(query, cp):
    """Solve once with generated root bindings as assumptions."""
    from cp_sat import Encoder
    model = cp.CpModel()
    variables = []
    for variable in query.variables:
        lo, hi = ((0, 1) if variable.kind == 'bool' else
                  (0, len(variable.members) - 1) if variable.kind == 'enum'
                  else query.integer_domain)
        variables.append(model.new_int_var(lo, hi, variable.name))
    encoder = Encoder(cp, model, variables)
    def variable_indices(predicate):
        if type(predicate) is bool:
            return set()
        if predicate.kind in ('and', 'or'):
            result = set()
            for child in predicate.args:
                result.update(variable_indices(child))
            return result
        return {index for index, _ in predicate.args[0].coefficients}

    assumptions = []
    for constraint in query.constraints:
        if constraint.rule != 0:
            continue
        roots = {query.variables[index].name.split('::', 1)[0]
                 for index in variable_indices(constraint.predicate)}
        if len(roots) == 1:
            assumptions.append((constraint, roots.pop()))
    assumption_constraints = {id(pair[0]) for pair in assumptions}
    if not assumptions:
        return ('error', 'configured',
                'binding assumptions are unavailable for this query', [], [])
    hard = [constraint for constraint in query.constraints
            if id(constraint) not in assumption_constraints]
    for constraint in hard:
        encoder.require(constraint.predicate)
    mapping = {}
    for index, (constraint, name) in enumerate(assumptions):
        gate = model.new_bool_var(f'binding{index}')
        model.add_bool_or([encoder.literal(constraint.predicate)]).only_enforce_if(gate)
        model.add_assumption(gate)
        mapping[gate.index] = name
    invalid = model.validate()
    if invalid:
        return 'error', 'configured', 'CP-SAT model invalid: ' + invalid, [], []
    solver = cp.CpSolver()
    solver.parameters.max_time_in_seconds = 5.0
    solver.parameters.num_search_workers = 1
    status = solver.solve(model)
    if status in (cp.OPTIMAL, cp.FEASIBLE):
        values = []
        for variable, expression in zip(query.variables, variables):
            value = int(solver.value(expression))
            values.append(bool(value) if variable.kind == 'bool' else
                          variable.members[value] if variable.kind == 'enum'
                          else value)
        return 'sat', 'configured', '', values, []
    if status == cp.INFEASIBLE:
        indices = solver.sufficient_assumptions_for_infeasibility()
        if any(index not in mapping for index in indices):
            return ('error', 'configured',
                    'binding core contains an unmapped assumption', [], [])
        rows = [{'rule': -1, 'condition': mapping[index]}
                for index in indices]
        return 'unsat', 'configured', '', [], rows
    return ('unknown', 'configured',
            'CP-SAT stopped before deciding feasibility', [], [])


def solve(request):
    if request.get('version') != 3:
        return 'error', 'configured', 'unsupported solver protocol version', [], []
    try:
        query = lower(request)
        if request.get('binding_core', False):
            from ortools.sat.python import cp_model
            return hold_with_binding_core(query, cp_model)
        analysis = presolve(query)
        if analysis.infeasible:
            reason, conflicts = (explain_unsat(query) if request.get('diagnostics', True)
                                 else ('', []))
            return 'unsat', 'configured', reason, [], conflicts
        from ortools.sat.python import cp_model
        compiled = compile_model(query, analysis, cp_model)
        result = hold(compiled)
        if result[0] == 'unsat':
            reason, conflicts = (explain_unsat(query, cp_model)
                                 if request.get('diagnostics', True) else ('', []))
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
