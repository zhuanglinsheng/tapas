#!/usr/bin/env python3
"""Standalone paper experiment: Python + OR-Tools; no Tapas build or imports."""
from __future__ import annotations
import argparse
from collections import Counter, defaultdict
import gzip
import hashlib
import html
import json
import math
import os
from pathlib import Path
import platform
import random
import statistics
import shutil
import sys
import time
import ortools
from oracle import Oracle
from prior import matches
from sampler import CONFIGS, Sampler
from workloads import OPERATIONS, synthetic, load_retail, retail_problem
from upstream import provenance, tools, check
from project import DEFAULT_SOURCE

HERE=Path(__file__).resolve().parent


def interval(values):
    mean=statistics.fmean(values)
    if len(values)<2:return dict(mean=mean,low=None,high=None,n=len(values))
    rng=random.Random(20261021)
    means=sorted(statistics.fmean(rng.choices(values,k=len(values))) for _ in range(2000))
    return dict(mean=mean,low=means[49],high=means[1949],n=len(values),method='paired-seed percentile bootstrap, 2000 resamples')


def exact_distribution(problem, limit):
    if problem.prior.size>limit:return dict(status='not_enumerated',support_size=problem.prior.size)
    target={x:problem.prior.pmf(x) for x in problem.prior.points() if problem.feasible(x)}
    z=math.fsum(target.values())
    return dict(status='exact',z=z,target={x:p/z for x,p in target.items()} if z else {},support_size=problem.prior.size)


def distribution_check(samples,exact):
    if exact['status']!='exact':return dict(status='not_enumerated')
    target=exact['target'];n=len(samples)
    if not n:return dict(status='no_samples')
    counts=Counter(samples)
    if set(counts)-set(target):raise AssertionError('Samples outside the exact feasible support')
    tv=.5*math.fsum(abs(counts[x]/n-p) for x,p in target.items())
    # A descriptive finite-sample reference, not a proof or a universal TV test.
    rng=random.Random(314159);population=list(target);weights=list(target.values());null=[]
    for _ in range(200):
        c=Counter(rng.choices(population,weights=weights,k=n))
        null.append(.5*math.fsum(abs(c[x]/n-p) for x,p in target.items()))
    null.sort()
    return dict(status='empirical',samples=n,tv=tv,iid_reference_tv_95=null[189],
                note='Only largest-horizon rejection samples pooled once per independent seed; strategies/prefixes are not pooled.')


def audit_row(problem,row,exact):
    proofs=set();certified=set();sat_certified={};queries={}
    for event in row['events']:
        kind=event['kind']
        if kind not in ('business','discovery','proof','memo','sat_cache'):
            continue
        bindings=tuple(tuple(v) for v in event['region'])
        if kind in ('business','discovery'):
            queries[bindings]=event['status']
            if event['status']=='unsat':
                certified.add(bindings)
                certified.add(tuple(tuple(v) for v in event['core']))
            elif event['status']=='sat':
                sat_certified.setdefault(bindings,event.get('witness'))
                if event.get('completion') is not None:
                    x=tuple(event['completion']);witness=event['witness']
                    if (len(x)!=len(problem.prior.names) or any(x[i]!=v for i,v in bindings) or not problem.feasible(x)
                        or not problem.check_witness(x,witness)):
                        raise AssertionError('Invalid recorded SAT completion')
                    sat_certified.setdefault(tuple(enumerate(x)),witness)
        elif kind=='memo':
            method=event.get('method','exact')
            evidence=tuple(tuple(v) for v in (event.get('evidence') or ()))
            if method=='exact':
                if queries.get(bindings)!=event['status']:raise AssertionError('Unproved exact memo')
            elif method=='unsat':
                if (event['status']!='unsat' or evidence not in certified or not set(evidence).issubset(bindings)
                    or tuple(tuple(v) for v in event['core'])!=evidence):
                    raise AssertionError('Unproved UNSAT inference')
                certified.add(bindings)
            elif method=='sat':
                if event['status']!='sat' or evidence not in sat_certified or not set(bindings).issubset(evidence):
                    raise AssertionError('Unproved SAT inference')
            else:raise AssertionError('Unknown inference method')
        elif kind=='sat_cache':
            evidence=tuple(tuple(v) for v in event['evidence'])
            x=tuple(v for _,v in bindings)
            if (evidence!=bindings or evidence not in sat_certified or event['witness']!=sat_certified[evidence]
                or len(x)!=len(problem.prior.names) or not problem.feasible(x)
                or not problem.check_witness(x,event['witness'])):
                raise AssertionError('Unproved complete-input witness reuse')
        elif kind=='proof':
            if bindings not in certified:raise AssertionError('Cache proof lacks a preceding UNSAT certificate')
            proofs.add(bindings)
    for kind,key in [('business','business_calls'),('discovery','discovery_calls')]:
        if sum(e['kind']==kind for e in row['events'])!=row[key]:raise AssertionError('Query accounting mismatch')
    if exact['status']=='exact':
        feasible=list(exact['target'])
        if any(any(matches(c,x) for x in feasible) for c in proofs):raise AssertionError('Unsafe cached region')
    if row['discovery_calls']>min(row['config']['core_limit'],math.floor(row['config']['beta']*math.sqrt(row['candidates']))):
        raise AssertionError('Discovery budget exceeded')
    if row['config'].get('charge_memo') and row['search_steps']>min(row['config']['core_limit'],math.floor(row['config']['beta']*math.sqrt(row['candidates']))):
        raise AssertionError('Logical search-step budget exceeded')
    if row['business_calls']+row['cache_hits']+row.get('sat_cache_hits',0)!=row['candidates']:raise AssertionError('Candidate accounting mismatch')
    if sum(e['kind']=='sat_cache' for e in row['events'])!=row.get('sat_cache_hits',0):raise AssertionError('SAT cache accounting mismatch')
    if sum(e['kind']=='memo' for e in row['events'])!=row.get('memo_hits',0):raise AssertionError('Inference accounting mismatch')
    if len(row['samples'])!=len(row['positions']):raise AssertionError('Acceptance position mismatch')
    return len(proofs)


def plot(summary,out):
    os.environ.setdefault('MPLCONFIGDIR',str(out/'.matplotlib'))
    os.environ.setdefault('XDG_CACHE_HOME',str(out/'.cache'))
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    groups=summary['groups'];names=list(dict.fromkeys(r['problem'] for r in groups))
    strategies=[name for name in CONFIGS if any(r['strategy']==name for r in groups)]
    horizon=max(summary['samples']);index={(r['problem'],r['strategy'],r['samples']):r for r in groups}
    fig,axes=plt.subplots(2,1,figsize=(max(10,len(names)*.8),8),layout='constrained')
    width=.8/len(strategies)
    for i,strategy in enumerate(strategies):
        xs=[j-.4+width*(i+.5) for j in range(len(names))]
        for ax,key in zip(axes,('total_calls','wall_seconds')):
            vals=[index[(name,strategy,horizon)][key]['mean'] for name in names]
            lower=[vals[j]-(index[(name,strategy,horizon)][key]['low'] if index[(name,strategy,horizon)][key]['low'] is not None else vals[j]) for j,name in enumerate(names)]
            upper=[(index[(name,strategy,horizon)][key]['high'] if index[(name,strategy,horizon)][key]['high'] is not None else vals[j])-vals[j] for j,name in enumerate(names)]
            ax.bar(xs,vals,width,label=strategy,yerr=[lower,upper],capsize=1,error_kw={'linewidth':.6})
    axes[0].set_ylabel('Total CP-SAT calls (mean)');axes[1].set_ylabel('Wall time / run (seconds)')
    axes[0].legend(ncols=3,fontsize=8)
    for ax in axes:
        ax.set_xticks(range(len(names)),names,rotation=55,ha='right',fontsize=7)
        ax.grid(axis='y',alpha=.2);ax.set_axisbelow(True)
    fig.savefig(out/'comparison.png',dpi=180);fig.savefig(out/'comparison.pdf');plt.close(fig)
    distribution=summary['distribution']
    exact_names=[n for n,d in distribution.items() if d.get('status')=='empirical']
    fig,ax=plt.subplots(figsize=(max(8,len(exact_names)*.8),4),layout='constrained')
    ax.bar(exact_names,[distribution[n]['tv'] for n in exact_names],label='Observed TV')
    ax.scatter(exact_names,[distribution[n]['iid_reference_tv_95'] for n in exact_names],marker='_',color='red',s=180,label='95th percentile iid reference')
    ax.set_ylabel('Empirical total variation');ax.tick_params(axis='x',rotation=45);ax.legend()
    fig.savefig(out/'distribution.png',dpi=180);fig.savefig(out/'distribution.pdf');plt.close(fig)


def write_html(summary,out,plots):
    rows=[]
    for r in summary['groups']:
        rows.append('<tr>'+''.join('<td>'+html.escape(str(v))+'</td>' for v in
            (r['problem'],r['strategy'],r['samples'],round(r['total_calls']['mean'],2),
             round(r['paired_calls_saved']['mean'],2),round(r['wall_seconds']['mean'],4)))+'</tr>')
    text='''<!doctype html><meta charset="utf-8"><title>CP sampling experiment</title>
<style>body{font:15px system-ui;margin:32px;max-width:1500px}table{border-collapse:collapse}td,th{border:1px solid #ccc;padding:6px}img{max-width:100%}</style>
<h1>Distribution-preserving CP sampling: independent Python experiment</h1>
<p>This is a pilot, not a claim of superiority or a completed publication evaluation. All strategy acceptance sequences and positions are paired against CP-backed rejection. UNKNOWN aborts the run. Retail models cover seven bounded structured update operations, not free-form conversations.</p>
<p>Timing includes model cloning, assumption creation, CP solve, core extraction, matching, mass evaluation, scheduling and instrumentation. Shared data loading/base-model construction, offline audits and plotting are reported separately/excluded. Selection time contains some mass time; do not add all timing components.</p>
'''
    if plots:text+='<img src="comparison.png"><img src="distribution.png">'
    text+='<table><tr><th>Problem</th><th>Strategy</th><th>Accepted n</th><th>Total calls</th><th>Paired calls saved</th><th>Wall seconds</th></tr>'+''.join(rows)+'</table>'
    text+='<p>Raw runs: rows.jsonl.gz. Machine-readable statistics and distributions: summary.json. Provenance and precise priors: metadata.json.</p>'
    (out/'report.html').write_text(text)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--suite',choices=('synthetic','retail','all'),default='all')
    parser.add_argument('--source',type=Path,default=DEFAULT_SOURCE)
    parser.add_argument('--orders',type=int,nargs='+',default=[8,32])
    parser.add_argument('--slots',type=int,nargs='+',default=[1,2])
    parser.add_argument('--operations',choices=OPERATIONS,nargs='+',default=list(OPERATIONS))
    parser.add_argument('--seeds',type=int,nargs='+',default=[11,29,47,71,97])
    parser.add_argument('--samples',type=int,nargs='+',default=[32,128])
    parser.add_argument('--strategies',choices=tuple(CONFIGS),nargs='+',default=['rejection','full','raw','mus','minimum','mass_b1','mass_b4','fifo_b4','width_b4','adaptive_b4'])
    parser.add_argument('--candidate-limit',type=int,default=100000)
    parser.add_argument('--exact-limit',type=int,default=4096)
    parser.add_argument('--output',type=Path,default=Path('tmp')/'external'/'tau-bench-python'/'results'/'pilot')
    parser.add_argument('--no-plot',action='store_true')
    args=parser.parse_args()
    if 'rejection' not in args.strategies:parser.error('Paired experiments require rejection baseline')
    if len(set(args.seeds))!=len(args.seeds) or len(set(args.samples))!=len(args.samples):parser.error('Duplicate seeds/horizons forbidden')
    if any(n<=0 for n in args.samples) or args.candidate_limit<=0:parser.error('Positive horizons/limit required')
    out=args.output.resolve()
    if out.exists() and any(out.iterdir()):parser.error('Output directory must be new or empty; never mix historical runs')
    out.mkdir(parents=True,exist_ok=True)
    started=time.perf_counter();problems=[];data=None;upstream=None
    if args.suite!='retail':problems.extend(synthetic())
    if args.suite!='synthetic':
        upstream=provenance(args.source);data=load_retail(args.source)
        for n in args.orders:
            for k in args.slots:
                for operation in args.operations:
                    if k>1 and operation not in ('exchange','modify_items','return'):continue
                    problems.append(retail_problem(data,operation,n,k))
    setup_seconds=time.perf_counter()-started
    exact={p.name:exact_distribution(p,args.exact_limit) for p in problems}
    for p in problems:
        if Oracle(p).query(()).status!='sat':raise RuntimeError('Preflight requires a SAT model: '+p.name)
        if exact[p.name]['status']=='exact' and exact[p.name]['z']==0:raise RuntimeError('Zero feasible probability')
    original=tools(args.source) if data else None
    implementation_files=sorted(HERE.glob('*.py'))+sorted(HERE.glob('*.c'))+[HERE/'UPSTREAM_SOURCE.json',HERE/'requirements.txt']
    from native_sampling import metadata as native_metadata
    metadata=dict(python=sys.version,ortools=ortools.__version__,platform=platform.platform(),
                  native_sampling=native_metadata(),
                  timing='Online wall excludes final cache-coverage reporting; diagnostic costs recorded separately',
                  protocol=dict(seeds=args.seeds,samples=args.samples,strategies=args.strategies,
                                candidate_limit=args.candidate_limit,solver_workers=1,solver_seed=0,
                                per_query_timeout=5,setup_seconds=setup_seconds,
                                design='paired iid proposal streams; no lookahead; all discovery queries counted'),
                  source=upstream,implementation_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in implementation_files},
                  problems={p.name:dict(model=p.metadata,prior=p.prior.metadata(),
                    cp_model_sha256=hashlib.sha256(str(p.model.proto).encode()).hexdigest()) for p in problems})
    snapshot=out/'implementation';snapshot.mkdir()
    for file in implementation_files:shutil.copyfile(file,snapshot/file.name)
    metadata['implementation_snapshot']='implementation/'
    (out/'metadata.json').write_text(json.dumps(metadata,indent=2)+'\n')
    summaries=[];distribution={};certificates=0;external_checks=0;completed=0;total= len(args.seeds)*len(args.samples)*len(args.strategies)*len(problems)
    with gzip.open(out/'rows.jsonl.gz','wt') as raw:
        for p in problems:
            records=[];pooled=[];checked=set();previous_prefix={}
            for seed in args.seeds:
                for count in sorted(args.samples):
                    order=list(args.strategies);random.Random(seed+count).shuffle(order);batch={}
                    for name in order:
                        row=Sampler(p,CONFIGS[name]).run(count,seed,args.candidate_limit)
                        row.update(problem=p.name,sample_count=count,strategy=name)
                        certificates+=audit_row(p,row,exact[p.name])
                        raw.write(json.dumps(row,separators=(',',':'))+'\n')
                        if row['status']!='complete':raise RuntimeError(f"Incomplete run {p.name}/{seed}/{name}: {row['status']}")
                        path=(row['samples'],row['positions'])
                        if name in previous_prefix:
                            old=previous_prefix[name]
                            if path[0][:len(old[0])]!=old[0] or path[1][:len(old[1])]!=old[1]:raise AssertionError('Horizon prefix mismatch')
                        previous_prefix[name]=path
                        # External checks do not contaminate sampler wall time.
                        if p.decode:
                            for x,y in zip(row['samples'],row['witnesses']):
                                if x not in checked:
                                    check(p,x,y,data,original);checked.add(x);external_checks+=1
                        batch[name]=row;completed+=1
                    baseline=batch['rejection']
                    for row in batch.values():
                        if row['samples']!=baseline['samples'] or row['positions']!=baseline['positions']:
                            raise AssertionError('Strategy changed acceptance sequence/positions')
                        records.append(dict(problem=p.name,strategy=row['strategy'],seed=seed,samples=count,
                            total_calls=row['total_calls'],business_calls=row['business_calls'],discovery_calls=row['discovery_calls'],
                            sat_cache_hits=row['sat_cache_hits'],memo_hits=row['memo_hits'],
                            wall_seconds=row['wall_seconds'],mass_seconds=row['timings']['mass'],candidates=row['candidates'],
                            cache_hits=row['cache_hits'],cache_mass=row['cache_mass'],active_searches=row['active_searches'],
                            paired_calls_saved=baseline['total_calls']-row['total_calls'],
                            paired_seconds_saved=baseline['wall_seconds']-row['wall_seconds']))
                    if count==max(args.samples):pooled.extend(baseline['samples'])
                previous_prefix={}  # Prefix comparisons are within a seed, never across seeds.
            groups=defaultdict(list)
            for r in records:groups[(r['strategy'],r['samples'])].append(r)
            for (strategy,count),rs in groups.items():
                summaries.append(dict(problem=p.name,strategy=strategy,samples=count,
                    **{key:interval([r[key] for r in rs]) for key in ('total_calls','business_calls','discovery_calls','sat_cache_hits','memo_hits','wall_seconds','mass_seconds','candidates','cache_hits','cache_mass','active_searches','paired_calls_saved','paired_seconds_saved')}))
            d=distribution_check(pooled,exact[p.name]);d['support_size']=p.prior.size
            if exact[p.name]['status']=='exact':
                d['feasible_probability']=exact[p.name]['z']
                d['target']=[dict(x=x,p=p_) for x,p_ in exact[p.name]['target'].items()]
            distribution[p.name]=d
            print(f'{completed}/{total} runs: {p.name}; exact={exact[p.name]["status"]}',flush=True)
    summary=dict(samples=args.samples,seeds=args.seeds,runs=completed,paired_sequences_verified=True,
                 unique_certificates_audited=certificates,original_tool_checks=external_checks,
                 groups=summaries,distribution=distribution,
                 limitations=['Pilot experiment, not publication-ready statistical evidence.',
                              'Retail uses declared priors, exact-cent business semantics, unique-item orders and bounded updates; no natural-language conversations.',
                              'CP-backed rejection shares the same oracle; no claim against every specialized business checker.',
                              'Python implementation is independent; not bit-for-bit equivalent to the Tapas implementation.'])
    (out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    if not args.no_plot:plot(summary,out)
    write_html(summary,out,not args.no_plot)
    print(json.dumps(dict(runs=completed,paired_sequences_verified=True,unique_certificates_audited=certificates,
                         original_tool_checks=external_checks,output=str(out)),indent=2))
    return 0

if __name__=='__main__':raise SystemExit(main())
