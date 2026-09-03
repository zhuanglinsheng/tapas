#include "tapas/textension.h"

#include "../arguments.h"

#include "tapas/runtime/tarray.h"
#include "tapas/tblas.h"


static tcompo_v *array_value(const tobj *value, const char *where)
{
	if (value->type != tcompo ||
	    (tobj_compo_type(value) != compo_tdarr &&
	     tobj_compo_type(value) != compo_tbarr))
		twarn(ErrRuntime_ParamsType, where, "array required");
	return value->val.v_tcompo;
}

static tcompo_v *array_param(tobj *params, uint_regs len, const char *where)
{
	tstdlib_require_arguments(where, len, 1);
	return array_value(&params[0], where);
}

static tdarr *real_array_value(const tobj *value, const char *where)
{
	if (value->type != tcompo || tobj_compo_type(value) != compo_tdarr)
		twarn(ErrRuntime_ParamsType, where, "real array required");
	return (tdarr *)value->val.v_tcompo;
}

static size_t element_count(const tdarr *array)
{
	return array->rows * array->cols;
}

static double number_value(const tobj *value, const char *where)
{
	if (value->type == tint)
		return (double)value->val.v_tint;
	if (value->type == tfloat)
		return value->val.v_tfloat;
	twarn(ErrRuntime_ParamsType, where, "number required");
	return 0.0;
}

static void require_same_shape(const tdarr *left, const tdarr *right,
			       const char *where)
{
	if (left->rows != right->rows || left->cols != right->cols)
		twarn(ErrRuntime_LenInconsis, where, "array shapes differ");
}

static void dense_rows(tobj *p, uint_regs n, tobj *r)
{
	tobj_set_int(r, (long)tarr_rows(array_param(p, n, "dense::rows")));
}

static void dense_cols(tobj *p, uint_regs n, tobj *r)
{
	tobj_set_int(r, (long)tarr_cols(array_param(p, n, "dense::cols")));
}

static void dense_transpose(tobj *p, uint_regs n, tobj *r)
{
	tcompo_v *arr = array_param(p, n, "dense::transpose");
	if (arr->vtable->get_compo_type_code() == compo_tdarr) {
		tdarr *source = (tdarr *)arr;
		tdarr *result = tdarr_new_uninitialized(source->cols, source->rows);
		for (size_t row = 0; row < source->rows; row++)
			for (size_t col = 0; col < source->cols; col++)
				result->data[col * result->cols + row] =
					source->data[row * source->cols + col];
		tobj_set_compo(r, (tcompo_v *)result);
	} else {
		tbarr *source = (tbarr *)arr;
		tbarr *result = tbarr_new_uninitialized(source->cols, source->rows);
		for (size_t row = 0; row < source->rows; row++)
			for (size_t col = 0; col < source->cols; col++)
				result->data[col * result->cols + row] =
					source->data[row * source->cols + col];
		tobj_set_compo(r, (tcompo_v *)result);
	}
}

static void dense_identity(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("dense::identity", len, 1);
	if (params[0].type != tint || params[0].val.v_tint < 0)
		twarn(ErrRuntime_ParamsType, "dense::identity",
		      "size must be a non-negative integer");
	size_t size = (size_t)params[0].val.v_tint;
	tdarr *identity = tdarr_new(size, size, 0.0);
	for (size_t i = 0; i < size; i++)
		identity->data[i * size + i] = 1.0;
	tobj_set_compo(result, (tcompo_v *)identity);
}

static void dense_trace(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("dense::trace", len, 1);
	tdarr *array = real_array_value(&params[0], "dense::trace");
	size_t diagonal = array->rows < array->cols ? array->rows : array->cols;
	double trace = 0.0;
	for (size_t i = 0; i < diagonal; i++)
		trace += array->data[i * array->cols + i];
	tobj_set_float(result, trace);
}

static void dense_inner(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("dense::inner", len, 2);
	tdarr *left = real_array_value(&params[0], "dense::inner");
	tdarr *right = real_array_value(&params[1], "dense::inner");
	require_same_shape(left, right, "dense::inner");
	tobj_set_float(result, tblas_dot(
		element_count(left), left->data, right->data));
}

static size_t vector_length(const tdarr *array, const char *where)
{
	if (array->rows != 1 && array->cols != 1)
		twarn(ErrRuntime_LenInconsis, where,
		      "row or column vector required");
	return element_count(array);
}

static void dense_outer(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("dense::outer", len, 2);
	tdarr *left = real_array_value(&params[0], "dense::outer");
	tdarr *right = real_array_value(&params[1], "dense::outer");
	size_t rows = vector_length(left, "dense::outer");
	size_t cols = vector_length(right, "dense::outer");
	tdarr *outer = tdarr_new_uninitialized(rows, cols);
	tblas_gemm(rows, 1, cols, 1.0, left->data, right->data,
		    0.0, outer->data);
	tobj_set_compo(result, (tcompo_v *)outer);
}

static void dense_norm(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("dense::norm", len, 1);
	tdarr *array = real_array_value(&params[0], "dense::norm");
	tobj_set_float(result, tblas_norm(element_count(array), array->data));
}

static void dense_normalize(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("dense::normalize", len, 1);
	tdarr *array = real_array_value(&params[0], "dense::normalize");
	size_t count = element_count(array);
	double norm = tblas_norm(count, array->data);
	if (norm == 0.0)
		twarn(ErrRuntime_Other, "dense::normalize",
		      "zero array cannot be normalized");
	tdarr *normalized = tdarr_new_uninitialized(array->rows, array->cols);
	for (size_t i = 0; i < count; i++)
		normalized->data[i] = array->data[i] / norm;
	tobj_set_compo(result, (tcompo_v *)normalized);
}

static void dense_copy_into(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("dense::copy_into", len, 2);
	tdarr *source = real_array_value(&params[0], "dense::copy_into");
	tdarr *target = real_array_value(&params[1], "dense::copy_into");
	require_same_shape(source, target, "dense::copy_into");
	if (source != target)
		tblas_copy(element_count(source), source->data, target->data);
	tobj_set_nil(result);
}

static void dense_scale_inplace(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("dense::scale_inplace", len, 2);
	tdarr *value = real_array_value(&params[0], "dense::scale_inplace");
	double factor = number_value(&params[1], "dense::scale_inplace");
	tblas_scal(element_count(value), factor, value->data);
	tobj_set_nil(result);
}

static void dense_add_scaled_inplace(tobj *params, uint_regs len,
				     tobj *result)
{
	tstdlib_require_arguments("dense::add_scaled_inplace", len, 3);
	tdarr *target = real_array_value(&params[0],
					"dense::add_scaled_inplace");
	tdarr *source = real_array_value(&params[1],
					"dense::add_scaled_inplace");
	double factor = number_value(&params[2], "dense::add_scaled_inplace");
	require_same_shape(target, source, "dense::add_scaled_inplace");
	tblas_axpy(element_count(target), factor, source->data, target->data);
	tobj_set_nil(result);
}

static void dense_gemm(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("dense::gemm", len, 5);
	double alpha = number_value(&params[0], "dense::gemm");
	tdarr *left = real_array_value(&params[1], "dense::gemm");
	tdarr *right = real_array_value(&params[2], "dense::gemm");
	double beta = number_value(&params[3], "dense::gemm");
	tdarr *output = real_array_value(&params[4], "dense::gemm");
	if (left->cols != right->rows || output->rows != left->rows ||
	    output->cols != right->cols)
		twarn(ErrRuntime_LenInconsis, "dense::gemm",
		      "incompatible array shapes");
	if (output == left || output == right)
		twarn(ErrRuntime_Other, "dense::gemm",
		      "output must not alias an input");
	tblas_gemm(left->rows, left->cols, right->cols,
		    alpha, left->data, right->data, beta, output->data);
	tobj_set_nil(result);
}

static const textension_symbol package_symbols[] = {
	{
		.name = "rows",
		.type = "Function[RealArray | BoolArray] -> Int",
		.detail = "dense::rows(value: RealArray | BoolArray) -> Int",
		.kind = textension_function,
		.function = dense_rows,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "cols",
		.type = "Function[RealArray | BoolArray] -> Int",
		.detail = "dense::cols(value: RealArray | BoolArray) -> Int",
		.kind = textension_function,
		.function = dense_cols,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "transpose",
		.type = "Function[RealArray | BoolArray] -> Unknown",
		.detail = "dense::transpose(value: RealArray | BoolArray) -> RealArray | BoolArray",
		.kind = textension_function,
		.function = dense_transpose,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "identity",
		.type = "Function[Int] -> RealArray",
		.detail = "dense::identity(size: Int) -> RealArray",
		.kind = textension_function,
		.function = dense_identity,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "trace",
		.type = "Function[RealArray] -> Float",
		.detail = "dense::trace(value: RealArray) -> Float",
		.kind = textension_function,
		.function = dense_trace,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "inner",
		.type = "Function[RealArray, RealArray] -> Float",
		.detail = "dense::inner(left: RealArray, right: RealArray) -> Float",
		.kind = textension_function,
		.function = dense_inner,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "outer",
		.type = "Function[RealArray, RealArray] -> RealArray",
		.detail = "dense::outer(left: RealArray, right: RealArray) -> RealArray",
		.kind = textension_function,
		.function = dense_outer,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "norm",
		.type = "Function[RealArray] -> Float",
		.detail = "dense::norm(value: RealArray) -> Float",
		.kind = textension_function,
		.function = dense_norm,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "normalize",
		.type = "Function[RealArray] -> RealArray",
		.detail = "dense::normalize(value: RealArray) -> RealArray",
		.kind = textension_function,
		.function = dense_normalize,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "copy_into",
		.type = "Function[RealArray, RealArray] -> Nil",
		.detail = "dense::copy_into(source: RealArray, target: RealArray) -> Nil",
		.kind = textension_function,
		.function = dense_copy_into,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "scale_inplace",
		.type = "Function[RealArray, Int | Float] -> Nil",
		.detail = "dense::scale_inplace(value: RealArray, factor: Int | Float) -> Nil",
		.kind = textension_function,
		.function = dense_scale_inplace,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "add_scaled_inplace",
		.type = "Function[RealArray, RealArray, Int | Float] -> Nil",
		.detail = "dense::add_scaled_inplace(target: RealArray, source: RealArray, factor: Int | Float) -> Nil",
		.kind = textension_function,
		.function = dense_add_scaled_inplace,
		.minimum_arguments = 3,
		.maximum_arguments = 3
	},
	{
		.name = "gemm",
		.type = "Function[Int | Float, RealArray, RealArray, Int | Float, RealArray] -> Nil",
		.detail = "dense::gemm(alpha: Int | Float, left: RealArray, right: RealArray, beta: Int | Float, output: RealArray) -> Nil",
		.kind = textension_function,
		.function = dense_gemm,
		.minimum_arguments = 5,
		.maximum_arguments = 5
	}
};

const textension_module tstdlib_dense_module = {
	.scope = textension_package,
	.name = "dense",
	.detail = "Dense array and linear algebra operations",
	.symbols = package_symbols,
	.symbol_count = sizeof(package_symbols) / sizeof(package_symbols[0])
};
