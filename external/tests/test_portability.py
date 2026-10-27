"""Regressions for using the paper tree without a parent repository or Git."""
from pathlib import Path
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

PACKAGE = Path(__file__).resolve().parents[1]/'tau-bench-python'
sys.path.insert(0, str(PACKAGE))
from project import PAPER_ROOT, DEFAULT_SOURCE, MANIFEST, verify_source


class SourcePortabilityTests(unittest.TestCase):
    def make_snapshot(self, destination):
        for name in json.loads(MANIFEST.read_text())['sha256']:
            target=destination/name;target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copyfile(DEFAULT_SOURCE/name,target)

    def test_default_paths_stay_in_paper_tree(self):
        self.assertEqual(PAPER_ROOT, PACKAGE.parent)
        self.assertTrue(DEFAULT_SOURCE.is_relative_to(PAPER_ROOT))

    def test_git_free_snapshot_and_no_git_process(self):
        with tempfile.TemporaryDirectory() as temp:
            source=Path(temp)/'dataset';self.make_snapshot(source)
            with patch('subprocess.check_output',side_effect=AssertionError('Git called')):
                meta=verify_source(source)
            self.assertIn('Git not required',meta['verification'])
            self.assertFalse((source/'.git').exists())
            env=dict(os.environ,PATH='',PYTHONPATH='')
            run=subprocess.run([sys.executable,str(PACKAGE/'fetch_upstream.py'),'--destination',str(source)],
                               cwd=temp,env=env,capture_output=True,text=True)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
            self.assertIn(str(source),run.stdout)

    def test_modified_file_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            source=Path(temp)/'dataset';self.make_snapshot(source)
            with (source/'tau_bench/envs/retail/wiki.md').open('a') as stream:stream.write('\nchanged\n')
            with self.assertRaisesRegex(ValueError,'changed'):verify_source(source)

    def test_unmanifested_tool_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            source=Path(temp)/'dataset';self.make_snapshot(source)
            (source/'tau_bench/envs/retail/tools/extra.py').write_text('raise AssertionError("must not execute")\n')
            with self.assertRaisesRegex(ValueError,'Unexpected'):verify_source(source)


if __name__=='__main__':unittest.main()
