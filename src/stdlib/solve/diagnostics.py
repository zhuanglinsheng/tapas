"""Bounded infeasibility explanations using original constraints and base domains.

Never reuse domains inferred from conditions being toggled: they would retain
hidden constraints and make an assumption core misleading.
"""
from dataclasses import dataclass
from time import monotonic
from model_ir import Query, SourceConstraint, junction
from presolve import presolve, PresolveOptions
from cp_sat import Encoder


@dataclass(frozen=True)
class Conflict:
    method: str
    conditions: tuple = ()
    detail: str = ''


def original_conditions(query):
    if query.constraints:
        return query.constraints
    nodes = query.predicate.args if getattr(query.predicate, 'kind', None) == 'and' else (query.predicate,)
    return tuple(SourceConstraint(node, f'condition {i}') for i, node in enumerate(nodes, 1))


def presolve_conflict(query, time_limit=0.5, max_checks=32):
    """Shrink an already-proven contradiction; inconclusive checks keep conditions."""
    core = list(original_conditions(query))
    deadline = monotonic() + time_limit
    checks = index = 0
    while index < len(core) and checks < max_checks and monotonic() < deadline:
        candidate = core[:index] + core[index + 1:]
        trial = Query(query.variables, junction('and', [c.predicate for c in candidate]),
                      query.bound_instance, query.integer_domain)
        analysis = presolve(trial, PresolveOptions(max_rounds=16, max_steps=10000))
        checks += 1
        if analysis.infeasible:
            core = candidate
        else:
            index += 1
    return Conflict('presolve', tuple(core))


def solver_conflict(query, cp, time_limit=1.5):
    """Extract a sufficient assumption core from a separate, unsimplified model."""
    conditions = original_conditions(query)
    if len(conditions) > 256:
        return Conflict('unavailable', detail='diagnostic condition budget exceeded')
    model = cp.CpModel()
    variables = []
    for variable in query.variables:
        lo, hi = ((0, 1) if variable.kind == 'bool' else (0, len(variable.members) - 1)
                  if variable.kind == 'enum' else query.integer_domain)
        variables.append(model.new_int_var(lo, hi, variable.name))
    encoder = Encoder(cp, model, variables)
    mapping = {}
    for i, condition in enumerate(conditions):
        gate = model.new_bool_var(f'assume{i}')
        model.add_bool_or([encoder.literal(condition.predicate)]).only_enforce_if(gate)
        model.add_assumption(gate)
        mapping[gate.index] = condition
    invalid = model.validate()
    if invalid:
        return Conflict('unavailable', detail='diagnostic model exceeds backend limits')
    solver = cp.CpSolver()
    solver.parameters.num_search_workers = 1
    solver.parameters.max_time_in_seconds = time_limit
    status = solver.solve(model)
    if status != cp.INFEASIBLE:
        return Conflict('unavailable', detail='diagnostic solver did not establish a conflict')
    indices = solver.sufficient_assumptions_for_infeasibility()
    if any(index not in mapping for index in indices):
        return Conflict('unavailable', detail='diagnostic core contains unmapped assumptions')
    return Conflict('CP-SAT assumptions', tuple(mapping[i] for i in indices))


def explain_unsat(query, cp=None):
    """Return (fallback reason, structured rows); never alter the proven status."""
    if query.bound_issue:
        return query.bound_issue, []
    try:
        conflict = presolve_conflict(query) if cp is None else solver_conflict(query, cp)
        if conflict.method == 'unavailable':
            return 'Conflict details unavailable: ' + conflict.detail, []
        if len(conflict.conditions) > 256:
            return 'Conflict details unavailable: condition budget exceeded', []
        rows = [{'rule': c.rule, 'condition': c.condition or c.label} for c in conflict.conditions]
        if any(len(row['condition'].encode('utf-8')) > 4000 for row in rows):
            return 'Conflict details unavailable: condition text exceeds protocol limit', []
        return '', rows
    except Exception:
        return 'Conflict details unavailable: diagnostic analysis failed', []
