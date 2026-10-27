"""Offline causal diagnostics for original-tool-checked Retail workflows.

Reachability and alternative explanations are diagnostics only, never supplied
as online proofs or used to choose the sampler's next query.
"""
import argparse
from collections import defaultdict,Counter
from functools import lru_cache
import gzip
import hashlib
import html
from itertools import product
import json
from pathlib import Path
from statistics import mean
from retail_trajectories import RetailCatalog,trajectory_problem
from workloads import load_retail
from run_experiments import interval


def coupling_effects(p):
    """Exact joint acceptance masses under reset versus carried state."""
    c=p.catalog;states={(c.initial,True):1.};peak=1
    for t in range(p.metadata['horizon']):
        new=defaultdict(float)
        oi=p.prior.names.index(f'op_{t}');ri=p.prior.names.index(f'request_{t}')
        for (state,mixed_ok),mass in states.items():
            for op,po in p.prior.maps[oi].items():
                for rank,pr in p.prior.maps[ri].items():
                    k=2*op+rank
                    after=c.apply(state,k) if state is not None else None
                    reset_ok=mixed_ok and c.apply(c.initial,k) is not None
                    new[after,reset_ok]+=mass*po*pr
        states=new;peak=max(peak,len(states))
    flags=1.
    for i,n in enumerate(p.prior.names):
        if n.startswith(('auth_','confirmed_')):flags*=p.prior.maps[i][1]
    result=dict(both_feasible=0.,coupling_enabled=0.,coupling_forbidden=0.,both_infeasible=1-flags)
    for (state,mixed_ok),mass in states.items():
        key=('both_feasible' if mixed_ok else 'coupling_enabled') if state is not None else ('coupling_forbidden' if mixed_ok else 'both_infeasible')
        result[key]+=mass*flags
    assert abs(sum(result.values())-1)<1e-9
    result.update(mixed_z=result['both_feasible']+result['coupling_forbidden'],
                  coupled_z=result['both_feasible']+result['coupling_enabled'],peak_joint_states=peak)
    return result


def checker(p):
    c=p.catalog;positions={n:i for i,n in enumerate(p.prior.names)};h=p.metadata['horizon']
    @lru_cache(maxsize=8192)
    def exists(bindings):
        fixed=dict(bindings)
        choices=[]
        for t in range(h):
            if p.metadata['policy_flags'] and any(fixed.get(positions[f'{flag}_{t}'],1)!=1 for flag in ('auth','confirmed')):return False
            oi=positions[f'op_{t}'];ri=positions[f'request_{t}']
            ops=(fixed[oi],) if oi in fixed else p.prior.domains[oi]
            ranks=(fixed[ri],) if ri in fixed else p.prior.domains[ri]
            choices.append(tuple(2*op+rank for op,rank in product(ops,ranks)))
        @lru_cache(maxsize=None)
        def visit(t,state):
            if t==h:return True
            before=state if p.metadata['mode']=='coupled' else c.initial
            for k in choices[t]:
                after=c.apply(before,k)
                if after is not None and visit(t+1,after):return True
            return False
        return visit(0,c.initial)
    return exists


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('root',type=Path)
    ap.add_argument('--cohorts',nargs='+',default=['main','ablations','ops_first','longer','flags','terminal_prior','same_operation','call_budget'])
    ap.add_argument('--output',type=Path,required=True)
    ap.add_argument('--performance',type=Path,help='comparison.json from compare_sampling_algorithms.py')
    args=ap.parse_args()
    if args.output.exists() and any(args.output.iterdir()):ap.error('Output must be new/empty')
    args.output.mkdir(parents=True,exist_ok=True);source=Path(__file__).read_bytes()
    (args.output/'analyze_retail_trajectories.py').write_bytes(source)
    data=load_retail();groups=defaultdict(list);cohorts=[];characterization=[]
    for cohort in args.cohorts:
        root=args.root/cohort;meta=json.loads((root/'metadata.json').read_text());summary=json.loads((root/'summary.json').read_text())
        ps={};character={};proofs_checked=0;minimality_checks=0;pending={};expected_methods=len(meta['protocol']['strategies'])
        def problem(name):
            if name in ps:return ps[name]
            description=meta['problems'][name];m=description['model'];cm=m['catalog']
            c=RetailCatalog(data,cm['user'],cm['upgrade_style'])
            p=trajectory_problem(c,m['horizon'],m['mode'],m['operation'] or 'modify_items',m['layout'],m['prior_kind'],m['policy_flags'],m['operation'])
            assert p.name==name
            assert hashlib.sha256(str(p.model.proto).encode()).hexdigest()==description['cp_model_sha256']
            assert json.dumps(p.prior.metadata(),sort_keys=True)==json.dumps(description['prior'],sort_keys=True)
            ex=checker(p);ps[name]=(p,ex,{});return ps[name]
        with gzip.open(root/'rows.jsonl.gz','rt') as f:
            for line in f:
                row=json.loads(line);key=row['problem'],row['requested'],row['seed'];case=pending.setdefault(key,{})
                case[row['strategy']]=row
                if len(case)!=expected_methods:continue
                raw=case['raw'];p,exists,core_info=problem(raw['problem'])
                m=p.metadata
                if p.name not in character:
                    effect=coupling_effects(p)
                    expected=effect['coupled_z'] if m['mode']=='coupled' else effect['mixed_z']
                    assert abs(expected-meta['problems'][p.name]['feasible']['z'])<1e-10
                    character[p.name]=dict(problem=p.name,mode=m['mode'],user=m['catalog']['user'],style=m['catalog']['upgrade_style'],
                        horizon=m['horizon'],operation=m['operation'],layout=m['layout'],prior=m['prior_kind'],policy_flags=m['policy_flags'],
                        feasible=meta['problems'][p.name]['feasible'],state_coupling_effects=effect)
                for r in case.values():
                    n=min(len(raw['samples']),len(r['samples']))
                    assert (r['samples'][:n],r['positions'][:n])==(raw['samples'][:n],raw['positions'][:n])
                failures=[e for e in raw['events'] if e['kind']=='business' and e['status']=='unsat']
                info=[];outside=alternative=0
                for e in failures:
                    core=tuple(map(tuple,e['core']))
                    if core not in core_info:
                        assert not exists(core);proofs_checked+=1
                        removals=[lit for lit in core if not exists(tuple(v for v in core if v!=lit))]
                        minimality_checks+=len(core)
                        core_info[core]=dict(width=len(core),subset_minimal=not removals,removable=len(removals))
                    info.append(core_info[core])
                    op_projection=tuple((i,v) for i,v in e['region'] if p.prior.names[i].startswith('op_'))
                    if not exists(op_projection):
                        alternative+=1
                        candidate=op_projection
                        for literal in op_projection:
                            smaller=tuple(v for v in candidate if v!=literal)
                            if not exists(smaller):candidate=smaller
                        outside+=not set(candidate).issubset(core)
                for name,r in case.items():
                    if r['status']!=raw['status'] or r['status']!='complete':continue
                    discoveries=[e for e in r['events'] if e['kind']=='discovery']
                    bu=sum(e['kind']=='business' and e['status']=='unsat' for e in r['events'])
                    bs=sum(e['kind']=='business' and e['status']=='sat' for e in r['events'])
                    raws=sum(e['kind']=='business' and e['status']=='sat' for e in raw['events'])
                    net=raw['total_calls']-r['total_calls'];assert net==len(failures)-bu+raws-bs-r['discovery_calls']
                    vals=dict(net_calls_saved=net,unsat_saved=len(failures)-bu,sat_saved=raws-bs,
                        discovery_calls=r['discovery_calls'],discovery_sat=sum(e['status']=='sat' for e in discoveries),
                        discovery_unsat=sum(e['status']=='unsat' for e in discoveries),discovery_unknown=sum(e['status']=='unknown' for e in discoveries),
                        cpu_saved=raw['cpu_seconds']-r['cpu_seconds'],
                        raw_core_width=mean(v['width'] for v in info) if info else 0,
                        raw_unit_fraction=mean(v['width']==1 for v in info) if info else 0,
                        raw_subset_minimal_fraction=mean(v['subset_minimal'] for v in info) if info else 0,
                        raw_removable_literals=mean(v['removable'] for v in info) if info else 0,
                        operation_only_alternative_fraction=alternative/len(failures) if failures else 0,
                        outside_core_alternative_fraction=outside/len(failures) if failures else 0)
                    groups[cohort,p.name,r['requested'],name].append(vals)
                del pending[key]
        assert not pending
        characterization.extend(dict(cohort=cohort,**v) for v in character.values())
        cohorts.append(dict(cohort=cohort,runs=summary['runs'],completed=summary['completed'],
            original_tool_inputs=summary['original_tool_checks'],original_tool_step_calls=summary['original_tool_step_calls'],
            independently_checked_native_cores_in_driver=summary['independent_native_cores_checked'],
            raw_cores_rechecked=proofs_checked,one_literal_deletions_checked=minimality_checks,
            metadata_sha256=hashlib.sha256((root/'metadata.json').read_bytes()).hexdigest()))
    report=dict(cohorts=cohorts,characterization=characterization,
        groups=[dict(cohort=c,problem=p,samples=n,strategy=s,**{k:interval([v[k] for v in vs]) for k in vs[0]}) for (c,p,n,s),vs in groups.items()],
        script_sha256=hashlib.sha256(source).hexdigest(),
        interpretation='Subset-minimality is exact within a native core, not global maximum mass. Operation projections may use literals outside that core and need not contain its whole region. Reachability checks are offline, not free online queries. Different configurations/prefixes are not pooled as independent observations.')
    (args.output/'attribution.json').write_text(json.dumps(report,indent=2)+'\n')
    if args.performance:render_workflow(json.loads(args.performance.read_text()),report,args.output)
    print(json.dumps(cohorts))


def render_workflow(performance,diagnosis,out):
    index={(g['cohort'],g['problem'],g['samples'],g['strategy']):g for g in performance['groups']}
    chars={(g['cohort'],g['problem']):g for g in diagnosis['characterization']}
    pairs={(g['cohort'],g['problem'],g['samples'],g['baseline'],g['strategy']):g for g in performance['comparisons']}
    rows=[]
    for key,g in index.items():
        cohort,problem,n,strategy=key
        if strategy!='raw':continue
        char=chars[cohort,problem];active=index.get((cohort,problem,n,'context_gate'))
        reject=index.get((cohort,problem,n,'rejection'))
        def value(row,metric,scale=1):
            return f"{row['complete'][metric]['mean']*scale:.1f}" if row and row['complete'] else '—'
        pair=pairs.get((cohort,problem,n,'raw','context_gate'))
        net=value({'complete':pair} if pair else None,'total_calls_saved')
        complete=' / '.join(f"{r['completed']}/{r['attempts']}" if r else 'n/a' for r in (reject,g,active))
        mode=char['mode']+(':'+char['operation'] if char['operation'] else '')
        cells=[cohort,char['user'].split('_')[0],char['style'],mode,char['horizon'],n,
            f"{char['feasible']['z']:.4f}",value(reject,'total_calls'),value(g,'total_calls'),
            value(active,'business_calls'),value(active,'discovery_calls'),value(active,'total_calls'),net,
            value(reject,'cpu_seconds',1000),value(g,'cpu_seconds',1000),value(active,'cpu_seconds',1000),complete]
        rows.append('<tr>'+''.join('<td>'+html.escape(str(v))+'</td>' for v in cells)+'</tr>')
    effects=[]
    for c in diagnosis['characterization']:
        if c['cohort']!='main' or c['mode']!='coupled':continue
        e=c['state_coupling_effects']
        cells=[c['user'].split('_')[0],c['style'],c['horizon'],e['mixed_z'],e['coupled_z'],e['coupling_forbidden'],e['coupling_enabled']]
        effects.append('<tr>'+''.join('<td>'+html.escape(f'{v:.5f}' if isinstance(v,float) else str(v))+'</td>' for v in cells)+'</tr>')
    def table(headers,body):return '<div class="scroll"><table><thead><tr>'+''.join('<th>'+html.escape(v)+'</th>' for v in headers)+'</tr></thead><tbody>'+''.join(body)+'</tbody></table></div>'
    text='''<!doctype html><meta charset="utf-8"><title>Retail workflow experiment</title>
<style>body{font:13px system-ui;margin:22px}table{border-collapse:collapse}td,th{border:1px solid #ccc;padding:5px;white-space:nowrap}th{background:#eee}.scroll{overflow:auto;margin-bottom:20px}input{padding:8px;width:70%}</style>
<h1>Retail：独立操作与状态耦合</h1>
<p>主算法三类：拒绝采样、原始核缓存、冗余门控主动探索。每个样本是一整段动作序列；Q=B+D。所有金额来自原始数据，原始工具仅离线核对。时间为完整运行的 CPU 毫秒均值。未完成不显示为零，不与完整运行计算加速。</p>
<p>main 是三组主对照；其他 cohort 是独立敏感性/消融协议，不能合并当作独立重复。完成率依次为拒绝/缓存/主动；n/a 表示该消融没有该方法。完整 SAT/UNSAT/UNKNOWN、墙钟、置信区间和主动内部变体见通用 CSV/JSON 报告。</p>
<p><a href="../comparison/report.html">完整性能报告</a> · <a href="../comparison/comparison.csv">性能 CSV</a> · <a href="attribution.json">核与探索归因 JSON</a></p>
<input id="filter" placeholder="Filter: main, coupled, mixed, tight, nearest, emma, yusuf ...">
'''
    text+=table(['Cohort','Profile','Variant','Mode','H','N','Z','Q reject','Q cache','B active','D active','Q active','Q saved vs cache','CPU reject ms','CPU cache ms','CPU active ms','Complete R/C/A'],rows)
    text+='<h2>状态耦合的精确接受质量变化</h2><p>混合与耦合使用相同输入先验。forbidden 是原先独立合法但连续执行失败的质量；enabled 是原先独立失败但先前状态变化使之合法的质量。Z 的计算与本表全部离线，不供算法决策。</p>'
    text+=table(['Profile','Variant','H','Z mixed','Z coupled','Coupling forbidden','Coupling enabled'],effects)
    text+='<script>document.getElementById("filter").oninput=function(){let q=this.value.toLowerCase();document.querySelectorAll("tbody tr").forEach(r=>r.hidden=!r.textContent.toLowerCase().includes(q));}</script>'
    (out/'workflow_report.html').write_text(text)


if __name__=='__main__':main()
