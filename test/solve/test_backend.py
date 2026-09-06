"""Truth-table and finite-domain checks for the actual CP-SAT translation."""
import importlib.util
import itertools
from pathlib import Path
import sys
import json
import subprocess
import unittest

sys.path.insert(0, str(Path(__file__).parents[2] / 'src/stdlib/solve'))

spec = importlib.util.spec_from_file_location('tapas_solve_backend', Path(__file__).parents[2] / 'src/stdlib/solve/backend.py')
backend = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = backend
spec.loader.exec_module(backend)


class Graph:
    def __init__(self, kinds, bound=3):
        self.terms = {}
        self.parameters = [self.node('Parameter', typ=kind) for kind in kinds]
        self.items = []
        self.bound = bound

    def node(self, kind, args=(), payload=None, typ='bool'):
        key = len(self.terms)
        self.terms[str(key)] = {'kind': kind, 'type': {'kind': typ}, 'arguments': list(args), 'payload': payload}
        return key

    def constant(self, value):
        return self.node('Constant', payload=value, typ='bool' if type(value) is bool else 'int')

    def compare(self, op, a, b):
        return self.node('Intrinsic', [a, b], op)

    def request(self, bindings=None):
        return {'version': 3, 'integer_min': -self.bound, 'integer_max': self.bound, 'bindings': bindings, 'rules': [
            {'local_count': 0, 'parameters': self.parameters, 'terms': self.terms, 'items': self.items}]}

    def condition(self, term):
        self.items.append({'kind': 'Condition', 'term': term, 'arguments': []})


class TranslationTests(unittest.TestCase):
    def test_boolean_truth_tables_and_full_reification(self):
        for a, b, c in itertools.product([False, True], repeat=3):
            g = Graph(['bool'] * 3)
            x, y, z = g.parameters
            # not ((a and b) or not c), under symbolic vars fixed by constraints.
            conjunction = g.node('And', [x, y])
            disjunction = g.node('Or', [conjunction, g.node('Not', [z])])
            g.condition(g.node('Not', [disjunction]))
            for parameter, value in zip(g.parameters, (a, b, c)):
                g.condition(g.compare('==', parameter, g.constant(value)))
            status, scope, _, values, conflicts = backend.solve(g.request())
            expected = not ((a and b) or not c)
            self.assertEqual(status, 'sat' if expected else 'unsat')
            self.assertEqual(scope, 'configured')
            if expected:
                self.assertEqual(values, [a, b, c])

    def test_implication_is_not_unconditional_inclusion(self):
        for a, b in itertools.product([False, True], repeat=2):
            g = Graph(['bool', 'bool'])
            x, y = g.parameters
            g.items.append({'kind': 'Implication', 'term': x, 'arguments': [y]})
            g.condition(g.compare('==', x, g.constant(a)))
            g.condition(g.compare('==', y, g.constant(b)))
            self.assertEqual(backend.solve(g.request())[0], 'sat' if not a or b else 'unsat')

    def test_integer_constraints_match_exhaustive_search(self):
        for target in range(-8, 9):
            g = Graph(['int', 'int'])
            x, y = g.parameters
            total = g.node('Intrinsic', [x, y], '+', 'int')
            g.condition(g.compare('==', total, g.constant(target)))
            g.condition(g.compare('<', x, y))
            expected = [(a, b) for a in range(-3, 4) for b in range(-3, 4) if a + b == target and a < b]
            status, scope, reason, values, conflicts = backend.solve(g.request())
            self.assertEqual(scope, 'configured')
            self.assertEqual(status, 'sat' if expected else 'unsat')
            if expected:
                self.assertIn(tuple(values), expected)
            else:
                self.assertEqual(status, 'unsat')

    def test_points_under_negation_and_disjunction(self):
        g = Graph(['int'])
        x = g.parameters[0]
        domain = g.node('Constant', payload={'points': [1, 3]}, typ='unsupported')
        member = g.node('In', [x, domain])
        g.condition(g.node('Or', [g.node('Not', [member]), g.compare('==', x, g.constant(3))]))
        for value in range(-3, 4):
            # A fully bound Instance is an exact query, independent of default bounds.
            status, scope, _, _, conflicts = backend.solve(g.request([value]))
            self.assertEqual(scope, 'configured')
            self.assertEqual(status, 'unsat' if value == 1 else 'sat')

    def test_missing_dependency_is_a_protocol_error_result(self):
        g = Graph(['bool'])
        g.condition(g.parameters[0])
        payload = json.dumps(g.request()).encode()
        frame = len(payload).to_bytes(4, 'big') + payload
        process = subprocess.run([sys.executable, '-S', str(Path(backend.__file__))], input=frame * 2, capture_output=True)
        self.assertEqual(process.returncode, 0, process.stderr)
        remaining = process.stdout
        for _ in range(2):
            size = int.from_bytes(remaining[:4], 'big')
            lines = remaining[4:4 + size].decode().splitlines()
            self.assertEqual(lines[:3], ['TAPAS_SOLVE_3', 'error', 'configured'])
            self.assertIn('OR-Tools is unavailable', lines[3])
            remaining = remaining[4 + size:]
        self.assertEqual(remaining, b'')

    def test_presolve_contradiction_does_not_require_ortools(self):
        g = Graph(['int', 'int'])
        x, y = g.parameters
        g.condition(g.compare('<', x, y))
        g.condition(g.compare('<=', y, x))
        payload = json.dumps(g.request()).encode()
        process = subprocess.run([sys.executable, '-S', str(Path(backend.__file__))],
                                 input=len(payload).to_bytes(4, 'big') + payload, capture_output=True)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(process.stdout[4:].decode().splitlines()[:3], ['TAPAS_SOLVE_3', 'unsat', 'configured'])

    def test_worker_applies_each_requests_domain_and_bound_instances(self):
        g = Graph(['int'])
        g.condition(g.compare('==', g.parameters[0], g.constant(4)))
        requests = [g.request(), g.request(), g.request([4]), g.request([4])]
        requests[1]['integer_max'] = requests[3]['integer_max'] = 4
        data = b''
        for request in requests:
            payload = json.dumps(request).encode()
            data += len(payload).to_bytes(4, 'big') + payload
        process = subprocess.run([sys.executable, str(Path(backend.__file__))], input=data, capture_output=True)
        self.assertEqual(process.returncode, 0, process.stderr)
        remaining = process.stdout
        for status in ('unsat', 'sat', 'unsat', 'sat'):
            size = int.from_bytes(remaining[:4], 'big')
            self.assertEqual(remaining[4:4+size].decode().splitlines()[:3], ['TAPAS_SOLVE_3', status, 'configured'])
            remaining = remaining[4+size:]
        self.assertEqual(remaining, b'')
        for lower, upper in ((4, 3), (True, 3), (0, 2**63), ('0', 3)):
            request = g.request()
            request.update(integer_min=lower, integer_max=upper)
            self.assertEqual(backend.solve(request)[0], 'error')

    def test_truncated_and_oversized_frames(self):
        for frame in (b'\x00\x00', (17 * 1024 * 1024).to_bytes(4, 'big'), b'\x00\x00\x00\x05{}'):
            process = subprocess.run([sys.executable, str(Path(backend.__file__))], input=frame, capture_output=True)
            self.assertNotEqual(process.returncode, 0)

    def test_missing_native_and_invalid_arithmetic_are_not_false(self):
        g = Graph(['int'])
        x = g.parameters[0]
        product = g.node('Intrinsic', [x, x], '*', 'int')
        g.condition(g.compare('==', product, g.constant(4)))
        self.assertEqual(backend.solve(g.request())[0], 'unsupported')
        overflow = Graph(['int'], bound=10**12)
        x = overflow.parameters[0]
        product = overflow.node('Intrinsic', [x, overflow.constant(10**12)], '*', 'int')
        overflow.condition(overflow.compare('==', product, overflow.constant(1)))
        # Divisibility proves this impossible before constructing any CP-SAT integer.
        self.assertEqual(backend.solve(overflow.request())[:2], ('unsat', 'configured'))


if __name__ == '__main__':
    unittest.main()
