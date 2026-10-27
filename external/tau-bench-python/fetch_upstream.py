#!/usr/bin/env python3
"""Fetch the pinned benchmark next to this package, independent of outer repos."""
import argparse
from pathlib import Path
import subprocess
from project import DEFAULT_SOURCE, PIN, verify_source

URL = 'https://github.com/sierra-research/tau-bench.git'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--destination', type=Path, default=DEFAULT_SOURCE)
    args = parser.parse_args()
    dest = args.destination.resolve()
    if dest.exists():
        try:
            verify_source(dest)
        except ValueError:
            if not (dest/'.git').is_dir():
                raise SystemExit('Existing snapshot differs from the pin; refusing to overwrite it.')
        else:
            print(dest); print(PIN); return 0  # Also accepts a verified Git-free snapshot.
    else:
        dest.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(['git', 'clone', URL, str(dest)], check=True)
    dirty = subprocess.check_output(['git', '-C', str(dest), 'status', '--porcelain'], text=True)
    if dirty:
        raise SystemExit('Refusing to change a modified upstream checkout.')
    head = subprocess.check_output(['git', '-C', str(dest), 'rev-parse', 'HEAD'], text=True).strip()
    if head != PIN:
        subprocess.run(['git', '-C', str(dest), 'fetch', 'origin', PIN], check=True)
        subprocess.run(['git', '-C', str(dest), 'checkout', '--detach', PIN], check=True)
    verify_source(dest)
    print(dest); print(PIN)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
