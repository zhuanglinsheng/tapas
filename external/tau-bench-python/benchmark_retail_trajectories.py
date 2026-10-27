"""Matched single-operation, independent-mixture and state-coupled Retail tests."""
import argparse
from collections import defaultdict
import gc
import gzip
import hashlib
from itertools import product
import json
from pathlib import Path
import platform
import random
import shutil
import sys
import time
import ortools
from benchmark_exploration import strategy,summarize
from retail_trajectories import catalogs,trajectory_problem,OPS
from run_experiments import audit_row,exact_distribution
from upstream import tools,provenance
from project import DEFAULT_SOURCE
from native_sampling import metadata as native_metadata


def feasible_mass(p):
    c=p.catalog;states={c.initial:1.};peak=1
    for t in range(p.metadata['horizon']):
        new=defaultdict(float)
        reset=p.metadata['mode']!='coupled'
        before={c.initial:sum(states.values())} if reset else states
        for state,mass in before.items():
            for op,pop in p.prior.maps[p.prior.names.index(f'op_{t}')].items():
                for rank,pr in p.prior.maps[p.prior.names.index(f'request_{t}')].items():
                    out=c.apply(state,2*op+rank)
                    if out is not None:new[out]+=mass*pop*pr
        states=new;peak=max(peak,len(states))
    z=sum(states.values())
    if p.metadata['policy_flags']:
        z*=product_probability(p)
    return dict(status='exact_state_dynamic_program',z=z,peak_states=peak,
        note='Offline characterization only; neither sampler nor gate receives Z or reachable states')


def product_probability(p):
    value=1.
    for i,n in enumerate(p.prior.names):
        if n.startswith(('auth_','confirmed_')):value*=p.prior.maps[i][1]
    return value


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--modes',nargs='+',choices=['single','mixed','coupled'],default=['single','mixed','coupled'])
    ap.add_argument('--styles',nargs='+',choices=['tight','nearest'],default=['tight','nearest'])
    ap.add_argument('--users',nargs='+')
    ap.add_argument('--horizons',nargs='+',type=int,default=[2,4])
    ap.add_argument('--single-operation',choices=OPS,default='modify_items')
    ap.add_argument('--fixed-operation',choices=OPS,help='Diagnostic: restrict mixed/coupled to one operation to isolate state coupling')
    ap.add_argument('--layout',choices=['interleaved','ops_first'],default='interleaved')
    ap.add_argument('--prior',choices=['uniform','terminal_heavy'],default='uniform')
    ap.add_argument('--policy-flags',action='store_true')
    ap.add_argument('--samples',nargs='+',type=int,default=[16,128])
    ap.add_argument('--seeds',nargs='+',type=int,default=[901,907,911,919,929])
    ap.add_argument('--strategies',nargs='+',default=['rejection','raw','mass_b4','mass_safe','core_timed','context_gate','context_direct'])
    ap.add_argument('--aux-timeout',type=float,default=.05)
    ap.add_argument('--mass-nodes',type=int,default=64)
    ap.add_argument('--wall-limit',type=float,default=10.)
    ap.add_argument('--diagnostic-inputs',type=int,default=256)
    ap.add_argument('--output',type=Path,required=True)
    args=ap.parse_args()
    if ('raw' not in args.strategies or any(n<=0 for n in args.samples+args.horizons)
        or min(args.wall_limit,args.aux_timeout)<=0 or args.mass_nodes<0 or args.diagnostic_inputs<0
        or any(len(set(v))!=len(v) for v in (args.samples,args.seeds,args.strategies,args.styles,args.modes,args.horizons))):
        ap.error('Unique values, raw, and valid positive limits required')
    out=args.output.resolve()
    if out.exists() and any(out.iterdir()):ap.error('Output must be new/empty')
    out.mkdir(parents=True,exist_ok=True)
    ps=[];selected=[]
    for style in args.styles:
        for c in catalogs(upgrade_style=style):
            if args.users and c.uid not in args.users:continue
            selected.append(c.metadata)
            for mode,h in product(args.modes,args.horizons):
                ps.append(trajectory_problem(c,h,mode,args.single_operation,args.layout,args.prior,args.policy_flags,args.fixed_operation))
    if not ps:ap.error('No catalogs match')
    here=Path(__file__).resolve().parent;snapshot=out/'implementation';snapshot.mkdir()
    for f in list(here.glob('*.py'))+list(here.glob('*.c'))+[here/'requirements.txt',here/'UPSTREAM_SOURCE.json']:
        shutil.copy2(f,snapshot/f.name)
    original=tools(DEFAULT_SOURCE)
    meta=dict(protocol=vars(args)|dict(output=str(out)),python=sys.version,ortools=ortools.__version__,platform=platform.platform(),
        native_sampling=native_metadata(),solver_workers=1,solver_seed=0,source=provenance(DEFAULT_SOURCE),
        timing='Online CPU/wall exclude final cache-coverage reporting; all original-tool replay and independent DP checks are offline',
        scope='Bounded real-data catalogs, not full tau-bench conversations. Mixed and coupled share identical inputs and prior; independent controls reset original state at every step.',
        core_policy='OR-Tools default native sufficient assumptions cores; no padding or presolve changes',catalogs=selected,
        source_sha256={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in snapshot.iterdir()},
        problems={p.name:dict(prior=p.prior.metadata(),model=p.metadata,cp_model_sha256=hashlib.sha256(str(p.model.proto).encode()).hexdigest(),feasible=feasible_mass(p)) for p in ps})
    if any(v['feasible']['z']<=0 for v in meta['problems'].values()):
        ap.error('At least one constraint system has zero feasible mass; its conditional sampling distribution is undefined')
    (out/'metadata.json').write_text(json.dumps(meta,indent=2)+'\n')
    records=[];audits=core_checks=tool_calls=tool_inputs=0;diagnostics=[]
    with gzip.open(out/'rows.jsonl.gz','wt') as log:
        for p in ps:
            p.prior.mass_node_limit=args.mass_nodes
            checked=set();checked_cores=set();exact=exact_distribution(p,8192)
            def replay(x):
                nonlocal tool_calls,tool_inputs
                x=tuple(x)
                if x in checked:return
                ok,calls=p.replay(x,original)
                if ok!=p.feasible(x):raise AssertionError('Original tools and interpreter disagree')
                checked.add(x);tool_calls+=calls;tool_inputs+=1
            rng=random.Random(20261021)
            points=(list(p.prior.points()) if p.prior.size<=args.diagnostic_inputs else [p.prior.draw(rng) for _ in range(args.diagnostic_inputs)])
            for x in points:replay(x)
            for seed in args.seeds:
                prefixes={}
                for count in sorted(args.samples):
                    order=args.strategies.copy();random.Random(seed+count).shuffle(order);case={}
                    for name in order:
                        p.prior._residual_mass.cache_clear();p.partial_feasible.cache_clear();gc.collect()
                        started=time.process_time()
                        row=strategy(p,name,aux_timeout=args.aux_timeout).run(count,seed,candidate_limit=1000000,wall_limit=args.wall_limit)
                        row['cpu_seconds']=time.process_time()-started-row['diagnostics']['cpu_seconds']
                        row.update(problem=p.name,strategy=name)
                        row['business_unsat_calls']=sum(e['kind']=='business' and e['status']=='unsat' for e in row['events'])
                        row['business_core_widths']=[len(e['core']) for e in row['events'] if e['kind']=='business' and e['status']=='unsat']
                        sequence=row['samples'],row['positions']
                        if name in prefixes:
                            old=prefixes[name];n=min(len(old[0]),len(sequence[0]))
                            if (sequence[0][:n],sequence[1][:n])!=(old[0][:n],old[1][:n]):raise AssertionError('Horizon prefix changed')
                        prefixes[name]=sequence
                        audits+=audit_row(p,row,exact)
                        for e in row['events']:
                            if e['kind'] not in ('business','discovery'):continue
                            if e['status']=='unsat':
                                core=tuple(map(tuple,e['core']))
                                if core not in checked_cores:
                                    if p.partial_feasible(core):raise AssertionError('Native core permits an independent valid trajectory')
                                    checked_cores.add(core);core_checks+=1
                            elif e['status']=='sat':replay(e['completion'])
                        case[name]=row
                        records.append({k:v for k,v in row.items() if k not in ('events','samples','positions','witnesses')})
                        log.write(json.dumps(row)+'\n');log.flush()
                    ref=case['raw']
                    for row in case.values():
                        n=min(len(row['samples']),len(ref['samples']))
                        if (row['samples'][:n],row['positions'][:n])!=(ref['samples'][:n],ref['positions'][:n]):raise AssertionError('Candidate stream changed')
            diagnostics.append(dict(problem=p.name,original_inputs_checked=len(checked),native_cores_checked=len(checked_cores)))
            print(p.name,'finished',flush=True)
    s=summarize(records,out);s.update(certificates_audited=audits,independent_native_cores_checked=core_checks,
        original_tool_checks=tool_inputs,original_tool_step_calls=tool_calls,diagnostics=diagnostics)
    (out/'summary.json').write_text(json.dumps(s,indent=2)+'\n')
    print(json.dumps(dict(runs=s['runs'],completed=s['completed'],native_cores=core_checks,original_trajectories=tool_inputs,original_step_calls=tool_calls)))


if __name__=='__main__':main()
