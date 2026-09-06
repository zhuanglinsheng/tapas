"""Budgeted, sound domain propagation over Query, independent of CP-SAT.

Domains describe necessary conditions, not exact solution projections. Infinity
is symbolic; finite calculations use Python integers, never floating point.
"""
from dataclasses import dataclass
from math import inf
from model_ir import Linear, atom, junction


@dataclass(frozen=True)
class IntDomain:
    intervals: tuple = ((-inf, inf),)

    @classmethod
    def union(cls, domains, limit=256):
        intervals = sorted(pair for domain in domains for pair in domain.intervals)
        merged = []
        for lo, hi in intervals:
            if lo > hi:
                continue
            if merged and lo <= merged[-1][1] + 1:
                merged[-1] = (merged[-1][0], max(hi, merged[-1][1]))
            else:
                merged.append((lo, hi))
        if len(merged) > limit:
            merged = [(merged[0][0], merged[-1][1])]  # Safe over-approximation.
        return cls(tuple(merged))

    def intersect(self, other):
        result = []
        i = j = 0
        while i < len(self.intervals) and j < len(other.intervals):
            a, b = self.intervals[i], other.intervals[j]
            lo, hi = max(a[0], b[0]), min(a[1], b[1])
            if lo <= hi:
                result.append((lo, hi))
            if a[1] < b[1]:
                i += 1
            else:
                j += 1
        return IntDomain(tuple(result))

    @property
    def lower(self):
        return self.intervals[0][0]

    @property
    def upper(self):
        return self.intervals[-1][1]

    @property
    def finite(self):
        return bool(self.intervals) and self.lower != -inf and self.upper != inf

    @property
    def singleton(self):
        return self.lower if self.intervals and self.lower == self.upper else None


@dataclass(frozen=True)
class PresolveOptions:
    max_rounds: int = 32
    max_steps: int = 100000
    max_intervals: int = 256


@dataclass(frozen=True)
class PresolveResult:
    domains: tuple
    predicate: object
    infeasible: bool
    steps: int
    rounds: int
    converged: bool


def expression_bounds(expression, domains, omit=None):
    lo = hi = expression.constant
    for index, coefficient in expression.coefficients:
        if index == omit:
            continue
        domain = domains[index]
        lo += coefficient * (domain.lower if coefficient > 0 else domain.upper)
        hi += coefficient * (domain.upper if coefficient > 0 else domain.lower)
    return lo, hi


def truth(predicate, domains):
    if type(predicate) is bool:
        return predicate
    expression = predicate.args[0]
    lo, hi = expression_bounds(expression, domains)
    if predicate.kind == '<=':
        return True if hi <= 0 else False if lo > 0 else None
    if lo == hi == 0:
        return predicate.kind == '=='
    if lo > 0 or hi < 0:
        return predicate.kind == '!='
    return None


def simplify(predicate, domains):
    if type(predicate) is bool:
        return predicate
    if predicate.kind in ('and', 'or'):
        return junction(predicate.kind, [simplify(v, domains) for v in predicate.args])
    expression = predicate.args[0]
    constant = expression.constant
    terms = []
    for i, c in expression.coefficients:
        fixed = domains[i].singleton
        if fixed is None:
            terms.append((i, c))
        else:
            constant += c * fixed
    simplified = atom(predicate.kind, Linear(tuple(terms), constant))
    known = truth(simplified, domains)
    return simplified if known is None else known


class Propagator:
    def __init__(self, options):
        self.options = options
        self.steps = 0
        self.exhausted = False

    def spend(self, units=1):
        if self.steps + units > self.options.max_steps:
            self.exhausted = True
            return False
        self.steps += units
        return True

    def inequality(self, expression, domains):
        # sum(a_i*x_i) + c <= 0. Minimum contribution of all other
        # variables gives a necessary upper (a_i>0) or lower (a_i<0) bound.
        contributions = [(i, c, c * (domains[i].lower if c > 0 else domains[i].upper))
                         for i, c in expression.coefficients]
        unbounded = sum(v == -inf for _, _, v in contributions)
        finite_sum = expression.constant + sum(v for _, _, v in contributions if v != -inf)
        if not unbounded and finite_sum > 0:
            return None
        # Use one snapshot of contributions for linear-time propagation. Later
        # rounds carry newly tightened bounds back through the same constraint.
        for index, coefficient, contribution in contributions:
            if not self.spend():
                break
            if unbounded - (contribution == -inf):
                continue
            minimum = finite_sum - (contribution if contribution != -inf else 0)
            limit = -minimum
            if coefficient > 0:
                restriction = IntDomain(((-inf, limit // coefficient),))
            else:
                restriction = IntDomain(((-((-limit) // coefficient), inf),))
            domains[index] = domains[index].intersect(restriction)
            if not domains[index].intervals:
                return None
        return domains

    def propagate(self, predicate, domains):
        if not self.spend():
            return domains
        if type(predicate) is bool:
            return domains if predicate else None
        if predicate.kind == 'and':
            for child in predicate.args:
                domains = self.propagate(child, domains)
                if domains is None:
                    return None
            return domains
        if predicate.kind == 'or':
            branches = []
            for child in predicate.args:
                if not self.spend(max(1, len(domains))):
                    return domains
                branch = self.propagate(child, domains.copy())
                if branch is not None:
                    branches.append(branch)
            if not branches:
                return None
            return [IntDomain.union([branch[i] for branch in branches], self.options.max_intervals).intersect(domain)
                    for i, domain in enumerate(domains)]
        known = truth(predicate, domains)
        if known is not None:
            return domains if known else None
        expression = predicate.args[0]
        if predicate.kind == '<=':
            return self.inequality(expression, domains)
        if predicate.kind == '==':
            domains = self.inequality(expression, domains)
            return None if domains is None else self.inequality(-expression, domains)
        if len(expression.coefficients) == 1:
            index, coefficient = expression.coefficients[0]
            if expression.constant % coefficient == 0:
                excluded = -expression.constant // coefficient
                domain = IntDomain(((-inf, excluded - 1), (excluded + 1, inf)))
                domains[index] = domains[index].intersect(domain)
                if not domains[index].intervals:
                    return None
        return domains


def difference_constraints(predicate, domains, engine):
    """Bellman-Ford on mandatory x-y<=c constraints, including a zero anchor.

    A negative cycle proves infeasibility even without finite initial domains.
    Distances from/to zero imply upper/lower bounds. Disjunction children are
    deliberately excluded: no branch-local edge is unconditionally valid.
    """
    zero = len(domains)
    edges = []

    def collect(node):
        if not engine.spend():
            return
        if type(node) is bool:
            return
        if node.kind == 'and':
            for child in node.args:
                collect(child)
        elif node.kind in ('<=', '=='):
            expressions = (node.args[0], -node.args[0]) if node.kind == '==' else (node.args[0],)
            for expression in expressions:
                terms = expression.coefficients
                if len(terms) > 2 or any(abs(c) != 1 for _, c in terms):
                    continue
                positive = [i for i, c in terms if c == 1]
                negative = [i for i, c in terms if c == -1]
                if len(positive) <= 1 and len(negative) <= 1 and terms:
                    edges.append((negative[0] if negative else zero,
                                  positive[0] if positive else zero, -expression.constant))

    collect(predicate)
    for i, domain in enumerate(domains):
        if domain.lower != -inf:
            edges.append((i, zero, -domain.lower))
        if domain.upper != inf:
            edges.append((zero, i, domain.upper))
    if not edges:
        return domains
    count = zero + 1

    def distances(graph, initial, detect_cycle=False):
        distance = initial
        for iteration in range(count):
            changed = False
            for a, b, cost in graph:
                if not engine.spend():
                    return distance, False
                if distance[a] != inf and distance[a] + cost < distance[b]:
                    distance[b] = distance[a] + cost
                    changed = True
            if not changed:
                return distance, False
        return distance, detect_cycle

    _, cycle = distances(edges, [0] * count, True)
    if cycle:
        return None
    upper, _ = distances(edges, [inf] * zero + [0])
    lower, _ = distances([(b, a, c) for a, b, c in edges], [inf] * zero + [0])
    for i, domain in enumerate(domains):
        domains[i] = domain.intersect(IntDomain(((-lower[i], upper[i]),)))
        if not domains[i].intervals:
            return None
    return domains


def presolve(query, options=PresolveOptions()):
    """Return proven domains and an equivalent residual predicate within them.

    A budget stop only weakens precision. No temporary search bound enters this
    pass, so every contradiction it reports applies to the query and its declared domain.
    """
    if options.max_rounds < 1 or options.max_steps < 1 or options.max_intervals < 1:
        raise ValueError('presolve budgets must be positive')
    domains = [IntDomain(((0, 1),)) if v.kind == 'bool' else
               IntDomain(((0, len(v.members) - 1),)) if v.kind == 'enum' else IntDomain((query.integer_domain,)) if query.integer_domain is not None else IntDomain()
               for v in query.variables]
    engine = Propagator(options)
    initial = tuple(domains)
    domains = difference_constraints(query.predicate, domains, engine)
    if domains is None:
        return PresolveResult(initial, False, True, engine.steps, 0, True)
    converged = False
    for iteration in range(1, options.max_rounds + 1):
        before = domains.copy()
        domains = engine.propagate(query.predicate, domains)
        if domains is None:
            return PresolveResult(tuple(before), False, True, engine.steps, iteration, True)
        if domains == before:
            converged = not engine.exhausted
            break
        if engine.exhausted:
            break
    residual = simplify(query.predicate, domains)
    return PresolveResult(tuple(domains), residual, residual is False, engine.steps, iteration, converged)
