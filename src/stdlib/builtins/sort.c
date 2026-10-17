#include "tapas/textension.h"

#include "../arguments.h"

#include "tapas/objects/tlist.h"
#include "runtime/value_order.h"

#include <stdlib.h>

/* Default comparisons run no user code. Defer type errors until every
 * insertion shift has returned its temporarily held value to the List. */
static _Thread_local int comparison_failed;

static int compare_values(const tobj *a, const tobj *b)
{
	int order = 0;
	int valid = a->type != tcompo && b->type != tcompo ?
		tobj_compare_numbers(a, b, &order) : tobj_compare(a, b, &order);
	if (!valid)
		comparison_failed = 1;
	return order;
}

static int compare_numbers(const tobj *a, const tobj *b)
{
	int order = 0;
	/* Validation and the absence of composite values establish this domain. */
	tobj_compare_numbers(a, b, &order);
	return order;
}

static int compare_for_qsort(const void *a, const void *b)
{
	return compare_values(a, b);
}

static inline void sort_swap(tobj *a, tobj *b)
{
	if (a == b)
		return;
	tobj temporary = *a;
	*a = *b;
	*b = temporary;
}

#define SORT_TWO_ENDED
#define SORT_PREFIX sort_integer
#define SORT_COMPARE(a, b) (((a)->val.v_tint > (b)->val.v_tint) - \
			    ((a)->val.v_tint < (b)->val.v_tint))
#include "sort_impl.h"

#define SORT_TWO_ENDED
#define SORT_DOMINANT_EQUAL
#define SORT_PREFIX sort_float
#define SORT_COMPARE(a, b) (((a)->val.v_tfloat > (b)->val.v_tfloat) - \
			    ((a)->val.v_tfloat < (b)->val.v_tfloat))
#include "sort_impl.h"

#define SORT_PREFIX sort_number
#define SORT_COMPARE(a, b) compare_numbers(a, b)
#include "sort_impl.h"

#define SORT_PREFIX sort_generic
#define SORT_COMPARE(a, b) compare_values(a, b)
#define SORT_FALLBACK(data, size) qsort(data, size, sizeof(tobj), compare_for_qsort)
#include "sort_impl.h"

static inline int sort_orderable(const tobj *value)
{
	if (value->type == tint)
		return 1;
	if (value->type == tfloat)
		return !isnan(value->val.v_tfloat);
	return tobj_is_orderable(value);
}

static void builtin_sort(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("sort", len, 1);
	if (params[0].type != tcompo ||
	    tobj_compo_type(&params[0]) != compo_tlist)
		twarn(ErrRuntime_ParamsType, "sort", "");
	tlist *list = (tlist *)params[0].val.v_tcompo;
	ttypes homogeneous = list->items.len ? list->items.data[0].type : tnil;
	for (uint_count i = 0; i < list->items.len; i++) {
		if (list->items.data[i].type != homogeneous)
			homogeneous = tnil;
		if (!sort_orderable(&list->items.data[i]))
			twarn(ErrRuntime_ParamsType, "sort",
			      "non-NaN number, String or recursively orderable List required");
	}
	comparison_failed = 0;
	if (homogeneous == tint)
		sort_integer_values(list->items.data, list->items.len);
	else if (homogeneous == tfloat)
		sort_float_values(list->items.data, list->items.len);
	else if (!list->items.compo_count)
		sort_number_values(list->items.data, list->items.len);
	else
		sort_generic_values(list->items.data, list->items.len);
	if (comparison_failed)
		twarn(ErrRuntime_ParamsType, "sort", "incomparable elements");
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
