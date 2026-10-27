"""Balanced repeated timing; bootstrap seed means, never treat repeats as seeds."""
from collections import defaultdict
import argparse
import gc
import gzip
import hashlib
from itertools import permutations
import json
from pathlib import Path
import platform
import random
import shutil
import statistics
import sys
import time
import ortools
from exploration_workloads import allocation,packing
from benchmark_exploration import strategy
from native_sampling import metadata as native_metadata
from run_experiments import interval


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();out=args.output.resolve()
    if out.exists() and any(out.iterdir()):parser.error('Output must be new/empty')
    out.mkdir(parents=True,exist_ok=True)
    here=Path(__file__).resolve().parent;snapshot=out/'implementation';snapshot.mkdir()
    for file in list(here.glob('*.py'))+list(here.glob('*.c')):shutil.copy2(file,snapshot/file.name)
    cases=[allocation(guard_position=0),allocation(guard_position=4),packing(),allocation(encoding='simplified')]
    seeds=[401,409,419,421,431];methods=['raw','core_mass','core_scout'];orders=list(permutations(methods))
    meta=dict(samples=128,seeds=seeds,cycles=2,repeats_per_seed=12,strategies=methods,
        python=sys.version,ortools=ortools.__version__,platform=platform.platform(),native_sampling=native_metadata(),
        protocol='Each of six strategy orders occurs twice per seed. Diagnostic memo cleared and GC collected before timing; final coverage CPU excluded. Seed means are the resampling units.',
        implementation_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in snapshot.iterdir()},
        problems={p.name:dict(model=p.metadata,prior=p.prior.metadata(),
            cp_model_sha256=hashlib.sha256(str(p.model.proto).encode()).hexdigest()) for p in cases})
    (out/'metadata.json').write_text(json.dumps(meta,indent=2)+'\n')
    records=[];trace_control=[]
    with gzip.open(out/'trials.jsonl.gz','wt') as file:
        for p in cases:
            for seed in seeds:
                reference=None
                for cycle in range(2):
                    schedule=orders.copy();random.Random(f'{p.name}:{seed}:{cycle}').shuffle(schedule)
                    for slot,order in enumerate(schedule):
                        control={}
                        for position,name in enumerate(order):
                            p.prior._residual_mass.cache_clear();gc.collect()
                            started=time.process_time();row=strategy(p,name).run(128,seed)
                            cpu=time.process_time()-started-row['diagnostics']['cpu_seconds']
                            assert row['status']=='complete'
                            digest=hashlib.sha256(json.dumps((row['samples'],row['positions'])).encode()).hexdigest()
                            if reference is None:reference=digest
                            assert digest==reference
                            events=[{k:v for k,v in e.items() if k!='seconds'} for e in row['events']]
                            trace=hashlib.sha256(json.dumps(events,sort_keys=True).encode()).hexdigest()
                            control[name]=trace
                            result=dict(problem=p.name,seed=seed,cycle=cycle,slot=slot,position=position,
                                strategy=name,cpu_seconds=cpu,wall_seconds=row['wall_seconds'],
                                total_calls=row['total_calls'],event_trace_sha256=trace,acceptance_sha256=digest)
                            records.append(result);file.write(json.dumps(result)+'\n');file.flush()
                            # Do not retain old event trees during the next run.
                            del row,events
                        trace_control.append(control['core_mass']==control['core_scout'])
            print(p.name,'complete',flush=True)
    grouped=defaultdict(list)
    for r in records:grouped[r['problem'],r['seed'],r['strategy']].append(r)
    means={key:{metric:statistics.mean(r[metric] for r in rs) for metric in ('cpu_seconds','wall_seconds','total_calls')}
           for key,rs in grouped.items()}
    summary=[]
    for p in cases:
        for before,after in [('raw','core_scout'),('core_mass','core_scout')]:
            result=dict(problem=p.name,before=before,after=after,independent_seeds=len(seeds),repeats_per_seed=12)
            for metric in ('cpu_seconds','wall_seconds','total_calls'):
                result[metric]=dict(before=statistics.mean(means[p.name,s,before][metric] for s in seeds),
                    after=statistics.mean(means[p.name,s,after][metric] for s in seeds),
                    paired_saved=interval([means[p.name,s,before][metric]-means[p.name,s,after][metric] for s in seeds]))
            summary.append(result)
    report=dict(runs=len(records),paired_acceptance_verified=True,
        identical_core_mass_scout_event_traces=sum(trace_control),trace_control_pairs=len(trace_control),groups=summary)
    (out/'summary.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(dict(runs=len(records),trace_control_pairs=len(trace_control),identical_traces=sum(trace_control))))


if __name__=='__main__':main()
