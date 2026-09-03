#include "tapas/textension.h"

#include "../arguments.h"

#include "tapas/runtime/tlist.h"

#include <stdlib.h>


static int compare_values(const void *left, const void *right)
{
	const tobj *a = (const tobj *)left;
	const tobj *b = (const tobj *)right;
	if (a->type == tint && b->type == tint)
		return (a->val.v_tint > b->val.v_tint) -
		       (a->val.v_tint < b->val.v_tint);
	if ((a->type == tint || a->type == tfloat) &&
	    (b->type == tint || b->type == tfloat)) {
		double av = a->type == tint ? (double)a->val.v_tint : a->val.v_tfloat;
		double bv = b->type == tint ? (double)b->val.v_tint : b->val.v_tfloat;
		return (av > bv) - (av < bv);
	}
	return (int)a->type - (int)b->type;
}

static void builtin_sort(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("sort", len, 1);
	if (params[0].type != tcompo ||
	    tobj_compo_type(&params[0]) != compo_tlist)
		twarn(ErrRuntime_ParamsType, "sort", "");
	tlist *list = (tlist *)params[0].val.v_tcompo;
	qsort(tobj_vec_data(&list->items), tobj_vec_len(&list->items),
	      sizeof(tobj), compare_values);
	tobj_set_nil(result);
}

static const textension_symbol symbols[] = {
	{
		.name = "sort",
		.type = "Function[List] -> Nil",
		.detail = "sort(values: List) -> Nil",
		.kind = textension_function,
		.function = builtin_sort,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	}
};

const textension_module tstdlib_sort_functions = {
	.scope = textension_root,
	.symbols = symbols,
	.symbol_count = sizeof(symbols) / sizeof(symbols[0])
};
