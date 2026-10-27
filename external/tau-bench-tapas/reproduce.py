#!/usr/bin/env python3
"""Run original Retail tools and check independent Tapas contracts (no LLM/API key)."""
import argparse
import ast
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import types
from collections import Counter

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
PIN = '59a200c6d575d595120f1cb70fea53cef0632f6b'


def upstream_tools(source):
    # Import actual upstream files, without importing the unrelated LLM runner.
    for name in ('tau_bench', 'tau_bench.envs'):
        package = types.ModuleType(name)
        package.__path__ = []
        sys.modules[name] = package
    def load(name, path):
        spec = importlib.util.spec_from_file_location(name, path)
        module = importlib.util.module_from_spec(spec)
        sys.modules[name] = module
        spec.loader.exec_module(module)
        return module
    base = load('tau_bench.envs.tool', source / 'tau_bench/envs/tool.py')
    result = {}
    for path in sorted((source / 'tau_bench/envs/retail/tools').glob('*.py')):
        if path.name == '__init__.py':
            continue
        mod = load('retail_original_' + path.stem, path)
        for obj in vars(mod).values():
            if isinstance(obj, type) and obj is not base.Tool and issubclass(obj, base.Tool):
                result[obj.get_info()['function']['name']] = obj
    assert len(result) == 16
    return result


def read_tasks(path):
    # Parse source constructors as data, never execute task code or instructions.
    def value(node):
        if isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and node.func.id in ('Task', 'Action'):
            assert not node.args
            return {kw.arg: value(kw.value) for kw in node.keywords}
        if isinstance(node, (ast.List, ast.Tuple)):
            return [value(x) for x in node.elts]
        if isinstance(node, ast.Dict):
            return {value(k): value(v) for k, v in zip(node.keys, node.values)}
        return ast.literal_eval(node)
    for node in ast.parse(path.read_text()).body:
        if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id.startswith('TASKS') for t in node.targets):
            return value(node.value)
    raise ValueError(f'No tasks in {path}')


def scoped(data, actions):
    """Reference closure: target users/orders, products of order items, lookup matches."""
    users, orders, products = set(), set(), set()
    for call in actions:
        a, name = call['kwargs'], call['name']
        if 'order_id' in a and a['order_id'] in data['orders']:
            orders.add(a['order_id'])
        if 'user_id' in a and a['user_id'] in data['users']:
            users.add(a['user_id'])
        if 'product_id' in a and a['product_id'] in data['products']:
            products.add(a['product_id'])
        if name == 'list_all_product_types':
            products.update(data['products'])
        if name.startswith('find_user_id_by_'):
            for uid, u in data['users'].items():
                if name.endswith('email'):
                    match = u['email'].lower() == a['email'].lower()
                else:
                    match = (u['name']['first_name'].lower() == a['first_name'].lower()
                             and u['name']['last_name'].lower() == a['last_name'].lower()
                             and u['address']['zip'] == a['zip'])
                if match:
                    users.add(uid)
    for oid in orders:
        o = data['orders'][oid]
        users.add(o['user_id'])
        products.update(x['product_id'] for x in o['items'])
    return copy.deepcopy({section: {k: v for k, v in data[section].items() if k in ids}
                          for section, ids in [('users', users), ('orders', orders), ('products', products)]})


def invoke(tool, db, call, label):
    before = copy.deepcopy(db)
    exception = ''
    try:
        output = tool.invoke(db, **call['kwargs'])
        try:
            output = json.loads(output)
        except (ValueError, TypeError):
            pass
    except Exception as exc:
        output, exception = '', type(exc).__name__
    return {'label': label, 'before': before, 'call': call, 'output': output,
            'after': copy.deepcopy(db), 'exception': exception}


def edge_cases(data, tools):
    """One-factor and adversarial cases, independent from reference task traces."""
    pending = next(o for o in data['orders'].values() if o['status'] == 'pending')
    delivered = next(o for o in data['orders'].values() if o['status'] == 'delivered')
    addr = dict(address1='1 Test Street', address2='', city='Boston', state='MA', country='USA', zip='02101')
    cases = []
    def add(name, kwargs, mutate=None, tag=''):
        call = {'name': name, 'kwargs': kwargs}
        db = scoped(data, [call])
        if mutate:
            mutate(db)
        cases.append(invoke(tools[name], db, call, 'edge/' + name + '/' + tag))
    uid = pending['user_id']
    add('find_user_id_by_email', {'email': data['users'][uid]['email'].upper()}, tag='casefold')
    add('find_user_id_by_email', {'email': 'missing@example.invalid'}, tag='missing')
    u = data['users'][uid]
    add('find_user_id_by_name_zip', {**u['name'], 'zip': u['address']['zip']}, tag='valid')
    add('find_user_id_by_name_zip', {**u['name'], 'zip': 'missing'}, tag='missing')
    add('get_order_details', {'order_id': 'missing'})
    add('get_user_details', {'user_id': 'missing'})
    add('get_product_details', {'product_id': 'missing'})
    add('list_all_product_types', {})
    add('modify_user_address', {'user_id': uid, **addr}, tag='valid')
    add('modify_user_address', {'user_id': 'missing', **addr}, tag='missing')
    add('think', {'thought': 'No external action.'})
    add('transfer_to_human_agents', {'summary': 'Unsupported request.'})
    for name in ('cancel_pending_order', 'modify_pending_order_address', 'modify_pending_order_payment',
                 'modify_pending_order_items', 'return_delivered_order_items', 'exchange_delivered_order_items'):
        o = delivered if name.startswith(('return_', 'exchange_')) else pending
        oid = o['order_id']; old = o['items'][0]
        method = next(iter(data['users'][o['user_id']]['payment_methods']))
        a = {'order_id': oid}
        if name == 'cancel_pending_order': a['reason'] = 'no longer needed'
        if name == 'modify_pending_order_address': a.update(addr)
        if name.endswith(('payment', 'items')): a['payment_method_id'] = method
        if name.endswith('items'): a['item_ids'] = [old['item_id']]
        if name.startswith(('modify_pending_order_items', 'exchange_')):
            variant = next((k for k, v in data['products'][old['product_id']]['variants'].items()
                            if k != old['item_id'] and v['available']), old['item_id'])
            a['new_item_ids'] = [variant]
        add(name, a, tag='base')
        add(name, {**a, 'order_id': 'missing'}, tag='missing-order')
        add(name, a, lambda db, oid=oid: db['orders'][oid].update(status='processed'), 'wrong-status')
        if name == 'cancel_pending_order':
            add(name, {**a, 'reason': 'other'}, tag='invalid-reason')
            # Original loops over refunds too when cancelling after payment modification.
            def duplicate(db):
                h = db['orders'][oid]['payment_history']; h.append({**h[0], 'transaction_type': 'refund'})
            add(name, a, duplicate, 'refund-history')
        if 'payment_method_id' in a:
            add(name, {**a, 'payment_method_id': 'missing'}, tag='missing-payment')
            def giftcard(db):
                db['users'][o['user_id']]['payment_methods']['gift_card_test'] = {
                    'id': 'gift_card_test', 'source': 'gift_card', 'balance': 0.0}
            add(name, {**a, 'payment_method_id': 'gift_card_test'}, giftcard, 'gift-insufficient')
            def funded(db):
                giftcard(db); db['users'][o['user_id']]['payment_methods']['gift_card_test']['balance'] = 100000.0
            add(name, {**a, 'payment_method_id': 'gift_card_test'}, funded, 'gift-funded')
        if name == 'modify_pending_order_payment':
            add(name, {**a, 'payment_method_id': o['payment_history'][0]['payment_method_id']}, tag='same-method')
            add(name, a, lambda db: db['orders'][oid].update(payment_history=[]), 'empty-history-exception')
            add(name, a, lambda db: db['orders'][oid]['payment_history'].append(copy.deepcopy(o['payment_history'][0])), 'multiple-payments')
            add(name, a, lambda db: db['orders'][oid]['payment_history'][0].update(transaction_type='refund'), 'refund-only')
        if 'item_ids' in a:
            add(name, {**a, 'item_ids': ['missing']}, tag='missing-item')
            add(name, {**a, 'item_ids': [old['item_id']] * (len(o['items']) + 1)}, tag='multiplicity')
            add(name, {**a, 'item_ids': [], **({'new_item_ids': []} if 'new_item_ids' in a else {})}, tag='empty-list')
        if 'new_item_ids' in a:
            add(name, {**a, 'new_item_ids': []}, tag='length')
            add(name, {**a, 'new_item_ids': ['missing']}, tag='unavailable')
            add(name, {**a, 'new_item_ids': [old['item_id']]}, tag='same-option-accepted-upstream')
            add(name, a, lambda db: db['products'][old['product_id']]['variants'][a['new_item_ids'][0]].update(available=False), 'out-of-stock')
    # Controlled multi-step transaction paths complement the real-data reference traces.
    for scenario in ('modify-once', 'exchange-once', 'return-once', 'payment-then-address', 'gift-cancel', 'gift-payment-refund'):
        o = delivered if scenario.startswith(('exchange', 'return')) else pending
        oid = o['order_id']; uid = o['user_id']; item = o['items'][0]
        method = o['payment_history'][0]['payment_method_id']
        variants = data['products'][item['product_id']]['variants']
        replacement = next(k for k, v in variants.items() if v['available'] and k != item['item_id'])
        item_args = dict(order_id=oid, item_ids=[item['item_id']], new_item_ids=[replacement], payment_method_id=method)
        name = {'modify-once':'modify_pending_order_items', 'exchange-once':'exchange_delivered_order_items',
                'return-once':'return_delivered_order_items', 'payment-then-address':'modify_pending_order_payment',
                'gift-cancel':'cancel_pending_order', 'gift-payment-refund':'modify_pending_order_payment'}[scenario]
        if scenario == 'return-once':
            a = {k:v for k,v in item_args.items() if k != 'new_item_ids'}
        elif scenario.startswith(('modify-once', 'exchange-once')):
            a = item_args
        elif scenario == 'gift-cancel':
            a = dict(order_id=oid, reason='ordered by mistake')
        else:
            a = dict(order_id=oid, payment_method_id='gift_card_target')
        call = dict(name=name, kwargs=a); db = scoped(data, [call])
        pm = db['users'][uid]['payment_methods']
        pm['gift_card_target'] = dict(id='gift_card_target', source='gift_card', balance=100000.0)
        if scenario.startswith('gift-'):
            pm['gift_card_original'] = dict(id='gift_card_original', source='gift_card', balance=20.0)
            db['orders'][oid]['payment_history'][0]['payment_method_id'] = 'gift_card_original'
        cases.append(invoke(tools[name], db, call, 'sequence/' + scenario + '/first'))
        if scenario == 'payment-then-address':
            later = dict(name='modify_pending_order_address', kwargs=dict(order_id=oid, **addr))
        else:
            later = call
        cases.append(invoke(tools[later['name']], db, later, 'sequence/' + scenario + '/second'))
    # Cross-product variants, equal balance, negative differences and zero refunds.
    for name, o in [('modify_pending_order_items', pending), ('exchange_delivered_order_items', delivered)]:
        oid = o['order_id']; old = o['items'][0]
        method = o['payment_history'][0]['payment_method_id']
        foreign = next(v['item_id'] for pid, p in data['products'].items() if pid != old['product_id'] for v in p['variants'].values())
        a = dict(order_id=oid, item_ids=[old['item_id']], new_item_ids=[foreign], payment_method_id=method)
        add(name, a, tag='cross-product')
        for diff in (-10.0, 0.0, 10.0):
            a = dict(order_id=oid, item_ids=[old['item_id']], new_item_ids=[old['item_id']], payment_method_id='gift_card_boundary')
            def boundary(db):
                db['users'][o['user_id']]['payment_methods']['gift_card_boundary'] = dict(id='gift_card_boundary', source='gift_card', balance=max(diff, 0.0))
                v = db['products'][old['product_id']]['variants'][old['item_id']]
                v.update(price=old['price'] + diff, available=True)
            add(name, a, boundary, 'gift-boundary-' + str(diff))
    for expression in ('2 + 2', '2**3**2', '-2**2', '2**-2', '(12.5-2)/3', '5//2', '-5//2', '2.675', '2.685', '-2.675', '1/0', '2+(', '__import__(1)', '3+-2', '.5*2', '0**-1'):
        add('calculate', {'expression': expression}, tag=expression)
    return cases


def run_tapas(cases, binary):
    source = """import MODEL as m
let cases = json::parse(io::read_text(str(input())))
let failures = []
for (let c in cases) {
    let verdict = rules::check(m::ToolContract(c))
    if (not verdict['passed']) { append(failures, {'label': c['label'], 'violations': str(verdict['violations']), 'actual': c['output']}) }
}
print(json::stringify(failures))
""".replace('MODEL', str(HERE / 'model.tap'))
    failures = []
    with tempfile.TemporaryDirectory(prefix='tapas-retail-') as temp:
        path = Path(temp)
        (path / 'check.tap').write_text(source)
        for start in range(0, len(cases), 80):
            batch = path / 'cases.json'
            batch.write_text(json.dumps(cases[start:start + 80]))
            run = subprocess.run([str(binary), str(path / 'check.tap')], input=str(batch) + '\n',
                                 capture_output=True, text=True, timeout=120)
            if run.returncode:
                raise RuntimeError(run.stdout + run.stderr)
            failures.extend(json.loads(run.stdout))
    return failures


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT / 'external/tau-bench')
    parser.add_argument('--tapas', type=Path, default=ROOT / 'build/bin/tapas')
    parser.add_argument('--report', type=Path, default=ROOT / 'tmp/external/tau-bench-tapas/results.json')
    args = parser.parse_args()
    source = args.source.resolve(); retail = source / 'tau_bench/envs/retail'
    head = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()
    if head != PIN:
        raise SystemExit(f'Expected pinned commit {PIN}, got {head}')
    manifest = json.loads((HERE / 'SOURCE.json').read_text())
    for filename, expected_hash in manifest['sha256'].items():
        if hashlib.sha256((source / filename).read_bytes()).hexdigest() != expected_hash:
            raise SystemExit(f'Upstream source changed: {filename}')
    tools = upstream_tools(source)
    data = {name: json.loads((retail / 'data' / (name + '.json')).read_text()) for name in ('users', 'orders', 'products')}
    with tempfile.TemporaryDirectory(prefix='tapas-retail-domain-') as temp:
        path = Path(temp); (path / 'data.json').write_text(json.dumps(data))
        (path / 'domain.tap').write_text('import ' + str(HERE / 'domain.tap') + ' as d\nlet db = json::parse(io::read_text(str(input())))\nassert(d::Domain(db))\nprint(json::stringify(d::facts(db)))\n')
        check = subprocess.run([str(args.tapas.resolve()), str(path / 'domain.tap')], input=str(path / 'data.json') + '\n', capture_output=True, text=True, timeout=120)
        if check.returncode:
            raise RuntimeError(check.stdout + check.stderr)
        domain = json.loads(check.stdout)
    cases, counts = [], {}
    for split in ('train', 'dev', 'test'):
        tasks = read_tasks(retail / f'tasks_{split}.py')
        counts[split] = len(tasks)
        for i, task in enumerate(tasks):
            db = scoped(data, task['actions'])
            for j, action in enumerate(task['actions']):
                cases.append(invoke(tools[action['name']], db, action, f'{split}/{i}/{j}'))
    cases.extend(edge_cases(data, tools))
    # Every tool, including the independent safe calculator parser, is checked.
    checked = cases
    failures = run_tapas(checked, args.tapas.resolve())
    # Reject tampered output AND tampered state, not merely self-consistent fixtures.
    probes = []
    for c in checked:
        if c['call']['name'] in {p['call']['name'] for p in probes}:
            continue
        bad = copy.deepcopy(c); bad['label'] = 'tamper-output/' + c['call']['name']; bad['output'] = {'tampered': True}; probes.append(bad)
    output_detection = run_tapas(probes, args.tapas.resolve())
    state_probes = []
    for c in probes:
        bad = copy.deepcopy(c); bad['output'] = next(x['output'] for x in checked if x['call']['name'] == c['call']['name'])
        bad['label'] = 'tamper-state/' + c['call']['name']; bad['after']['unexpected'] = True; state_probes.append(bad)
    state_detection = run_tapas(state_probes, args.tapas.resolve())
    report = {'upstream_commit': head, 'domain': domain, 'tasks': counts, 'reference_and_edge_calls': len(cases),
              'independently_checked_calls': len(checked), 'tools': dict(Counter(c['call']['name'] for c in checked)),
              'reference_tool_errors': [{'label': c['label'], 'error': c['output']} for c in checked if c['label'].startswith(('train/', 'dev/', 'test/')) and isinstance(c['output'], str) and c['output'].startswith('Error:')],
              'upstream_errors': sum(isinstance(c['output'], str) and c['output'].startswith('Error:') for c in checked),
              'upstream_exceptions': [c['label'] for c in checked if c['exception']],
              'calculator_calls': sum(c['call']['name'] == 'calculate' for c in cases),
              'failures': failures, 'output_tamper_detection': [len(output_detection), len(probes)],
              'state_tamper_detection': [len(state_detection), len(state_probes)],
              'policy_evidence': 'Reference tasks contain tool actions, not explicit user confirmations. Not scored as policy-compliant conversations.'}
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({k: v for k, v in report.items() if k not in ('failures', 'reference_tool_errors')}, ensure_ascii=False, indent=2))
    if failures:
        print(json.dumps(failures[:5], ensure_ascii=False, indent=2))
    return int(bool(failures) or len(output_detection) != len(probes) or len(state_detection) != len(state_probes))

if __name__ == '__main__':
    sys.exit(main())
