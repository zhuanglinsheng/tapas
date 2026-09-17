"""Smoke-test an extracted Core archive from outside the source tree."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('prefix', type=Path)
parser.add_argument('--version', required=True)
args = parser.parse_args()
prefix = args.prefix.resolve()
for name in ('bin/tapas', 'bin/tapas-language-server',
             'include/tapas/tsession.h',
             'include/tapas/objects/ttype.h',
             'include/tapas/version.h',
             'share/tapas/stdlib/format/format.tapc',
             'share/tapas/stdlib/format/edit.tap',
             'share/tapas/solve/requirements.txt',
             'share/licenses/tapas/pcg-c/LICENSE-MIT.txt'):
    if not (prefix / name).is_file():
        raise SystemExit(f'Missing archive file: {name}')
env = os.environ.copy()
# Verify the default PATH-based backend, not a developer-only override.
env.pop('TAPAS_SOLVE_PYTHON', None)
env.pop('TAPAS_STDLIB', None)
env.pop('TAPAS_STDLIB_PATH', None)
env.pop('TAPAS_PATH', None)
env['TAPAS_SOLVE_INT_MIN'] = '-1000000000000'
env['TAPAS_SOLVE_INT_MAX'] = '1000000000000'
env['PYTHONDONTWRITEBYTECODE'] = '1'
with tempfile.TemporaryDirectory(prefix='tapas-installed-') as directory:
    work = Path(directory)
    def run(*command):
        result = subprocess.run(command, cwd=work, env=env, text=True,
                                capture_output=True, timeout=90)
        if result.returncode:
            raise AssertionError(f'{command}: {result.stdout}{result.stderr}')
        return result.stdout
    run('python3', '-c', 'import ortools; from ortools.sat.python import cp_model')
    runtime = str(prefix / 'bin/tapas')
    assert f'Tapas {args.version} ' in run(runtime, '--version')
    source = work / 'smoke.tap'
    source.write_text('''let Positive = rule (x: Int) {
    x in rules::range(1, 4)
}
let result = solve::hold(Positive)
assert(rule { result::status == 'sat' })
assert(result::witness)
let rng = random::generator(random::pcg32_xsh_rr, 42)
let samples = solve::sample(Positive, 2, {'x': rules::range(1, 4)},
    rng=rng, candidate_limit=20, time_limit=10.0)
assert(rule { samples::status == 'complete'; len(samples::samples) == 2 })
print('installed solve and sample: ok')
''')
    expected = 'installed solve and sample: ok\n'
    assert run(runtime, str(source)) == expected
    run(runtime, '-c', str(source))
    assert run(runtime, '-e', str(source) + 'c') == expected
    formatted = work / 'format.tap'
    formatted.write_text('function f(x: Int) -> Int { return x }\n')
    run(runtime, '-m', 'format', str(formatted))
    run(runtime, '-m', 'format', '--check', str(formatted))
    # Exercise the shipped language server without Node or a running editor.
    import json
    def frame(message):
        data = json.dumps(message).encode()
        return f'Content-Length: {len(data)}\r\n\r\n'.encode() + data
    messages = [
        {'jsonrpc': '2.0', 'id': 1, 'method': 'initialize', 'params': {'capabilities': {}}},
        {'jsonrpc': '2.0', 'id': 2, 'method': 'shutdown', 'params': None},
        {'jsonrpc': '2.0', 'method': 'exit', 'params': None},
    ]
    server = subprocess.run([str(prefix / 'bin/tapas-language-server')],
        input=b''.join(map(frame, messages)), cwd=work, env=env,
        capture_output=True, timeout=15)
    assert server.returncode == 0, server.stderr
    raw = server.stdout
    responses = []
    while raw:
        header, raw = raw.split(b'\r\n\r\n', 1)
        size = int(header.split(b':', 1)[1])
        responses.append(json.loads(raw[:size]))
        raw = raw[size:]
    initialized = next(r for r in responses if r.get('id') == 1)
    assert initialized['result']['serverInfo']['version'] == args.version
print(f'Core {args.version}: relocated runtime, source/bytecode solver, sampling, formatter, LSP passed')
