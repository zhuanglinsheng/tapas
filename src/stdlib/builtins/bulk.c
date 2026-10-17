#include "tapas/textension.h"

#include "../arguments.h"
#include "../iterables.h"
#include "../ranges.h"

#include "tapas/objects/titer.h"
#include "tapas/objects/tlist.h"
#include "tapas/objects/tstr.h"
#include "tapas/dsa/tstring.h"

#include <limits.h>

/* Iterator indices are immutable. Keep List writes in native storage while
 * preserving the same bounds and reference ownership as individual writes. */
static void fill_list_range(tlist *list, const tobj *value, const titer *range)
{
	long index = range->start;
	long step = range->step;
	long length = (long)tlist_size(list);
	int scalar = value->type != tcompo && list->items.compo_count == 0;
	tstdlib_range checked;
	if (scalar && tstdlib_range_in_bounds(range, length, &checked)) {
		long position = checked.start;
		for (size_t remaining = checked.count; remaining; remaining--) {
			list->items.data[position] = *value;
			if (remaining > 1)
				position += checked.step;
		}
		return;
	}
	if (!step)
		return;
	while (step > 0 ? index < range->end : index > range->end) {
		long position = index < 0 ? index + length : index;
		if (position < 0 || position >= length)
			twarn(ErrRuntime_IdxOutRange, "fill", "List index out of range");
		if (scalar)
			list->items.data[position] = *value;
		else
			tobj_vec_set(&list->items, (uint_count)position, value);
		/* The mathematical next index can lie outside Int. In that case
		 * it also lies beyond the representable exclusive endpoint. */
		if ((step > 0 && index > LONG_MAX - step) ||
		    (step < 0 && index < LONG_MIN - step))
			break;
		index += step;
	}
}

static void builtin_fill(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("fill", len, 3);
	tcompo_v *target = params[0].type == tcompo ? params[0].val.v_tcompo : nullptr;
	if (!target || !target->vtable->capabilities ||
	    !target->vtable->capabilities->index_settable)
		twarn(ErrRuntime_ParamsType, "fill", "IndexSettable required");
	tcompo_v *indices = tstdlib_require_iterable(&params[2], "fill");
	const tcompo_bulk_operations *bulk = target->vtable->bulk_operations;
	if (bulk && bulk->fill && bulk->fill(target, &params[1], indices)) {
		tobj_set_nil(result);
		return;
	}
	if (target->vtable == &tlist_vtable && indices->vtable == &titer_vtable) {
		fill_list_range((tlist *)target, &params[1], (const titer *)indices);
	} else {
		long position = 0;
		tobj key;
		tobj_set_nil(&key);
		while (tcompo_next(indices, &position, &key)) {
			if (target->vtable == &tlist_vtable) {
				tlist *list = (tlist *)target;
				if (key.type != tint) {
					tobj_ddc_ref_clear(&key);
					twarn(ErrRuntime_ParamsType, "fill", "List index must be Int");
				}
				long index = key.val.v_tint;
				tobj_ddc_ref_clear(&key);
				long length = (long)tlist_size(list);
				if (index < 0)
					index += length;
				if (index < 0 || index >= length)
					twarn(ErrRuntime_IdxOutRange, "fill", "List index out of range");
				tobj_vec_set(&list->items, (uint_count)index, &params[1]);
			} else {
				tcompo_index_set(target, &key, 1, &params[1]);
				tobj_ddc_ref_clear(&key);
			}
		}
	}
	tobj_set_nil(result);
}

/* Scalar identical comparisons can be inlined without invoking overloaded
 * equality or converting numeric Types. Other values keep the generic path. */
static long count_list(const tlist *list, const tobj *value)
{
	long count = 0;
	if (value->type == tbool) {
		for (uint_count i = 0; i < list->items.len; i++) {
			const tobj *item = &list->items.data[i];
			count += item->type == tbool && item->val.v_tbool == value->val.v_tbool;
		}
	} else if (value->type == tint) {
		for (uint_count i = 0; i < list->items.len; i++) {
			const tobj *item = &list->items.data[i];
			count += item->type == tint && item->val.v_tint == value->val.v_tint;
		}
	} else if (value->type == tfloat) {
		for (uint_count i = 0; i < list->items.len; i++) {
			const tobj *item = &list->items.data[i];
			count += item->type == tfloat && item->val.v_tfloat == value->val.v_tfloat;
		}
	} else {
		for (uint_count i = 0; i < list->items.len; i++)
			count += tobj_identical(&list->items.data[i], value);
	}
	return count;
}

static long count_string(const tstr *string, const tobj *value)
{
	/* String iteration yields one-byte Strings, including embedded NULs. */
	if (tobj_compo_type(value) != compo_tstr)
		return 0;
	const tstr *needle = (const tstr *)value->val.v_tcompo;
	if (tstring_len(needle->data) != 1)
		return 0;
	char byte = tstring_cstr(needle->data)[0];
	const char *data = tstring_cstr(string->data);
	long count = 0;
	size_t length = tstring_len(string->data);
	for (size_t i = 0; i < length; i++)
		count += data[i] == byte;
	return count;
}

static void builtin_count(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("count", len, 2);
	tcompo_v *values = tstdlib_require_iterable(&params[0], "count");
	long count = 0;
	const tcompo_bulk_operations *bulk = values->vtable->bulk_operations;
	if (bulk && bulk->count && bulk->count(values, &params[1], &count)) {
		/* The implementation uses the same identical semantics as iteration. */
	} else if (values->vtable == &tlist_vtable) {
		count = count_list((const tlist *)values, &params[1]);
	} else if (values->vtable == &tstr_vtable) {
		count = count_string((const tstr *)values, &params[1]);
	} else {
		long position = 0;
		tobj item;
		tobj_set_nil(&item);
		while (tcompo_next(values, &position, &item)) {
			int match = tobj_identical(&item, &params[1]);
			tobj_ddc_ref_clear(&item);
			if (match && count == LONG_MAX)
				twarn(ErrRuntime_IntOutOfRange, "count", "result exceeds Int");
			count += match;
		}
	}
	tobj_set_int(result, count);
}

static const textension_symbol symbols[] = {
	{
		.name = "fill",
		.type = "Function[IndexSettable, AnyType, Iterable] -> Nil",
		.detail = "fill(target: IndexSettable, value: AnyType, indices: Iterable) -> Nil",
		.kind = textension_function,
		.function = builtin_fill,
		.minimum_arguments = 3,
		.maximum_arguments = 3
	},
	{
		.name = "count",
		.type = "Function[Iterable, AnyType] -> Int",
		.detail = "count(values: Iterable, value: AnyType) -> Int",
		.kind = textension_function,
		.function = builtin_count,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	}
};

const textension_module tstdlib_bulk_functions = {
	.scope = textension_root,
	.symbols = symbols,
	.symbol_count = sizeof(symbols) / sizeof(symbols[0])
};
