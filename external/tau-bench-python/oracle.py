"""Direct CP-SAT feasibility oracle; input bindings are assumption literals."""
from dataclasses import dataclass
from time import perf_counter
from ortools.sat.python import cp_model


@dataclass
class Answer:
    status: str
    witness: dict
    core: tuple
    build_seconds: float = 0.0
    solve_seconds: float = 0.0
    extract_seconds: float = 0.0
    completion: tuple | None = None


class Oracle:
    def __init__(self, problem, timeout=5.0):
        self.problem, self.timeout = problem, timeout
        self._model = None

    def query(self, bindings):
        started = perf_counter()
        # Reuse the Python model wrapper, but reset the entire proto each time.
        # This is identical to clone's copy/rebuild sequence and retains no
        # assumptions, constraints, or solver state from the previous query.
        if self._model is None:
            self._model = self.problem.model.clone()
        else:
            self._model.proto.copy_from(self.problem.model.proto)
            self._model.rebuild_constant_map()
        model = self._model
        assumptions = {}
        for i, value in bindings:
            if not 0 <= i < len(self.problem.xvars) or value not in self.problem.prior.domains[i]:
                raise ValueError('Binding outside the declared input domain')
            variable = model.get_int_var_from_proto_index(self.problem.xvars[i].index)
            gate = model.new_bool_var(f'bind_{i}')
            model.add(variable == value).only_enforce_if(gate)
            model.add_assumption(gate)
            assumptions[gate.index] = (i, value)
        solver = cp_model.CpSolver()
        solver.parameters.num_search_workers = 1
        solver.parameters.random_seed = 0
        solver.parameters.max_time_in_seconds = self.timeout
        built = perf_counter()
        status = solver.solve(model)
        solved = perf_counter()
        witness, core, completion = {}, (), None
        if status in (cp_model.OPTIMAL, cp_model.FEASIBLE):
            result = 'sat'
            witness = {name: solver.value(model.get_int_var_from_proto_index(var.index))
                       for name, var in self.problem.witness.items()}
            completion = tuple(solver.value(model.get_int_var_from_proto_index(var.index))
                               for var in self.problem.xvars)
        elif status == cp_model.INFEASIBLE:
            result = 'unsat'
            indices = solver.sufficient_assumptions_for_infeasibility()
            if any(i not in assumptions for i in indices):
                raise RuntimeError('Unmapped assumption in native core')
            core = tuple(sorted(assumptions[i] for i in indices))
        elif status == cp_model.MODEL_INVALID:
            raise RuntimeError('Invalid CP-SAT model: ' + model.validate())
        else:
            result = 'unknown'
        return Answer(result, witness, core, built - started, solved - built, perf_counter() - solved, completion)
