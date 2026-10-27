"""Audit online redundancy triggers and paired net gains from saved event logs."""
import argparse
from collections import defaultdict
import gzip
import hashlib
from itertools import combinations
import json
from pathlib import Path
from analyze_exploration import verify_native_certificates
from prior import matches
from run_experiments import interval


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root',type=Path)
    parser.add_argument('--cohorts',nargs='+',default=['grid','multiguard','packing','retail','hardness'])
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    source=Path(__file__).read_bytes();(args.output/'analyze_context_adaptation.py').write_bytes(source)
    groups=defaultdict(list);comparisons=defaultdict(list);cohorts=[]
    for cohort in args.cohorts:
        root=args.root/cohort;meta=json.loads((root/'metadata.json').read_text())
        summary=json.loads((root/'summary.json').read_text())
        expected=len(meta['protocol']['strategies']);pending={};checkers={}
        checked=triggers=count=0
        with gzip.open(root/'rows.jsonl.gz','rt') as f:
            for line in f:
                r=json.loads(line);count+=1
                checked+=verify_native_certificates(meta,r,checkers)
                business=[]
                for event in r['events']:
                    if event['kind']=='business' and event['status']=='unsat':business.append(set(map(tuple,event['core'])))
                    if event['kind']=='context_gate' and event['decision']=='allowed':
                        hypothesis=set(map(tuple,event['region']))
                        assert any(a.intersection(b)==hypothesis and len(hypothesis)<min(len(a),len(b))
                            for a,b in combinations(business,2)), 'Trigger used no observed native context pair'
                        assert event['score']['support']>=2
                        assert event['score']['opportunity']>max(2,2*event['cost_ratio'])
                        triggers+=1
                key=r['problem'],r['requested'],r['seed'];case=pending.setdefault(key,{})
                case[r['strategy']]=r
                if len(case)!=expected:continue
                raw=case['raw']
                def business_count(row,status):return sum(e['kind']=='business' and e['status']==status for e in row['events'])
                for name,row in case.items():
                    n=len(row['samples']);assert (row['samples'],row['positions'])==(raw['samples'][:n],raw['positions'][:n])
                    if row['status']!=raw['status'] or row['status']!='complete':continue
                    u=business_count(raw,'unsat')-business_count(row,'unsat')
                    s=business_count(raw,'sat')-business_count(row,'sat');d=row['discovery_calls']
                    assert raw['total_calls']-row['total_calls']==u+s-d
                    native=[e for e in row['events'] if e['kind']=='discovery' and e['status']=='unsat']
                    queried={e['candidate'] for e in row['events'] if e['kind']=='business' and e['status']=='unsat'}
                    covered=sum(any(e['candidate']<b['candidate'] and matches(e['core'],tuple(v for i,v in b['region'])) for e in native)
                        for b in raw['events'] if b['kind']=='business' and b['status']=='unsat' and b['candidate'] not in queried)
                    adaptation=row.get('context_adaptation',{})
                    values=dict(net_calls_saved=u+s-d,unsat_saved=u,sat_saved=s,discovery_calls=d,
                        cpu_saved=raw['cpu_seconds']-row['cpu_seconds'],wall_saved=raw['wall_seconds']-row['wall_seconds'],
                        covered_by_prior_discovery=covered,
                        discovery_unknown=sum(e['kind']=='discovery' and e['status']=='unknown' for e in row['events']),
                        signal_seconds=adaptation.get('signal_seconds',0),gate_seconds=adaptation.get('gate_seconds',0),
                        time_stopped=int(row.get('core_refinement',{}).get('time_stopped',False)))
                    groups[cohort,r['problem'],r['requested'],name].append(values)
                for before,after in [('core_scout','context_direct'),('core_timed','context_direct'),
                    ('core_timed','context_gate'),('context_gate','context_direct')]:
                    a,b=case[before],case[after]
                    if a['status']==b['status']=='complete':
                        comparisons[cohort,r['problem'],r['requested'],before,after].append({k:a[k]-b[k] for k in ('total_calls','discovery_calls','cpu_seconds')})
                del pending[key]
        assert not pending and count==summary['runs']
        cohorts.append(dict(cohort=cohort,runs=count,completed=summary['completed'],
            native_answers_independently_checked=checked,online_triggers_checked=triggers,
            original_tool_checks=summary.get('original_tool_checks',0),
            metadata_sha256=hashlib.sha256((root/'metadata.json').read_bytes()).hexdigest()))
    output=dict(cohorts=cohorts,groups=[dict(cohort=c,problem=p,samples=n,strategy=s,
        **{k:interval([v[k] for v in vs]) for k in vs[0]}) for (c,p,n,s),vs in groups.items()],
        comparisons=[dict(cohort=c,problem=p,samples=n,before=a,after=b,
        **{k:interval([v[k] for v in vs]) for k in vs[0]}) for (c,p,n,a,b),vs in comparisons.items()],
        script_sha256=hashlib.sha256(source).hexdigest(),
        note='Intervals per five paired seeds; configurations are not independent trials. Pilot excluded. Timing includes online signal/gate overhead, excludes final diagnostic coverage.')
    (args.output/'attribution.json').write_text(json.dumps(output,indent=2)+'\n')
    print(json.dumps(cohorts))


if __name__=='__main__':main()
