"""Core-directed refinement with optional bounded unsuccessful scouting.

Every probe deletes one literal from an actually certified core. Native cores
replace the working core immediately. SAT witnesses mark essential literals.
This is a core-refinement ablation, not a claim of a new MUS algorithm.
"""
from sampler import Sampler


class CoreSampler(Sampler):
    def __init__(self,problem,config,*,mass_order=True,stop_after=0,skip_empty=True,start_after=0,time_budget=0,oracle=None):
        if config.mode!='mus':raise ValueError('CoreSampler uses the deletion-search budget mode')
        if min(stop_after,start_after,time_budget)<0:raise ValueError('Exploration budgets must be nonnegative')
        super().__init__(problem,config,oracle)
        self.mass_order=mass_order;self.stop_after=stop_after
        self.last_core=();self.core_seen=set();self.unsuccessful=0;self.stopped=False
        self.pending_tasks=0;self.skip_empty=skip_empty;self.start_after=start_after
        self.time_budget=time_budget
        self.probe_stats=dict(sat=0,unsat=0,unknown=0,positive_score=0,skipped_singleton=0,stopped=False,time_stopped=False)

    def query(self,r,discovery=False):
        answer=super().query(r,discovery)
        if not discovery and answer.status=='unsat':self.last_core=answer.core
        return answer

    def add_task(self,x):
        core=tuple(self.last_core)
        if len(core)<=1:
            self.probe_stats['skipped_singleton']+=1
            return  # Safe spending heuristic, not a new feasibility inference.
        if core in self.core_seen or self.stopped:return
        self.core_seen.add(core)
        self.tasks.append(dict(core=core,essential=set(),closed=False))
        self.pending_tasks+=1

    def close(self,task):
        if not task['closed']:
            task['closed']=True;self.pending_tasks-=1

    def choose(self):
        if self.stopped:return None
        available=[]
        for age,task in enumerate(self.tasks):
            if task['closed']:continue
            # A different active proof may already supersede this target.
            known=self.proofs.nogoods.find(task['core'])
            if known is not None and len(known)<len(task['core']):
                task['core']=known;task['essential'].intersection_update(known)
            if len(task['core'])<=1:
                self.close(task);continue
            for literal in task['core']:
                if literal in task['essential']:continue
                query=tuple(v for v in task['core'] if v!=literal)
                gain=self.gain(query)
                if gain>0:available.append((gain,age,literal,task,query))
            if not any(v[3] is task for v in available):self.close(task)
        if not available:return None
        if self.mass_order:
            chosen=max(available,key=lambda r:(r[0],-r[1],-r[2][0]))
        else:chosen=min(available,key=lambda r:(r[1],r[2][0]))
        gain,age,literal,task,query=chosen
        task['testing']=literal
        return task,query,gain

    def allow_learning(self):
        if (self.stopped or self.candidates<self.start_after or
            (self.skip_empty and not self.pending_tasks)):return False
        if self.time_budget and self.learning_seconds>=self.time_budget:
            self.stopped=True;self.probe_stats['stopped']=True;self.probe_stats['time_stopped']=True
            return False
        return super().allow_learning()

    def refine(self,task,bindings,inferred=None):
        answer,evidence,method=self.lookup_proof(bindings) if inferred is None else inferred
        paid=answer is None
        if paid:answer=self.query(bindings,True)
        else:
            self.memo_hits+=1;self.inference_hits[method]+=1
            self.events.append(dict(kind='memo',candidate=self.candidates,region=bindings,
                status=answer.status,method=method,evidence=evidence,core=answer.core))
        positive=False
        if answer.status=='unknown':
            # This is an auxiliary query about an already processed candidate.
            # Abandon the task, never infer a status from the timeout. A business
            # UNKNOWN still follows Sampler.run and stops at that exact input.
            self.close(task)
        elif answer.status=='unsat':
            positive=self.gain(answer.core)>0
            if not set(answer.core).issubset(bindings):raise AssertionError('Invalid native refinement core')
            self.insert(answer.core);task['core']=tuple(answer.core)
            task['essential'].intersection_update(answer.core)
        elif answer.status=='sat':task['essential'].add(task['testing'])
        else:raise AssertionError('Invalid discovery status')
        if len(task['core'])<=1 or len(task['essential'])==len(task['core']):self.close(task)
        if paid:
            self.probe_stats[answer.status]+=1
            self.probe_stats['positive_score']+=int(positive)
            self.unsuccessful=0 if positive else self.unsuccessful+1
            if self.stop_after and self.unsuccessful>=self.stop_after:
                self.stopped=True;self.probe_stats['stopped']=True
                self.events.append(dict(kind='scout_stop',candidate=self.candidates,
                    consecutive_unproductive_probes=self.unsuccessful))
        return ''

    def run(self,*args,**kwargs):
        row=super().run(*args,**kwargs)
        row['core_refinement']=dict(mass_order=self.mass_order,stop_after=self.stop_after,
            skip_empty=self.skip_empty,start_after=self.start_after,time_budget=self.time_budget,**self.probe_stats)
        return row


class SafeAuxMassSampler(Sampler):
    """Original interval search, sharing the safe auxiliary UNKNOWN policy.

    This control isolates search strategy from error/timeout handling. It does
    not add a proof when abandoning an unresolved interval.
    """
    def __init__(self,problem,config,oracle=None):
        if config.mode!='mass':raise ValueError('Safe auxiliary control uses interval search')
        super().__init__(problem,config,oracle)

    def refine(self,task,bindings,inferred=None):
        failure=super().refine(task,bindings,inferred)
        if failure=='unknown':
            task.closed=True
            self.events.append(dict(kind='auxiliary_abandoned',candidate=self.candidates,region=bindings))
            return ''
        return failure
