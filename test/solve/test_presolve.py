"""Semantic checks: sound domains and residual equivalence, not implementation mirroring."""
import itertools
import random
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parents[2] / 'src/stdlib/solve'))
from model_ir import Linear, Variable, Query, atom, negate, junction
from presolve import presolve, PresolveOptions
from cp_sat import compile_model, hold
from ortools.sat.python import cp_model


def evaluate(node, values):
    if type(node) is bool:
        return node
    if node.kind == 'and':
        return all(evaluate(child, values) for child in node.args)
    if node.kind == 'or':
        return any(evaluate(child, values) for child in node.args)
    expr = node.args[0]
    n = expr.constant + sum(c * values[i] for i, c in expr.coefficients)
    return n <= 0 if node.kind == '<=' else n == 0 if node.kind == '==' else n != 0


def inside(domain, value):
    return any(lo <= value <= hi for lo, hi in domain.intervals)


X, Y = Linear(((0, 1),)), Linear(((1, 1),))
VARIABLES = (Variable('x', 'int'), Variable('y', 'int'))


class PresolveTests(unittest.TestCase):
    def analyze(self, node, variables=VARIABLES, **options):
        return presolve(Query(variables, node), PresolveOptions(**options))

    def test_affine_bounds_and_integer_rounding(self):
        predicate = junction('and', [atom('>=', Y - 3), atom('<=', Y - 8), atom('==', 2 * X + Y - 12)])
        result = self.analyze(predicate)
        self.assertFalse(result.infeasible)
        self.assertEqual(result.domains[0].intervals, ((2, 4),))
        self.assertEqual(result.domains[1].intervals, ((4, 8),))
        negative = self.analyze(junction('and', [atom('<=', -2 * X + 5), atom('<=', X - 6)]))
        self.assertEqual(negative.domains[0].intervals, ((3, 6),))
        self.assertTrue(self.analyze(atom('==', 6 * X + 4 * Y - 3)).infeasible)

    def test_points_holes_negation_and_disjunction(self):
        points = junction('or', [atom('==', X - n) for n in (-7, 3, 1000000000001)])
        result = self.analyze(junction('and', [points, atom('!=', X - 3)]))
        self.assertEqual(result.domains[0].intervals, ((-7, -7), (1000000000001, 1000000000001)))
        outside = self.analyze(negate(junction('and', [atom('>=', X), atom('<=', X - 3)])))
        self.assertEqual(outside.domains[0].intervals, ((-float('inf'), -1), (4, float('inf'))))
        self.assertTrue(self.analyze(junction('and', [points, atom('==', X - 2)])).infeasible)

    def test_guarded_bounds_do_not_escape_their_branch(self):
        # (y == 1 implies x <= 3) and y == 0 permits x=8.
        predicate = junction('and', [junction('or', [atom('!=', Y - 1), atom('<=', X - 3)]), atom('==', Y)])
        result = self.analyze(predicate)
        self.assertTrue(inside(result.domains[0], 8))
        self.assertFalse(result.domains[0].finite)
        # An unconstrained feasible branch prevents deriving a global upper bound.
        result = self.analyze(junction('or', [atom('<=', X - 3), atom('==', Y)]))
        self.assertFalse(result.domains[0].finite)

    def test_difference_cycles_and_transitive_anchors(self):
        self.assertTrue(self.analyze(junction('and', [atom('<', X - Y), atom('<=', Y - X)])).infeasible)
        result = self.analyze(junction('and', [atom('<=', X - Y - 3), atom('<=', Y - 7)]))
        self.assertEqual(result.domains[0].upper, 10)
        # The same contradictory constraints in alternative branches are feasible.
        self.assertFalse(self.analyze(junction('or', [atom('<', X - Y), atom('<=', Y - X)])).infeasible)

    def test_configured_domain_is_a_constraint_before_presolve(self):
        query = Query((VARIABLES[0],), atom('==', X - 1000000000001), integer_domain=(-3, 3))
        self.assertTrue(presolve(query).infeasible)
        self.assertEqual(hold(compile_model(query, presolve(query), cp_model))[:2], ('unsat', 'configured'))
        query = Query((VARIABLES[0],), atom('==', X - 1000000000001), integer_domain=(0, 2000000000000))
        compiled = compile_model(query, presolve(query), cp_model)
        self.assertEqual(hold(compiled), ('sat', 'configured', '', [1000000000001]))
        self.assertEqual(len(compiled.model.proto.variables), 0)
        query = Query(VARIABLES, atom('==', X + Y - 5), integer_domain=(0, 3))
        result = presolve(query)
        self.assertEqual([d.intervals for d in result.domains], [((2, 3),), ((2, 3),)])
        query = Query((VARIABLES[0],), True)
        with self.assertRaises(ValueError):
            compile_model(query, presolve(query), cp_model)

    def test_domain_widening_budget_and_exact_unsat_with_unused_integer(self):
        points = junction('or', [atom('==', X - n) for n in (-7, 3, 20)])
        result = self.analyze(points, max_intervals=1)
        self.assertEqual(result.domains[0].intervals, ((-7, 20),))
        for value in range(-8, 22):
            self.assertEqual(evaluate(points, (value, 0)),
                             inside(result.domains[0], value) and evaluate(result.predicate, (value, 0)))
        result = self.analyze(junction('and', [atom('<=', X - Y - 1), atom('<=', Y - X - 1)]), max_steps=1)
        self.assertFalse(result.infeasible)
        self.assertFalse(result.converged)
        query = Query(VARIABLES, False)
        compiled = compile_model(query, presolve(query), cp_model)
        self.assertEqual(hold(compiled)[:2], ('unsat', 'exact'))

    def test_random_formulas_against_exhaustive_semantics(self):
        rng = random.Random(73921)
        window = junction('and', [atom('>=', X + 3), atom('<=', X - 3), atom('>=', Y + 3), atom('<=', Y - 3)])
        assignments = list(itertools.product(range(-3, 4), repeat=2))
        for case in range(160):
            leaves = [atom(rng.choice(['<=', '==', '!=', '>', '<']),
                           rng.randint(-3, 3) * X + rng.randint(-3, 3) * Y + rng.randint(-5, 5)) for _ in range(5)]
            node = junction(rng.choice(['and', 'or']), [leaves[0], negate(leaves[1]),
                            junction(rng.choice(['and', 'or']), leaves[2:])])
            node = junction('and', [window, node])
            solutions = [v for v in assignments if evaluate(node, v)]
            query = Query(VARIABLES, node)
            for options in (PresolveOptions(), PresolveOptions(max_steps=3, max_rounds=1, max_intervals=1)):
                result = presolve(query, options)
                if result.infeasible:
                    self.assertFalse(solutions, case)
                    continue
                for values in assignments:
                    retained = all(inside(d, v) for d, v in zip(result.domains, values))
                    self.assertEqual(evaluate(node, values), retained and evaluate(result.predicate, values), (case, values))
            compiled = compile_model(query, presolve(query), cp_model)
            status, _, _, witness = hold(compiled)
            self.assertEqual(status, 'sat' if solutions else 'unsat', case)
            if solutions:
                self.assertIn(tuple(witness), solutions)


if __name__ == '__main__':
    unittest.main()
