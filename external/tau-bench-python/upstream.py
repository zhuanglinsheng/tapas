"""Pinned Retail provenance and differential checks against original tool files."""
import copy
import importlib.util
import json
from pathlib import Path
import sys
import types
from project import verify_source


def provenance(source):
    return verify_source(source)


def tools(source):
    verify_source(source)
    for name in ('tau_bench','tau_bench.envs'):
        package=types.ModuleType(name);package.__path__=[];sys.modules[name]=package
    def load(name,path):
        spec=importlib.util.spec_from_file_location(name,path);module=importlib.util.module_from_spec(spec)
        sys.modules[name]=module;spec.loader.exec_module(module);return module
    base=load('tau_bench.envs.tool',Path(source)/'tau_bench/envs/tool.py')
    result={}
    for path in sorted((Path(source)/'tau_bench/envs/retail/tools').glob('*.py')):
        if path.name=='__init__.py':continue
        module=load('tau_retail_'+path.stem,path)
        for cls in vars(module).values():
            if isinstance(cls,type) and cls is not base.Tool and issubclass(cls,base.Tool):
                result[cls.get_info()['function']['name']]=cls
    return result


def check(problem,x,y,data,original_tools):
    """External validation, not timed sampler work; no LLM or Tapas dependency."""
    call=problem.decode(x);a=call['kwargs']
    if 'order_id' in a:
        order=data['orders'][a['order_id']];uid=order['user_id'];oid=order['order_id']
        db=dict(users={uid:copy.deepcopy(data['users'][uid])},orders={oid:copy.deepcopy(order)},
                products={pid:copy.deepcopy(data['products'][pid]) for pid in {i['product_id'] for i in order['items']}})
    else:
        uid=a['user_id'];db=dict(users={uid:copy.deepcopy(data['users'][uid])},orders={},products={})
    result=original_tools[call['name']].invoke(db,**a)
    if result.startswith('Error:'):raise AssertionError(f'Original tool rejected CP sample: {call}: {result}')
    out=json.loads(result)
    if 'post_status' in y:
        from workloads import STATUS
        if STATUS[out['status']]!=y['post_status']:raise AssertionError('Post-status mismatch')
    if 'difference' in y and 'exchange_price_difference' in out:
        if round(out['exchange_price_difference']*100)!=y['difference']:raise AssertionError('Price difference mismatch')
    if 'post_balance' in y:
        pm=db['users'][uid]['payment_methods'][a['payment_method_id']]
        # Original floating operations may produce extra decimal digits before round.
        if round(pm.get('balance',0)*100)!=y['post_balance']:raise AssertionError('Gift balance mismatch')
    if call['name'] in ('modify_user_address','modify_pending_order_address'):
        expected={k:a[k] for k in ('address1','address2','city','state','country','zip')}
        if out['address']!=expected:raise AssertionError('Address mismatch')
    return call
