#!/usr/bin/env python3
"""Copy the paper tree away from Tapas, deny access to its source, and run it."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

HERE=Path(__file__).resolve().parent


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--report',type=Path,default=Path('tmp')/'external'/'verification'/'standalone.json')
    parser.add_argument('--skip-paper',action='store_true',help='Verify Python only when TeX is not installed')
    parser.add_argument('--native',action='store_true',help='Also rebuild and verify the optional C accelerator; requires a compiler')
    args=parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='constraint-paper-detached-') as temp:
        base=Path(temp);bundle=base/'unrelated'/'deep'/'research-root';guard=base/'guard';guard.mkdir()
        def ignored(directory,names):
            excluded={'.git','__pycache__','.venv','.cache','.matplotlib','.DS_Store','tmp','results','verification'}
            if Path(directory)==HERE:excluded.add('tau-bench-tapas')
            if Path(directory)==HERE/'paper':excluded.add('build')
            return (set(names)&excluded) | {n for n in names if n.startswith('_sampling_native') and n.endswith(('.so','.pyd','.tmp'))}
        shutil.copytree(HERE,bundle,ignore=ignored)
        # The test never runs the exception, and source provenance must work without Git.
        assert not (bundle/'tau-bench-tapas').exists()
        assert not (bundle/'tau-bench'/'.git').exists()
        original_container=HERE.parent
        blocked=guard/'blocked.txt'
        # These markers are used only to locate a parent Tapas checkout for the
        # negative access guard. Normal paper execution never consults its parent.
        forbidden=original_container if (original_container/'include/tapas').is_dir() else HERE
        (guard/'sitecustomize.py').write_text('''import os,sys
from pathlib import Path
root=Path(os.environ['PAPER_TEST_FORBIDDEN']).resolve()
log=Path(os.environ['PAPER_TEST_BLOCK_LOG'])
def audit(event,args):
    path=None
    if event in ('open','os.chdir','os.listdir','os.scandir') and args and isinstance(args[0],(str,bytes)):
        path=Path(os.fsdecode(args[0])).resolve()
    if path is not None and (path==root or root in path.parents):
        with log.open('a') as out:out.write(event+' '+str(path)+'\\n')
        raise RuntimeError('Standalone run attempted to access original source tree: '+str(path))
    if event=='subprocess.Popen' and str(root) in str(args[:3]):
        with log.open('a') as out:out.write(event+' '+str(args[:3])+'\\n')
        raise RuntimeError('Standalone run attempted to launch original source tree')
sys.addaudithook(audit)
''')
        env={k:v for k,v in os.environ.items() if k not in ('PYTHONPATH','PYTHONHOME') and not k.startswith('TAPAS_')}
        env['PYTHONPATH']=str(guard)
        env['MPLCONFIGDIR']=str(base/'plot-config')
        env['XDG_CACHE_HOME']=str(base/'cache')
        env['PAPER_TEST_FORBIDDEN']=str(forbidden)
        env['PAPER_TEST_BLOCK_LOG']=str(blocked)
        env['PATH']=os.pathsep.join(p for p in env.get('PATH','').split(os.pathsep) if p and not Path(p).resolve().is_relative_to(forbidden))
        work=base/'empty-working-directory';work.mkdir()
        checks=[]
        def run(label,arguments,timeout=180):
            proc=subprocess.run([sys.executable,*arguments],cwd=work,env=env,capture_output=True,text=True,timeout=timeout)
            checks.append(dict(name=label,passed=proc.returncode==0,returncode=proc.returncode,
                               stdout=proc.stdout[-5000:],stderr=proc.stderr[-5000:]))
            if proc.returncode:raise RuntimeError(label+' failed:\n'+proc.stdout+proc.stderr)
            print(label+': passed',flush=True)
        run('git_free_source_verification',[str(bundle/'tau-bench-python/fetch_upstream.py')])
        env['SAMPLING_NATIVE']='0'
        run('sampling_python_fallback_tests',['-m','unittest','discover','-s',str(bundle/'tau-bench-python'),'-p','test_*.py'])
        if args.native:
            run('build_relocated_native_extension',[str(bundle/'tau-bench-python/build_native.py')])
            env.pop('SAMPLING_NATIVE',None)
            run('sampling_native_unit_tests',['-m','unittest','discover','-s',str(bundle/'tau-bench-python'),'-p','test_*.py'])
        run('portability_unit_tests',['-m','unittest','discover','-s',str(bundle/'tests'),'-p','test_*.py'])
        active_output=bundle/'tmp/tau-bench-python/results/detached_active_verification'
        run('active_native_cp_experiment',[str(bundle/'tau-bench-python/benchmark_exploration.py'),
            '--preset','pilot','--limit','4','--samples','4','--seeds','11',
            '--strategies','raw','mass_b4','core_scout','--output',str(active_output)])
        active_summary=json.loads((active_output/'summary.json').read_text())
        assert active_summary['runs']==active_summary['completed']==12 and active_summary['paired_sequences_verified']
        packing_output=bundle/'tmp/tau-bench-python/results/detached_packing_verification'
        run('active_packing_witness_experiment',[str(bundle/'tau-bench-python/benchmark_exploration.py'),
            '--preset','packing','--limit','1','--samples','2','--seeds','11',
            '--strategies','raw','mass_b4','core_scout','--output',str(packing_output)])
        packing_summary=json.loads((packing_output/'summary.json').read_text())
        assert packing_summary['runs']==packing_summary['completed']==3 and packing_summary['paired_sequences_verified']
        context_output=bundle/'tmp/tau-bench-python/results/detached_context_verification'
        run('context_adaptive_native_experiment',[str(bundle/'tau-bench-python/benchmark_exploration.py'),
            '--preset','pilot','--limit','6','--samples','8','--seeds','11',
            '--strategies','raw','context_gate','context_direct','--output',str(context_output)])
        context_summary=json.loads((context_output/'summary.json').read_text())
        assert context_summary['runs']==context_summary['completed']==18 and context_summary['paired_sequences_verified']
        three_output=bundle/'tmp/tau-bench-python/results/detached_three_families'
        run('three_family_native_experiment',[str(bundle/'tau-bench-python/benchmark_exploration.py'),
            '--preset','pilot','--limit','1','--samples','4','--seeds','11',
            '--strategies','rejection','raw','context_gate','--wall-limit','10','--output',str(three_output)])
        three_report=bundle/'tmp/tau-bench-python/results/detached_three_report'
        run('three_family_accounting_report',[str(bundle/'tau-bench-python/compare_sampling_algorithms.py'),
            '--cohort','smoke='+str(three_output),'--primary-active','context_gate','--plots','--output',str(three_report)])
        three_summary=json.loads((three_report/'comparison.json').read_text())
        assert len(three_summary['groups'])==3 and all(g['completed']==1 for g in three_summary['groups'])
        workflow_output=bundle/'tmp/tau-bench-python/results/detached_retail_workflow'
        run('retail_workflow_native_experiment',[str(bundle/'tau-bench-python/benchmark_retail_trajectories.py'),
            '--users','emma_santos_9753','--styles','tight','--horizons','2','--samples','4','--seeds','11',
            '--strategies','rejection','raw','context_gate','--diagnostic-inputs','8','--output',str(workflow_output)])
        workflow_summary=json.loads((workflow_output/'summary.json').read_text())
        assert workflow_summary['runs']==workflow_summary['completed']==9
        assert workflow_summary['original_tool_step_calls']>0 and workflow_summary['paired_sequences_verified']
        certificate_input=bundle/'tmp/tau-bench-python/results/detached_certificate_input'
        run('native_certificate_input',[str(bundle/'tau-bench-python/benchmark_exploration.py'),
            '--preset','pilot','--limit','2','--samples','2','--seeds','11',
            '--strategies','raw','core_scout','--output',str(certificate_input)])
        certificate_output=bundle/'tmp/native_certificate.json'
        run('native_vocabulary_private_witness_certificate',[str(bundle/'tau-bench-python/certify_exploration_gap.py'),
            str(certificate_input),'--verify-native','--output',str(certificate_output)])
        certificate=json.loads(certificate_output.read_text())
        assert certificate['solver_calls']==324 and len(certificate['models'])==2
        output=bundle/'tmp/tau-bench-python/results/detached_verification'
        run('paired_synthetic_and_retail_experiment',[str(bundle/'tau-bench-python/run_experiments.py'),
            '--orders','2','--slots','1','--samples','2','--seeds','11','--strategies','rejection','raw','adaptive_b4',
            '--output',str(output)])
        summary=json.loads((output/'summary.json').read_text())
        assert summary['runs']==39 and summary['paired_sequences_verified']
        for name in ('comparison.png','comparison.pdf','distribution.png','distribution.pdf','report.html'):
            assert (output/name).is_file(),name
        print('standalone_figures: passed',flush=True)
        if not args.skip_paper:
            run('paper_build',[str(bundle/'paper/build.py')])
        denied=blocked.read_text() if blocked.exists() else ''
        if denied:raise RuntimeError('Original tree access was attempted:\n'+denied)
        report=dict(status='passed',date='2026-10-21',copied_to_arbitrary_directory=True,
                    tapas_exception_absent=True,upstream_git_metadata_absent=True,
                    original_source_access_denied=True,blocked_attempts=denied,
                    figures_generated=True,runs=summary['runs'],original_tool_checks=summary['original_tool_checks'],
                    active_exploration_runs=active_summary['runs']+packing_summary['runs']+4+context_summary['runs']+3+workflow_summary['runs'],
                    retail_workflow_runs=workflow_summary['runs'],
                    context_adaptive_runs=context_summary['runs'],
                    certificate_diagnostic_solver_calls=certificate['solver_calls'],
                    paper_compiled=not args.skip_paper,checks=checks,
                    native_rebuilt_and_tested=args.native,
                    source_sha256={str(p.relative_to(HERE)):hashlib.sha256(p.read_bytes()).hexdigest()
                        for directory in ('tau-bench-python','tests','paper')
                        for p in (HERE/directory).iterdir() if p.suffix in ('.py','.c')})
        for file in (HERE/'verify_standalone.py',HERE/'requirements.txt',HERE/'tau-bench-python/UPSTREAM_SOURCE.json'):
            report['source_sha256'][str(file.relative_to(HERE))]=hashlib.sha256(file.read_bytes()).hexdigest()
        args.report.parent.mkdir(parents=True,exist_ok=True)
        args.report.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
        print(args.report.resolve())
    return 0


if __name__=='__main__':raise SystemExit(main())
