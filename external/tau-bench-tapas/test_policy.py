#!/usr/bin/env python3
"""Policy evidence tests. Fixtures call original upstream tools, then audit in Tapas."""
import argparse
import copy
import json
from pathlib import Path
import subprocess
import tempfile
from reproduce import HERE, ROOT, upstream_tools, scoped, invoke, edge_cases

REVIEW = dict(grounded=True, objective=True, fulfillment_claims=True, requires_authentication=True)


def fixtures(data, tools):
    edges = edge_cases(data, tools)
    mutations = ('cancel_pending_order', 'modify_user_address', 'modify_pending_order_address',
                 'modify_pending_order_payment', 'modify_pending_order_items',
                 'return_delivered_order_items', 'exchange_delivered_order_items')
    tests = []
    def add(label, events, expected=True, field=None, profile='wiki', unknown=False):
        tests.append(dict(label=label, events=copy.deepcopy(events), expected=expected,
                          field=field or '', profile=profile, unknown=unknown))
    def valid(c):
        db = copy.deepcopy(c['before'])
        a = c['call']['kwargs']
        uid = a.get('user_id') or db['orders'][a['order_id']]['user_id']
        auth = {'name': 'find_user_id_by_email', 'kwargs': {'email': db['users'][uid]['email']}}
        events = [dict(kind='request', reviewed=True, supported=True, human_requested=False, email_unavailable=False)]
        record = invoke(tools[auth['name']], db, auth, 'auth')
        events.append(dict(record, kind='tool', turn=1))
        if 'order_id' in a:
            read = {'name': 'get_order_details', 'kwargs': {'order_id': a['order_id']}}
            events.append(dict(invoke(tools[read['name']], db, read, 'status'), kind='tool', turn=2))
        events.extend([dict(kind='proposal', turn=3, call=copy.deepcopy(c['call']), details_reviewed=True,
                            all_items_reminded=True, review=REVIEW), dict(kind='user', text='yes')])
        events.append(dict(invoke(tools[c['call']['name']], db, c['call'], 'mutation'), kind='tool', turn=4))
        return events
    bases = {}
    for name in mutations:
        c = next(c for c in edges if c['call']['name'] == name and not c['exception']
                 and isinstance(c['output'], dict) and c['label'].endswith(('/base', '/valid', '/gift-funded')))
        events = valid(c); bases[name] = events
        add('valid/' + name, events)
    base = bases['exchange_delivered_order_items']
    e = [dict(kind='assistant', turn=1, review=REVIEW)]
    add('unauthenticated-information-in-reply', e, False, 'authenticated')
    e = copy.deepcopy(base); del e[-3]['review']['requires_authentication']
    add('unknown-reply-authentication-scope', e, False, 'authenticated', unknown=True)
    e = copy.deepcopy(base); del e[1]; add('missing-auth', e, False, 'authenticated')
    e = copy.deepcopy(base); e[1]['output'] = 'missing'; add('forged-auth-output', e, False)
    e = copy.deepcopy(base)
    # Authenticate as another genuine profile and retain the real order request.
    other = next(uid for uid in data['users'] if uid not in e[-1]['before']['users'])
    other_call = {'name': 'find_user_id_by_email', 'kwargs': {'email': data['users'][other]['email']}}
    other_db = scoped(data, [other_call]); e[1] = dict(invoke(tools[other_call['name']], other_db, other_call, 'other-auth'), kind='tool', turn=1)
    add('other-user-order', e, False, 'same_user')
    # Separate genuine second identity switch.
    e = copy.deepcopy(base); e.insert(2, dict(invoke(tools[other_call['name']], other_db, other_call, 'switch'), kind='tool', turn=9))
    add('identity-switch', e, False, 'same_user')
    e = copy.deepcopy(base); e[-2]['text'] = 'no'; add('explicit-no', e, False, 'confirmation')
    e = copy.deepcopy(base); e[-2]['text'] = 'sounds good'; add('implicit-consent', e, False, 'confirmation')
    e = copy.deepcopy(base); e[-3]['call']['kwargs']['payment_method_id'] = 'different'; add('stale-consent-parameters', e, False, 'confirmation')
    e = copy.deepcopy(base); e[-3]['details_reviewed'] = False; add('unreviewed-details', e, False, 'confirmation', unknown=True)
    e = copy.deepcopy(base); e.append(dict(e[-1], turn=5)); add('consent-consumed', e, False, 'confirmation')
    e = copy.deepcopy(base); del e[2]; add('missing-status', e, False, 'status_checked')
    e = copy.deepcopy(base); e[2]['output']['status'] = 'pending'; add('stale-status', e, False, 'status_checked')
    e = copy.deepcopy(base); e[-3]['all_items_reminded'] = False; add('all-items-reminder', e, False, 'all_items_reminded')
    e = copy.deepcopy(base); e[-1]['turn'] = e[2]['turn']; add('parallel-tools', e, False, 'serialized')
    e = copy.deepcopy(base); e[-1]['turn'] = e[-3]['turn']; add('reply-with-tool', e, False, 'serialized')
    e = copy.deepcopy(base); del e[-1]['turn']; add('missing-turn-evidence', e, False, 'serialized', unknown=True)
    for field in ('grounded', 'objective', 'fulfillment_claims'):
        e = copy.deepcopy(base); e[-3]['review'][field] = False; add('semantic-false/' + field, e, False, field)
        e = copy.deepcopy(base); del e[-3]['review'][field]; add('semantic-unknown/' + field, e, False, field, unknown=True)
    c = next(c for c in edges if c['label'].endswith('/same-option-accepted-upstream') and c['call']['name'].startswith('exchange'))
    add('policy-rejects-same-option', valid(c), False, 'different_options')
    # Scope contradictions are source profiles, not one invented policy.
    transfer_call = {'name': 'transfer_to_human_agents', 'kwargs': {'summary': 'Human requested'}}
    transfer = dict(invoke(tools[transfer_call['name']], base[1]['after'], transfer_call, 'human'), kind='tool', turn=2)
    for profile in ('wiki', 'rules', 'tool_description'):
        e = copy.deepcopy(base[:2]); e[0].update(human_requested=True); e.append(transfer)
        add('supported-human-request/' + profile, e, profile == 'tool_description',
            None if profile == 'tool_description' else 'transfer_scope', profile)
        e[0].update(supported=False)
        add('unsupported-transfer/' + profile, e, profile != 'rules',
            'transfer_scope' if profile == 'rules' else None, profile)
    e = copy.deepcopy(base[:2]); e[0]['supported'] = False; add('missing-required-transfer', e, False, 'transfer_scope')
    # Name/ZIP fallback is only a restriction in the tool-description profile.
    uid = base[1]['output']; u = base[1]['before']['users'][uid]
    lookup = {'name': 'find_user_id_by_name_zip', 'kwargs': {**u['name'], 'zip': u['address']['zip']}}
    e = copy.deepcopy(base[:1]); e.append(dict(invoke(tools[lookup['name']], base[1]['before'], lookup, 'name'), kind='tool', turn=1))
    add('name-zip/wiki', e)
    add('name-zip/no-fallback-evidence', e, False, 'lookup_fallback', 'tool_description')
    e[0]['email_unavailable'] = True; add('name-zip/email-unavailable', e, profile='tool_description')
    return tests


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT / 'external/tau-bench')
    parser.add_argument('--tapas', type=Path, default=ROOT / 'build/bin/tapas')
    parser.add_argument('--report', type=Path, default=ROOT / 'tmp/external/tau-bench-tapas/policy_results.json')
    args = parser.parse_args()
    source = args.source.resolve(); retail = source / 'tau_bench/envs/retail'
    tools = upstream_tools(source)
    data = {name: json.loads((retail / 'data' / (name + '.json')).read_text()) for name in ('users', 'orders', 'products')}
    tests = fixtures(data, tools)
    script = """import POLICY as p
let cases = json::parse(io::read_text(str(input())))
let results = []
for (let c in cases) {
    let check = p::audit(c['events'], c['profile'])
    let matches = check['passed'] == c['expected']
    if (c['field'] != '' and check['facts'][c['field']]) { matches = false }
    if (c['unknown'] and len(check['unknown']) == 0) { matches = false }
    append(results, {'label': c['label'], 'matches': matches, 'audit': check})
}
print(json::stringify(results))
""".replace('POLICY', str(HERE / 'policy.tap'))
    with tempfile.TemporaryDirectory(prefix='tapas-retail-policy-') as temp:
        path = Path(temp); (path / 'test.tap').write_text(script); (path / 'tests.json').write_text(json.dumps(tests))
        run = subprocess.run([str(args.tapas.resolve()), str(path / 'test.tap')], input=str(path / 'tests.json') + '\n', capture_output=True, text=True, timeout=120)
        if run.returncode:
            raise RuntimeError(run.stdout + run.stderr)
        results = json.loads(run.stdout)
    failures = [x for x in results if not x['matches']]
    report = {'tests': len(results), 'passed': len(results) - len(failures), 'failures': failures,
              'cases': [{'label': x['label'], 'policy_passed': x['audit']['passed'], 'unknown': x['audit']['unknown']} for x in results]}
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps(report if failures else {k:v for k,v in report.items() if k != 'cases'}, ensure_ascii=False, indent=2))
    return int(bool(failures))

if __name__ == '__main__':
    raise SystemExit(main())
