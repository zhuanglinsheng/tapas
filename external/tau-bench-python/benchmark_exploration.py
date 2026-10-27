"""Paired native-CP tests of active exploration, including negative controls."""
import argparse
from collections import defaultdict
from dataclasses import replace
import gc
import gzip
import hashlib
import html
import json
from pathlib import Path
import platform
import random
import shutil
import sys
import time
import ortools
from exploration import CoreSampler,SafeAuxMassSampler
from context_exploration import ContextSampler
from exploration_workloads import allocation,packing,multiguard,partial_unsat
from native_sampling import metadata as native_metadata
from run_experiments import audit_row,exact_distribution,interval
from sampler import Sampler,CONFIGS
from oracle import Oracle
from project import DEFAULT_SOURCE
from upstream import tools,check,provenance
from workloads import load_retail,retail_problem,OPERATIONS


def problems(preset):
    families=[('clique',3,'pairwise'),('clique',3,'simplified'),('feasible',3,'pairwise'),('wheel',3,'pairwise')]
    if preset=='grid':families.append(('clique',4,'pairwise'))
    if preset=='hardness':families=[('clique',k,'pairwise') for k in (3,5,7,9)]
    for family,k,encoding in families:
        n=6 if family=='wheel' else k if family=='feasible' else k+1
        for pos in ((0,n) if preset in ('pilot','hardness') else (0,n//2,n)):
            for probability in ((.9,) if preset in ('pilot','hardness') else (.2,.9)):
                for skew in ((False,) if preset in ('pilot','hardness') else (False,True)):
                    yield allocation(family,k,probability,pos,encoding,skew)


class AuxiliaryBudgetOracle(Oracle):
    def __init__(self,p,aux_timeout):
        super().__init__(p);self.aux_timeout=aux_timeout

    def query(self,bindings):
        # All business inputs bind every coordinate. Shared rule for every
        # strategy; auxiliary timeouts are not fake UNSAT/SAT answers.
        saved=self.timeout
        if len(bindings)<len(self.problem.xvars):self.timeout=self.aux_timeout
        try:return super().query(bindings)
        finally:self.timeout=saved


def strategy(p,name,capacity=0,aux_timeout=None):
    oracle=AuxiliaryBudgetOracle(p,aux_timeout) if aux_timeout is not None else None
    if name in ('context_gate','context_direct'):
        return ContextSampler(p,replace(CONFIGS['mus'],name=name,capacity=capacity),
            direct=name=='context_direct',oracle=oracle)
    if name=='mass_safe':
        return SafeAuxMassSampler(p,replace(CONFIGS['mass_b4'],name=name,capacity=capacity),oracle)
    if name in ('rejection','raw','mass_b4','fifo_b4','width_b4','mus','adaptive_b4'):
        return Sampler(p,replace(CONFIGS[name],capacity=capacity),oracle)
    if name not in ('core_fifo','core_mass','core_scout','core_scout_unskipped','core_delayed','core_timed'):raise ValueError('Unknown strategy '+name)
    cfg=replace(CONFIGS['mus'],name=name,capacity=capacity)
    return CoreSampler(p,cfg,mass_order=name!='core_fifo',
        stop_after=3 if name.startswith('core_scout') or name in ('core_delayed','core_timed') else 0,
        skip_empty=name!='core_scout_unskipped',start_after=32 if name=='core_delayed' else 0,
        time_budget=.01 if name=='core_timed' else 0,oracle=oracle)


def summarize(records,out):
    grouped=defaultdict(list);by={}
    for r in records:
        grouped[r['problem'],r['requested'],r['strategy']].append(r)
        by[r['problem'],r['requested'],r['seed'],r['strategy']]=r
    groups=[]
    for (problem,count,name),rows in sorted(grouped.items()):
        bases=[by[problem,count,r['seed'],'raw'] for r in rows]
        pairs=[(b,r) for b,r in zip(bases,rows) if b['status']==r['status']=='complete']
        def paired(metric):return interval([b[metric]-r[metric] for b,r in pairs]) if pairs else None
        row=dict(problem=problem,samples=count,strategy=name,completed=sum(r['status']=='complete' for r in rows),
            attempts=len(rows),complete_pairs=len(pairs),
            **{k:interval([r[k] for r in rows]) for k in
               ('cpu_seconds','wall_seconds','business_calls','discovery_calls','total_calls','candidates')},
            paired_calls_saved=paired('total_calls'),paired_cpu_saved=paired('cpu_seconds'),
            paired_wall_saved=paired('wall_seconds'),business_calls_saved=paired('business_calls'),
            business_unsat_saved=interval([b['business_unsat_calls']-r['business_unsat_calls'] for b,r in pairs]) if pairs else None)
        groups.append(row)
    summary=dict(runs=len(records),completed=sum(r['status']=='complete' for r in records),paired_sequences_verified=True,groups=groups,
                 note='Pilot cohorts and horizons are reported separately; native cores unchanged; no external oracle latency injected.')
    (out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    body=''.join('<tr>'+''.join('<td>'+html.escape(str(v))+'</td>' for v in
        (g['problem'],g['samples'],g['strategy'],round(g['total_calls']['mean'],2),
         round(g['paired_calls_saved']['mean'],2) if g['paired_calls_saved'] else None,round(g['cpu_seconds']['mean'],5),
         round(g['paired_cpu_saved']['mean'],5) if g['paired_cpu_saved'] else None,
         str(g['completed'])+'/'+str(g['attempts'])))+'</tr>' for g in groups)
    (out/'report.html').write_text('<!doctype html><meta charset="utf-8"><title>Active exploration</title>'
        '<style>body{font:14px system-ui;margin:24px}table{border-collapse:collapse}td,th{border:1px solid #ccc;padding:6px}</style>'
        '<h1>Native CP exploration, paired with enhanced raw</h1><p>Positive savings favor exploration. All controls retained.</p>'
        '<table><tr><th>Problem</th><th>Samples</th><th>Strategy</th><th>Calls</th><th>Calls saved, complete pairs</th><th>CPU</th><th>CPU saved, complete pairs</th><th>Complete</th></tr>'+body+'</table>')
    return summary


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--preset',choices=('pilot','grid','hardness','retail','packing','multiguard'),default='pilot')
    parser.add_argument('--capacity',type=int,default=0)
    parser.add_argument('--aux-timeout',type=float)
    parser.add_argument('--wall-limit',type=float,help='Soft online wall limit per run; incomplete prefixes are retained, never counted as speed wins')
    parser.add_argument('--retail-orders',type=int,default=8)
    parser.add_argument('--retail-slots',type=int,default=2)
    parser.add_argument('--retail-operations',nargs='+',choices=OPERATIONS,default=['exchange','modify_items','return'])
    parser.add_argument('--mass-nodes',type=int,default=0,help='Per-score recursive state limit; zero is exact. Fallback is a mass upper bound.')
    parser.add_argument('--samples',nargs='+',type=int,default=[16,128])
    parser.add_argument('--seeds',nargs='+',type=int,default=[11,29,47])
    parser.add_argument('--strategies',nargs='+',default=['raw','mass_b4','mus','core_fifo','core_mass','core_scout'])
    parser.add_argument('--limit',type=int,help='Smoke only: first N problems, explicitly recorded')
    parser.add_argument('--filter',help='Explicit case-name substring; recorded for targeted diagnostics')
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if ('raw' not in args.strategies or len(set(args.strategies))!=len(args.strategies)
        or len(set(args.seeds))!=len(args.seeds) or len(set(args.samples))!=len(args.samples)
        or any(n<=0 for n in args.samples) or (args.limit is not None and args.limit<=0)
        or args.capacity<0 or args.mass_nodes<0 or min(args.retail_orders,args.retail_slots)<=0
        or (args.wall_limit is not None and args.wall_limit<=0)
        or (args.aux_timeout is not None and args.aux_timeout<=0)):
        parser.error('Unique strategies/seeds/horizons, raw comparator, and positive counts required')
    out=args.output.resolve()
    if out.exists() and any(out.iterdir()):parser.error('Output must be new/empty')
    out.mkdir(parents=True,exist_ok=True)
    data=load_retail() if args.preset=='retail' else None
    ps=([retail_problem(data,op,args.retail_orders,args.retail_slots) for op in args.retail_operations] if data else
        [packing(b,q,g,s) for b in (3,4) for q in (.2,.9) for g in (0,2,5) for s in (False,True)]
        if args.preset=='packing' else
        [multiguard(w,q,g,e) for w in (2,4) for q in (.01,.2,.9) for g in (0,4) for e in ('pairwise','simplified')]
        if args.preset=='multiguard' else list(problems(args.preset)))
    original=tools(DEFAULT_SOURCE) if data else None
    if args.filter:ps=[p for p in ps if args.filter in p.name]
    if args.limit:ps=ps[:args.limit]
    if not ps:parser.error('No matching problems')
    here=Path(__file__).resolve().parent;snapshot=out/'implementation';snapshot.mkdir()
    for f in list(here.glob('*.py'))+list(here.glob('*.c'))+[here/'requirements.txt',here/'UPSTREAM_SOURCE.json']:
        shutil.copy2(f,snapshot/f.name)
    meta=dict(protocol=vars(args)|dict(output=str(out)),python=sys.version,ortools=ortools.__version__,
        platform=platform.platform(),native_sampling=native_metadata(),solver_workers=1,solver_seed=0,
        core_policy='OR-Tools default native sufficient assumptions core, unmodified',
        timing='Online CPU/wall exclude final cache-coverage reporting; diagnostic costs recorded separately',
        source=provenance(DEFAULT_SOURCE) if data else None,
        source_sha256={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in snapshot.iterdir()},
        problems={p.name:dict(prior=p.prior.metadata(),model=p.metadata,
            cp_model_sha256=hashlib.sha256(str(p.model.proto).encode()).hexdigest()) for p in ps})
    (out/'metadata.json').write_text(json.dumps(meta,indent=2)+'\n')
    records=[];audits=0;tool_checks=0
    with gzip.open(out/'rows.jsonl.gz','wt') as file:
        for p in ps:
            p.prior.mass_node_limit=args.mass_nodes
            exact=exact_distribution(p,8192)
            checked=set()
            for seed in args.seeds:
                prefixes={}
                for count in sorted(args.samples):
                    order=args.strategies.copy();random.Random(seed+count).shuffle(order);case={}
                    for name in order:
                        # Dispose of the PREVIOUS run's diagnostic memo before
                        # timing this run; Sampler also starts with a cold memo.
                        p.prior._residual_mass.cache_clear()
                        gc.collect();started=time.process_time()
                        row=strategy(p,name,args.capacity,args.aux_timeout).run(count,seed,candidate_limit=1000000,wall_limit=args.wall_limit)
                        row['cpu_seconds']=time.process_time()-started-row.get('diagnostics',{}).get('cpu_seconds',0)
                        row.update(problem=p.name,strategy=name)
                        row['business_unsat_calls']=sum(e['kind']=='business' and e['status']=='unsat' for e in row['events'])
                        row['business_core_widths']=[len(e['core']) for e in row['events'] if e['kind']=='business' and e['status']=='unsat']
                        sequence=row['samples'],row['positions']
                        if name in prefixes:
                            old=prefixes[name];n=len(old[0])
                            prefix=min(n,len(sequence[0]))
                            if (sequence[0][:prefix],sequence[1][:prefix])!=(old[0][:prefix],old[1][:prefix]):raise AssertionError('Horizon prefix changed')
                        prefixes[name]=sequence
                        audits+=audit_row(p,row,exact)
                        if data:
                            for x,y in zip(row['samples'],row['witnesses']):
                                if x not in checked:check(p,x,y,data,original);checked.add(x);tool_checks+=1
                        else:
                            for event in row['events']:
                                if event['kind'] in ('business','discovery'):
                                    if event['status']!='unknown' and (event['status']=='unsat')!=partial_unsat(p,event['region']):
                                        raise AssertionError('Native query disagrees with independent partial checker')
                                    if event['status']=='unsat' and not partial_unsat(p,event['core']):
                                        raise AssertionError('Unsafe native core')
                        case[name]=row
                        # Keep detailed events on disk, not alive across timed
                        # runs: otherwise gc.collect scans all prior cohorts.
                        records.append({k:v for k,v in row.items() if k not in ('events','samples','positions','witnesses')})
                        file.write(json.dumps(row)+'\n');file.flush()
                    reference=case['raw']
                    for r in case.values():
                        n=min(len(r['samples']),len(reference['samples']))
                        if (r['samples'][:n],r['positions'][:n])!=(reference['samples'][:n],reference['positions'][:n]):
                            raise AssertionError('Candidate acceptance prefix changed')
            print(p.name,'complete',flush=True)
    s=summarize(records,out);s['certificates_audited']=audits;s['original_tool_checks']=tool_checks
    (out/'summary.json').write_text(json.dumps(s,indent=2)+'\n')
    print(json.dumps(dict(runs=len(records),certificates=audits,output=str(out))))


if __name__=='__main__':main()
