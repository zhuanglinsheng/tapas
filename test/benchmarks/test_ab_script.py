"""Keep A/B comparisons paired, checksum-checked, and free of warm-up samples."""
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import ab_script


class RuntimeComparisonTests(unittest.TestCase):
    def test_alternating_order_and_warmup_exclusion(self):
        baseline = Path('/baseline')
        candidate = Path('/candidate')
        order = []

        def execute(binary, source):
            order.append(binary.name)
            if len(order) <= 2:
                return 42, 99999
            return 42, 200 if binary == baseline else 100

        with tempfile.TemporaryDirectory() as scratch:
            source = Path(scratch) / 'case.tap'
            source.write_text('unchanged')
            with patch.object(ab_script, 'execute', side_effect=execute):
                result = ab_script.measure(baseline, candidate, source, 3)
        self.assertEqual(order, ['baseline', 'candidate', 'baseline', 'candidate',
                                 'candidate', 'baseline', 'baseline', 'candidate'])
        self.assertEqual(result['samples_ns'], {'baseline': [200] * 3, 'candidate': [100] * 3})
        self.assertEqual(result['paired_ratio_median'], 0.5)

    def test_reverse_first_and_combine_preserve_paired_samples(self):
        orders = []

        def execute(binary, source):
            orders.append(binary.name)
            return 42, 200 if binary.name == 'a' else 100

        with tempfile.TemporaryDirectory() as scratch:
            source = Path(scratch) / 'case.tap'
            source.write_text('unchanged')
            with patch.object(ab_script, 'execute', side_effect=execute):
                first = ab_script.measure(Path('/a'), Path('/b'), source, 3, 'candidate')
                second = ab_script.measure(Path('/a'), Path('/b'), source, 3, 'baseline')
        self.assertEqual(orders[:8], ['b', 'a', 'b', 'a', 'a', 'b', 'b', 'a'])
        combined = ab_script.combine([first, second])
        self.assertEqual(combined['samples_ns']['baseline'], [200] * 6)
        self.assertEqual(combined['round_ratio_medians'], [0.5, 0.5])
        second['checksum'] = 99
        with self.assertRaisesRegex(RuntimeError, 'changed between rounds'):
            ab_script.combine([first, second])

    def test_checksum_mismatch_rejects_comparison(self):
        with tempfile.TemporaryDirectory() as scratch:
            source = Path(scratch) / 'case.tap'
            source.write_text('unchanged')
            with patch.object(ab_script, 'execute', side_effect=[(42, 100), (43, 100)]):
                with self.assertRaisesRegex(RuntimeError, 'expected 42, got 43'):
                    ab_script.measure(Path('/a'), Path('/b'), source, 3)

    def test_changed_source_rejects_comparison(self):
        with tempfile.TemporaryDirectory() as scratch:
            source = Path(scratch) / 'case.tap'
            source.write_text('before')

            def execute(binary, source):
                source.write_text('after')
                return 42, 100

            with patch.object(ab_script, 'execute', side_effect=execute):
                with self.assertRaisesRegex(RuntimeError, 'source changed'):
                    ab_script.measure(Path('/a'), Path('/b'), source, 3)

    def test_each_execution_has_an_isolated_bytecode_cache(self):
        directories = []

        def run(command, **options):
            directory = Path(options['cwd'])
            self.assertTrue(directory.is_dir())
            self.assertTrue(all(Path(value).is_absolute() for value in command))
            self.assertFalse((directory / '__tapas_build__').exists())
            (directory / '__tapas_build__').mkdir()
            directories.append(directory)
            return SimpleNamespace(stdout='42\n100\n')

        with patch.object(ab_script.subprocess, 'run', side_effect=run):
            self.assertEqual(ab_script.execute(Path('/a'), Path('/source')), (42, 100))
            self.assertEqual(ab_script.execute(Path('/b'), Path('/source')), (42, 100))
        self.assertNotEqual(*directories)
        self.assertTrue(all(not directory.exists() for directory in directories))

    def test_nonpositive_timer_is_rejected(self):
        with patch.object(ab_script.subprocess, 'run',
                          return_value=SimpleNamespace(stdout='42\n0\n')):
            with self.assertRaisesRegex(RuntimeError, 'nonpositive CPU time'):
                ab_script.execute(Path('/a'), Path('/source'))


if __name__ == '__main__':
    unittest.main()
