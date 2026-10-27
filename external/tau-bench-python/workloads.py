"""CP models and independent Python predicates, without Tapas dependencies."""
from __future__ import annotations
from dataclasses import dataclass
from collections import defaultdict
from decimal import Decimal
from itertools import product
import json
from pathlib import Path
from ortools.sat.python import cp_model
from prior import ProductPrior, JointPrior

from project import PAPER_ROOT, DEFAULT_SOURCE, PIN, verify_source
OPERATIONS = ('cancel', 'address', 'payment', 'modify_items', 'return', 'exchange', 'user_address')
STATUS = {'pending':0, 'processed':1, 'delivered':2, 'cancelled':3,
          'pending (item modified)':4, 'return requested':5, 'exchange requested':6}


def cents(value):
    # Business model uses exact cents; no silent int truncation or binary epsilon.
    scaled = Decimal(str(value)) * 100
    if scaled != scaled.to_integral_value():
        raise ValueError('Retail source amount is not an exact cent value')
    return int(scaled)


@dataclass
class Problem:
    name: str
    prior: object
    model: object
    xvars: list
    witness: dict
    feasible: object
    check_witness: object
    metadata: dict
    decode: object = None


def inputs(prior):
    model = cp_model.CpModel()
    variables = [model.new_int_var_from_domain(cp_model.Domain.from_values(d), name)
                 for name, d in zip(prior.names, prior.domains)]
    return model, variables


def synthetic():
    families = [
        ('equal_width',[.8,.8,.2,.2,.5,.5],[(0,1),(2,3)]),
        ('overlap_low',[.8,.8,.6,.6,.1,.5],[(0,1),(2,3),(0,4)]),
        ('overlap_high',[.8,.8,.6,.6,.9,.5],[(0,1),(2,3),(0,4)]),
        ('cardinality',[.02,.5,.5,.5,.5,.5],[(0,),(1,2)])]
    result=[]
    for name, ps, clauses in families:
        prior = ProductPrior([f'x{i}' for i in range(len(ps))], [(0,1)] * len(ps), [[1-p,p] for p in ps])
        model, xs = inputs(prior)
        for clause in clauses:
            model.add(sum(xs[i] for i in clause) <= len(clause)-1)
        def valid(x, clauses=clauses): return not any(all(x[i] for i in c) for c in clauses)
        result.append(Problem(name, prior, model, xs, {}, valid, lambda x,y: y=={},
                              dict(family='synthetic',clauses=clauses)))
    prior = JointPrior(['x0','x1','x2','x3'],[(0,1)]*4,
                       {x:(9 if x[0]==x[1] else 1)*(3 if x[2] else 1) for x in product((0,1),repeat=4)})
    model,xs=inputs(prior);model.add(xs[0]+xs[1]<=1);model.add(xs[2]+xs[3]<=1)
    result.append(Problem('correlated',prior,model,xs,{},lambda x:not(x[0] and x[1]) and not(x[2] and x[3]),
                          lambda x,y:y=={},dict(family='synthetic',joint=True)))
    prior=ProductPrior(['choice','noise'],[(0,1,2),(0,1)],[[1,3,2],[1,4]])
    model,xs=inputs(prior);model.add(xs[0]!=2);y=model.new_int_var(0,2,'completion');model.add(y<=xs[0])
    result.append(Problem('witness_multiplicity',prior,model,xs,{'completion':y},lambda x:x[0]!=2,
                          lambda x,y:0<=y['completion']<=x[0],
                          dict(family='synthetic',witness_count_by_choice=[1,2,0])))
    return result


def load_retail(source=None):
    source=Path(source or DEFAULT_SOURCE)
    verify_source(source)
    retail=source/'tau_bench/envs/retail/data'
    return {section:json.loads((retail/(section+'.json')).read_text()) for section in ('users','orders','products')}


def retail_problem(data, operation='exchange', order_count=8, slots=1, micro=False):
    if operation not in OPERATIONS or order_count<2 or slots<1:
        raise ValueError('Known operation, >=2 orders, >=1 slots required')
    grouped=defaultdict(list)
    for oid,o in data['orders'].items():
        if len({i['item_id'] for i in o['items']})==len(o['items']):
            grouped[o['status']].append(oid)
    for ids in grouped.values():
        ids.sort(key=lambda oid:(len(data['users'][data['orders'][oid]['user_id']]['payment_methods'])<2,oid))
    ids=[]
    for row in range(max(map(len,grouped.values()))):
        for state in ('pending','delivered','processed','cancelled'):
            if row<len(grouped[state]): ids.append(grouped[state][row])
    ids=ids[:order_count]
    orders=[data['orders'][oid] for oid in ids]
    users=list(dict.fromkeys(o['user_id'] for o in orders))
    has_items=operation in ('exchange','modify_items','return')
    has_new=operation in ('exchange','modify_items')
    has_payment=operation in ('exchange','modify_items','return','payment')
    profiles=[data['users'][o['user_id']] for o in orders]
    methods=[sorted(u['payment_methods']) for u in profiles]
    def variants(order, slot):
        item=order['items'][slot]
        # Local rank encoding, not a claim about observed customer preferences.
        vs=data['products'][item['product_id']]['variants']
        return sorted(vs, key=lambda iid:(not vs[iid]['available'], vs[iid]['options']==item['options'], iid))
    max_items=max(len(o['items']) for o in orders)
    max_variants=max(len(data['products'][i['product_id']]['variants']) for o in orders for i in o['items'])
    names=['user' if operation=='user_address' else 'order','auth','confirmed']
    domains=[tuple(range(len(users) if operation=='user_address' else len(orders))), (0,1) if micro else (0,1,2),(0,1)]
    weights=[[1]*len(domains[0]), [1,4] if micro else [1,7,2],[1,9]]
    def add(name,domain,ws=None):
        names.append(name);domains.append(tuple(domain));weights.append(ws or [1]*len(domain))
    if operation!='user_address': add('status_seen',(0,1),[1,9])
    if has_new: add('all_items_reminded',(0,1),[1,9])
    if operation=='cancel': add('reason',(0,1,2),[4,4,1])
    if operation in ('address','user_address'): add('address',(0,1),[3,1])
    if has_payment: add('payment',range(min(2,max(map(len,methods))) if micro else max(map(len,methods))))
    if has_items:
        for j in range(slots):
            add(f'old{j}',range(1 if micro else max_items))
            if has_new: add(f'new{j}',range(min(2,max_variants) if micro else max_variants))
    prior=ProductPrior(names,domains,weights);model,xs=inputs(prior);v=dict(zip(names,xs));witness={}
    model.add(v['auth']==1);model.add(v['confirmed']==1)
    if operation!='user_address':
        model.add(v['status_seen']==1)
        allowed='delivered' if operation in ('return','exchange') else 'pending'
        model.add_allowed_assignments([v['order']],[(i,) for i,o in enumerate(orders) if o['status']==allowed])
    if has_new: model.add(v['all_items_reminded']==1)
    if operation=='cancel': model.add(v['reason']!=2)
    def fixed_y(name, value):
        y=model.new_int_var(value,value,name);witness[name]=y;return y
    def feature_table(columns, rows, ranges):
        ys=[model.new_int_var(lo,hi,name) for name,(lo,hi) in zip(columns,ranges)]
        model.add_allowed_assignments([v['order'],*ys],rows)
        return ys
    expected_status={'cancel':3,'address':0,'payment':0,'modify_items':4,'return':5,'exchange':6}
    if operation!='user_address': fixed_y('post_status',expected_status[operation])
    else:
        updated=model.new_int_var(0,len(users)-1,'updated_user');model.add(updated==v['user']);witness['updated_user']=updated
    max_money=max(cents(z['price']) for p in data['products'].values() for z in p['variants'].values())
    difference=0; total=0; balance=None; gift=None; original=None
    if has_payment:
        rows=[]
        for oi,o in enumerate(orders):
            old=o['payment_history'][0]['payment_method_id']
            for mi,mid in enumerate(methods[oi]):
                pm=profiles[oi]['payment_methods'][mid]
                rows.append((oi,mi,int(pm['source']=='gift_card'),cents(pm.get('balance',0)),int(mid==old),int('gift_card' in mid)))
        gift=model.new_int_var(0,1,'gift');balance=model.new_int_var(0,100000000,'balance')
        original=model.new_int_var(0,1,'original');gift_id=model.new_int_var(0,1,'gift_id')
        model.add_allowed_assignments([v['order'],v['payment'],gift,balance,original,gift_id],rows)
        if operation=='payment':
            model.add(original==0)
            model.add_allowed_assignments([v['order']],[(i,) for i,o in enumerate(orders)
                if len(o['payment_history'])==1 and o['payment_history'][0]['transaction_type']=='payment'])
            amount=model.new_int_var(0,100000000,'amount');witness['amount']=amount
            model.add_allowed_assignments([v['order'],amount],[(i,cents(o['payment_history'][0]['amount'])) for i,o in enumerate(orders)])
            difference=amount
        if operation=='return': model.add(original+gift_id>=1)
    if has_items:
        old_vars=[];differences=[];amounts=[]
        for j in range(slots):
            old_vars.append(v[f'old{j}'])
            oldprice=model.new_int_var(0,max_money*2,'oldprice'+str(j));amounts.append(oldprice)
            if has_new:
                newprice=model.new_int_var(0,max_money,'newprice'+str(j));rows=[]
                for oi,o in enumerate(orders):
                    for slot,item in enumerate(o['items']):
                        vs=data['products'][item['product_id']]['variants']
                        for rank,iid in enumerate(variants(o,slot)):
                            variant=vs[iid]
                            if variant['available'] and iid!=item['item_id'] and variant['options']!=item['options']:
                                rows.append((oi,slot,rank,cents(item['price']),cents(variant['price'])))
                model.add_allowed_assignments([v['order'],v[f'old{j}'],v[f'new{j}'],oldprice,newprice],rows)
                differences.append(newprice-oldprice)
            else:
                model.add_allowed_assignments([v['order'],v[f'old{j}'],oldprice],
                    [(oi,slot,cents(item['price'])) for oi,o in enumerate(orders) for slot,item in enumerate(o['items'])])
        if len(old_vars)>1: model.add_all_different(old_vars)  # Instance positions, not item IDs.
        total=sum(amounts)
        if has_new:
            diff=model.new_int_var(-max_money*slots*2,max_money*slots,'difference')
            model.add(diff==sum(differences));witness['difference']=diff;difference=diff
        else:
            amount=model.new_int_var(0,max_money*slots*2,'amount');model.add(amount==total);witness['amount']=amount
    if has_payment:
        if operation!='return': model.add(balance>=difference).only_enforce_if(gift)
        after=model.new_int_var(0,100000000,'post_balance');witness['post_balance']=after
        if operation in ('modify_items','payment'):
            model.add(after==balance-difference).only_enforce_if(gift)
            model.add(after==balance).only_enforce_if(gift.Not())
        else: model.add(after==balance)
    if operation=='cancel':
        amount=model.new_int_var(0,100000000,'amount');witness['amount']=amount
        model.add_allowed_assignments([v['order'],amount],[(i,sum(cents(t['amount']) for t in o['payment_history'])) for i,o in enumerate(orders)])

    def facts(x):
        a=dict(zip(names,x));oi=a.get('order',0);o=orders[oi]
        if a['auth']!=1 or a['confirmed']!=1: return None
        if operation=='user_address': return {'updated_user':a['user']}
        if not a['status_seen'] or o['status']!=allowed: return None
        y={'post_status':expected_status[operation]}
        if operation=='cancel':
            if a['reason']==2: return None
            y['amount']=sum(cents(t['amount']) for t in o['payment_history'])
        if has_new and not a['all_items_reminded']: return None
        if has_payment:
            if a['payment']>=len(methods[oi]): return None
            mid=methods[oi][a['payment']];pm=profiles[oi]['payment_methods'][mid]
            old=o['payment_history'][0]['payment_method_id'];bal=cents(pm.get('balance',0));isgift=pm['source']=='gift_card'
            y['post_balance']=bal
            if operation=='payment':
                h=o['payment_history']
                if mid==old or len(h)!=1 or h[0]['transaction_type']!='payment':return None
                y['amount']=cents(h[0]['amount']);delta=y['amount']
            if operation=='return' and mid!=old and 'gift_card' not in mid:return None
        if has_items:
            oldslots=[a[f'old{j}'] for j in range(slots)]
            if len(set(oldslots))!=slots or any(slot>=len(o['items']) for slot in oldslots): return None
            delta=0;amount=0
            for j,slot in enumerate(oldslots):
                item=o['items'][slot];amount+=cents(item['price'])
                if has_new:
                    rank=a[f'new{j}'];vs=data['products'][item['product_id']]['variants'];ids_=variants(o,slot)
                    if rank>=len(ids_):return None
                    iid=ids_[rank];target=vs[iid]
                    if not target['available'] or iid==item['item_id'] or target['options']==item['options']:return None
                    delta+=cents(target['price'])-cents(item['price'])
            y['difference' if has_new else 'amount']=delta if has_new else amount
        if has_payment and operation!='return':
            if isgift and bal<delta:return None
            if isgift and operation in ('modify_items','payment'):y['post_balance']=bal-delta
        return y

    addresses=[dict(address1='1 Test Street',address2='',city='Boston',state='MA',country='USA',zip='02101'),
               dict(address1='2 Test Street',address2='Apt 2',city='Seattle',state='WA',country='USA',zip='98101')]
    def decode(x):
        a=dict(zip(names,x));oi=a.get('order',0);o=orders[oi]
        tool={'cancel':'cancel_pending_order','address':'modify_pending_order_address','payment':'modify_pending_order_payment',
              'modify_items':'modify_pending_order_items','return':'return_delivered_order_items','exchange':'exchange_delivered_order_items',
              'user_address':'modify_user_address'}[operation]
        if operation=='user_address':kwargs=dict(user_id=users[a['user']],**addresses[a['address']])
        else:
            kwargs=dict(order_id=o['order_id'])
            if operation=='cancel':kwargs['reason']=['no longer needed','ordered by mistake','other'][a['reason']]
            if operation=='address':kwargs.update(addresses[a['address']])
            if has_payment:kwargs['payment_method_id']=methods[oi][a['payment']]
            if has_items:kwargs['item_ids']=[o['items'][a[f'old{j}']]['item_id'] for j in range(slots)]
            if has_new:kwargs['new_item_ids']=[variants(o,a[f'old{j}'])[a[f'new{j}']] for j in range(slots)]
        return dict(name=tool,kwargs=kwargs)
    return Problem(f'retail_{operation}_o{len(orders)}_k{slots}',prior,model,xs,witness,
                   lambda x:facts(x) is not None,lambda x,y:facts(x)==y,
                   dict(family='retail',operation=operation,orders=ids,users=users,slots=slots,micro=micro,
                        money='exact integer cents',excluded_duplicate_item_orders=sum(len({i['item_id'] for i in o['items']})!=len(o['items']) for o in data['orders'].values()),
                        prior_source='declared experimental prior; not observed customer traffic',
                        variant_encoding='local rank: available and different options first, then item ID'),decode)
