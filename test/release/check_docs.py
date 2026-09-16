"""Check maintained docs, their executable examples, and language contracts.

Design drafts and literature are excluded. text/ebnf/C fences are descriptive;
this check does not claim to prove every prose statement or run partial snippets.
"""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile
from urllib.parse import unquote

parser = argparse.ArgumentParser()
parser.add_argument('--tapas', type=Path, required=True)
parser.add_argument('--blas', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
runtime = args.tapas.resolve()
if args.blas is None:
    args.blas = next(runtime.parent.parent.glob('libtest_cblas.*'), None)
docs = sorted(p for p in (root / 'docs').rglob('*.md')
              if 'plan' not in p.relative_to(root).parts)
errors = []
fences = 0
output_documents = 0
for path in docs + [root / 'README.md', root / 'README_en.md']:
    source = path.read_text()
    for target in re.findall(r'\]\(([^)]+)\)', source):
        target = unquote(target.split('#')[0])
        if not target or '://' in target or target.startswith('mailto:'):
            continue
        if not (path.parent / target).exists():
            errors.append(f'{path.relative_to(root)}: missing link {target}')
    for header in re.findall(r'#include "(tapas/[^\"]+)"', source):
        if not (root / 'include' / header).exists():
            errors.append(f'{path.name}: missing public header {header}')
    fences += len(re.findall(r'^```(?:tap|tapas)\s*$', source, re.M))
    env = os.environ.copy()
    if args.blas:
        env['TAPAS_BLAS_LIBRARY'] = str(args.blas.resolve())
    result = subprocess.run([str(runtime), '--stdout', str(path)],
        cwd=root, env=env, text=True, capture_output=True, timeout=90)
    if result.returncode:
        errors.append(f'{path.name}: {result.stdout}{result.stderr}')
    # Ignore literal HTML demonstrations; compare actual recorded output blocks.
    displayed = re.sub(r'^```html\n.*?^```\s*$', '', source, flags=re.M | re.S)
    outputs = re.findall(r"<pre class=['\"]Tapas-Return['\"]>\n(.*?)</pre>", displayed, re.S)
    if outputs:
        output_documents += 1
        def normalize(text):
            return re.sub(r'0x[0-9a-fA-F]+', '<address>', text).strip()
        if normalize(result.stdout) != normalize(''.join(outputs)):
            errors.append(f'{path.name}: recorded output differs from execution')

examples = sorted((root / 'docs/examples').rglob('*.tap'))
for path in examples:
    result = subprocess.run([str(runtime), str(path)], cwd=root, env=env,
                            text=True, capture_output=True, timeout=30)
    if result.returncode:
        errors.append(f'{path.relative_to(root)}: {result.stdout}{result.stderr}')
# Keep both normative keyword lists in sync with the lexer.
lexer = (root / 'src/compile/frontend/syntax.c').read_text()
keywords = set(re.findall(r'\{ "(\w+)", tsyntax_kw_', lexer))
for language in ('en', 'zh'):
    source = (root / f'docs/Syntax_{language}.md').read_text()
    block = next(b for b in re.findall(r'```text\n(.*?)```', source, re.S)
                 if b.startswith('and as base'))
    if set(block.split()) != keywords:
        errors.append(f'Syntax_{language}: reserved words differ from lexer')

positive = '''let 字典 = {'count': 1}
字典['count'] = 'changed'
append(字典, 'new': true)
assert(rule { 字典['count'] == 'changed'; 字典['new'] })
let Shape = types::make_type('count': types::Int, 'name': types::String)
let item: Shape = {3, name='three'}
assert(rule { item::count == 3 })
let Good = rule (n: Int) { n > 0 }
let Combined = rule (n: Int) {
    Good(n) and not Good(0)
    Good(n) implies n > 0
}
assert(Combined(3))
function reflected(n: Int) -> Int { return n }
assert(rule { len(parameters(reflected)) == 1 })
let T = types::parameter('T')
let Box = types::template([T], [], types::make_type('value': T))
let box: Box[String] = {'value': 'ok'}
assert(rule { box::value == 'ok' })
print('documentation contracts: ok')
'''
negative = {
    'retired_require': 'let R = rule { true }\nlet Old = rule { require R() }\n',
    'rule_instance_if': 'let R = rule { true }\nif (R()) { print(1) }\n',
    'unqualified_domain': 'let value: RangeOf[Int] = rules::range(0, 1)\n',
    'invalid_escape': "let value = '\\q'\n",
    'positional_after_named': 'print(name=1, 2)\n',
}
with tempfile.TemporaryDirectory(prefix='tapas-docs-') as tmp:
    source = Path(tmp) / 'contract.tap'
    source.write_text(positive)
    for mode in ('source', 'bytecode'):
        if mode == 'bytecode':
            subprocess.run([str(runtime), '-c', str(source)], check=True, capture_output=True)
        command = [str(source)] if mode == 'source' else ['-e', str(source) + 'c']
        result = subprocess.run([str(runtime), *command], text=True,
                                capture_output=True, timeout=30)
        if result.returncode or result.stdout != 'documentation contracts: ok\n':
            errors.append(f'{mode} contract: {result.stdout}{result.stderr}')
    for name, text in negative.items():
        source.write_text(text)
        command = [str(runtime), str(source)] if name == 'rule_instance_if' else [str(runtime), '-c', str(source)]
        result = subprocess.run(command, capture_output=True, timeout=30)
        if result.returncode == 0:
            errors.append(f'Expected rejection: {name}')
if errors:
    raise SystemExit('\n'.join(errors))
print(f'{len(docs)} maintained docs + 2 READMEs; {fences} executable fences; '
      f'{output_documents} output snapshots; local link targets, public headers, keywords, source/bytecode contracts, '
      f'{len(examples)} standalone examples, {len(negative)} rejection cases passed')
