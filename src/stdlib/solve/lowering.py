"""Captured RuleIR to canonical, solver-independent linear/Boolean IR."""
import operator
from dataclasses import dataclass
from model_ir import Unsupported, Linear, Formula, Variable, Query, SourceConstraint, linear, atom, negate, junction


class Record(dict):
    """Snapshot contents plus runtime identity; Dictionary equality is identity."""
    def __init__(self, values, identity=None):
        super().__init__(values)
        self.identity = identity


@dataclass
class Symbol:
    expr: object
    kind: str
    members: object = None


@dataclass
class RuleRef:
    index: int


@dataclass
class Applied:
    rule: int
    args: list


@dataclass
class Native:
    name: str


@dataclass
class Domain:
    kind: str
    values: object


class RuleLowerer:
    def __init__(self, request):
        self.request = request
        self.rules = request['rules']
        self.stack = []
        self.variables = []
        self.term_steps = 0
        self.constraints = []
        self.completed_sources = ()

    def decode(self, value):
        if not isinstance(value, dict):
            return value
        if 'rule' in value:
            return RuleRef(value['rule'])
        if 'instance' in value:
            return Applied(value['instance'], [self.decode(v) for v in value['args']])
        if 'native' in value:
            return Native(value['native'])
        if 'range' in value:
            return Domain('range', value['range'])
        if 'points' in value:
            return Domain('points', [self.decode(v) for v in value['points']])
        if 'list' in value:
            return [self.decode(v) for v in value['list']]
        if 'dict' in value:
            return Record(((self.decode(k), self.decode(v)) for k, v in value['dict']), value.get('identity'))
        if 'type' in value:
            return ('type', value['type'])
        return value  # An explicit unsupported marker is rejected when used.

    def variable(self, typ, name):
        kind = typ['kind']
        if kind not in ('int', 'bool', 'enum') or kind == 'enum' and not typ['members']:
            raise Unsupported(f'parameter {name}: unsupported Type {kind}')
        index = len(self.variables)
        self.variables.append(Variable(name, kind, tuple(typ.get('members', ()))))
        value = Linear(((index, 1),))
        return Symbol(atom('==', value - 1) if kind == 'bool' else value, kind, typ.get('members'))

    def literal(self, value):
        if isinstance(value, Applied):
            value = self.rule(value.rule, value.args)
        if type(value) is bool:
            return value
        if isinstance(value, Symbol) and value.kind == 'bool':
            return value.expr
        raise Unsupported('Bool or RuleInstance required in logical expression')

    def negate(self, value):
        result = negate(self.literal(value))
        return result if type(result) is bool else Symbol(result, 'bool')

    def logic(self, values, conjunction):
        result = junction('and' if conjunction else 'or', [self.literal(v) for v in values])
        return result if type(result) is bool else Symbol(result, 'bool')

    @staticmethod
    def integer(value):
        if type(value) is int:
            if not -(2**63) < value < 2**63 - 1:
                raise Unsupported('integer constant exceeds safe backend limits')
            return value
        if isinstance(value, Symbol) and value.kind == 'int':
            return value.expr
        raise Unsupported('integer arithmetic requires Int operands')

    def compare(self, op, left, right):
        if isinstance(left, Record) or isinstance(right, Record):
            if op not in ('==', '!='):
                raise Unsupported('ordered record comparison')
            if not isinstance(left, Record) or not isinstance(right, Record):
                return op == '!='
            if left.identity is None or right.identity is None:
                raise Unsupported('record equality requires captured runtime identity')
            equal = left.identity == right.identity
            return equal if op == '==' else not equal
        if isinstance(left, Applied) or isinstance(right, Applied):
            raise Unsupported('RuleInstance equality is not logical equivalence')
        if isinstance(left, Symbol) and left.kind == 'enum':
            if op not in ('==', '!='):
                raise Unsupported('ordered Enum comparisons are not supported')
            if isinstance(right, Symbol):
                if right.kind != 'enum':
                    return op == '!='
                # Different declarations may use different encodings.
                if left.members == right.members:
                    return self.reified(op, left.expr, right.expr)
                if op == '!=':
                    return self.negate(self.compare('==', left, right))
                return self.logic([
                    self.logic([self.compare('==', left, member), self.compare('==', right, member)], True)
                    for member in left.members if member in right.members
                ], False)
            if type(right) is not str or right not in left.members:
                return op == '!='
            return self.reified(op, left.expr, left.members.index(right))
        if isinstance(right, Symbol) and right.kind == 'enum':
            return self.compare(op, right, left)
        if isinstance(left, Symbol) or isinstance(right, Symbol):
            kind_l = left.kind if isinstance(left, Symbol) else ('bool' if type(left) is bool else 'int' if type(left) is int else 'other')
            kind_r = right.kind if isinstance(right, Symbol) else ('bool' if type(right) is bool else 'int' if type(right) is int else 'other')
            if kind_l != kind_r or kind_l not in ('int', 'bool'):
                if op in ('==', '!='):
                    return op == '!='
                raise Unsupported('incompatible comparison operands')
            if kind_l == 'bool' and op not in ('==', '!='):
                raise Unsupported('ordered Bool comparison')
            a = left.expr if isinstance(left, Symbol) else left
            b = right.expr if isinstance(right, Symbol) else right
            return self.reified(op, a, b)
        if type(left) not in (int, bool, str) or type(right) not in (int, bool, str):
            raise Unsupported('comparison of non-scalar values')
        if type(left) is not type(right):
            if op in ('==', '!='):
                return op == '!='
            raise Unsupported('mixed comparison Types')
        return {'==': operator.eq, '!=': operator.ne, '<': operator.lt, '<=': operator.le,
                '>': operator.gt, '>=': operator.ge}[op](left, right)

    def reified(self, op, left, right):
        if isinstance(left, Formula) or isinstance(right, Formula) or type(left) is bool or type(right) is bool:
            equal = junction('or', [junction('and', [left, right]), junction('and', [negate(left), negate(right)])])
            result = equal if op == '==' else negate(equal)
        else:
            result = atom(op, linear(left) - linear(right))
        return result if type(result) is bool else Symbol(result, 'bool')

    def membership(self, value, domain):
        if isinstance(domain, Domain):
            if domain.kind == 'range':
                if not (type(value) is int or isinstance(value, Symbol) and value.kind == 'int'):
                    return False
                return self.logic([self.compare('>=', value, domain.values[0]),
                                   self.compare('<=', value, domain.values[1])], True)
            points = domain.values
        elif isinstance(domain, list):
            points = domain
        elif isinstance(domain, dict) and 'unsupported' not in domain:
            points = list(domain)
        else:
            raise Unsupported('unsupported membership domain')
        return self.logic([self.compare('==', value, point) for point in points], False)

    def call(self, target, args):
        if isinstance(target, RuleRef):
            return Applied(target.index, args)
        if isinstance(target, Native):
            if target.name == 'rules::range' and len(args) == 2 and all(type(a) is int for a in args):
                if args[0] > args[1]:
                    raise Unsupported('reversed range would raise in the checker')
                return Domain('range', args)
            if target.name == 'rules::points' and args and isinstance(args[0], tuple) and args[0][0] == 'type':
                typ = args[0][1]
                for value in args[1:]:
                    if isinstance(value, Symbol) or not self.matches(value, typ):
                        raise Unsupported('points requires fixed members matching its Type')
                return Domain('points', args[1:])
        raise Unsupported('arbitrary function calls are not executed by solve')

    @staticmethod
    def matches(value, typ):
        if isinstance(value, Symbol):
            if value.kind == 'enum' and typ['kind'] == 'string':
                return True
            return (value.kind == typ['kind'] and
                    (value.kind != 'enum' or set(value.members).issubset(typ['members'])))
        if typ['kind'] == 'record':
            return (isinstance(value, Record) and 'fields' in typ and
                    all((optional and name not in value) or
                        (name in value and RuleLowerer.matches(value[name], field_type))
                        for name, field_type, optional in typ['fields']))
        return ((typ['kind'] == 'int' and type(value) is int) or
                (typ['kind'] == 'bool' and type(value) is bool) or
                (typ['kind'] == 'string' and type(value) is str) or
                (typ['kind'] == 'enum' and type(value) is str and value in typ['members']))

    def rule(self, index, args):
        if index in self.stack or len(self.stack) >= 64:
            raise Unsupported('recursive Rule dependency')
        rule = self.rules[index]
        if len(args) != len(rule['parameters']):
            raise Unsupported('nested Rule argument count mismatch')
        for parameter, value in zip(rule['parameters'], args):
            typ = rule['terms'][str(parameter)]['type']
            if typ['kind'] not in ('int', 'bool', 'string', 'enum', 'record'):
                raise Unsupported('parameter Type is not supported by the first compiler')
            if not self.matches(value, typ):
                raise Unsupported('argument Type is not guaranteed to match the Rule signature')
        self.stack.append(index)
        frame = Frame(self, rule, args)
        try:
            # Rule-local initializers execute even if all their uses are short-circuited.
            # Older source IR omits unused declarations: reject those conservatively.
            locals_by_binding = {}
            for key, term in rule['terms'].items():
                if term['kind'] == 'Construct' and isinstance(term['payload'], str) and term['payload'].startswith('local:'):
                    locals_by_binding.setdefault(term['payload'], int(key))
            if len(locals_by_binding) != rule.get('local_count', 0):
                raise Unsupported('unused or unrepresented local declarations require fuller query preparation')
            for key in locals_by_binding.values():
                frame.term(key)
            conditions = []
            sources = []
            for ordinal, item in enumerate(rule['items'], 1):
                value = frame.term(item['term'])
                if item['kind'] == 'Requirement':
                    if isinstance(value, RuleRef):
                        value = Applied(value.index, [frame.term(a) for a in item['arguments']])
                    if not isinstance(value, Applied):
                        raise Unsupported('Requirement target is not a Rule application')
                if item['kind'] == 'Implication':
                    guard = self.literal(value)
                    if guard is False:
                        conditions.append(True)
                        continue
                    body = self.logic([frame.term(a) for a in item['arguments']], True)
                    value = self.logic([self.negate(value), body], False)
                predicate = self.literal(value)
                if isinstance(value, Applied):
                    # Only a directly required positive application can be split.
                    # Calls below OR/NOT/implication remain part of the outer predicate.
                    sources.extend(self.completed_sources)
                else:
                    text = describe_term(rule, item['term'], values=frame.cache)
                    if item['kind'] == 'Implication':
                        text += ' implies (' + ', '.join(describe_term(rule, a, values=frame.cache) for a in item['arguments']) + ')'
                    label = f"condition {ordinal} [term {item['term']}]: " + text
                    sources.append(SourceConstraint(predicate, label, index, text))
                conditions.append(Symbol(predicate, 'bool') if type(predicate) is not bool else predicate)
            result = self.logic(conditions, True)
            self.completed_sources = tuple(sources)
            if len(self.stack) == 1:
                self.constraints = sources
            return result
        finally:
            self.stack.pop()

    def lower(self):
        lo, hi = self.request['integer_min'], self.request['integer_max']
        if type(lo) is not int or type(hi) is not int or not -(2**63) < lo <= hi < 2**63 - 1:
            raise ValueError('invalid configured integer domain')
        root = self.rules[0]
        bound_args = self.request['bindings']
        args = ([self.decode(a) for a in bound_args] if bound_args is not None else
                [self.variable(root['terms'][str(p)]['type'], parameter_name(root, p, i)) for i, p in enumerate(root['parameters'])])
        predicate = self.literal(self.rule(0, args))
        issue = ''
        def integer_fields(value, typ, name):
            if typ['kind'] == 'int' and type(value) is int:
                yield name, value
            elif typ['kind'] == 'record' and isinstance(value, Record):
                for field, field_type, _ in typ.get('fields', ()):
                    if field in value:
                        yield from integer_fields(value[field], field_type, name + '::' + field)
        if bound_args is not None:
            for i, value in enumerate(args):
                parameter = root['parameters'][i]
                name = parameter_name(root, parameter, i)
                for path, integer in integer_fields(value, root['terms'][str(parameter)]['type'], name):
                    if not lo <= integer <= hi:
                        issue = f'bound parameter {path} = {integer} is outside configured Int domain [{lo}, {hi}]'
                        predicate = False
                        self.constraints.append(SourceConstraint(False, issue))
                        break
                if issue:
                    break
        bindings = tuple((parameter_name(root, p, i), args[i]) for i, p in enumerate(root['parameters'])) if bound_args is not None else ()
        return Query(tuple(self.variables), predicate, bound_args is not None, (lo, hi), tuple(self.constraints), issue, bindings)



def lower(request):
    """Prepare a solver-independent query from an already captured RuleIR snapshot."""
    return RuleLowerer(request).lower()


def parameter_name(rule, index, ordinal):
    payload = rule['terms'][str(index)].get('payload')
    return payload if type(payload) is str else f'p{ordinal}'


def describe_term(rule, index, depth=0, values=None):
    """Bounded display of original RuleIR, never evaluation of user code."""
    if depth >= 6:
        return '...'
    value = values.get(index) if values is not None else None
    if isinstance(value, Native):
        return value.name
    if isinstance(value, tuple) and value and value[0] == 'type':
        return value[1].get('kind', 'Type').capitalize()
    node = rule['terms'].get(str(index), {})
    kind, op = node.get('kind'), node.get('payload')
    children = node.get('arguments', [])
    def child(i):
        return describe_term(rule, i, depth + 1, values)
    if kind == 'Parameter':
        return str(op)[:80] if type(op) is str else f'parameter#{index}'
    if kind in ('Constant', 'Capture'):
        value = node.get('value') if kind == 'Capture' else op
        if type(value) in (int, bool, str):
            return repr(value)[:100]
        if isinstance(value, dict) and 'range' in value:
            return 'range' + repr(value['range'])[:100]
        if isinstance(value, dict) and 'native' in value:
            return value['native']
        if isinstance(value, dict) and 'type' in value:
            return value['type'].get('kind', 'Type')
        if isinstance(value, dict) and 'points' in value:
            return 'points' + repr(value['points'])[:100]
        return f'{kind.lower()}#{index}'
    if kind in ('And', 'Or', 'In') or kind == 'Intrinsic' and op in ('==', '!=', '<', '<=', '>', '>=', '+', '-', '*'):
        operator = {'And': 'and', 'Or': 'or', 'In': 'in'}.get(kind, op)
        return ('(' + f' {operator} '.join(child(c) for c in children[:2]) + ')')[:240]
    if kind == 'Intrinsic' and op in ('member', 'index') and len(children) == 2:
        key = rule['terms'].get(str(children[1]), {})
        if op == 'member' and key.get('kind') == 'Constant' and type(key.get('payload')) is str:
            return child(children[0]) + '::' + key['payload']
        return child(children[0]) + '[' + child(children[1]) + ']'
    if kind == 'Not':
        return 'not ' + child(children[0])
    if kind == 'Construct' and isinstance(op, str) and op.startswith('local:'):
        return child(children[0])
    if kind == 'Call':
        return child(op['term']) + "(" + ', '.join(child(c) for c in children[:4]) + ')'
    return f'{kind}#{index}'


class Frame:
    def __init__(self, compiler, rule, args):
        self.c = compiler
        self.rule = rule
        self.bindings = dict(zip(rule['parameters'], args))
        self.cache = {}
        self.active = set()

    def term(self, index):
        if index in self.cache:
            return self.cache[index]
        if index in self.active:
            raise Unsupported('cyclic expression')
        self.c.term_steps += 1
        if self.c.term_steps > 100000:
            raise Unsupported('normalized model size limit exceeded')
        self.active.add(index)
        try:
            value = self.evaluate(self.rule['terms'][str(index)], index)
            self.cache[index] = value
            return value
        except Unsupported as exc:
            raise Unsupported(f"{exc}; Rule {self.c.stack[-1] if self.c.stack else 0}, term {index} ({self.rule['terms'][str(index)]['kind']})") from None
        finally:
            self.active.remove(index)

    def evaluate(self, node, index):
        c = self.c
        kind, op, children = node['kind'], node.get('payload'), node['arguments']
        if kind == 'Parameter':
            if index not in self.bindings:
                raise Unsupported('unbound Parameter identity')
            return self.bindings[index]
        if kind == 'Capture':
            return c.decode(node['value'])
        if kind == 'Constant':
            return c.decode(op)
        if kind in ('And', 'Or'):
            left = self.term(children[0])
            literal = c.literal(left)
            if type(literal) is bool and literal == (kind == 'Or'):
                return literal
            return c.logic([left, self.term(children[1])], kind == 'And')
        if kind == 'Not':
            return c.negate(self.term(children[0]))
        if kind == 'In':
            domain = self.term(children[1])
            return c.membership(self.term(children[0]), domain)
        if kind == 'Call':
            args = [self.term(a) for a in children]
            return c.call(self.term(op['term']), args)
        if kind == 'Construct' and op.startswith('local:'):
            return self.term(children[0])
        if kind == 'Construct' and op == 'list':
            return [self.term(a) for a in children]
        if kind == 'Construct' and op == 'dictionary':
            keys = [self.term(children[i]) for i in range(0, len(children), 2)]
            if any(type(key) not in (str, int, bool) for key in keys):
                raise Unsupported('constructed record requires fixed scalar keys')
            return Record((key, self.term(children[2*i+1])) for i, key in enumerate(keys))
        if kind == 'Intrinsic':
            values = [self.term(a) for a in children]
            if op in ('member', 'index'):
                if len(values) != 2 or isinstance(values[1], Symbol):
                    raise Unsupported('symbolic or multi-dimensional index')
                try:
                    return values[0][values[1]]
                except (KeyError, IndexError, TypeError):
                    raise Unsupported('field or index is unavailable') from None
            if op in ('==', '!=', '<', '<=', '>', '>='):
                return c.compare(op, *values)
            if op in ('pos', 'neg', '+', '-', '*'):
                integers = [c.integer(v) for v in values]
                if op == '*' and all(isinstance(v, Symbol) for v in values):
                    raise Unsupported('variable multiplication is not in the first compiler')
                expression = (integers[0] if op == 'pos' else -integers[0] if op == 'neg' else
                              integers[0] + integers[1] if op == '+' else integers[0] - integers[1] if op == '-' else
                              integers[0] * integers[1])
                if all(type(v) is int for v in values):
                    return c.integer(expression)
                return Symbol(expression, 'int')
        raise Unsupported(f'unsupported {kind} expression: {op}')
