"""Offline attribution and figures for explicitly selected exploration cohorts."""
import argparse
from collections import defaultdict
import gzip
import hashlib
import json
import os
from pathlib import Path
import statistics
from prior import matches
from run_experiments import interval
from exploration_workloads import allocation,packing,multiguard,partial_unsat


def verify_native_certificates(meta,row,cache):
    model=meta['problems'][row['problem']]['model'];family=model['family']
    if family=='retail':return 0
    if row['problem'] in cache:p=cache[row['problem']]
    elif family in ('clique','feasible','wheel'):
        p=allocation(family,model['colors'],model['guard_probability'],model['guard_position'],model['encoding'],model['skew'])
    elif family in ('packing','packing_feasible'):
        p=packing(model['bins'],model['guard_probability'],model['guard_position'],model['skew'])
    elif family=='multiguard':
        p=multiguard(model['guard_width'],model['guard_probability'],model['guard_position'],model['encoding'])
    else:return 0  # Retail metadata has operation/type fields instead.
    cache[row['problem']]=p
    assert hashlib.sha256(str(p.model.proto).encode()).hexdigest()==meta['problems'][row['problem']]['cp_model_sha256']
    assert json.dumps(p.prior.metadata(),sort_keys=True)==json.dumps(meta['problems'][row['problem']]['prior'],sort_keys=True)
    checked=0
    for e in row['events']:
        if e['kind'] not in ('business','discovery'):continue
        if e['status']!='unknown':
            assert (e['status']=='unsat')==partial_unsat(p,e['region'])
            checked+=1
        if e['status']=='unsat':assert partial_unsat(p,e['core'])
    return checked


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root',type=Path)
    parser.add_argument('--cohorts',nargs='+',required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--plots',action='store_true')
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    script_bytes=Path(__file__).read_bytes()
    (args.output/'analyze_exploration.py').write_bytes(script_bytes)
    details=[];cohorts=[];algorithm_comparisons=[]
    for cohort in args.cohorts:
        folder=args.root/cohort;meta=json.loads((folder/'metadata.json').read_text())
        summary=json.loads((folder/'summary.json').read_text());pending={};certificates=0;groups=defaultdict(list);checkers={};comparisons=defaultdict(list);equivalent={}
        # The raw comparator is randomized in time; hold only a single paired
        # case until all strategy rows arrive, not the whole experiment log.
        expected=len(meta['protocol']['strategies']);count=0
        with gzip.open(folder/'rows.jsonl.gz','rt') as file:
            for line in file:
                r=json.loads(line);count+=1
                family=meta['problems'][r['problem']]['model'].get('family')
                if family in ('clique','feasible','wheel','packing','packing_feasible','multiguard'):
                    certificates+=verify_native_certificates(meta,r,checkers)
                key=r['problem'],r['requested'],r['seed'];case=pending.setdefault(key,{})
                case[r['strategy']]=r
                if len(case)<expected:continue
                raw=case['raw'];raw_unsat={e['candidate']:e for e in raw['events'] if e['kind']=='business' and e['status']=='unsat'}
                model=meta['problems'][raw['problem']]['model']
                if 'encoding' in model and model.get('globally_infeasible_enabled_branch') and raw['status']=='complete':
                    semantic=tuple(model.get(k) for k in ('family','colors','guard_width','guard_probability','guard_position','skew'))+(raw['requested'],raw['seed'])
                    digest=hashlib.sha256(json.dumps((raw['samples'],raw['positions'])).encode()).hexdigest()
                    equivalent.setdefault(semantic,{})[model['encoding']]=digest
                raw_sat=sum(e['kind']=='business' and e['status']=='sat' for e in raw['events'])
                for name,row in case.items():
                    n=len(row['samples']);assert (row['samples'],row['positions'])==(raw['samples'][:n],raw['positions'][:n])
                    if row['status']!='complete' or raw['status']!='complete':continue
                    unsat={e['candidate']:e for e in row['events'] if e['kind']=='business' and e['status']=='unsat'}
                    sat=sum(e['kind']=='business' and e['status']=='sat' for e in row['events'])
                    discoveries=[e for e in row['events'] if e['kind']=='discovery' and e['status']=='unsat']
                    seconds=lambda events,kind:sum(sum(e.get('seconds',{}).values()) for e in events if e['kind']==kind)
                    probe_seconds=seconds(row['events'],'discovery')
                    avoided=set(raw_unsat)-set(unsat);extra=set(unsat)-set(raw_unsat)
                    covered=0
                    for position in avoided:
                        x=tuple(v for i,v in raw_unsat[position]['region'])
                        covered+=any(e['candidate']<position and matches(e['core'],x) for e in discoveries)
                    u=len(raw_unsat)-len(unsat);s=raw_sat-sat;d=row['discovery_calls'];net=raw['total_calls']-row['total_calls']
                    assert net==u+s-d
                    widths=[len(e['core']) for e in raw_unsat.values()]
                    model=meta['problems'][row['problem']]['model']
                    minimum=model.get('semantic_minimum_width',1) if model.get('globally_infeasible_enabled_branch') else None
                    groups[row['problem'],row['requested'],name].append(dict(seed=row['seed'],
                        unsat_saved=u,sat_saved=s,discovery_calls=d,net_saved=net,
                        avoided_unsat=len(avoided),extra_unsat=len(extra),covered_by_prior_discovery=covered,
                        raw_core_width=statistics.mean(widths) if widths else 0,
                        raw_nonunit_fraction=sum(v>1 for v in widths)/len(widths) if widths else 0,
                        raw_excess_width=statistics.mean(max(0,v-minimum) for v in widths) if widths and minimum is not None else 0,
                        cpu_saved=raw['cpu_seconds']-row['cpu_seconds'],
                        wall_saved=raw['wall_seconds']-row['wall_seconds'],
                        mass_online=row['timings']['mass'],
                        diagnostics_cpu=row.get('diagnostics',{}).get('cpu_seconds',0),
                        coverage_fallbacks=row.get('coverage',{}).get('online_fallbacks',0),
                        discovery_query_seconds=probe_seconds,
                        local_learning_seconds=max(0,row['learning_seconds']-probe_seconds),
                        business_query_seconds_saved=seconds(raw['events'],'business')-seconds(row['events'],'business'),
                        matching_seconds_saved=raw['timings']['match']-row['timings']['match'],
                        sat_inferences=row.get('inference_hits',{}).get('sat',0),
                        discovery_unknown=sum(e['kind']=='discovery' and e['status']=='unknown' for e in row['events'])))
                for before,after in [('core_fifo','core_mass'),('core_mass','core_scout'),
                                     ('mass_b4','core_scout'),('fifo_b4','mass_b4'),('width_b4','mass_b4'),
                                     ('core_scout_unskipped','core_scout'),('core_delayed','core_scout'),
                                     ('core_scout','core_timed'),('mass_safe','core_scout'),('mass_safe','core_timed')]:
                    if before not in case or after not in case:continue
                    a,b=case[before],case[after]
                    if a['status']!='complete' or b['status']!='complete':continue
                    def event_trace(row):
                        return [{k:v for k,v in e.items() if k!='seconds'} for e in row['events']]
                    delta={k:a[k]-b[k] for k in ('total_calls','cpu_seconds','wall_seconds','discovery_calls')}
                    delta['identical_event_trace']=int(event_trace(a)==event_trace(b))
                    comparisons[r['problem'],r['requested'],before,after].append(delta)
                del pending[key]
        assert not pending and count==summary['runs']
        equivalent_checks=0
        for encodings in equivalent.values():
            if 'pairwise' in encodings and 'simplified' in encodings:
                assert encodings['pairwise']==encodings['simplified'],'Equivalent encodings changed acceptance stream'
                equivalent_checks+=1
        for (problem,n,strategy),values in groups.items():
            details.append(dict(cohort=cohort,problem=problem,samples=n,strategy=strategy,
                **{key:interval([v[key] for v in values]) for key in values[0] if key!='seed'}))
        for (problem,n,before,after),values in comparisons.items():
            algorithm_comparisons.append(dict(cohort=cohort,problem=problem,samples=n,before=before,after=after,
                **{key:interval([v[key] for v in values]) for key in values[0]}))
        cohorts.append(dict(cohort=cohort,runs=count,completed=summary.get('completed',count),
            equivalent_encoding_stream_checks=equivalent_checks,
            native_answers_independently_checked=certificates,original_tool_checks=summary.get('original_tool_checks',0),
            timing=meta.get('timing','Historical protocol includes final coverage reporting'),
            metadata_sha256=hashlib.sha256((folder/'metadata.json').read_bytes()).hexdigest()))
        if args.plots and cohort=='grid_online':plot(summary,meta,args.output)
    report=dict(cohorts=cohorts,groups=details,algorithm_comparisons=algorithm_comparisons,
        script_sha256=hashlib.sha256(script_bytes).hexdigest(),
        note='Each case has its own paired seed interval. Missing/incomplete strategies are reported in cohort summaries, never counted as speed wins.')
    (args.output/'attribution.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(dict(cohorts=cohorts,output=str(args.output))))


def plot(summary,meta,out):
    os.environ.setdefault('MPLCONFIGDIR',str(out/'.matplotlib'))
    (out/'.cache/fontconfig').mkdir(parents=True,exist_ok=True)
    os.environ.setdefault('XDG_CACHE_HOME',str((out/'.cache').resolve()))
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from matplotlib.colors import TwoSlopeNorm
    for horizon in (8,128):
        strategies=['mass_b4','fifo_b4','width_b4','core_fifo','core_mass','core_scout']
        models=meta['problems'];families=list(dict.fromkeys((v['model']['family'],v['model'].get('colors'),v['model'].get('encoding')) for v in models.values()))
        fig,axes=plt.subplots(len(families),1,figsize=(15,2.0*len(families)+1),layout='constrained')
        index={(r['problem'],r['samples'],r['strategy']):r for r in summary['groups']}
        image=None
        for ax,identity in zip(axes,families):
            names=[n for n,v in models.items() if (v['model']['family'],v['model'].get('colors'),v['model'].get('encoding'))==identity]
            values=[]
            for strategy in strategies:
                values.append([100*index[n,horizon,strategy]['paired_calls_saved']['mean']/index[n,horizon,'raw']['total_calls']['mean'] for n in names])
            all_values=[100*r['paired_calls_saved']['mean']/index[r['problem'],horizon,'raw']['total_calls']['mean']
                for r in summary['groups'] if r['samples']==horizon and r['strategy'] in strategies]
            image=ax.imshow(values,aspect='auto',cmap='RdBu',
                norm=TwoSlopeNorm(vmin=min(-1,min(all_values)),vcenter=0,vmax=max(1,max(all_values))))
            ax.set_yticks(range(len(strategies)),strategies,fontsize=8)
            labels=[f"g{models[n]['model']['guard_position']} p{models[n]['model']['guard_probability']} s{int(models[n]['model']['skew'])}" for n in names]
            ax.set_xticks(range(len(names)),labels,rotation=30,ha='right',fontsize=7)
            ax.set_title(' / '.join(map(str,identity)),fontsize=10)
            for y,row in enumerate(values):
                for x,value in enumerate(row):ax.text(x,y,f'{value:.0f}',ha='center',va='center',fontsize=7)
        fig.colorbar(image,ax=list(axes),label='Total calls saved vs enhanced raw (%)')
        fig.suptitle(f'{horizon} accepted samples; 5 held-out seeds per cell; positive favors exploration')
        fig.savefig(out/f'calls_phase_{horizon}.png',dpi=180);fig.savefig(out/f'calls_phase_{horizon}.pdf');plt.close(fig)


if __name__=='__main__':main()
