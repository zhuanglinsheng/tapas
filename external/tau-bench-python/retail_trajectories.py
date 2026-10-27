"""Bounded Retail workflows: independent actions versus native state propagation.

Finite catalogs contain real, structurally valid original-tool requests. Balance,
status and payment-history eligibility remain solver constraints. No trajectory
solutions or global nogoods are compiled into the model.
"""
from dataclasses import dataclass
from copy import deepcopy
from functools import lru_cache
from itertools import product
import json
from prior import ProductPrior
from workloads import Problem,STATUS,cents,inputs,load_retail

OPS=('cancel','address','payment','modify_items','return','exchange','user_address')
TOOLS=('cancel_pending_order','modify_pending_order_address','modify_pending_order_payment',
       'modify_pending_order_items','return_delivered_order_items','exchange_delivered_order_items','modify_user_address')
ADDRESSES=[dict(address1=f'{i} Sampling Loop',address2='',city='Boston',state='MA',country='USA',zip='02101') for i in (1,2)]


@dataclass(frozen=True)
class Request:
    operation: str
    order: int
    kwargs: dict
    required_status: int=-1
    post_status: int=-1
    check_gift: int=-1
    amount: int=0
    debit: tuple=()
    history_add: tuple=()
    history_count: int=0
    single_payment: bool=False
    cancel: bool=False
    address: int=0
    item_version: int=0


@dataclass(frozen=True)
class State:
    status: tuple
    history: tuple
    credit: tuple
    balance: tuple
    address: tuple
    items: tuple
    user_address: int=0


class RetailCatalog:
    def __init__(self,data,uid,upgrade_style='tight'):
        if upgrade_style not in ('tight','nearest'):raise ValueError('Known real-variant selection required')
        self.uid=uid;user=data['users'][uid]
        orders=sorted([o for o in data['orders'].values() if o['user_id']==uid],key=lambda o:o['order_id'])
        unique=lambda o:len({i['item_id'] for i in o['items']})==len(o['items'])
        pending=[o for o in orders if o['status']=='pending' and unique(o) and len(o['payment_history'])==1
                 and o['payment_history'][0]['transaction_type']=='payment']
        delivered=[o for o in orders if o['status']=='delivered' and unique(o)]
        self.gifts=sorted(mid for mid,pm in user['payment_methods'].items() if pm['source']=='gift_card')
        nongift=sorted(mid for mid,pm in user['payment_methods'].items() if pm['source']!='gift_card')
        if len(pending)<2 or len(delivered)<2 or not self.gifts or not nongift:raise ValueError('Catalog requires two pending, two delivered, gift and non-gift methods')
        if any(('gift_card' in mid)!=(pm['source']=='gift_card') for mid,pm in user['payment_methods'].items()):
            raise ValueError('This bounded catalog requires consistent gift method IDs')
        gift=self.gifts[0];budget=cents(user['payment_methods'][gift]['balance'])
        def variants(o,positive=False):
            candidates=[]
            for slot,item in enumerate(o['items']):
                for iid,v in sorted(data['products'][item['product_id']]['variants'].items()):
                    delta=cents(v['price'])-cents(item['price'])
                    if v['available'] and v['options']!=item['options'] and (not positive or 0<delta<=budget):
                        candidates.append((delta,slot,iid))
            return sorted(candidates)
        upgrades={o['order_id']:variants(o,True) for o in pending}
        tight=[(a,b) for i,a in enumerate(pending) for b in pending[i+1:]
               if upgrades[a['order_id']] and upgrades[b['order_id']]
               and upgrades[a['order_id']][-1][0]+upgrades[b['order_id']][-1][0]>budget]
        if tight:
            selected=list(tight[0]);changes=[upgrades[o['order_id']][-1] for o in selected]
        else:
            eligible=[o for o in pending if variants(o)]
            if len(eligible)<2:raise ValueError('No two real modification options')
            selected=eligible[:2];changes=[min(variants(o),key=lambda v:abs(v[0])) for o in selected]
        if upgrade_style=='nearest':changes=[min(variants(o),key=lambda v:abs(v[0])) for o in selected]
        ds=[o for o in delivered if variants(o)]
        if len(ds)<2:raise ValueError('No two delivered exchange options')
        self.orders=selected+ds[:2];self.ids=[o['order_id'] for o in self.orders]
        pids={i['product_id'] for o in self.orders for i in o['items']}
        self.db=dict(users={uid:deepcopy(user)},orders={o['order_id']:deepcopy(o) for o in self.orders},
                     products={pid:deepcopy(data['products'][pid]) for pid in pids})
        self.initial=State(tuple(STATUS[o['status']] for o in self.orders),
            tuple(len(o['payment_history']) for o in self.orders),
            tuple(sum(cents(t['amount']) for t in o['payment_history'] if t['payment_method_id']==mid)
                  for o in self.orders for mid in self.gifts),
            tuple(cents(user['payment_methods'][mid]['balance']) for mid in self.gifts),(0,)*4,(0,)*4)
        rows=[]
        def gift_index(mid):return self.gifts.index(mid) if mid in self.gifts else -1
        for op in OPS:
            for rank in (0,1):
                q=rank if op in ('cancel','address','payment','modify_items') else rank+2
                o=self.orders[q] if op!='user_address' else None
                kw={'order_id':o['order_id']} if o else {'user_id':uid}
                args=dict(operation=op,order=q if o else -1,kwargs=kw,debit=(0,)*len(self.gifts),history_add=(0,)*len(self.gifts))
                if o:args.update(required_status=STATUS['pending' if q<2 else 'delivered'],post_status=STATUS[o['status']])
                if op=='cancel':
                    kw['reason']=('no longer needed','ordered by mistake')[rank]
                    args.update(cancel=True,post_status=STATUS['cancelled'])
                elif op in ('address','user_address'):
                    kw.update(ADDRESSES[rank]);args.update(address=rank+1)
                elif op=='payment':
                    old=o['payment_history'][0]['payment_method_id'];new=nongift[0] if old in self.gifts else gift
                    kw['payment_method_id']=new;amount=cents(o['payment_history'][0]['amount'])
                    debit=[0]*len(self.gifts);history=[0]*len(self.gifts)
                    for mid,sign in ((new,1),(old,-1)):
                        g=gift_index(mid)
                        if g>=0:debit[g]+=sign*amount;history[g]+=amount
                    args.update(single_payment=True,check_gift=gift_index(new),amount=amount,
                                debit=tuple(debit),history_add=tuple(history),history_count=2)
                elif op in ('modify_items','exchange','return'):
                    slot=changes[rank][1] if op=='modify_items' else 0
                    item=o['items'][slot];kw['item_ids']=[item['item_id']]
                    mid=gift if op=='modify_items' else nongift[0] if op=='exchange' else o['payment_history'][0]['payment_method_id']
                    kw['payment_method_id']=mid
                    if op in ('modify_items','exchange'):
                        delta,slot,new=changes[rank] if op=='modify_items' else min(variants(o),key=lambda v:abs(v[0]))
                        item=o['items'][slot];kw['item_ids']=[item['item_id']];kw['new_item_ids']=[new]
                        args.update(check_gift=gift_index(mid),amount=delta)
                    if op=='modify_items':
                        debit=[0]*len(self.gifts);history=[0]*len(self.gifts);g=gift_index(mid)
                        if g>=0:debit[g]=delta;history[g]=abs(delta)
                        args.update(debit=tuple(debit),history_add=tuple(history),history_count=1,
                                    post_status=STATUS['pending (item modified)'],item_version=len(rows)+1)
                    elif op=='exchange':args.update(post_status=STATUS['exchange requested'])
                    else:args.update(post_status=STATUS['return requested'])
                rows.append(Request(**args))
        self.requests=tuple(rows)
        self.metadata=dict(user=uid,orders=self.ids,gift_ids=self.gifts,initial_gift_cents=list(self.initial.balance),
            tight_two_upgrade_pair=all(0<v[0]<=budget for v in changes) and sum(v[0] for v in changes)>budget,
            upgrade_style=upgrade_style,upgrade_deltas=[v[0] for v in changes],
            catalog=[dict(operation=r.operation,kwargs=r.kwargs) for r in rows],
            selection='Data properties only: two pending and two delivered; positive affordable upgrades whose sum exceeds real balance when present. No solver/performance selection.',
            coverage='Finite one-item concrete request catalog, seven original tools. No arbitrary IDs, duplicate-item requests, LLM dialogue or inventory mutation.')

    def apply(self,state,k):
        """Independent integer transition interpreter, not a solver invocation."""
        r=self.requests[k];q=r.order;g=len(self.gifts)
        if q>=0 and state.status[q]!=r.required_status:return None
        if r.single_payment and state.history[q]!=1:return None
        if r.check_gift>=0 and state.balance[r.check_gift]<r.amount:return None
        status=list(state.status);hist=list(state.history);credit=list(state.credit)
        balance=list(state.balance);addr=list(state.address);items=list(state.items);ua=state.user_address
        if q>=0:
            status[q]=r.post_status
            if r.cancel:
                for j in range(g):balance[j]+=credit[q*g+j];credit[q*g+j]*=2
                hist[q]*=2
            else:
                hist[q]+=r.history_count
                for j in range(g):credit[q*g+j]+=r.history_add[j]
            if r.operation=='address':addr[q]=r.address
            if r.item_version:items[q]=r.item_version
        elif r.operation=='user_address':ua=r.address
        if not r.cancel:
            for j in range(g):balance[j]-=r.debit[j]
        return State(tuple(status),tuple(hist),tuple(credit),tuple(balance),tuple(addr),tuple(items),ua)

    def project(self,db):
        orders=[db['orders'][oid] for oid in self.ids];user=db['users'][self.uid]
        def address_code(address,initial):
            if address==initial:return 0
            for i,a in enumerate(ADDRESSES,1):
                if address==a:return i
            raise AssertionError('Unmodeled address')
        def normalize(x):
            if isinstance(x,float):return round(x,2)
            if isinstance(x,dict):return {k:normalize(v) for k,v in x.items()}
            if isinstance(x,list):return [normalize(v) for v in x]
            return x
        versions=[]
        for q,o in enumerate(orders):
            old=self.orders[q]
            if normalize(o['items'])==normalize(old['items']):versions.append(0);continue
            for r in self.requests:
                if r.order!=q or not r.item_version:continue
                expected=deepcopy(old['items']);item=next(v for v in expected if v['item_id']==r.kwargs['item_ids'][0])
                iid=r.kwargs['new_item_ids'][0];v=db['products'][item['product_id']]['variants'][iid]
                item.update(item_id=iid,price=v['price'],options=v['options'])
                if normalize(o['items'])==normalize(expected):versions.append(r.item_version);break
            else:raise AssertionError('Unmodeled item update')
        return State(tuple(STATUS[o['status']] for o in orders),tuple(len(o['payment_history']) for o in orders),
            tuple(sum(round(t['amount']*100) for t in o['payment_history'] if t['payment_method_id']==mid)
                  for o in orders for mid in self.gifts),
            tuple(round(user['payment_methods'][mid]['balance']*100) for mid in self.gifts),
            tuple(address_code(o['address'],self.orders[q]['address']) for q,o in enumerate(orders)),tuple(versions),
            address_code(user['address'],self.db['users'][self.uid]['address']))


def catalogs(data=None,upgrade_style='tight'):
    data=data or load_retail();result=[]
    for uid in sorted(data['users']):
        try:result.append(RetailCatalog(data,uid,upgrade_style))
        except ValueError:continue
    return result


def flatten(state,t):
    result={}
    for name in ('status','history','credit','balance','address','items'):
        for i,v in enumerate(getattr(state,name)):result[f'{name}_{t}_{i}']=v
    result[f'user_address_{t}']=state.user_address
    return result


def trajectory_problem(catalog,horizon=4,mode='coupled',operation='modify_items',layout='interleaved',prior_kind='uniform',policy_flags=False,fixed_operation=None):
    if mode not in ('single','mixed','coupled') or horizon<1 or layout not in ('interleaved','ops_first'):raise ValueError('Known mode/layout and positive horizon required')
    pairs=([(f'op_{t}',tuple(range(7)),[1]*7) for t in range(horizon)]+[(f'request_{t}',(0,1),[1,1]) for t in range(horizon)]
           if layout=='ops_first' else [spec for t in range(horizon) for spec in ((f'op_{t}',tuple(range(7)),[1]*7),(f'request_{t}',(0,1),[1,1]))])
    if prior_kind not in ('uniform','terminal_heavy'):raise ValueError('Known prior required')
    restriction=operation if mode=='single' else fixed_operation
    if restriction is not None and restriction not in OPS:raise ValueError('Known fixed operation required')
    pairs=[(n,(OPS.index(restriction),) if restriction is not None and n.startswith('op_') else d,
            [1] if restriction is not None and n.startswith('op_') else [3,1,1,3,3,3,1] if n.startswith('op_') and prior_kind=='terminal_heavy' else w) for n,d,w in pairs]
    if policy_flags:pairs.extend((f'{flag}_{t}',(0,1),[1,19]) for t in range(horizon) for flag in ('auth','confirmed'))
    prior=ProductPrior([p[0] for p in pairs],[p[1] for p in pairs],[p[2] for p in pairs]);model,xs=inputs(prior);v=dict(zip(prior.names,xs))
    witness={};initial=catalog.initial;ng=len(catalog.gifts)
    state_fields=('status','history','credit','balance','address','items')
    # Bounds cover every original history-doubling/credit update in this finite
    # horizon, rather than restricting reachable balances to initial funds.
    max_money=max([1,*initial.balance,*initial.credit,*[abs(r.amount) for r in catalog.requests]])
    monetary_bound=(max_money*(len(catalog.orders)+horizon+2))*(2**(horizon+2))
    if monetary_bound>=2**60:raise ValueError('Horizon exceeds safe integer bounds')
    bounds=dict(status=(0,6),history=(0,max(initial.history)*2**horizon+2*horizon),
                credit=(0,monetary_bound),balance=(0,monetary_bound),address=(0,2),items=(0,len(catalog.requests)))
    previous={name:list(getattr(initial,name)) for name in state_fields};previous['user_address']=[initial.user_address]
    for t in range(horizon):
        before=previous if mode=='coupled' else {name:list(getattr(initial,name)) for name in state_fields}|{'user_address':[initial.user_address]}
        after={}
        for name in state_fields:
            after[name]=[]
            for i in range(len(getattr(initial,name))):
                y=model.new_int_var(*bounds[name],f'{name}_{t+1}_{i}');witness[y.name]=y;after[name].append(y)
        ua=model.new_int_var(0,2,f'user_address_{t+1}');witness[ua.name]=ua;after['user_address']=[ua]
        if policy_flags:
            for flag in ('auth','confirmed'):model.add(v[f'{flag}_{t}']==1)
        index=model.new_int_var(0,len(catalog.requests)-1,f'action_{t}');model.add(index==2*v[f'op_{t}']+v[f'request_{t}'])
        gates=[]
        for k,r in enumerate(catalog.requests):
            gate=model.new_bool_var(f'action_{t}_{k}');gates.append(gate)
            model.add(index==k).only_enforce_if(gate);model.add(index!=k).only_enforce_if(gate.Not())
            q=r.order
            if q>=0:model.add(before['status'][q]==r.required_status).only_enforce_if(gate)
            if r.single_payment:model.add(before['history'][q]==1).only_enforce_if(gate)
            if r.check_gift>=0:model.add(before['balance'][r.check_gift]>=r.amount).only_enforce_if(gate)
        def selected_value(value,gate,bound,name):
            if isinstance(value,int):return value*gate
            z=model.new_int_var(0,bound,name)
            model.add(z==value).only_enforce_if(gate);model.add(z==0).only_enforce_if(gate.Not())
            return z
        cancel_credits={}
        for oi in range(len(catalog.orders)):
            relevant=[(g,r) for g,r in zip(gates,catalog.requests) if r.order==oi]
            cancels=[g for g,r in relevant if r.cancel]
            # Only pending orders have cancellation templates, at most one each.
            cancel=cancels[0] if cancels else None
            model.add(after['status'][oi]==before['status'][oi]+sum(g*(r.post_status-r.required_status) for g,r in relevant))
            extra=selected_value(before['history'][oi],cancel,bounds['history'][1],f'cancel_history_{t}_{oi}') if cancel is not None else 0
            model.add(after['history'][oi]==before['history'][oi]+extra+sum(g*r.history_count for g,r in relevant))
            for j in range(ng):
                ci=oi*ng+j
                extra=selected_value(before['credit'][ci],cancel,monetary_bound,f'cancel_credit_{t}_{oi}_{j}') if cancel is not None else 0
                cancel_credits[oi,j]=extra
                model.add(after['credit'][ci]==before['credit'][ci]+extra+sum(g*r.history_add[j] for g,r in relevant))
            for field,choices in [('address',[(g,r.address) for g,r in relevant if r.operation=='address']),
                                  ('items',[(g,r.item_version) for g,r in relevant if r.item_version])]:
                model.add(after[field][oi]==before[field][oi]).only_enforce_if([g.Not() for g,value in choices])
                for g,value in choices:model.add(after[field][oi]==value).only_enforce_if(g)
        for j in range(ng):
            model.add(after['balance'][j]==before['balance'][j]+sum(cancel_credits[oi,j] for oi in range(len(catalog.orders)))
                      -sum(g*r.debit[j] for g,r in zip(gates,catalog.requests)))
        addresses=[(g,r.address) for g,r in zip(gates,catalog.requests) if r.operation=='user_address']
        model.add(ua==before['user_address'][0]).only_enforce_if([g.Not() for g,value in addresses])
        for g,value in addresses:model.add(ua==value).only_enforce_if(g)
        previous=after
    def simulate(x):
        values=dict(zip(prior.names,x));state=initial;y={}
        for t in range(horizon):
            if policy_flags and (values[f'auth_{t}']!=1 or values[f'confirmed_{t}']!=1):return None
            if mode!='coupled':state=initial
            state=catalog.apply(state,2*values[f'op_{t}']+values[f'request_{t}'])
            if state is None:return None
            y.update(flatten(state,t+1))
        return y
    positions={n:i for i,n in enumerate(prior.names)}
    @lru_cache(maxsize=4096)
    def partial(bindings):
        fixed=dict(bindings);states={initial}
        for t in range(horizon):
            if policy_flags and any(fixed.get(positions[f'{flag}_{t}'],1)!=1 for flag in ('auth','confirmed')):return False
            op_index=positions[f'op_{t}'];request_index=positions[f'request_{t}']
            os=(fixed[op_index],) if op_index in fixed else prior.domains[op_index]
            rs=(fixed[request_index],) if request_index in fixed else prior.domains[request_index]
            before=states if mode=='coupled' else {initial};new=set()
            for state in before:
                for op,rank in product(os,rs):
                    out=catalog.apply(state,2*op+rank)
                    if out is not None:new.add(out)
            if not new:return False
            states=new
        return True
    def replay(x,original):
        values=dict(zip(prior.names,x));db=deepcopy(catalog.db);expected=initial;count=0
        for t in range(horizon):
            if policy_flags and (values[f'auth_{t}']!=1 or values[f'confirmed_{t}']!=1):return False,count
            if mode!='coupled':db=deepcopy(catalog.db);expected=initial
            k=2*values[f'op_{t}']+values[f'request_{t}'];r=catalog.requests[k]
            target=catalog.apply(expected,k)
            output=original[TOOLS[OPS.index(r.operation)]].invoke(db,**r.kwargs);count+=1
            if output.startswith('Error:'):
                if target is not None:raise AssertionError('Interpreter accepted original-tool error: '+output)
                return False,count
            if target is None:raise AssertionError('Interpreter rejected original-tool success')
            actual=catalog.project(db)
            if actual!=target:raise AssertionError(f'Original transition mismatch step {t}: {actual} != {target}')
            expected=target
        return True,count
    name=f'workflow_{catalog.uid}_{catalog.metadata["upgrade_style"]}_{mode}_{restriction or "all"}_h{horizon}_{layout}_{prior_kind}_f{int(policy_flags)}'
    p=Problem(name,prior,model,xs,witness,lambda x:simulate(x) is not None,lambda x,y:simulate(x)==y,
        dict(family='retail_trajectory',mode=mode,horizon=horizon,operation=restriction,
             layout=layout,prior_kind=prior_kind,policy_flags=policy_flags,catalog=catalog.metadata,
             transition_encoding='Sparse indicator-weighted integer increments and guarded original-tool preconditions; no whole-trajectory table or inferred global nogoods',
             prior_source='Declared experimental request prior, not customer traffic',
             semantics='mixed/single reset the same original snapshot per action; coupled carries exact projected state across actions'))
    p.simulate=simulate;p.partial_feasible=partial;p.replay=replay;p.catalog=catalog
    return p
