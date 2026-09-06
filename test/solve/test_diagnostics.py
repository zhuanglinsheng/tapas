"""Conflict explanations must be sufficient under original, not inferred domains."""
import itertools
import unittest
from unittest.mock import patch
from test_backend import Graph, backend
from lowering import lower
from diagnostics import presolve_conflict, solver_conflict, explain_unsat
from presolve import presolve
from test_presolve import evaluate
from ortools.sat.python import cp_model


class DiagnosticTests(unittest.TestCase):
    def assert_sufficient(self, query, conflict):
        self.assertNotEqual(conflict.method, 'unavailable', conflict)
        self.assertTrue(conflict.conditions)
        lo, hi = query.integer_domain
        for values in itertools.product(range(lo, hi + 1), repeat=len(query.variables)):
            self.assertFalse(all(evaluate(c.predicate, values) for c in conflict.conditions), (conflict, values))

    def test_presolve_removes_irrelevant_conditions_with_proof(self):
        g = Graph(['int'], bound=10)
        x = g.parameters[0]
        g.terms[str(x)]['payload'] = 'quantity'
        g.condition(g.compare('>=', x, g.constant(8)))
        g.condition(g.compare('<=', x, g.constant(4)))
        g.condition(g.compare('!=', x, g.constant(-1)))
        query = lower(g.request())
        conflict = presolve_conflict(query)
        self.assert_sufficient(query, conflict)
        self.assertEqual(len(conflict.conditions), 2)
        reason = str(backend.solve(g.request())[4])
        self.assertIn('quantity >= 8', reason)
        self.assertIn('quantity <= 4', reason)
        self.assertNotIn('quantity != -1', reason)
        self.assertEqual(backend.solve(g.request())[2], '')

    def test_assumptions_never_reuse_inferred_bounds(self):
        g = Graph(['int'])
        x = g.parameters[0]
        for value in (0, 1):
            g.condition(g.compare('==', x, g.constant(value)))
        query = lower(g.request())
        conflict = solver_conflict(query, cp_model)
        self.assertEqual(len(conflict.conditions), 2)
        self.assert_sufficient(query, conflict)

    def test_cpsat_only_contradiction_has_core(self):
        g = Graph(['int'] * 3, bound=2)
        x, y, z = g.parameters
        for op, a, b in [('==', x, y), ('==', y, z), ('!=', x, z)]:
            g.condition(g.compare(op, a, b))
        query = lower(g.request())
        self.assertFalse(presolve(query).infeasible)
        self.assert_sufficient(query, solver_conflict(query, cp_model))
        status, _, reason, _, rows = backend.solve(g.request())
        self.assertEqual(status, 'unsat')
        self.assertEqual(reason, '')
        self.assertEqual(len(rows), 3)

    def test_or_is_one_guarded_condition_not_unconditional_branches(self):
        g = Graph(['int'])
        x = g.parameters[0]
        g.condition(g.node('Or', [g.compare('==', x, g.constant(0)), g.compare('==', x, g.constant(1))]))
        g.condition(g.compare('==', x, g.constant(2)))
        query = lower(g.request())
        self.assert_sufficient(query, solver_conflict(query, cp_model))
        self.assertEqual(len(query.constraints), 2)

    def test_domain_and_bound_argument_reasons(self):
        g = Graph(['int'])
        x = g.parameters[0]
        g.terms[str(x)]['payload'] = 'count'
        g.condition(g.compare('==', x, g.constant(4)))
        reason = str(backend.solve(g.request())[4])
        self.assertEqual(backend.solve(g.request())[2], '')
        self.assertIn('count == 4', reason)
        self.assertIn('count = 4 is outside', backend.solve(g.request([4]))[2])
        self.assertTrue(backend.solve(g.request([2]))[4])

    def test_budget_and_diagnostic_failure_preserve_unsat(self):
        g = Graph(['int'])
        x = g.parameters[0]
        for v in (0, 1):
            g.condition(g.compare('==', x, g.constant(v)))
        query = lower(g.request())
        self.assert_sufficient(query, presolve_conflict(query, time_limit=0, max_checks=0))
        with patch('diagnostics.presolve_conflict', side_effect=RuntimeError('failure')):
            result = backend.solve(g.request())
            self.assertEqual(result[0], 'unsat')
            self.assertIn('details unavailable', result[2])
        with patch('diagnostics.solver_conflict', side_effect=RuntimeError('failure')):
            self.assertIn('details unavailable', explain_unsat(query, cp_model)[0])

    def test_positive_nested_origins_and_negative_context(self):
        inner = Graph(['int'])
        y = inner.parameters[0]
        inner.condition(inner.compare('==', y, inner.constant(1)))
        for negated in (False, True):
            outer = Graph(['int'])
            x = outer.parameters[0]
            target = outer.node('Constant', payload={'rule': 1})
            call = outer.node('Call', [x], {'term': target})
            outer.condition(outer.node('Not', [call]) if negated else call)
            outer.condition(outer.compare('==', x, outer.constant(1 if negated else 2)))
            request = outer.request()
            request['rules'].append(inner.request()['rules'][0])
            query = lower(request)
            self.assertEqual(query.constraints[0].rule, 0 if negated else 1)
            self.assert_sufficient(query, presolve_conflict(query))
            rows = backend.solve(request)[4]
            self.assertEqual({row['rule'] for row in rows}, {0} if negated else {0, 1})

    def test_conflict_list_is_not_truncated_to_four_rows(self):
        g = Graph(['int'] * 6)
        for i in range(6):
            g.condition(g.compare('<' if i < 5 else '<=', g.parameters[i], g.parameters[(i + 1) % 6]))
        result = backend.solve(g.request())
        self.assertEqual(result[0], 'unsat')
        self.assertEqual(result[2], '')
        self.assertEqual(len(result[4]), 6)

    def test_unsupported_has_term_context(self):
        g = Graph(['int'])
        x = g.parameters[0]
        product = g.node('Intrinsic', [x, x], '*', 'int')
        g.condition(g.compare('==', product, g.constant(4)))
        status, _, reason, _, rows = backend.solve(g.request())
        self.assertEqual(status, 'unsupported')
        self.assertIn('term', reason)
        self.assertIn('variable multiplication', reason)


if __name__ == '__main__':
    unittest.main()
