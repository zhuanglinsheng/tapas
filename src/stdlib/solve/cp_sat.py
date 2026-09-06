"""CP-SAT adapter for presolved models. Compilation is separate from queries."""
from dataclasses import dataclass
import operator


@dataclass
class CompiledModel:
    cp: object
    model: object
    query: object
    analysis: object
    variables: tuple
    domains: tuple
    @property
    def scope(self):
        return 'configured' if self.query.integer_domain is not None else 'exact'


class Encoder:
    def __init__(self, cp, model, variables):
        self.cp, self.model, self.variables = cp, model, variables
        self.cache = {}

    def expression(self, expression):
        return sum(c * self.variables[i] for i, c in expression.coefficients) + expression.constant

    def comparison(self, predicate, inverted=False):
        operation = {'<=': operator.gt if inverted else operator.le,
                     '==': operator.ne if inverted else operator.eq,
                     '!=': operator.eq if inverted else operator.ne}[predicate.kind]
        return operation(self.expression(predicate.args[0]), 0)

    def literal(self, predicate):
        if type(predicate) is bool:
            return predicate
        if predicate in self.cache:
            return self.cache[predicate]
        value = self.model.new_bool_var(f'b{len(self.cache)}')
        self.cache[predicate] = value
        if predicate.kind in ('and', 'or'):
            children = [self.literal(child) for child in predicate.args]
            negative = [not v if type(v) is bool else v.Not() for v in children]
            if predicate.kind == 'and':
                self.model.add_bool_and(children).only_enforce_if(value)
                self.model.add_bool_or(negative).only_enforce_if(value.Not())
            else:
                self.model.add_bool_or(children).only_enforce_if(value)
                self.model.add_bool_and(negative).only_enforce_if(value.Not())
        else:
            self.model.add(self.comparison(predicate)).only_enforce_if(value)
            self.model.add(self.comparison(predicate, True)).only_enforce_if(value.Not())
        return value

    def require(self, predicate):
        # Assert root conjunctions/linear atoms directly, avoiding redundant gates.
        if type(predicate) is bool:
            if not predicate:
                self.model.add_bool_or([])
        elif predicate.kind == 'and':
            for child in predicate.args:
                self.require(child)
        elif predicate.kind == 'or':
            self.model.add_bool_or([self.literal(child) for child in predicate.args])
        else:
            self.model.add(self.comparison(predicate))


def compile_model(query, analysis, cp_model):
    """Reusable compilation entry point for hold and future optimization queries."""
    model = cp_model.CpModel()
    if analysis.infeasible:
        model.add_bool_or([])
        return CompiledModel(cp_model, model, query, analysis, (), analysis.domains)
    domains = analysis.domains
    if any(not domain.finite for domain in domains):
        raise ValueError('finite domains required for compilation; configure the query before presolve')
    variables = []
    for variable, domain in zip(query.variables, domains):
        fixed = domain.singleton
        variables.append(fixed if fixed is not None else model.new_int_var_from_domain(
            cp_model.Domain.from_intervals(domain.intervals), variable.name))
    Encoder(cp_model, model, variables).require(analysis.predicate)
    return CompiledModel(cp_model, model, query, analysis, tuple(variables), domains)


def hold(compiled, time_limit=5.0):
    """Existence query within the declared query domain."""
    cp, model, scope = compiled.cp, compiled.model, compiled.scope
    invalid = model.validate()
    if invalid:
        return 'error', scope, 'CP-SAT model invalid: ' + invalid, []
    solver = cp.CpSolver()
    solver.parameters.max_time_in_seconds = time_limit
    solver.parameters.num_search_workers = 1
    status = solver.solve(model)
    if status in (cp.OPTIMAL, cp.FEASIBLE):
        values = []
        if not compiled.query.bound_instance:
            for variable, expression in zip(compiled.query.variables, compiled.variables):
                value = int(solver.value(expression))
                values.append(bool(value) if variable.kind == 'bool' else
                              variable.members[value] if variable.kind == 'enum' else value)
        return 'sat', scope, '', values
    if status == cp.INFEASIBLE:
        return 'unsat', scope, '', []
    return 'unknown', scope, 'CP-SAT stopped before deciding feasibility', []
