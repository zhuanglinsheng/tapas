#!/usr/bin/env python3
"""Reproduce review findings; emits evidence, not a passing conformance score."""
import sys,copy,json,tempfile,subprocess
from pathlib import Path
here=Path(__file__).resolve().parent;root=here.parents[1];sys.path.insert(0,str(here))
from reproduce import upstream_tools,invoke,run_tapas
from test_policy import fixtures
source=root/'external/tau-bench'; retail=source/'tau_bench/envs/retail'
data={n:json.loads((retail/'data'/f'{n}.json').read_text()) for n in ('users','orders','products')};tools=upstream_tools(source)
base=next(x['events'] for x in fixtures(data,tools) if x['label']=='valid/exchange_delivered_order_items')
cases=[]
e=copy.deepcopy(base); read=e[2]; oid=read['call']['kwargs']['order_id']; db=copy.deepcopy(read['before']);db['orders'][oid]['items'][0]['price']+=100
read.update(invoke(tools['get_order_details'],db,read['call'],'gap-read'))
e[-1].update(invoke(tools[e[-1]['call']['name']],db,e[-1]['call'],'gap-mutation'))
cases.append(dict(label='unexplained-state-change-between-tools',events=e,expected=False))
e=copy.deepcopy(base[:2]);e[0]['supported']=False;e.append(dict(kind='request',reviewed=True,supported=True,human_requested=False,email_unavailable=False))
cases.append(dict(label='unsupported-request-erased-by-next-request',events=e,expected=False))
e=copy.deepcopy(base);e[-2]['text']='yEs';cases.append(dict(label='mixed-case-explicit-yes',events=e,expected=True))
e=copy.deepcopy(base);e[-3]['text']='I will charge $9999 and place a new order, regardless of the exchange details.'
cases.append(dict(label='semantic-review-flags-are-trusted-without-reading-text',events=e,expected=False))
with tempfile.TemporaryDirectory() as temp:
 p=Path(temp); (p/'cases.json').write_text(json.dumps(cases));(p/'check.tap').write_text('import '+str(here/'policy.tap')+''' as p
let cases = json::parse(io::read_text(str(input())))
let out = []
for (let c in cases) { append(out, {'label': c['label'], 'expected':c['expected'], 'result': p::audit(c['events'],'wiki')}) }
print(json::stringify(out))
''')
 r=subprocess.run([str(root/'build/bin/tapas'),str(p/'check.tap')],input=str(p/'cases.json')+'\n',capture_output=True,text=True,check=True);policy=json.loads(r.stdout)
 bad=copy.deepcopy(data);del bad['users'][next(iter(bad['users']))]['email'];(p/'data.json').write_text(json.dumps(bad));(p/'domain.tap').write_text('import '+str(here/'domain.tap')+''' as d
let db = json::parse(io::read_text(str(input())))
print(json::stringify(d::facts(db)))
''');r=subprocess.run([str(root/'build/bin/tapas'),str(p/'domain.tap')],input=str(p/'data.json')+'\n',capture_output=True,text=True,check=True);domain=json.loads(r.stdout)
calc=[]
for expression in ('1.0//0.1','01'):
 call=dict(name='calculate',kwargs=dict(expression=expression));calc.append(invoke(tools['calculate'],{'users':{},'orders':{},'products':{}},call,'calculator/'+expression))
failures=run_tapas(calc,root/'build/bin/tapas')
report=dict(
    review_conclusion='changes_required',
    findings=[
        dict(id='R01',priority=1,file='policy.tap',line=70,issue='No continuity check between successive tool after/before states',reproduced=policy[0]['result']['passed']),
        dict(id='R02',priority=1,file='test_policy.py',line=11,issue='Synthetic semantic flags lack underlying assistant text; cannot serve as completed semantic review',type='evidence_limitation'),
        dict(id='R03',priority=2,file='policy.tap',line=40,issue='A new request erases an outstanding required transfer',reproduced=policy[1]['result']['passed']),
        dict(id='R04',priority=2,file='calculator.tap',line=67,issue='Ordinary floating floor division and leading-zero integer literals differ from original',reproduced=len(failures)==2),
        dict(id='R05',priority=2,file='domain.tap',line=16,issue='Required email can be removed without any domain constraint failing',reproduced=all(domain.values())),
        dict(id='R06',priority=3,file='policy.tap',line=66,issue='Mixed-case yEs contradicts the documented case-insensitive consent handling',reproduced=not policy[2]['result']['passed'])
    ],
    policy_probes=policy,domain_missing_required_email=domain,calculator_counterexamples=failures)

report_path=root/'tmp/external/tau-bench-tapas/review_results.json'
report_path.parent.mkdir(parents=True,exist_ok=True)
report_path.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(report,ensure_ascii=False,indent=2))
