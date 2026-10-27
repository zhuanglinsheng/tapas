"""Native-core mechanism tests: conditional resource/graph allocation.

No cores are padded, no solver options changed. Guarded infeasible graphs and
their explicitly simplified equivalent encoding isolate explanation strength.
Feasible graphs are negative controls for needless core refinement.
"""
from itertools import combinations,product
from prior import ProductPrior
from workloads import Problem, inputs


def partial_unsat(problem,bindings):
    """Offline exact checker, never consulted by the sampling algorithm.

    K_(k+1) is not k-colorable. A 5-cycle needs three colors, and its universal
    hub needs a fourth. K_k extends every injective partial k-coloring.
    """
    values=dict(bindings)
    if problem.metadata['family']=='multiguard':
        return all(values.get(i)==1 for i in problem.metadata['guard_positions'])
    guard=problem.metadata['guard_position']
    if values.get(guard)!=1:return False
    if problem.metadata['globally_infeasible_enabled_branch']:return True
    if problem.metadata['family']=='packing_feasible':
        free=[i for i in range(len(problem.xvars)) if i not in values]
        for rest in product(*(problem.prior.domains[i] for i in free)):
            full=values|dict(zip(free,rest))
            if problem.feasible(tuple(full[i] for i in range(len(problem.xvars)))):return False
        return True
    assigned=[v for i,v in bindings if i!=guard]
    return len(set(assigned))<len(assigned)


def allocation(family='clique', colors=3, probability=.9, guard_position=0,
               encoding='pairwise', skew=False):
    if family=='clique':
        n=colors+1;edges=list(combinations(range(n),2));impossible=True
    elif family=='feasible':
        n=colors;edges=list(combinations(range(n),2));impossible=False
    elif family=='wheel':
        if colors!=3:raise ValueError('Odd wheel control uses three colors')
        n=6;edges=[(i,(i+1)%5) for i in range(5)]+[(i,5) for i in range(5)];impossible=True
    else:raise ValueError('Unknown allocation family')
    if not 0<=guard_position<=n or not 0<probability<1:raise ValueError('Invalid prior or guard position')
    if encoding not in ('pairwise','simplified') or (encoding=='simplified' and not impossible):
        raise ValueError('Invalid encoding/control combination')
    names=[f'resource_{i}' for i in range(n)];names.insert(guard_position,'enabled')
    domains=[(0,1) if name=='enabled' else tuple(range(colors)) for name in names]
    weights=[[1-probability,probability] if name=='enabled' else
             ([colors]+[1]*(colors-1) if skew else [1]*colors) for name in names]
    prior=ProductPrior(names,domains,weights);model,x=inputs(prior)
    positions=[i for i,name in enumerate(names) if name!='enabled'];guard=x[guard_position]
    if encoding=='simplified':model.add(guard==0)
    else:
        for a,b in edges:model.add(x[positions[a]]!=x[positions[b]]).only_enforce_if(guard)
    def feasible(point):
        return point[guard_position]==0 or all(point[positions[a]]!=point[positions[b]] for a,b in edges)
    label=f'{family}_k{colors}_{encoding}_g{guard_position}_p{probability:g}_s{int(skew)}'
    return Problem(label,prior,model,x,{},feasible,lambda x,y:y=={},
        dict(family=family,colors=colors,edges=edges,guard_position=guard_position,
             encoding=encoding,guard_probability=probability,skew=skew,
             globally_infeasible_enabled_branch=impossible,core_policy='unmodified native CP-SAT'))


def packing(bins=3,probability=.9,guard_position=0,skew=False):
    """Five jobs of sizes 4,4,4,3,3, bin capacity 6, exact load witnesses.

    Three bins are impossible even though total demand equals total capacity:
    each size-4 job needs its own bin and cannot share with size 3. Four bins
    admit a packing. No global infeasibility lemma is supplied to CP-SAT.
    """
    if bins not in (3,4) or not 0<=guard_position<=5 or not 0<probability<1:
        raise ValueError('Invalid packing parameters')
    weights=(4,4,4,3,3);names=[f'job_bin_{i}' for i in range(5)];names.insert(guard_position,'enabled')
    domains=[(0,1) if name=='enabled' else tuple(range(bins)) for name in names]
    ps=[[1-probability,probability] if name=='enabled' else ([bins]+[1]*(bins-1) if skew else [1]*bins) for name in names]
    prior=ProductPrior(names,domains,ps);model,x=inputs(prior)
    positions=[i for i,name in enumerate(names) if name!='enabled'];witness={}
    for bucket in range(bins):
        indicators=[]
        for job,position in enumerate(positions):
            here=model.new_bool_var(f'job{job}_in{bucket}')
            model.add(x[position]==bucket).only_enforce_if(here)
            model.add(x[position]!=bucket).only_enforce_if(here.Not())
            indicators.append(here)
        load=model.new_int_var(0,sum(weights),f'load_{bucket}')
        model.add(load==sum(w*b for w,b in zip(weights,indicators)))
        model.add(load<=6).only_enforce_if(x[guard_position]);witness[f'load_{bucket}']=load
    def loads(point):
        return {f'load_{b}':sum(w for w,i in zip(weights,positions) if point[i]==b) for b in range(bins)}
    def feasible(point):return point[guard_position]==0 or all(v<=6 for v in loads(point).values())
    family='packing' if bins==3 else 'packing_feasible'
    return Problem(f'{family}_b{bins}_g{guard_position}_p{probability:g}_s{int(skew)}',prior,model,x,witness,
        feasible,lambda point,y:y==loads(point),dict(family=family,bins=bins,capacity=6,weights=weights,
        guard_position=guard_position,guard_probability=probability,skew=skew,
        globally_infeasible_enabled_branch=bins==3,core_policy='unmodified native CP-SAT'))


def multiguard(width=2,probability=.9,guard_position=0,encoding='pairwise'):
    """A conjunction of guards, with unique minimal infeasible core of size w."""
    if width not in (2,4) or guard_position not in (0,4) or encoding not in ('pairwise','simplified'):
        raise ValueError('Invalid multiguard parameters')
    names=[f'resource_{i}' for i in range(4)]
    names[guard_position:guard_position]=[f'condition_{i}' for i in range(width)]
    guards=[i for i,n in enumerate(names) if n.startswith('condition_')]
    resources=[i for i,n in enumerate(names) if n.startswith('resource_')]
    p=probability**(1/width)
    prior=ProductPrior(names,[(0,1) if i in guards else (0,1,2) for i in range(len(names))],
        [[1-p,p] if i in guards else [1,1,1] for i in range(len(names))])
    model,x=inputs(prior)
    if encoding=='simplified':model.add_bool_or([x[i].Not() for i in guards])
    else:
        for a,b in combinations(resources,2):model.add(x[a]!=x[b]).only_enforce_if([x[i] for i in guards])
    def feasible(point):
        return not all(point[i]==1 for i in guards) or len({point[i] for i in resources})==4
    return Problem(f'multi_w{width}_{encoding}_g{guard_position}_p{probability:g}',prior,model,x,{},
        feasible,lambda x,y:y=={},dict(family='multiguard',guard_width=width,guard_positions=guards,
        guard_position=guard_position,guard_probability=probability,encoding=encoding,
        globally_infeasible_enabled_branch=True,semantic_minimum_width=width,core_policy='unmodified native CP-SAT'))
