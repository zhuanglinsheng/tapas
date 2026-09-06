#include "tapas/textension.h"

#include "../arguments.h"

#include "tapas/objects/tarray.h"
#include "tapas/objects/tlist.h"

#include <stdint.h>


static size_t array_size(size_t rows, size_t cols)
{
	if (rows && cols > SIZE_MAX / rows)
		twarn(ErrRuntime_Other, "array", "array dimensions overflow");
	return rows * cols;
}

static tdarr *real_array_from_list(size_t rows, size_t cols, const tlist *values)
{
	size_t count = array_size(rows, cols);
	if (tlist_size(values) != count)
		twarn(ErrRuntime_LenInconsis, "array",
		      "shape and list length differ");
	for (size_t i = 0; i < count; i++) {
		const tobj *value = tlist_at(values, (uint_objs)i);
		if (value->type != tint && value->type != tfloat)
			twarn(ErrRuntime_ParamsType, "array", "numeric list required");
	}
	tdarr *array = tdarr_new_uninitialized(rows, cols);
	for (size_t i = 0; i < count; i++) {
		const tobj *value = tlist_at(values, (uint_objs)i);
		if (value->type == tint)
			array->data[i] = (double)value->val.v_tint;
		else
			array->data[i] = value->val.v_tfloat;
	}
	return array;
}

static tbarr *bool_array_from_list(size_t rows, size_t cols, const tlist *values)
{
	size_t count = array_size(rows, cols);
	if (tlist_size(values) != count)
		twarn(ErrRuntime_LenInconsis, "array",
		      "shape and list length differ");
	for (size_t i = 0; i < count; i++) {
		const tobj *value = tlist_at(values, (uint_objs)i);
		if (value->type != tbool)
			twarn(ErrRuntime_ParamsType, "array", "boolean list required");
	}
	tbarr *array = tbarr_new_uninitialized(rows, cols);
	for (size_t i = 0; i < count; i++) {
		const tobj *value = tlist_at(values, (uint_objs)i);
		array->data[i] = value->val.v_tbool != 0;
	}
	return array;
}

static size_t array_dimension(const tobj *value)
{
	if (value->type != tint || value->val.v_tint < 0)
		twarn(ErrRuntime_ParamsType, "array",
		      "array dimensions must be non-negative integers");
	return (size_t)value->val.v_tint;
}

static void builtin_array(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("array", len, 3);
	size_t rows = array_dimension(&params[0]);
	size_t cols = array_dimension(&params[1]);
	const tobj *value = &params[2];
	if (value->type == tbool) {
		tobj_set_compo(result,
			(tcompo_v *)tbarr_new(rows, cols, value->val.v_tbool));
		return;
	}
	if (value->type == tint || value->type == tfloat) {
		double scalar = value->type == tint ?
			(double)value->val.v_tint : value->val.v_tfloat;
		tobj_set_compo(result, (tcompo_v *)tdarr_new(rows, cols, scalar));
		return;
	}
	if (value->type == tcompo && tobj_compo_type(value) == compo_tlist) {
		tlist *values = (tlist *)value->val.v_tcompo;
		const tobj *first = tlist_size(values) ? tlist_at(values, 0) : nullptr;
		tobj_set_compo(result, first && first->type == tbool ?
			(tcompo_v *)bool_array_from_list(rows, cols, values) :
			(tcompo_v *)real_array_from_list(rows, cols, values));
		return;
	}
	twarn(ErrRuntime_ParamsType, "array", "value must be scalar or list");
}

static const textension_symbol symbols[] = {
	{
		.name = "array",
		.type = "Function[Int, Int, AnyType] -> Unknown",
		.detail = "array(rows: Int, cols: Int, value: AnyType) -> RealArray | BoolArray",
		.kind = textension_function,
		.function = builtin_array,
		.minimum_arguments = 3,
		.maximum_arguments = 3
	}
};

const textension_module tstdlib_array_functions = {
	.scope = textension_root,
	.symbols = symbols,
	.symbol_count = sizeof(symbols) / sizeof(symbols[0])
};
