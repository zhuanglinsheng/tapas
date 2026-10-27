"""Explicit old/new ablation; requires a user-selected baseline source directory."""
from __future__ import annotations
import argparse
from collections import defaultdict
from dataclasses import replace
import gzip
import gc
import hashlib
import html
import importlib.util
import json
from pathlib import Path
import platform
import random
import shutil
import statistics
import sys
import time
from types import MethodType
import ortools
import prior
from native_sampling import metadata as native_metadata
from project import DEFAULT_SOURCE
from run_experiments import audit_row, exact_distribution, interval
from sampler import CONFIGS, Sampler
from upstream import tools, check, provenance
from workloads import synthetic, retail_problem, load_retail


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--suite', choices=('synthetic', 'retail'), default='synthetic')
    parser.add_argument('--seeds', nargs='+', type=int, default=[11,29,47,71,97])
    parser.add_argument('--samples', nargs='+', type=int, default=[32,128])
    parser.add_argument('--candidate-limit',type=int,default=100000)
    parser.add_argument('--problems', nargs='+', help='Exact problem names; default: all problems in suite')
    parser.add_argument('--clean-gc', action='store_true', help='Diagnostic: collect prior-run garbage before timing and record online GC CPU cost')
    parser.add_argument('--strategies', nargs='+', default=['legacy_raw','raw','legacy_mass','mass_score','mass_optimized','adaptive'])
    args = parser.parse_args()
    if (len(set(args.seeds)) != len(args.seeds) or len(set(args.samples)) != len(args.samples)
        or len(set(args.strategies)) != len(args.strategies) or any(n <= 0 for n in args.samples)
        or args.candidate_limit<=0):
        parser.error('Unique seeds, sample counts and strategies; positive sample counts required')
    out = args.output.resolve()
    if out.exists() and any(out.iterdir()): parser.error('Output must be new/empty')
    out.mkdir(parents=True, exist_ok=True)
    old = load('optimization_baseline_sampler', args.baseline/'sampler.py')
    if (args.baseline/'oracle.py').exists():
        old.Oracle=load('optimization_baseline_oracle',args.baseline/'oracle.py').Oracle
    if (args.baseline/'proofs.py').exists():
        old.ProofStore=load('optimization_baseline_proofs',args.baseline/'proofs.py').ProofStore
    old_prior = load('optimization_baseline_prior', args.baseline/'prior.py')
    new_uncovered = prior.ProductPrior.uncovered
    variants = dict(legacy_raw=(old.Sampler,old.CONFIGS['raw'],False),
                    raw=(Sampler,CONFIGS['raw'],True),
                    raw_python=(Sampler,CONFIGS['raw'],True),
                    mass_python=(Sampler,CONFIGS['mass_b4'],True),
                    legacy_mass=(old.Sampler,old.CONFIGS['mass_b4'],False),
                    mass_score=(old.Sampler,old.CONFIGS['mass_b4'],True),
                    mass_optimized=(Sampler,CONFIGS['mass_b4'],True),
                    mass_no_miss_cache=(Sampler,replace(CONFIGS['mass_b4'],memoize_misses=False),True),
                    mass_reinvest=(Sampler,replace(CONFIGS['mass_b4'],charge_memo=False),True),
                    raw_linear=(Sampler,replace(CONFIGS['raw'],proof_index='linear'),True),
                    mass_linear=(Sampler,replace(CONFIGS['mass_b4'],proof_index='linear'),True),
                    raw_no_witness=(Sampler,replace(CONFIGS['raw'],sat_memo=False),True),
                    mass_no_witness=(Sampler,replace(CONFIGS['mass_b4'],sat_memo=False),True),
                    mass_exact=(Sampler,replace(CONFIGS['mass_b4'],proof_reuse=False),True),
                    mass_lazy=(Sampler,replace(CONFIGS['mass_b4'],eager_proofs=False),True),
                    mass_eager_scored=(Sampler,replace(CONFIGS['mass_b4'],defer_scores=False),True),
                    legacy_adaptive=(old.Sampler,old.CONFIGS['adaptive_b4'],False),
                    adaptive=(Sampler,CONFIGS['adaptive_b4'],True))
    if any(v not in variants for v in args.strategies): parser.error('Unknown strategy')
    data = load_retail() if args.suite == 'retail' else None
    problems = ([retail_problem(data, op, 8, 2) for op in ('exchange','modify_items','return')]
                if data else synthetic())
    if args.problems:
        available={p.name for p in problems}
        if set(args.problems)-available:parser.error('Unknown problem names: '+str(set(args.problems)-available))
        problems=[p for p in problems if p.name in args.problems]
    original = tools(DEFAULT_SOURCE) if data else None
    snapshot = out/'implementation'; snapshot.mkdir()
    here = Path(__file__).resolve().parent
    for file in list(here.glob('*.py'))+list(here.glob('*.c'))+[here/'UPSTREAM_SOURCE.json',here/'requirements.txt']:
        shutil.copyfile(file,snapshot/file.name)
    shutil.copytree(args.baseline,out/'baseline',ignore=shutil.ignore_patterns('__pycache__'))
    metadata = dict(protocol=vars(args)|dict(baseline=str(args.baseline),output=str(out)),
                    python=sys.version,ortools=ortools.__version__,platform=platform.platform(),
                    native_sampling=native_metadata(),
                    implementation_sha256={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in snapshot.iterdir()},
                    baseline_sha256={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in (out/'baseline').glob('*.py')},
                    source=provenance(DEFAULT_SOURCE) if data else None,
                    problems={p.name:dict(prior=p.prior.metadata(),model=p.metadata,
                              cp_model_sha256=hashlib.sha256(str(p.model.proto).encode()).hexdigest()) for p in problems},
                    timing='Legacy ablation protocol includes final coverage reporting for BOTH old and new variants; setup/audit excluded; randomized order and cold probability memo',
                    baseline_oracle='baseline/oracle.py' if (args.baseline/'oracle.py').exists() else 'current oracle.py')
    (out/'metadata.json').write_text(json.dumps(metadata,indent=2)+'\n')
    records=[];certificates=checks=0
    with gzip.open(out/'rows.jsonl.gz','wt') as output:
        for p in problems:
            exact=exact_distribution(p,4096);checked=set()
            for seed in args.seeds:
                for count in args.samples:
                    order=args.strategies.copy();random.Random(seed+count).shuffle(order)
                    reference=None
                    for name in order:
                        cls,config,new_mass=variants[name]
                        prior.ProductPrior.uncovered = new_uncovered if new_mass else old_prior.ProductPrior.uncovered
                        # A baseline must use its own draw loop as well as its
                        # own sampler/oracle/proof index; do not share speedups.
                        original_draw=p.prior.draw
                        original_native=p.prior._native_draw
                        if name in ('raw_python','mass_python'):
                            p.prior._native_draw=None
                        if not new_mass:
                            baseline_type=old_prior.JointPrior if isinstance(p.prior,prior.JointPrior) else old_prior.ProductPrior
                            p.prior.draw=MethodType(baseline_type.draw,p.prior)
                        p.prior._residual_mass.cache_clear()
                        gc_stats=dict(cpu_seconds=0.0,collections=0)
                        gc_start=0.0
                        def gc_event(phase,info):
                            nonlocal gc_start
                            if phase=='start':gc_start=time.process_time()
                            else:
                                gc_stats['cpu_seconds']+=time.process_time()-gc_start
                                gc_stats['collections']+=1
                        if args.clean_gc:
                            gc.collect()
                            gc.callbacks.append(gc_event)
                        cpu_started=time.process_time()
                        try: row=cls(p,config).run(count,seed,candidate_limit=args.candidate_limit)
                        finally:
                            prior.ProductPrior.uncovered=new_uncovered
                            p.prior.draw=original_draw
                            p.prior._native_draw=original_native
                            if args.clean_gc:gc.callbacks.remove(gc_event)
                        row['cpu_seconds']=time.process_time()-cpu_started
                        if 'diagnostics' in row:
                            row['online_wall_seconds']=row['wall_seconds']
                            row['wall_seconds']+=row['diagnostics']['wall_seconds']
                            row['online_mass_seconds']=row['timings']['mass']
                            row['timings']['mass']+=row['diagnostics']['mass_seconds']
                        row['gc']=gc_stats if args.clean_gc else None
                        if row['status']!='complete':
                            (out/'failure.json').write_text(json.dumps(dict(problem=p.name,strategy=name,
                                sample_count=count,seed=seed,status=row['status'],candidates=row['candidates'],
                                accepted=len(row['samples'])),indent=2)+'\n')
                            raise AssertionError((p.name,name,row['status']))
                        path=(row['samples'],row['positions'])
                        if reference is None:reference=path
                        if path!=reference:raise AssertionError('Acceptance stream changed')
                        certificates+=audit_row(p,row,exact)
                        if data:
                            for x,y in zip(row['samples'],row['witnesses']):
                                if x not in checked:check(p,x,y,data,original);checked.add(x);checks+=1
                        row.update(problem=p.name,strategy=name,sample_count=count)
                        output.write(json.dumps(row)+'\n');output.flush()
                        records.append(row)
            print(p.name, 'complete',flush=True)
    groups=defaultdict(list)
    for r in records:groups[r['problem'],r['sample_count'],r['strategy']].append(r)
    summaries=[]
    for (problem,count,strategy),rs in groups.items():
        summaries.append(dict(problem=problem,samples=count,strategy=strategy,
                         **{k:interval([r[k] for r in rs]) for k in ('wall_seconds','cpu_seconds','total_calls','business_calls','discovery_calls')},
                         mass_seconds=statistics.mean(r['timings']['mass'] for r in rs),
                         memo_hits=statistics.mean(r.get('memo_hits',0) for r in rs),
                         sat_cache_hits=statistics.mean(r.get('sat_cache_hits',0) for r in rs),
                         inference_hits={k:statistics.mean(r.get('inference_hits',{}).get(k,0) for r in rs) for k in ('sat','unsat','exact')}))
    paired=[]
    by={(r['problem'],r['sample_count'],r['seed'],r['strategy']):r for r in records}
    for (problem,count,strategy),rs in groups.items():
        for base in ('legacy_raw','legacy_mass','legacy_adaptive','mass_linear','mass_no_witness','mass_eager_scored','mass_lazy','raw','raw_python','mass_python','mass_no_miss_cache'):
            if base not in args.strategies:continue
            deltas=[by[problem,count,r['seed'],base]['wall_seconds']-r['wall_seconds'] for r in rs]
            paired.append(dict(problem=problem,samples=count,strategy=strategy,baseline=base,
                               seconds_saved=interval(deltas),wins=sum(d>0 for d in deltas),
                               cpu_seconds_saved=interval([by[problem,count,r['seed'],base]['cpu_seconds']-r['cpu_seconds'] for r in rs])))
    summary=dict(runs=len(records),paired_sequences_verified=True,certificates_audited=certificates,
                 original_tool_checks=checks,groups=summaries,paired=paired)
    (out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    cells=''.join('<tr>'+''.join('<td>'+html.escape(str(value))+'</td>' for value in
                 (g['problem'],g['samples'],g['strategy'],round(g['wall_seconds']['mean'],5),
                  round(g['cpu_seconds']['mean'],5),g['business_calls']['mean'],g['discovery_calls']['mean'],g['total_calls']['mean'],
                  g['sat_cache_hits'],round(g['mass_seconds'],5)))+'</tr>' for g in summaries)
    (out/'report.html').write_text('<!doctype html><meta charset="utf-8"><title>MassCore optimization ablation</title>'
        '<style>body{font:15px system-ui;margin:32px}td,th{padding:8px;border:1px solid #ccc}table{border-collapse:collapse}</style>'
        '<h1>MassCore optimization ablation</h1><p>Paired candidate streams verified; all online costs included. '
        'Small-seed pilot; paired intervals in summary.json. Timings are means in seconds.</p>'
        '<table><tr><th>Problem</th><th>Samples</th><th>Strategy</th><th>Wall</th><th>CPU</th><th>Business</th><th>Discovery</th><th>Total</th><th>Witness hits</th><th>Mass</th></tr>'+cells+'</table>')
    print(json.dumps(dict(runs=len(records),certificates=certificates,original_checks=checks,output=str(out))))

if __name__=='__main__':main()
