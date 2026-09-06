#include "tapas/textension.h"

#include "../arguments.h"

#include "tapas/objects/tlist.h"


static void builtin_list(tobj *params, uint_regs len, tobj *result)
{
	tlist *list = tlist_new();
	for (uint_regs i = 0; i < len; i++)
		tobj_vec_push(&list->items, &params[i]);
	tobj_set_compo(result, (tcompo_v *)list);
}

static tlist *require_list(tobj *value, const char *function)
{
	if (value->type != tcompo || tobj_compo_type(value) != compo_tlist)
		twarn(ErrRuntime_ParamsType, function, "list required");
	return (tlist *)value->val.v_tcompo;
}

static void builtin_push_front(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("push_front", len, 2);
	tobj_vec_insert(&require_list(&params[0], "push_front")->items,
			0, &params[1]);
	tobj_set_nil(result);
}

static void builtin_push_back(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("push_back", len, 2);
	tobj_vec_push(&require_list(&params[0], "push_back")->items, &params[1]);
	tobj_set_nil(result);
}

static void pop_at(tobj *params, uint_regs len, tobj *result,
		   const char *function, int back)
{
	tstdlib_require_arguments(function, len, 1);
	tlist *list = require_list(&params[0], function);
	uint_objs size = tlist_size(list);
	if (!size)
		twarn(ErrRuntime_IdxOutRange, function, "list is empty");
	tobj_vec_take(&list->items, back ? size - 1 : 0, result);
}

static void builtin_pop_front(tobj *params, uint_regs len, tobj *result)
{
	pop_at(params, len, result, "pop_front", 0);
}

static void builtin_pop_back(tobj *params, uint_regs len, tobj *result)
{
	pop_at(params, len, result, "pop_back", 1);
}

static void builtin_insert(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("insert", len, 3);
	tlist *list = require_list(&params[0], "insert");
	if (params[2].type != tint)
		twarn(ErrRuntime_ParamsType, "insert", "integer index required");
	long index = params[2].val.v_tint;
	if (index < 0)
		index += (long)tlist_size(list);
	if (index < 0 || (uint_objs)index > tlist_size(list))
		twarn(ErrRuntime_IdxOutRange, "insert", "");
	tobj_vec_insert(&list->items, (uint_objs)index, &params[1]);
	tobj_set_nil(result);
}

static void builtin_concat(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("concat", len, 2);
	tlist *left = require_list(&params[0], "concat");
	tlist *right = require_list(&params[1], "concat");
	tlist *combined = tlist_new();
	for (uint_objs i = 0; i < tlist_size(left); i++)
		tobj_vec_push(&combined->items, tlist_at(left, i));
	for (uint_objs i = 0; i < tlist_size(right); i++)
		tobj_vec_push(&combined->items, tlist_at(right, i));
	tobj_set_compo(result, (tcompo_v *)combined);
}

static const textension_symbol symbols[] = {
	{
		.name = "list",
		.type = "Function[...] -> List",
		.detail = "list(...values: AnyType) -> List",
		.kind = textension_function,
		.function = builtin_list,
		.minimum_arguments = 0,
		.maximum_arguments = UNDEF_NPARAMS
	},
	{
		.name = "push_front",
		.type = "Function[List, AnyType] -> Nil",
		.detail = "push_front(list: List, value: AnyType) -> Nil",
		.kind = textension_function,
		.function = builtin_push_front,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "push_back",
		.type = "Function[List, AnyType] -> Nil",
		.detail = "push_back(list: List, value: AnyType) -> Nil",
		.kind = textension_function,
		.function = builtin_push_back,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "pop_front",
		.type = "Function[List] -> AnyType",
		.detail = "pop_front(list: List) -> AnyType",
		.kind = textension_function,
		.function = builtin_pop_front,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "pop_back",
		.type = "Function[List] -> AnyType",
		.detail = "pop_back(list: List) -> AnyType",
		.kind = textension_function,
		.function = builtin_pop_back,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "insert",
		.type = "Function[List, AnyType, Int] -> Nil",
		.detail = "insert(list: List, value: AnyType, index: Int) -> Nil",
		.kind = textension_function,
		.function = builtin_insert,
		.minimum_arguments = 3,
		.maximum_arguments = 3
	},
	{
		.name = "concat",
		.type = "Function[List, List] -> List",
		.detail = "concat(left: List, right: List) -> List",
		.kind = textension_function,
		.function = builtin_concat,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	}
};

const textension_module tstdlib_list_functions = {
	.scope = textension_root,
	.symbols = symbols,
	.symbol_count = sizeof(symbols) / sizeof(symbols[0])
};
