"""Finite private-witness certificate for the observed passive core vocabulary.

Default mode calls no solver. --verify-native also exhausts the tiny input
space to check the native explanation vocabulary; these are diagnostic calls.
This is not a lower bound for arbitrary full-model preprocessing algorithms.
"""
import argparse
import gzip
import hashlib
from itertools import combinations
import json
from pathlib import Path
from prior import matches
from exploration_workloads import allocation
from oracle import Oracle


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('cohort',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--verify-native',action='store_true')
    args=parser.parse_args();meta=json.loads((args.cohort/'metadata.json').read_text())
    models={};observed=0;solver_calls=0
    for name,entry in meta['problems'].items():
        m=entry['model']
        if (m['family'],m['colors'],m['encoding'])!=('clique',3,'pairwise'):
            raise ValueError('Certificate expects the specified K4/3-resource native cohort')
        p=allocation('clique',3,m['guard_probability'],m['guard_position'],'pairwise',m['skew'])
        assert hashlib.sha256(str(p.model.proto).encode()).hexdigest()==entry['cp_model_sha256']
        guard=m['guard_position'];resources=[i for i in range(5) if i!=guard];certificates=[]
        patterns=[tuple(sorted(((guard,1),(a,color),(b,color))))
                  for a,b in combinations(resources,2) for color in range(3)]
        for region in patterns:
            x=[0]*5;x[guard]=1
            fixed=dict(region)
            duplicated=next(v for i,v in region if i!=guard)
            free=[i for i in resources if i not in fixed]
            for i,v in fixed.items():x[i]=v
            for i,v in zip(free,[c for c in range(3) if c!=duplicated]):x[i]=v
            x=tuple(x)
            assert not p.feasible(x) and matches(region,x)
            assert not any(matches(other,x) for other in patterns if other!=region)
            certificates.append(dict(region=region,private_witness=x))
        assert len(patterns)==18
        assert all(any(matches(r,x) for r in patterns) for x in p.prior.points() if x[guard]==1)
        verified=None
        if args.verify_native:
            oracle=Oracle(p);returned=set()
            for point in p.prior.points():
                answer=oracle.query(tuple(enumerate(point)));solver_calls+=1
                assert answer.status==('sat' if p.feasible(point) else 'unsat')
                if answer.status=='unsat':
                    assert answer.core in patterns;returned.add(answer.core)
            assert returned==set(patterns)
            verified=True
        models[name]=dict(vocabulary=patterns,certificates=certificates,native_vocabulary_exhaustively_verified=verified)
    with gzip.open(args.cohort/'rows.jsonl.gz','rt') as file:
        for line in file:
            row=json.loads(line)
            if row['strategy']!='raw':continue
            vocabulary={tuple(map(tuple,r)) for r in models[row['problem']]['vocabulary']}
            for event in row['events']:
                if event['kind']=='business' and event['status']=='unsat':
                    assert tuple(map(tuple,event['core'])) in vocabulary
                    observed+=1
    report=dict(statement='Every one of the 18 local patterns has a point excluded only by that pattern. Any missing pattern leaves a satisfying assignment of the OTHER cached clauses with the guard enabled; sound reasoning from those clauses alone cannot derive the global guard nogood.',
        limitation='Conditional cache-vocabulary lower bound; arbitrary full-model preprocessing or other oracle explanations are outside the claim.',
        native_full_failure_answers_checked=observed,solver_calls=solver_calls,models=models,
        diagnostic_note='Exhaustive verification calls are NOT included in any online sampler call count.',
        cohort_metadata_sha256=hashlib.sha256((args.cohort/'metadata.json').read_bytes()).hexdigest())
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(dict(models=len(models),native_answers=observed,private_witnesses=sum(len(v['certificates']) for v in models.values()),solver_calls=solver_calls)))


if __name__=='__main__':main()
