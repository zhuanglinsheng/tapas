#!/usr/bin/env python3
"""Fetch the original MIT-licensed Sierra repository at the reproducible commit."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
DEST = ROOT / 'external/tau-bench'
URL = 'https://github.com/sierra-research/tau-bench.git'
COMMIT = '59a200c6d575d595120f1cb70fea53cef0632f6b'
if not DEST.exists():
    subprocess.run(['git', 'clone', URL, str(DEST)], check=True)
head = subprocess.check_output(['git', '-C', str(DEST), 'rev-parse', 'HEAD'], text=True).strip()
if head != COMMIT:
    dirty = subprocess.check_output(['git', '-C', str(DEST), 'status', '--porcelain'], text=True)
    if dirty:
        raise SystemExit('Refusing to change a modified upstream checkout.')
    subprocess.run(['git', '-C', str(DEST), 'fetch', 'origin', COMMIT], check=True)
    subprocess.run(['git', '-C', str(DEST), 'checkout', '--detach', COMMIT], check=True)
print(DEST)
print(COMMIT)
