#include "tapas/textension.h"

#include "../arguments.h"

#include "tapas/objects/tlist.h"
#include "runtime/value_order.h"
#include "tapas/vm_service.h"

#include <math.h>


typedef struct {
	tlist *list;
	tobj less;
	tcompo_env *environment;
	const char *where;
} heap_order;

static tlist *require_list(const tobj *value, const char *where)
{
	if (tobj_compo_type(value) != compo_tlist)
		twarn(ErrRuntime_ParamsType, where, "List required");
	return (tlist *)value->val.v_tcompo;
}

static void require_orderable(const tobj *value, const char *where)
{
	if (!tobj_is_orderable(value))
		twarn(ErrRuntime_ParamsType, where,
		      "non-NaN number, String or recursively orderable List required");
}

static heap_order make_order(tobj *params, uint_regs len,
			     uint_regs required, const char *where,
			     tcompo_env *environment)
{
	if (len != required && len != required + 1)
		twarn(ErrRuntime_ParamsCtr, where,
		      "optional final comparator expected");
	heap_order order = {
		.list = require_list(&params[0], where),
		.environment = environment,
		.where = where
	};
	tobj_set_nil(&order.less);
	if (len > required) {
		/* Native argument slots can move when a callback grows the VM stack.
		 * Keep a borrowed value, rather than a pointer to one of those slots. */
		order.less = params[required];
		tcompo_type type = tobj_compo_type(&order.less);
		if (type != compo_tfunc && type != compo_cppfunc &&
		    type != compo_sessfunc)
			twarn(ErrRuntime_ParamsType, where,
			      "comparator must be a Function");
	}
	return order;
}

static int less_at(const heap_order *order, uint_count left, uint_count right)
{
	tobj_vec *items = &order->list->items;
	const tobj *a = &items->data[left];
	const tobj *b = &items->data[right];
	if (order->less.type == tnil) {
		/* Keep homogeneous numeric heaps on their direct scalar paths. */
		if (a->type == tint && b->type == tint)
			return a->val.v_tint < b->val.v_tint;
		if (a->type == tfloat && b->type == tfloat) {
			if (isnan(a->val.v_tfloat) || isnan(b->val.v_tfloat))
				twarn(ErrRuntime_ParamsType, order->where, "NaN is not orderable");
			return a->val.v_tfloat < b->val.v_tfloat;
		}
		int comparison;
		int valid = a->type != tcompo && b->type != tcompo ?
			tobj_compare_numbers(a, b, &comparison) :
			tobj_compare(a, b, &comparison);
		if (!valid)
			twarn(ErrRuntime_ParamsType, order->where,
			      "incomparable elements, NaN or excessive nesting");
		return comparison < 0;
	}

	/* The list owns these borrowed values; VM function frames retain their
	 * parameters before running user code. Keep no buffer pointers or extra
	 * owned references across an invocation, which may raise an error. */
	tobj arguments[2] = {*a, *b};
	tobj answer;
	tobj_set_nil(&answer);
	uint_count size = items->len;
	tobj *data = items->data;
	tvm_services_invoke(order->environment, &order->less, arguments, 2, &answer);
	int valid = answer.type == tbool;
	int less = valid && answer.val.v_tbool;
	tobj_ddc_ref_clear(&answer);
	if (items->len != size || items->data != data)
		twarn(ErrRuntime_Other, order->where,
		      "comparator must not modify the heap");
	if (!valid)
		twarn(ErrRuntime_ParamsType, order->where,
		      "comparator must return Bool");
	return less;
}

/* Swaps transfer owned references; the vector's composite count is unchanged.
 * Every comparison sees a complete list, even if a callback raises an error. */
static void swap_items(tlist *list, uint_count a, uint_count b)
{
	tobj temporary = list->items.data[a];
	list->items.data[a] = list->items.data[b];
	list->items.data[b] = temporary;
}

static void sift_down(const heap_order *order, uint_count parent, uint_count size)
{
	while (parent < size / 2) {
		uint_count child = parent * 2 + 1;
		if (child + 1 < size && less_at(order, child + 1, child))
			child++;
		if (!less_at(order, child, parent))
			break;
		swap_items(order->list, parent, child);
		parent = child;
	}
}

/* Pop replaces the root with a leaf value, which usually travels downward.
 * Compare siblings on the descent, then settle the replacement upward.
 * Swaps keep every payload rooted in the List across fallible callbacks. */
static void sift_pop(const heap_order *order, uint_count parent, uint_count size)
{
	if (parent >= size / 2)
		return;
	uint_count child = parent * 2 + 1;
	if (child + 1 < size && less_at(order, child + 1, child))
		child++;
	/* Equal priorities and replacements already in order need no movement.
	 * Otherwise the first comparison proves the replacement cannot rise
	 * above this child again, so exclude the settled root on the ascent. */
	if (!less_at(order, child, parent))
		return;
	swap_items(order->list, parent, child);
	parent = child;
	uint_count start = parent;
	while (parent < size / 2) {
		child = parent * 2 + 1;
		if (child + 1 < size && less_at(order, child + 1, child))
			child++;
		swap_items(order->list, parent, child);
		parent = child;
	}
	while (parent > start) {
		uint_count previous = (parent - 1) / 2;
		if (!less_at(order, parent, previous))
			break;
		swap_items(order->list, parent, previous);
		parent = previous;
	}
}

static void heap_heapify(tobj *params, uint_regs len, tobj *result,
			 tcompo_env *environment)
{
	heap_order order = make_order(params, len, 1, "heap::heapify", environment);
	uint_count size = tlist_size(order.list);
	if (order.less.type == tnil)
		for (uint_count i = 0; i < size; i++)
			require_orderable(tlist_at(order.list, i), order.where);
	for (uint_count i = size / 2; i > 0; i--)
		sift_down(&order, i - 1, size);
	tobj_set_nil(result);
}

static void heap_push(tobj *params, uint_regs len, tobj *result,
		      tcompo_env *environment)
{
	heap_order order = make_order(params, len, 2, "heap::push", environment);
	if (order.less.type == tnil)
		require_orderable(&params[1], order.where);
	uint_count child = tlist_size(order.list);
	tobj_vec_push(&order.list->items, &params[1]);
	while (child > 0) {
		uint_count parent = (child - 1) / 2;
		if (!less_at(&order, child, parent))
			break;
		swap_items(order.list, child, parent);
		child = parent;
	}
	tobj_set_nil(result);
}

static void heap_pop(tobj *params, uint_regs len, tobj *result,
		     tcompo_env *environment)
{
	heap_order order = make_order(params, len, 1, "heap::pop", environment);
	uint_count size = tlist_size(order.list);
	if (!size)
		twarn(ErrRuntime_IdxOutRange, order.where, "heap is empty");
	/* Keep the removed value in the list during comparisons, so an error
	 * cannot strand an owned payload in a C local. Exclude it while sifting. */
	swap_items(order.list, 0, size - 1);
	sift_pop(&order, 0, size - 1);
	tobj_vec_take(&order.list->items, size - 1, result);
}

static void heap_peek(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("heap::peek", len, 1);
	tlist *list = require_list(&params[0], "heap::peek");
	if (!tlist_size(list))
		twarn(ErrRuntime_IdxOutRange, "heap::peek", "heap is empty");
	tobj_copy(result, tlist_at(list, 0));
}

static const textension_symbol symbols[] = {
	{
		.name = "heapify",
		.type = "Function[...] -> Nil",
		.detail = "heap::heapify(items: List[, less: Function]) -> Nil",
		.kind = textension_function,
		.session_function = heap_heapify,
		.minimum_arguments = 1,
		.maximum_arguments = 2
	},
	{
		.name = "push",
		.type = "Function[...] -> Nil",
		.detail = "heap::push(items: List, value: AnyType[, less: Function]) -> Nil",
		.kind = textension_function,
		.session_function = heap_push,
		.minimum_arguments = 2,
		.maximum_arguments = 3
	},
	{
		.name = "pop",
		.type = "Function[...] -> AnyType",
		.detail = "heap::pop(items: List[, less: Function]) -> AnyType",
		.kind = textension_function,
		.session_function = heap_pop,
		.minimum_arguments = 1,
		.maximum_arguments = 2
	},
	{
		.name = "peek",
		.type = "Function[List] -> AnyType",
		.detail = "heap::peek(items: List) -> AnyType",
		.kind = textension_function,
		.function = heap_peek,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	}
};

const textension_module tstdlib_heap_module = {
	.scope = textension_package,
	.name = "heap",
	.detail = "In-place binary heaps with optional comparators",
	.symbols = symbols,
	.symbol_count = sizeof(symbols) / sizeof(symbols[0])
};
