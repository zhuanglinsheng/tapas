"""Comprehensive three-family accounting from paired, saved native-CP trials.

Historical cohorts remain separate. Incomplete trials never earn speed/call wins.
The active implementation in the main table is fixed globally, never per case.
"""
import argparse
from collections import Counter,defaultdict
import csv
import gzip
import hashlib
import html
import json
from pathlib import Path
from statistics import mean
from run_experiments import interval

FAMILIES={'rejection':'Rejection sampling','raw':'Passive native-core cache'}


def metrics(row):
    counts=Counter();seconds=Counter()
    for e in row['events']:
        if e['kind'] in ('business','discovery'):
            counts[e['kind']+'_'+e['status']]+=1
            for k,v in e.get('seconds',{}).items():seconds[k]+=v;seconds[e['kind']+'_'+k]+=v
    b=sum(counts['business_'+s] for s in ('sat','unsat','unknown'))
    d=sum(counts['discovery_'+s] for s in ('sat','unsat','unknown'))
    assert b==row['business_calls'] and d==row['discovery_calls'] and b+d==row['total_calls']
    assert b+row['cache_hits']+row.get('sat_cache_hits',0)==row['candidates']
    if row['strategy']=='rejection':assert b==row['candidates'] and d==0
    if row['status']=='complete':assert len(row['samples'])==row['requested']
    wall=row['wall_seconds'];oracle=sum(seconds[k] for k in ('build','solve','extract'))
    return dict(candidates=row['candidates'],accepted=len(row['samples']),
        business_calls=b,discovery_calls=d,total_calls=b+d,
        **{kind+'_'+s:counts[kind+'_'+s] for kind in ('business','discovery') for s in ('sat','unsat','unknown')},
        cpu_seconds=row['cpu_seconds'],wall_seconds=wall,
        **{k+'_seconds':seconds[k] for k in ('build','solve','extract','business_solve','discovery_solve')},
        business_oracle_seconds=sum(seconds['business_'+k] for k in ('build','solve','extract')),
        discovery_oracle_seconds=sum(seconds['discovery_'+k] for k in ('build','solve','extract')),
        other_online_wall_seconds=max(0.,wall-oracle),
        cache_hits=row['cache_hits'],sat_cache_hits=row.get('sat_cache_hits',0),
        memo_hits=row.get('memo_hits',0),search_steps=row.get('search_steps',0),
        active_nogoods=len(row['cache']),nogood_literals=sum(map(len,row['cache'])),
        sat_proofs=row.get('proof_store',{}).get('sat_entries',0),active_searches=row.get('active_searches',0),
        signal_hypotheses=row.get('context_adaptation',{}).get('retained_hypotheses',0),
        diagnostics_cpu_seconds=row.get('diagnostics',{}).get('cpu_seconds',0),
        signal_seconds=row.get('context_adaptation',{}).get('signal_seconds',0),
        gate_seconds=row.get('context_adaptation',{}).get('gate_seconds',0))


def analyze(cohorts):
    grouped=defaultdict(list);paired=defaultdict(list);trials=[];provenance=[];references=[]
    for label,folder in cohorts:
        meta=json.loads((folder/'metadata.json').read_text());pending={};count=0;case_count=0
        expected=len(meta['protocol']['strategies'])
        with gzip.open(folder/'rows.jsonl.gz','rt') as f:
            for line in f:
                row=json.loads(line);count+=1;m=metrics(row)
                key=row['problem'],row['requested'],row['seed'];case=pending.setdefault(key,{})
                case[row['strategy']]=(row,m)
                if len(case)!=expected:continue
                assert 'raw' in case
                raw,_=case['raw']
                if raw['status']=='complete':
                    references.append(dict(cohort=label,problem=raw['problem'],samples=raw['requested'],seed=raw['seed'],
                        ideal_rejection_calls=raw['candidates'],
                        note='Q=T for rejection completing this exact stream. Derived reference, not an observed completed trial or time estimate.'))
                for name,(r,v) in case.items():
                    n=min(len(r['samples']),len(raw['samples']))
                    assert (r['samples'][:n],r['positions'][:n])==(raw['samples'][:n],raw['positions'][:n])
                    if r['status']==raw['status']=='complete':assert r['candidates']==raw['candidates']
                    # A censored run may produce a shorter accepted prefix.
                    grouped[label,r['problem'],r['requested'],name].append((r['status'],v))
                    trials.append(dict(cohort=label,problem=r['problem'],samples=r['requested'],seed=r['seed'],
                        strategy=name,status=r['status'],**v))
                    for base in ('raw','rejection'):
                        if base not in case:continue
                        br,bv=case[base]
                        if br['status']!=r['status'] or r['status']!='complete':continue
                        su=bv['business_unsat']-v['business_unsat'];ss=bv['business_sat']-v['business_sat']
                        bu=bv['business_unknown']-v['business_unknown']
                        net=bv['total_calls']-v['total_calls']
                        assert net==su+ss+bu+bv['discovery_calls']-v['discovery_calls']
                        paired[label,r['problem'],r['requested'],base,name].append(dict(
                            total_calls_saved=net,business_calls_saved=bv['business_calls']-v['business_calls'],
                            business_unsat_saved=su,business_sat_saved=ss,
                            cpu_seconds_saved=bv['cpu_seconds']-v['cpu_seconds'],
                            wall_seconds_saved=bv['wall_seconds']-v['wall_seconds'],
                            solve_seconds_saved=bv['solve_seconds']-v['solve_seconds'],
                            calls_saved_percent=100*net/bv['total_calls'] if bv['total_calls'] else 0,
                            cpu_speedup=bv['cpu_seconds']/v['cpu_seconds'] if v['cpu_seconds'] else 0))
                case_count+=1;del pending[key]
        assert not pending
        provenance.append(dict(cohort=label,runs=count,paired_cases=case_count,
            path=str(folder.resolve()),timing=meta.get('timing'),protocol=meta['protocol'],
            metadata_sha256=hashlib.sha256((folder/'metadata.json').read_bytes()).hexdigest(),
            log_sha256=hashlib.sha256((folder/'rows.jsonl.gz').read_bytes()).hexdigest()))
    groups=[]
    for (cohort,p,n,s),values in grouped.items():
        complete=[v for status,v in values if status=='complete']
        observed=[v for status,v in values]
        groups.append(dict(cohort=cohort,problem=p,samples=n,strategy=s,
            family=FAMILIES.get(s,'Active exploration'),attempts=len(values),completed=len(complete),
            statuses=dict(Counter(status for status,v in values)),
            complete={k:interval([v[k] for v in complete]) for k in observed[0]} if complete else {},
            observed={k:mean(v[k] for v in observed) for k in observed[0]}))
    comparisons=[dict(cohort=c,problem=p,samples=n,baseline=b,strategy=s,
        **{k:interval([v[k] for v in values]) for k in values[0]}) for (c,p,n,b,s),values in paired.items()]
    return dict(cohorts=provenance,groups=groups,comparisons=comparisons,trials=trials,rejection_stream_references=references)


def render(report,out,primary,plots):
    comparisons={(g['cohort'],g['problem'],g['samples'],g['baseline'],g['strategy']):g for g in report['comparisons']}
    refs=defaultdict(list)
    for r in report['rejection_stream_references']:refs[r['cohort'],r['problem'],r['samples']].append(r['ideal_rejection_calls'])
    flat=[]
    for g in report['groups']:
        row={k:g[k] for k in ('cohort','problem','samples','family','strategy','attempts','completed')}
        row['statuses']=json.dumps(g['statuses'],sort_keys=True)
        reference=refs.get((g['cohort'],g['problem'],g['samples']))
        if reference:row['derived_rejection_calls_same_stream_mean']=mean(reference)
        for k,v in g['complete'].items():
            for stat in ('mean','low','high','n'):row[k+'_'+stat]=v[stat]
        for k,v in g['observed'].items():row['observed_'+k]=v
        for base in ('raw','rejection'):
            paired=comparisons.get((g['cohort'],g['problem'],g['samples'],base,g['strategy']))
            if paired:
                for k,v in paired.items():
                    if isinstance(v,dict) and 'mean' in v:
                        for stat in ('mean','low','high','n'):row['vs_'+base+'_'+k+'_'+stat]=v[stat]
        flat.append(row)
    keys=list(dict.fromkeys(k for row in flat for k in row))
    with (out/'comparison.csv').open('w',newline='') as f:
        w=csv.DictWriter(f,keys);w.writeheader();w.writerows(flat)
    keys=list(report['trials'][0])
    with gzip.open(out/'trials.csv.gz','wt',newline='') as f:
        w=csv.DictWriter(f,keys);w.writeheader();w.writerows(report['trials'])
    def val(g,k,scale=1):
        return f"{g['complete'][k]['mean']*scale:.3f}" if k in g['complete'] else '—'
    tables=[];md=[]
    for main in (True,False):
        selected=[g for g in report['groups'] if (g['strategy'] in ('rejection','raw',primary))==main]
        rows=[]
        for g in sorted(selected,key=lambda g:(g['cohort'],g['problem'],g['samples'],g['strategy'])):
            comp=comparisons.get((g['cohort'],g['problem'],g['samples'],'raw',g['strategy']))
            net='—'
            if comp:
                gain=comp['total_calls_saved'];net=f"{gain['mean']:.2f}"
                net+=(f" [{gain['low']:.2f}, {gain['high']:.2f}]" if gain['low'] is not None
                      else f" (n={gain['n']}; no interval)")
            cells=[g['cohort'],g['problem'],g['samples'],g['strategy'],f"{g['completed']}/{g['attempts']}",
                val(g,'business_calls'),val(g,'discovery_calls'),val(g,'total_calls'),net,
                val(g,'cpu_seconds',1000),val(g,'wall_seconds',1000),val(g,'solve_seconds',1000),
                val(g,'other_online_wall_seconds',1000)]
            if g['completed']<g['attempts']:
                cells.append(f"Observed incomplete/mixed trials: accepted={g['observed']['accepted']:.1f}; Q={g['observed']['total_calls']:.1f}; wall={g['observed']['wall_seconds']:.3f}s; {g['statuses']}")
            else:cells.append('')
            rows.append('<tr>'+''.join('<td>'+html.escape(str(v))+'</td>' for v in cells)+'</tr>')
            if main:md.append('| '+' | '.join(str(v) for v in cells[:12])+' |')
        heading='Three primary algorithms' if main else 'Active algorithm ablations'
        tables.append('<h2>'+heading+'</h2><table><thead><tr>'+''.join('<th>'+h+'</th>' for h in
            ['Cohort','Problem','N','Implementation','Complete','Business B','Exploration D','Total Q','Q saved vs raw [interval]',
             'CPU ms','Wall ms','CP solve ms','Other online wall ms','Incomplete observation'])+'</tr></thead><tbody>'+''.join(rows)+'</tbody></table>')
    note=f"Three families: rejection, passive native-core cache, active exploration. Fixed main active implementation: {primary}. Other active versions are ablations. All Q=B+D. Complete means requested N outputs. Intervals use paired seeds per case; no pooling of configurations. Blank values are incomplete, not zero. Final diagnostic coverage is excluded from online timing. Full SAT/UNSAT/UNKNOWN and construction/extraction/local breakdowns are in CSV/JSON. Local proof/memo operations are not CP calls. Soft wall censoring may overrun an in-flight step."
    (out/'report.html').write_text('<!doctype html><meta charset="utf-8"><title>Three sampling algorithms</title>'
        '<style>body{font:14px system-ui;margin:22px}table{border-collapse:collapse}td,th{border:1px solid #ccc;padding:5px;white-space:nowrap}th{position:sticky;top:0;background:#eee}input{padding:8px;width:60%}</style>'
        '<h1>Three sampling algorithms: solver calls and time</h1><p>'+html.escape(note)+'</p>'
        '<p><a href="comparison.csv">All metrics CSV</a> · <a href="comparison.json">Intervals and provenance JSON</a></p>'
        '<input id="filter" placeholder="Filter by cohort, problem or implementation">'+''.join(tables)+
        '<script>document.getElementById("filter").oninput=function(){let q=this.value.toLowerCase();document.querySelectorAll("tbody tr").forEach(r=>r.hidden=!r.textContent.toLowerCase().includes(q));}</script>')
    (out/'main_table.md').write_text('# Three sampling algorithms\n\n'+note+'\n\n'
        '| Cohort | Problem | N | Implementation | Complete | B | D | Q | Q saved vs raw | CPU ms | Wall ms | CP ms |\n'
        '|---|---|---:|---|---:|---:|---:|---:|---|---:|---:|---:|\n'+'\n'.join(md)+'\n')
    report['primary_active']=primary;report['note']=note
    report.pop('trials')
    (out/'comparison.json').write_text(json.dumps(report,indent=2)+'\n')
    if plots:plot(report,out,primary)


def plot(report,out,primary):
    import os
    os.environ.setdefault('MPLCONFIGDIR',str(out/'.matplotlib'))
    os.environ.setdefault('XDG_CACHE_HOME',str(out/'.cache'))
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from matplotlib.ticker import LogLocator,MaxNLocator,NullFormatter,FuncFormatter
    groups=report['groups'];index={(g['cohort'],g['problem'],g['samples'],g['strategy']):g for g in groups}
    cohorts=[v['cohort'] for v in report['cohorts']];ncols=3;nrows=(len(cohorts)+2)//3
    fig,axes=plt.subplots(nrows,ncols,figsize=(14,4*nrows),squeeze=False,layout='constrained')
    for ax,cohort in zip(axes.flat,cohorts):
        all_points=[]
        for s,color in [('rejection','#c05736'),(primary,'#246b9e')]:
            points=[]
            for g in groups:
                if g['cohort']!=cohort or g['strategy']!=s or g['completed']!=g['attempts']:continue
                raw=index.get((cohort,g['problem'],g['samples'],'raw'))
                if not raw or raw['completed']!=raw['attempts']:continue
                a,b=g['complete'],raw['complete']
                points.append((a['total_calls']['mean']/b['total_calls']['mean'],a['cpu_seconds']['mean']/b['cpu_seconds']['mean']))
            if points:
                all_points.extend(points)
                ax.scatter(*zip(*points),label=s,color=color,alpha=.65,s=24)
        ax.axvline(1,color='gray',ls='--',lw=.8);ax.axhline(1,color='gray',ls='--',lw=.8)
        ax.set_xscale('log');ax.set_yscale('log');ax.set_title(cohort)
        for dim,axis in enumerate((ax.xaxis,ax.yaxis)):
            values=[1.]+[p[dim] for p in all_points]
            lo,hi=min(values),max(values)
            if hi/lo<1.5:
                # A log axis with only the tick "1" hides small regressions.
                getattr(ax,('set_xscale','set_yscale')[dim])('linear')
                pad=max((hi-lo)*.1,.002)
                getattr(ax,('set_xlim','set_ylim')[dim])(lo-pad,hi+pad)
                axis.set_major_locator(MaxNLocator(nbins=5))
            else:axis.set_major_locator(LogLocator(base=10,subs=(1,2,5),numticks=8))
            axis.set_major_formatter(FuncFormatter(lambda v,pos:f'{v:.4g}'))
            axis.set_minor_formatter(NullFormatter())
        ax.set_xlabel('Total CP calls / passive cache');ax.set_ylabel('CPU time / passive cache')
        if ax.collections:ax.legend(fontsize=8)
        ax.grid(alpha=.15)
    for ax in list(axes.flat)[len(cohorts):]:ax.set_visible(False)
    fig.suptitle('Calls and time are separate outcomes; completed matched cells only')
    fig.savefig(out/'calls_vs_time.png',dpi=170);fig.savefig(out/'calls_vs_time.pdf');plt.close(fig)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cohort',action='append',required=True,help='Unique label=directory, kept separate')
    parser.add_argument('--primary-active',default='mass_b4')
    parser.add_argument('--plots',action='store_true')
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if any('=' not in s or not all(s.split('=',1)) for s in args.cohort):parser.error('Use label=directory for each cohort')
    cohorts=[(s.split('=',1)[0],Path(s.split('=',1)[1])) for s in args.cohort]
    if len({c for c,p in cohorts})!=len(cohorts):parser.error('Unique cohort labels required')
    if args.output.exists() and any(args.output.iterdir()):parser.error('Output must be new/empty')
    args.output.mkdir(parents=True,exist_ok=True)
    source=Path(__file__).read_bytes();(args.output/'compare_sampling_algorithms.py').write_bytes(source)
    report=analyze(cohorts);report['script_sha256']=hashlib.sha256(source).hexdigest()
    count=len(report['trials']);render(report,args.output,args.primary_active,args.plots)
    print(json.dumps(dict(runs=count,groups=len(report['groups']),output=str(args.output.resolve()))))


if __name__=='__main__':main()
