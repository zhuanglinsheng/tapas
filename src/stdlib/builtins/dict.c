#include "tapas/textension.h"

#include "../arguments.h"

#include "tapas/runtime/tdict.h"
#include "tapas/runtime/tlist.h"
#include "tapas/runtime/ttype.h"


static void append_key(const tobj *key, const tobj *value, void *context)
{
	(void)value;
	tobj_vec_push(&((tlist *)context)->items, key);
}

static void append_value(const tobj *key, const tobj *value, void *context)
{
	(void)key;
	tobj_vec_push(&((tlist *)context)->items, value);
}

static tlist *dictionary_items(tdict *dictionary, thashtbl_each_fn append)
{
	tlist *items = tlist_new();
	thashtbl_each(dictionary->items, append, items);
	return items;
}

static void builtin_keys(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("keys", len, 1);
	if (params[0].type != tcompo)
		twarn(ErrRuntime_ParamsType, "keys", "");
	if (tobj_compo_type(&params[0]) == compo_tdict) {
		tobj_set_compo(result,
			(tcompo_v *)dictionary_items(
				(tdict *)params[0].val.v_tcompo, append_key));
		return;
	}
	if (tobj_compo_type(&params[0]) == compo_ttypeval) {
		tlist *keys = tlist_new();
		long position = 0;
		tobj key;
		tobj_set_nil(&key);
		while (tcompo_next(params[0].val.v_tcompo, &position, &key)) {
			tobj_vec_push(&keys->items, &key);
			tobj_ddc_ref_clear(&key);
		}
		tobj_set_compo(result, (tcompo_v *)keys);
		return;
	}
	twarn(ErrRuntime_ParamsType, "keys", "Dictionary or Type required");
}

static void builtin_values(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("values", len, 1);
	if (params[0].type != tcompo ||
	    tobj_compo_type(&params[0]) != compo_tdict)
		twarn(ErrRuntime_ParamsType, "values", "");
	tobj_set_compo(result,
		(tcompo_v *)dictionary_items(
			(tdict *)params[0].val.v_tcompo, append_value));
}

static const textension_symbol symbols[] = {
	{
		.name = "keys",
		.type = "Function[AnyType] -> List",
		.detail = "keys(value: Dictionary | Type) -> List",
		.kind = textension_function,
		.function = builtin_keys,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "values",
		.type = "Function[Dictionary] -> List",
		.detail = "values(dictionary: Dictionary) -> List",
		.kind = textension_function,
		.function = builtin_values,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	}
};

const textension_module tstdlib_dict_functions = {
	.scope = textension_root,
	.symbols = symbols,
	.symbol_count = sizeof(symbols) / sizeof(symbols[0])
};
