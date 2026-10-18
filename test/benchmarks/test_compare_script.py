"""Regression checks for complete, freshly generated benchmark reports."""
import contextlib
import io
import re
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import compare_script as comparison


class BenchmarkReportTests(unittest.TestCase):
    def test_algorithm_target_excludes_hotspots_and_uses_unrounded_ratios(self):
        for ratio in (0.899999, 0.90, 0.900001):
            rows = [(name, 1, 1, None, ratio, None)
                    for name in comparison.ALGORITHM_BENCHMARKS]
            rows.extend((name, 1, 1, None, 0.001, None)
                        for name in comparison.HOT_PATH_BENCHMARKS)
            mean = comparison.algorithm_ratio(rows)
            self.assertAlmostEqual(mean, ratio)
            self.assertEqual(mean <= comparison.ALGORITHM_TARGET_RATIO,
                             ratio <= 0.90)

    def test_target_exit_status_and_reports_use_algorithm_group(self):
        for algorithm_ns, expected_status in ((899, 0), (901, 1)):
            def fake_measure(command, runs):
                source = Path(command[1])
                duration = 1000
                if command[0] == 'tapas-test':
                    duration = (algorithm_ns if source.stem in
                                comparison.ALGORITHM_BENCHMARKS else 1)
                return 42, [duration] * runs

            with patch.object(sys, 'argv', ['compare_script.py', 'tapas-test',
                                           '--require-algorithm-target']), \
                 patch.object(sys, 'version_info', (3, 14, 2)), \
                 patch.object(comparison, 'lua_version_text', return_value='Lua test'), \
                 patch.object(comparison, 'measure', side_effect=fake_measure), \
                 patch.object(comparison, 'write_markdown') as writer, \
                 contextlib.redirect_stdout(io.StringIO()), \
                 contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(comparison.main(), expected_status)
                self.assertEqual(writer.call_count, 2)

    def test_target_rejects_a_different_python_version_before_measuring(self):
        with patch.object(sys, 'argv', ['compare_script.py', 'tapas-test',
                                       '--require-algorithm-target']), \
             patch.object(sys, 'version_info', (3, 13, 0)), \
             patch.object(comparison, 'measure') as measure, \
             contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit) as error:
                comparison.main()
            self.assertEqual(error.exception.code, 2)
            measure.assert_not_called()

    def test_discovery_includes_new_python_only_case_and_rejects_missing_reference(self):
        with tempfile.TemporaryDirectory() as scratch:
            directory = Path(scratch)
            (directory / 'future_hotspot.tap').write_text('')
            with self.assertRaisesRegex(RuntimeError, 'missing Python reference'):
                comparison.discover_hot_paths(directory)
            (directory / 'future_hotspot.py').write_text('')
            self.assertEqual(comparison.discover_hot_paths(directory), ('future_hotspot',))

    def test_bilingual_reports_include_all_cases_and_only_available_lua_links(self):
        rows = []
        for name in comparison.BENCHMARKS:
            source = comparison.HERE / comparison.benchmark_source(name)
            lua = source.with_suffix('.lua').is_file()
            rows.append((name, 1000, 2000, 4000 if lua else None,
                         0.5, 0.25 if lua else None))
        with tempfile.TemporaryDirectory() as scratch:
            # Custom report paths also need working relative source links.
            for language in ('zh', 'en'):
                report = Path(scratch) / f'README_{language}.md'
                with patch.object(comparison.subprocess, 'run',
                                  return_value=SimpleNamespace(stdout='Tapas test\n')), \
                     patch.object(comparison.platform, 'platform', return_value='Test system'):
                    comparison.write_markdown(report, Path('/unused/tapas'),
                                              'Lua test', 3, rows, language)
                text = report.read_text()
                table_rows = [line for line in text.splitlines() if line.startswith('| `') and line.count('|') >= 8]
                self.assertEqual(len(table_rows), len(comparison.BENCHMARKS))
                for name, *_, lua_ratio in rows:
                    line = next(line for line in table_rows if line.startswith(f'| `{name}` |'))
                    self.assertEqual('[Lua]' in line, lua_ratio is not None)
                    if lua_ratio is None:
                        self.assertIn('| — | 0.500× | — |', line)
                    for link in re.findall(r'\]\(([^)]+)\)', line):
                        self.assertTrue((report.parent / link).is_file(), link)
                self.assertIn('Tapas/Python **0.500×**', text)
                self.assertTrue(all(line.count('|') == 8 for line in table_rows))
                self.assertNotIn('测量内容', text)
                self.assertNotIn('Measured workload', text)
                self.assertNotIn('Python 结构对照', text)
                self.assertNotIn('Python structural controls', text)
                self.assertIn('AUDIT.md', text)
                self.assertNotIn('综合性能目标：', text)
                self.assertNotIn('Algorithm target:', text)
                self.assertIn('Tapas/Lua **0.250×**', text)
                count = len(comparison.HOT_PATH_BENCHMARKS)
                coverage = f'（{count} 项）' if language == 'zh' else f' ({count} cases)'
                self.assertIn('Tapas/Python **0.500×**' + coverage, text)
                lua_count = sum((comparison.HERE / comparison.benchmark_source(name))
                                .with_suffix('.lua').is_file()
                                for name in comparison.HOT_PATH_BENCHMARKS)
                lua_coverage = f'（{lua_count} 项）' if language == 'zh' else f' ({lua_count} cases)'
                self.assertIn('Tapas/Lua **0.250×**' + lua_coverage, text)

    def test_catalog_and_structural_controls_describe_current_hotspots(self):
        self.assertEqual(set(comparison.HOT_PATH_CATALOG), set(comparison.HOT_PATH_BENCHMARKS))
        for entry in comparison.HOT_PATH_CATALOG.values():
            self.assertIn(entry['kind'], ('kernel', 'task', 'representation', 'semantics'))
            self.assertTrue(entry['zh'])
            self.assertTrue(entry['en'])
        self.assertEqual(comparison.HOT_PATH_CATALOG['string_count']['kind'], 'task')
        self.assertTrue(comparison.HOT_PATH_CATALOG['dp_row']['python_matched'])

    def test_default_run_remeasures_all_cases_and_updates_both_readmes(self):
        commands = []

        def fake_measure(command, runs):
            commands.append(command)
            duration = 1000 if command[0] == 'tapas-test' else 2000
            return 42, [duration] * runs

        with patch.object(sys, 'argv', ['compare_script.py', 'tapas-test']), \
             patch.object(comparison, 'lua_version_text', return_value='Lua test'), \
             patch.object(comparison, 'measure', side_effect=fake_measure), \
             patch.object(comparison, 'write_markdown') as writer, \
             contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(comparison.main(), 0)
        self.assertEqual(writer.call_count, 2)
        self.assertEqual({call.args[0].name for call in writer.call_args_list},
                         {'README.md', 'README_en.md'})
        self.assertIs(writer.call_args_list[0].args[4], writer.call_args_list[1].args[4])
        self.assertEqual(len(writer.call_args_list[0].args[4]), len(comparison.BENCHMARKS))
        expected = sum(2 + (comparison.HERE / comparison.benchmark_source(name))
                       .with_suffix('.lua').is_file()
                       + comparison.HOT_PATH_CATALOG.get(name, {}).get('python_matched', False)
                       for name in comparison.BENCHMARKS)
        self.assertEqual(len(commands), expected)
        self.assertTrue(all(Path(command[1]).is_file() for command in commands))


if __name__ == '__main__':
    unittest.main()
