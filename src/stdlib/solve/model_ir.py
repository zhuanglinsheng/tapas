"""Canonical affine expressions and Boolean predicates; no backend dependency."""
from dataclasses import dataclass
from math import gcd
from functools import reduce


class Unsupported(Exception):
    pass


@dataclass(frozen=True)
class Linear:
    coefficients: tuple = ()  # sorted (variable index, nonzero integer coefficient)
    constant: int = 0

    def __add__(self, other):
        other = linear(other)
        terms = dict(self.coefficients)
        for index, coefficient in other.coefficients:
            terms[index] = terms.get(index, 0) + coefficient
        return Linear(tuple(sorted((i, c) for i, c in terms.items() if c)), self.constant + other.constant)

    __radd__ = __add__

    def __neg__(self):
        return self * -1

    def __sub__(self, other):
        return self + -linear(other)

    def __rsub__(self, other):
        return linear(other) + -self

    def __mul__(self, other):
        if type(other) is not int:
            raise Unsupported('only constant multiplication is supported')
        return Linear(tuple((i, c * other) for i, c in self.coefficients if c * other), self.constant * other)

    __rmul__ = __mul__


def linear(value):
    if isinstance(value, Linear):
        return value
    if type(value) is int:
        return Linear((), value)
    raise Unsupported('affine expression requires integers')


@dataclass(frozen=True)
class Formula:
    kind: str  # and, or, <=, ==, !=, table, not_table
    args: tuple


def atom(op, expression):
    if op == '>=':
        return atom('<=', -expression)
    if op == '>':
        return atom('<=', -expression + 1)
    if op == '<':
        return atom('<=', expression + 1)
    if not expression.coefficients:
        k = expression.constant
        return k <= 0 if op == '<=' else k == 0 if op == '==' else k != 0
    # Integer divisibility and gcd normalization also reduce backend overflow risk.
    divisor = reduce(gcd, (abs(c) for _, c in expression.coefficients))
    if op in ('==', '!=') and expression.constant % divisor:
        return op == '!='
    if divisor > 1:
        k = expression.constant
        k = -((-k) // divisor) if op == '<=' else k // divisor
        expression = Linear(tuple((i, c // divisor) for i, c in expression.coefficients), k)
    return Formula(op, (expression,))


def negate(value):
    if type(value) is bool:
        return not value
    if value.kind in ('and', 'or'):
        return junction('or' if value.kind == 'and' else 'and', [negate(v) for v in value.args])
    if value.kind in ('table', 'not_table'):
        return Formula('not_table' if value.kind == 'table' else 'table', value.args)
    return atom({'<=': '>', '==': '!=', '!=': '=='}[value.kind], value.args[0])


def table(expressions, rows, forbidden=False):
    expressions = tuple(expressions)
    rows = tuple(dict.fromkeys(tuple(row) for row in rows))
    if not rows:
        return forbidden
    if all(isinstance(expression, Linear) and not expression.coefficients for expression in expressions):
        found = tuple(expression.constant for expression in expressions) in rows
        return not found if forbidden else found
    return Formula('not_table' if forbidden else 'table', (expressions, rows))


def variable_indices(predicate):
    if type(predicate) is bool:
        return set()
    if predicate.kind in ('and', 'or'):
        return set().union(*(variable_indices(child) for child in predicate.args))
    expressions = predicate.args[0] if predicate.kind in ('table', 'not_table') else (predicate.args[0],)
    result = set()
    for expression in expressions:
        if isinstance(expression, Linear):
            result.update(index for index, _ in expression.coefficients)
        else:
            result.update(variable_indices(expression))
    return result


def junction(kind, values):
    decisive = kind == 'or'
    flattened = []
    for value in values:
        if type(value) is bool:
            if value == decisive:
                return decisive
        elif value.kind == kind:
            flattened.extend(value.args)
        else:
            flattened.append(value)
    unique = tuple(dict.fromkeys(flattened))
    if not unique:
        return not decisive
    if len(unique) == 1:
        return unique[0]
    return Formula(kind, unique)


@dataclass(frozen=True)
class Variable:
    name: str
    kind: str
    members: tuple = ()


@dataclass(frozen=True)
class SourceConstraint:
    predicate: object
    label: str
    rule: int = 0
    condition: str = ""


@dataclass(frozen=True)
class Query:
    variables: tuple
    predicate: object
    bound_instance: bool = False
    integer_domain: object = None
    constraints: tuple = ()
    bound_issue: str = ""
    bindings: tuple = ()
    output_variables: tuple = ()  # Root parameter order; None retains a fixed Table in the VM.

    def __post_init__(self):
        if self.integer_domain is not None:
            lo, hi = self.integer_domain
            if type(lo) is not int or type(hi) is not int or not -(2**63) < lo <= hi < 2**63 - 1:
                raise ValueError('invalid configured integer domain')
