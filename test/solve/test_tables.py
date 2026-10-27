"""Native relation encoding against exhaustive finite membership semantics."""
import itertools
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'src/stdlib/solve'))
from ortools.sat.python import cp_model
from lowering import RuleLowerer, Symbol, Table
from model_ir import Query, atom, junction, negate, variable_indices
from presolve import presolve
from cp_sat import compile_model, hold


class TableTests(unittest.TestCase):
    def lowerer(self):
        return RuleLowerer({'rules': []})

    def test_native_table_and_joint_row_membership(self):
        lowerer = self.lowerer()
        x = lowerer.variable({'kind': 'int'}, 'x')
        y = lowerer.variable({'kind': 'int'}, 'y')
        relation = Table(('x', 'y'), ((1, 2), (2, 1), (1, 2)))
        predicate = lowerer.literal(lowerer.membership([x, y], relation))
        query = Query(tuple(lowerer.variables), predicate, integer_domain=(0, 3))
        compiled = compile_model(query, presolve(query), cp_model)
        constraints = compiled.model.proto.constraints
        self.assertEqual(len(constraints), 1)
        self.assertTrue(constraints[0].has_table())
        self.assertFalse(constraints[0].table.negated)
        for a, b in itertools.product(range(4), repeat=2):
            bindings = [atom('==', x.expr - a), atom('==', y.expr - b)]
            for negative in (False, True):
                p = negate(predicate) if negative else predicate
                q = Query(query.variables, junction('and', [p, *bindings]), integer_domain=(0, 3))
                status = hold(compile_model(q, presolve(q), cp_model))[0]
                self.assertEqual(status == 'sat', ((a, b) in relation.rows) != negative)

    def test_bool_expression_and_enum_encoding(self):
        lowerer = self.lowerer()
        item = lowerer.variable({'kind': 'enum', 'members': ['B', 'A']}, 'item')
        x = lowerer.variable({'kind': 'int'}, 'x')
        positive = lowerer.compare('>', x, 0)
        relation = Table(('item', 'positive'), (('A', True), ('B', False), ('outside', True)))
        predicate = lowerer.literal(lowerer.membership([item, positive], relation))
        self.assertEqual(variable_indices(predicate), {0, 1})
        for name, integer in itertools.product(('A', 'B'), (-1, 0, 1)):
            bindings = [lowerer.literal(lowerer.compare('==', item, name)), atom('==', x.expr - integer)]
            q = Query(tuple(lowerer.variables), junction('and', [predicate, *bindings]), integer_domain=(-2, 2))
            self.assertEqual(hold(compile_model(q, presolve(q), cp_model))[0] == 'sat',
                             (name, integer > 0) in relation.rows)

    def test_full_reification_under_or_and_not(self):
        lowerer = self.lowerer()
        x = lowerer.variable({'kind': 'int'}, 'x')
        flag = lowerer.variable({'kind': 'bool'}, 'flag')
        member = lowerer.literal(lowerer.membership([x], Table(('x',), ((1,), (3,)))))
        # flag is exactly the truth of membership, not merely an enable gate.
        equivalence = lowerer.literal(lowerer.compare('==', flag, Symbol(member, 'bool')))
        for value, enabled in itertools.product(range(4), (False, True)):
            bindings = [atom('==', x.expr - value), lowerer.literal(lowerer.compare('==', flag, enabled))]
            q = Query(tuple(lowerer.variables), junction('and', [equivalence, *bindings]), integer_domain=(0, 3))
            self.assertEqual(hold(compile_model(q, presolve(q), cp_model))[0] == 'sat',
                             enabled == (value in (1, 3)))

    def test_shape_type_and_empty_relation(self):
        lowerer = self.lowerer()
        relation = Table(('x', 'y'), ((1, True),))
        for row in ([True, True], [1, 1], [1], 1, {'x': 1}):
            self.assertIs(lowerer.membership(row, relation), False)
        self.assertIs(lowerer.membership([1, True], relation), True)
        self.assertIs(lowerer.membership([1], Table(('x',), ())), False)
        with self.assertRaises(ValueError):
            Table(('x',), ((1,), (True,)))

    def test_affine_row_and_independent_enum_column_orders(self):
        lowerer = self.lowerer()
        x = lowerer.variable({'kind': 'int'}, 'x')
        a = lowerer.variable({'kind': 'enum', 'members': ['A', 'B']}, 'a')
        b = lowerer.variable({'kind': 'enum', 'members': ['B', 'A']}, 'b')
        row = [Symbol(x.expr + 1, 'int'), a, b]
        relation = Table(('number', 'a', 'b'), ((2, 'A', 'B'),))
        predicate = lowerer.literal(lowerer.membership(row, relation))
        query = Query(tuple(lowerer.variables), predicate, integer_domain=(0, 3))
        result = hold(compile_model(query, presolve(query), cp_model))
        self.assertEqual(result[0], 'sat')
        self.assertEqual(result[3], [1, 'A', 'B'])


if __name__ == '__main__':
    unittest.main()
