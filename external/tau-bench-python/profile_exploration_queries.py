"""Re-solve selected recorded bindings to diagnose CP search work, not speedups."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
from unittest.mock import patch
from ortools.sat.python import cp_model
from oracle import Oracle
from exploration_workloads import allocation


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('cohort',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();meta=json.loads((args.cohort/'metadata.json').read_text())
    selected={}
    with gzip.open(args.cohort/'rows.jsonl.gz','rt') as file:
        for line in file:
            row=json.loads(line);model=meta['problems'][row['problem']]['model']
            if model['guard_position']!=0 or row['strategy']!='raw' or row['problem'] in selected:continue
            failed=next(e for e in row['events'] if e['kind']=='business' and e['status']=='unsat')
            selected[row['problem']]=(row['seed'],failed)
    records=[];original=cp_model.CpSolver;solvers=[]
    class CaptureSolver(original):
        def __init__(self):
            super().__init__();solvers.append(self)
    for name,(seed,failed) in selected.items():
        m=meta['problems'][name]['model']
        p=allocation(m['family'],m['colors'],m['guard_probability'],m['guard_position'],m['encoding'],m['skew'])
        assert hashlib.sha256(str(p.model.proto).encode()).hexdigest()==meta['problems'][name]['cp_model_sha256']
        queries=[('recorded_full',tuple(map(tuple,failed['region'])),5.0),
                 ('recorded_native_core',tuple(map(tuple,failed['core'])),.02),
                 ('global_guard',((0,1),),.02)]
        for kind,bindings,timeout in queries:
            with patch.object(cp_model,'CpSolver',CaptureSolver):answer=Oracle(p,timeout).query(bindings)
            solver=solvers.pop();response=solver.response_proto
            stats={field:getattr(response,field,None) for field in
                ('num_conflicts','num_branches','num_binary_propagations','num_integer_propagations','deterministic_time')}
            records.append(dict(problem=name,seed=seed,kind=kind,bindings=bindings,timeout=timeout,
                status=answer.status,core=answer.core,solver_statistics=stats,
                seconds=dict(build=answer.build_seconds,solve=answer.solve_seconds,extract=answer.extract_seconds),
                raw_response_stats=solver.response_stats()))
    output=dict(protocol='Offline diagnostic re-solves; not part of sampler call counts, not substituted for original timing',
        cohort_metadata_sha256=hashlib.sha256((args.cohort/'metadata.json').read_bytes()).hexdigest(),records=records)
    args.output.write_text(json.dumps(output,indent=2)+'\n')
    print(json.dumps(dict(queries=len(records),output=str(args.output))))


if __name__=='__main__':main()
