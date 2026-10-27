"""Paper-local paths and pinned snapshot verification; standard library only."""
import hashlib
import json
from pathlib import Path, PurePosixPath

PAPER_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SOURCE = PAPER_ROOT / 'tau-bench'
PIN = '59a200c6d575d595120f1cb70fea53cef0632f6b'
MANIFEST = Path(__file__).with_name('UPSTREAM_SOURCE.json')


def verify_source(source=DEFAULT_SOURCE):
    """Validate source bytes without consulting Git, cwd, or a parent repository."""
    source = Path(source).resolve()
    manifest = json.loads(MANIFEST.read_text())
    if manifest['commit'] != PIN:
        raise ValueError('Snapshot manifest has an unexpected commit')
    for name, expected in manifest['sha256'].items():
        path = PurePosixPath(name)
        if path.is_absolute() or '..' in path.parts:
            raise ValueError('Invalid manifest path: ' + name)
        file = source.joinpath(*path.parts)
        if not file.is_file() or hashlib.sha256(file.read_bytes()).hexdigest() != expected:
            raise ValueError('Upstream source missing or changed: ' + name)
    expected_tools = {name for name in manifest['sha256'] if name.startswith('tau_bench/envs/retail/tools/') and name.endswith('.py')}
    actual_tools = {str(p.relative_to(source)) for p in (source/'tau_bench/envs/retail/tools').glob('*.py')}
    if actual_tools != expected_tools:
        raise ValueError('Unexpected Retail tool files outside the pinned manifest')
    return dict(repository=manifest['repository'], commit=PIN, verification='pinned SHA-256 manifest; Git not required', sha256=manifest['sha256'])
