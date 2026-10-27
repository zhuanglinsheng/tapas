"""Replay saved query events without calling a solver or executing snapshots.

Inference opportunities describe recorded trajectories, not the performance of
an implemented replacement: reusing an old UNSAT proof may forgo a new core.
"""
from __future__ import annotations
import argparse
from collections import Counter, defaultdict
import gzip
import hashlib
import json
from pathlib import Path
import statistics
from prior import ProductPrior, JointPrior


def analyze(directory, strategies):
    metadata=json.loads((directory/'metadata.json').read_text())
    results=[]
    for line in gzip.open(directory/'rows.jsonl.gz','rt'):
        row=json.loads(line)
        if row['strategy'] not in strategies:continue
        desc=metadata['problems'][row['problem']]['prior']
        if desc['kind']=='product':
            prior=ProductPrior(desc['names'],desc['domains'],desc['probabilities'])
        else:
            prior=JointPrior(desc['names'],desc['domains'],{tuple(a['x']):a['probability'] for a in desc['atoms']})
        cache=[];sat=[];counts=Counter();widths=[];gains=[];raw_widths=[]
        for event in row['events']:
            kind=event['kind']
            if kind in ('business','discovery'):
                bindings=tuple(tuple(v) for v in event['region']);query=frozenset(bindings)
                if kind=='discovery':
                    widths.append(len(bindings));counts[event['status']]+=1
                    if not bindings:counts['empty']+=1
                    infer_unsat=any(old.issubset(query) for old in cache)
                    infer_sat=any(query.issubset(old) for old in sat)
                    assert not (infer_sat and infer_unsat)
                    if infer_unsat:
                        counts['infer_unsat']+=1
                        assert event['status']=='unsat'
                    if infer_sat:
                        counts['infer_sat']+=1
                        assert event['status']=='sat'
                    if event['status']=='unsat':
                        core=tuple(tuple(v) for v in event['core'])
                        gain=prior.uncovered(core,tuple(tuple(sorted(r)) for r in cache))
                        gains.append(gain)
                        positive=gain>1e-15
                        counts['new_positive_core' if positive else 'zero_gain_core']+=1
                        if infer_unsat and positive:counts['infer_unsat_new_core']+=1
                if event['status']=='sat':sat.append(query)
                elif kind=='business':raw_widths.append(len(event['core']))
            elif kind=='proof':
                region=frozenset(tuple(v) for v in event['region'])
                if not any(old.issubset(region) for old in cache):
                    cache=[old for old in cache if not region.issubset(old)];cache.append(region)
            elif kind=='evict':
                cache.remove(frozenset(tuple(v) for v in event['region']))
        final_mass=1-prior.uncovered((),tuple(tuple(sorted(r)) for r in cache))
        assert abs(final_mass-row['cache_mass'])<1e-10
        assert counts['sat']+counts['unsat']==row['discovery_calls']
        assert row['business_calls']+row['discovery_calls']==row['total_calls']
        unique=len({tuple(x) for x in row['samples']})
        results.append(dict(problem=row['problem'],strategy=row['strategy'],seed=row['seed'],samples=row['sample_count'],
                            candidates=row['candidates'],business=row['business_calls'],discovery=row['discovery_calls'],total=row['total_calls'],
                            unique_accepted=unique,repeated_accepted=len(row['samples'])-unique,
                            cache_mass=row['cache_mass'],cache_size=len(row['cache']),memo_hits=row.get('memo_hits',0),
                            active_searches=row['active_searches'],logical_steps=row.get('search_steps',0),
                            initial_core_width=statistics.mean(raw_widths) if raw_widths else 0,
                            singleton_business_cores=sum(w==1 for w in raw_widths),
                            discovery_width=statistics.mean(widths) if widths else 0,
                            discovery_new_mass=sum(gains),
                            **{k:counts[k] for k in ('sat','unsat','empty','infer_unsat','infer_sat','new_positive_core','zero_gain_core','infer_unsat_new_core')}))
    groups=defaultdict(list)
    for row in results:groups[row['problem'],row['samples'],row['strategy']].append(row)
    aggregates=[]
    for (problem,samples,strategy),rows in groups.items():
        averages={k:statistics.mean(r[k] for r in rows) for k in rows[0] if k not in ('problem','samples','seed','strategy')}
        aggregates.append(dict(problem=problem,samples=samples,strategy=strategy,n=len(rows),means=averages))
    return dict(source=str(directory),metadata_sha256=hashlib.sha256((directory/'metadata.json').read_bytes()).hexdigest(),
                rows_sha256=hashlib.sha256((directory/'rows.jsonl.gz').read_bytes()).hexdigest(),runs=results,groups=aggregates)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directories',type=Path,nargs='+')
    parser.add_argument('--strategies',nargs='+',default=['raw','mass_optimized','adaptive'])
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if args.output.exists():parser.error('Output must be new; do not replace prior analyses')
    result=dict(protocol='Post-hoc replay; only preceding certificates used. Positive gain threshold 1e-15 is descriptive, not a safety proof. Repeated accepted inputs must remain repeated in the output distribution.',
                analysis_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (Path(__file__),Path(__file__).with_name('prior.py'))},
                suites=[analyze(path,args.strategies) for path in args.directories])
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    for suite in result['suites']:
        for group in suite['groups']:
            if group['samples']!=max(g['samples'] for g in suite['groups'] if g['problem']==group['problem']):continue
            m=group['means']
            print(group['problem'],group['samples'],group['strategy'],
                  'B/D/total',*[round(m[k],3) for k in ('business','discovery','total')],
                  'inferU/S',*[round(m[k],3) for k in ('infer_unsat','infer_sat')],
                  'inferredU_with_new_core',round(m['infer_unsat_new_core'],3),
                  'positive_core',round(m['new_positive_core'],3),'unique_accepted',round(m['unique_accepted'],3))

if __name__=='__main__':main()
