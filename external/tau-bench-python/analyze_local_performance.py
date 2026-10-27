"""Audit stored optimization traces and summarize paired CPU/wall measurements.

No solver or historical implementation is executed. The stopped initial Retail
run contributes only its fully completed exchange cohort; the seven preliminary
modify_items rows are excluded in favor of the explicit larger-limit rerun.
"""
import argparse
from collections import defaultdict
import gzip
import hashlib
import json
from pathlib import Path
import statistics
from run_experiments import interval


def trace(row):
    return [{k:v for k,v in event.items() if k!='seconds'} for event in row['events']
            if event['kind'] in ('business','discovery','memo','sat_cache','proof','evict')]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root',type=Path)
    args=parser.parse_args();root=args.root
    cohorts=[('retail',lambda r:r['problem']=='retail_exchange_o8_k2'),
             ('retail_extended_limit',lambda r:True),('synthetic',lambda r:True)]
    rows=[]
    for name,keep in cohorts:
        with gzip.open(root/name/'rows.jsonl.gz','rt') as file:
            rows.extend(dict(r,cohort=name) for line in file if keep(r:=json.loads(line)))
    groups=defaultdict(dict)
    for r in rows:
        key=r['problem'],r['sample_count'],r['seed']
        assert r['status']=='complete'
        assert r['strategy'] not in groups[key]
        groups[key][r['strategy']]=r
    comparisons=[('legacy_mass','mass_optimized'),('legacy_raw','raw'),
                 ('mass_python','mass_optimized'),('raw_python','raw'),
                 ('mass_no_miss_cache','mass_optimized')]
    paired=defaultdict(list);trace_checks=0
    for (problem,count,seed),group in groups.items():
        assert len(group)==7,(problem,count,seed)
        reference=next(iter(group.values()))
        for r in group.values():
            assert (r['samples'],r['positions'])==(reference['samples'],reference['positions'])
        for before,after in comparisons:
            a,b=group[before],group[after]
            assert trace(a)==trace(b),(problem,count,seed,before,after)
            trace_checks+=1
            paired[problem,count,before,after].append((a,b))
    summary=[]
    for (problem,count,before,after),pairs in sorted(paired.items()):
        row=dict(problem=problem,samples=count,before=before,after=after,seeds=len(pairs))
        for metric in ('cpu_seconds','wall_seconds'):
            old=statistics.mean(a[metric] for a,b in pairs)
            new=statistics.mean(b[metric] for a,b in pairs)
            row[metric]=dict(before_mean=old,after_mean=new,reduction=1-new/old,
                paired_seconds_saved=interval([a[metric]-b[metric] for a,b in pairs]))
        row.update(total_calls=statistics.mean(b['total_calls'] for a,b in pairs),
                   candidates=statistics.mean(b['candidates'] for a,b in pairs),
                   proof_miss_hits=statistics.mean(b.get('proof_miss_hits',0) for a,b in pairs))
        summary.append(row)
    additional=[]
    for name,expected in [('adaptive_check',60),('return_miss_recheck',20),('small_local_recheck',360)]:
        details=json.loads((root/name/'summary.json').read_text())
        assert details['runs']==expected and details['paired_sequences_verified']
        extra=defaultdict(dict)
        with gzip.open(root/name/'rows.jsonl.gz','rt') as file:
            for line in file:
                r=json.loads(line)
                extra[r['problem'],r['sample_count'],r['seed']][r['strategy']]=r
        checked=0
        for group in extra.values():
            reference=next(iter(group.values()))
            for r in group.values():
                assert (r['samples'],r['positions'])==(reference['samples'],reference['positions'])
            if name!='adaptive_check':
                for strategy,r in group.items():
                    if strategy!='mass_optimized':
                        assert trace(r)==trace(group['mass_optimized'])
                        checked+=1
        additional.append(dict(cohort=name,runs=expected,paired_cases=len(extra),
            exact_event_trace_checks=checked,original_tool_checks=details['original_tool_checks']))
    output=dict(complete_runs=len(rows),paired_cases=len(groups),exact_event_trace_checks=trace_checks,
                analysis_script_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                additional_protocols=additional,total_strategy_runs=len(rows)+sum(c['runs'] for c in additional),
                excluded_preliminary_modify_items_runs=7,
                comparison='Paired nonadaptive strategies; identical query, memo, acceptance and cache event traces, excluding timing fields',
                groups=summary)
    (root/'analysis.json').write_text(json.dumps(output,indent=2)+'\n')
    print(json.dumps({k:v for k,v in output.items() if k!='groups'}))


if __name__=='__main__':main()
