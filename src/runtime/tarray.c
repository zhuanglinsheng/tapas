#include "tapas/runtime/tarray.h"

#include "tapas/runtime/tpair.h"
#include "tapas/tblas.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
	TARR_ADD, TARR_SUB, TARR_MUL, TARR_DIV, TARR_MOD, TARR_POW,
	TARR_EQ, TARR_NE, TARR_GT, TARR_LT, TARR_GE, TARR_LE
} tarr_op;

static size_t checked_count(size_t rows, size_t cols, size_t item_size,
			    const char *where)
{
	if (rows && cols > SIZE_MAX / rows)
		twarn(ErrRuntime_Other, where, "array dimensions overflow");
	size_t count = rows * cols;
	if (count && item_size > SIZE_MAX / count)
		twarn(ErrRuntime_Other, where, "array allocation is too large");
	return count;
}

static int is_array_code(tcompo_type code)
{
	return code == compo_tdarr || code == compo_tbarr;
}

static tcompo_type array_code(const tcompo_v *array, const char *where)
{
	if (!array || !array->vtable)
		twarn(ErrRuntime_ParamsType, where, "array required");
	tcompo_type code = array->vtable->get_compo_type_code();
	if (!is_array_code(code))
		twarn(ErrRuntime_ParamsType, where, "array required");
	return code;
}

static size_t array_rows(const tcompo_v *arr)
{
	return arr->vtable->get_compo_type_code() == compo_tdarr
		       ? ((const tdarr *)arr)->rows : ((const tbarr *)arr)->rows;
}

static size_t array_cols(const tcompo_v *arr)
{
	return arr->vtable->get_compo_type_code() == compo_tdarr
		       ? ((const tdarr *)arr)->cols : ((const tbarr *)arr)->cols;
}

size_t tarr_rows(const tcompo_v *arr)
{
	array_code(arr, "tarr_rows");
	return array_rows(arr);
}

size_t tarr_cols(const tcompo_v *arr)
{
	array_code(arr, "tarr_cols");
	return array_cols(arr);
}

static size_t offset(size_t rows, size_t cols, size_t row, size_t col,
		     const char *where)
{
	if (row >= rows || col >= cols)
		twarn(ErrRuntime_IdxOutRange, where, "array index out of range");
	return row * cols + col;
}

tdarr *tdarr_new_uninitialized(size_t rows, size_t cols)
{
	tdarr *arr = (tdarr *)calloc(1, sizeof(*arr));
	if (!arr)
		twarn(ErrRuntime_Other, "tdarr_new", "out of memory");
	arr->base.vtable = &tdarr_vtable;
	arr->rows = rows;
	arr->cols = cols;
	size_t count = checked_count(rows, cols, sizeof(*arr->data), "tdarr_new");
	if (count) {
		arr->data = (double *)malloc(count * sizeof(*arr->data));
		if (!arr->data)
			twarn(ErrRuntime_Other, "tdarr_new", "out of memory");
	}
	return arr;
}

tdarr *tdarr_new(size_t rows, size_t cols, double value)
{
	tdarr *arr = tdarr_new_uninitialized(rows, cols);
	for (size_t i = 0; i < rows * cols; i++)
		arr->data[i] = value;
	return arr;
}

tbarr *tbarr_new_uninitialized(size_t rows, size_t cols)
{
	tbarr *arr = (tbarr *)calloc(1, sizeof(*arr));
	if (!arr)
		twarn(ErrRuntime_Other, "tbarr_new", "out of memory");
	arr->base.vtable = &tbarr_vtable;
	arr->rows = rows;
	arr->cols = cols;
	size_t count = checked_count(rows, cols, sizeof(*arr->data), "tbarr_new");
	if (count) {
		arr->data = (unsigned char *)malloc(count);
		if (!arr->data)
			twarn(ErrRuntime_Other, "tbarr_new", "out of memory");
	}
	return arr;
}

tbarr *tbarr_new(size_t rows, size_t cols, int value)
{
	tbarr *arr = tbarr_new_uninitialized(rows, cols);
	if (rows && cols)
		memset(arr->data, value ? 1 : 0, rows * cols);
	return arr;
}

double tdarr_at(const tdarr *arr, size_t row, size_t col)
{
	return arr->data[offset(arr->rows, arr->cols, row, col, "tdarr_at")];
}

int tbarr_at(const tbarr *arr, size_t row, size_t col)
{
	return arr->data[offset(arr->rows, arr->cols, row, col, "tbarr_at")] != 0;
}

void tdarr_set(tdarr *arr, size_t row, size_t col, double value)
{
	arr->data[offset(arr->rows, arr->cols, row, col, "tdarr_set")] = value;
}

void tbarr_set(tbarr *arr, size_t row, size_t col, int value)
{
	arr->data[offset(arr->rows, arr->cols, row, col, "tbarr_set")] = value ? 1 : 0;
}

/*----------------------- Required Vtable Operations -----------------------*/

static const char *tdarr_type(void)
{
	return "Real Array";
}

static const char *tbarr_type(void)
{
	return "Boolean Array";
}

static tcompo_type tdarr_code(void)
{
	return compo_tdarr;
}

static tcompo_type tbarr_code(void)
{
	return compo_tbarr;
}

static long tdarr_len(void *self)
{
	tdarr *array = (tdarr *)self;
	size_t count = array->rows * array->cols;
	if (count > LONG_MAX)
		twarn(ErrRuntime_IntOutOfRange, "len", "array length is too large");
	return (long)count;
}

static long tbarr_len(void *self)
{
	tbarr *array = (tbarr *)self;
	size_t count = array->rows * array->cols;
	if (count > LONG_MAX)
		twarn(ErrRuntime_IntOutOfRange, "len", "array length is too large");
	return (long)count;
}

static void *tdarr_copy_impl(void *self)
{
	tdarr *src = (tdarr *)self;
	tdarr *dst = tdarr_new_uninitialized(src->rows, src->cols);
	size_t count = src->rows * src->cols;
	if (count)
		memcpy(dst->data, src->data, count * sizeof(double));
	return dst;
}

static void *tbarr_copy_impl(void *self)
{
	tbarr *src = (tbarr *)self;
	tbarr *dst = tbarr_new_uninitialized(src->rows, src->cols);
	size_t count = src->rows * src->cols;
	if (count)
		memcpy(dst->data, src->data, count);
	return dst;
}

static void tdarr_free(void *self)
{
	tdarr *array = (tdarr *)self;
	free(array->data);
	free(array);
}

static void tbarr_free(void *self)
{
	tbarr *array = (tbarr *)self;
	free(array->data);
	free(array);
}

static int tdarr_identical(void *self, void *other)
{
	tdarr *left = (tdarr *)self;
	tdarr *right = (tdarr *)other;
	if (left->rows != right->rows || left->cols != right->cols)
		return 0;
	for (size_t i = 0; i < left->rows * left->cols; i++)
		if (left->data[i] != right->data[i])
			return 0;
	return 1;
}

static int tbarr_identical(void *self, void *other)
{
	tbarr *left = (tbarr *)self;
	tbarr *right = (tbarr *)other;
	size_t count = left->rows * left->cols;
	return left->rows == right->rows && left->cols == right->cols &&
	       (!count || memcmp(left->data, right->data, count) == 0);
}

static tstring *array_abbr(void *self)
{
	tcompo_v *arr = (tcompo_v *)self;
	return tobj_tostring_pointer(arr->vtable->get_type(), self);
}

static tstring *tdarr_string(void *self)
{
	tdarr *arr = (tdarr *)self;
	tstring *out = tstring_new("[");
	for (size_t r = 0; r < arr->rows; r++) {
		if (r) tstring_append(out, ",\n ");
		tstring_append_c(out, '[');
		for (size_t c = 0; c < arr->cols; c++) {
			if (c) tstring_append(out, ", ");
			tstring_append_fmt(out, "%g", arr->data[r * arr->cols + c]);
		}
		tstring_append_c(out, ']');
	}
	tstring_append_c(out, ']');
	return out;
}

static tstring *tbarr_string(void *self)
{
	tbarr *arr = (tbarr *)self;
	tstring *out = tstring_new("[");
	for (size_t r = 0; r < arr->rows; r++) {
		if (r) tstring_append(out, ",\n ");
		tstring_append_c(out, '[');
		for (size_t c = 0; c < arr->cols; c++) {
			if (c) tstring_append(out, ", ");
			tstring_append(out, arr->data[r * arr->cols + c] ? "true" : "false");
		}
		tstring_append_c(out, ']');
	}
	tstring_append_c(out, ']');
	return out;
}

/*------------------------------- Operators --------------------------------*/

tdarr *tdarr_matmul(const tdarr *left, const tdarr *right)
{
	if (left->cols != right->rows)
		twarn(ErrRuntime_LenInconsis, "tdarr_matmul", "incompatible shapes");
	tdarr *out = tdarr_new_uninitialized(left->rows, right->cols);
	tblas_gemm(left->rows, left->cols, right->cols,
		    1.0, left->data, right->data, 0.0, out->data);
	return out;
}

static void tdarr_neg(void *self, tobj *result)
{
	tdarr *array = (tdarr *)self;
	size_t count = array->rows * array->cols;
	tdarr *negated = tdarr_new_uninitialized(array->rows, array->cols);
	for (size_t i = 0; i < count; i++)
		negated->data[i] = -array->data[i];
	tobj_set_compo(result, (tcompo_v *)negated);
}

static double numeric_value(const tobj *value, const char *where)
{
	if (value->type == tint)
		return (double)value->val.v_tint;
	if (value->type == tfloat)
		return value->val.v_tfloat;
	twarn(ErrRuntime_ParamsType, where, "number or real array required");
	return 0.0;
}

static int comparison_operation(tarr_op operation)
{
	switch (operation) {
	case TARR_EQ:
	case TARR_NE:
	case TARR_GT:
	case TARR_LT:
	case TARR_GE:
	case TARR_LE:
		return 1;
	default:
		return 0;
	}
}

static double calculate(tarr_op operation, double left, double right)
{
	switch (operation) {
	case TARR_ADD: return left + right;
	case TARR_SUB: return left - right;
	case TARR_MUL: return left * right;
	case TARR_DIV: return left / right;
	case TARR_MOD: return fmod(left, right);
	case TARR_POW: return pow(left, right);
	default:
		twarn(ErrRuntime_Other, "array operator",
		      "arithmetic operation required");
		return 0.0;
	}
}

static int compare(tarr_op operation, double left, double right)
{
	switch (operation) {
	case TARR_EQ: return left == right;
	case TARR_NE: return left != right;
	case TARR_GT: return left > right;
	case TARR_LT: return left < right;
	case TARR_GE: return left >= right;
	case TARR_LE: return left <= right;
	default:
		twarn(ErrRuntime_Other, "array operator",
		      "comparison operation required");
		return 0;
	}
}

static void tdarr_compare_values(const tdarr *array, const tdarr *other,
				 double scalar, int rhs, tarr_op operation,
				 tobj *result)
{
	size_t count = array->rows * array->cols;
	tbarr *compared = tbarr_new_uninitialized(array->rows, array->cols);
	for (size_t i = 0; i < count; i++) {
		double operand = other ? other->data[i] : scalar;
		double left = rhs ? operand : array->data[i];
		double right = rhs ? array->data[i] : operand;
		compared->data[i] = compare(operation, left, right);
	}
	tobj_set_compo(result, (tcompo_v *)compared);
}

static void tdarr_calculate_values(const tdarr *array, const tdarr *other,
				   double scalar, int rhs,
				   tarr_op operation, tobj *result)
{
	size_t count = array->rows * array->cols;
	tdarr *calculated = tdarr_new_uninitialized(array->rows, array->cols);
	for (size_t i = 0; i < count; i++) {
		double operand = other ? other->data[i] : scalar;
		double left = rhs ? operand : array->data[i];
		double right = rhs ? array->data[i] : operand;
		calculated->data[i] = calculate(operation, left, right);
	}
	tobj_set_compo(result, (tcompo_v *)calculated);
}

static void tdarr_binary(void *self, const tobj *value, int rhs,
			 tobj *result, tarr_op operation)
{
	tdarr *array = (tdarr *)self;
	tdarr *other = nullptr;
	double scalar = 0.0;
	if (value->type == tcompo && tobj_compo_type(value) == compo_tdarr) {
		other = (tdarr *)value->val.v_tcompo;
		if (array->rows != other->rows || array->cols != other->cols)
			twarn(ErrRuntime_LenInconsis, "array operator",
			      "array shapes differ");
	} else {
		scalar = numeric_value(value, "array operator");
	}
	if (comparison_operation(operation))
		tdarr_compare_values(array, other, scalar, rhs, operation, result);
	else
		tdarr_calculate_values(array, other, scalar, rhs, operation, result);
}

static void tdarr_add(void *self, const tobj *value, int rhs, tobj *result)
{
	tdarr_binary(self, value, rhs, result, TARR_ADD);
}

static void tdarr_sub(void *self, const tobj *value, int rhs, tobj *result)
{
	tdarr_binary(self, value, rhs, result, TARR_SUB);
}

static void tdarr_mul(void *self, const tobj *value, int rhs, tobj *result)
{
	tdarr_binary(self, value, rhs, result, TARR_MUL);
}

static void tdarr_div(void *self, const tobj *value, int rhs, tobj *result)
{
	tdarr_binary(self, value, rhs, result, TARR_DIV);
}

static void tdarr_mod(void *self, const tobj *value, int rhs, tobj *result)
{
	tdarr_binary(self, value, rhs, result, TARR_MOD);
}

static void tdarr_pow(void *self, const tobj *value, int rhs, tobj *result)
{
	tdarr_binary(self, value, rhs, result, TARR_POW);
}

static void tdarr_eq(void *self, const tobj *value, int rhs, tobj *result)
{
	tdarr_binary(self, value, rhs, result, TARR_EQ);
}

static void tdarr_ne(void *self, const tobj *value, int rhs, tobj *result)
{
	tdarr_binary(self, value, rhs, result, TARR_NE);
}

static void tdarr_gt(void *self, const tobj *value, int rhs, tobj *result)
{
	tdarr_binary(self, value, rhs, result, TARR_GT);
}

static void tdarr_lt(void *self, const tobj *value, int rhs, tobj *result)
{
	tdarr_binary(self, value, rhs, result, TARR_LT);
}

static void tdarr_ge(void *self, const tobj *value, int rhs, tobj *result)
{
	tdarr_binary(self, value, rhs, result, TARR_GE);
}

static void tdarr_le(void *self, const tobj *value, int rhs, tobj *result)
{
	tdarr_binary(self, value, rhs, result, TARR_LE);
}

static void tdarr_mmul(void *self, const tobj *other, int rhs, tobj *out)
{
	if (other->type != tcompo || tobj_compo_type(other) != compo_tdarr)
		twarn(ErrRuntime_ParamsType, "tdarr_mmul", "two real arrays required");
	tdarr *a = (tdarr *)self, *b = (tdarr *)other->val.v_tcompo;
	tobj_set_compo(out, (tcompo_v *)(rhs ? tdarr_matmul(b, a) : tdarr_matmul(a, b)));
}

static void tbarr_logic(void *self, const tobj *other, int rhs, tobj *out, int is_and)
{
	(void)rhs;
	tbarr *array = (tbarr *)self;
	tbarr *operand = nullptr;
	int scalar = 0;
	if (other->type == tbool)
		scalar = other->val.v_tbool;
	else if (other->type == tcompo &&
		 tobj_compo_type(other) == compo_tbarr) {
		operand = (tbarr *)other->val.v_tcompo;
		if (array->rows != operand->rows || array->cols != operand->cols)
			twarn(ErrRuntime_LenInconsis, "tbarr_logic", "array shapes differ");
	} else {
		twarn(ErrRuntime_ParamsType, "tbarr_logic", "boolean or boolean array required");
	}
	tbarr *result = tbarr_new_uninitialized(array->rows, array->cols);
	for (size_t i = 0; i < array->rows * array->cols; i++) {
		int value = operand ? operand->data[i] : scalar;
		result->data[i] = is_and
			? array->data[i] && value
			: array->data[i] || value;
	}
	tobj_set_compo(out, (tcompo_v *)result);
}

static void tbarr_and(void *self, const tobj *value, int rhs, tobj *result)
{
	tbarr_logic(self, value, rhs, result, 1);
}

static void tbarr_or(void *self, const tobj *value, int rhs, tobj *result)
{
	tbarr_logic(self, value, rhs, result, 0);
}

/*------------------------------ Capabilities ------------------------------*/

/*
 * RealArray and BoolArray support two-dimensional indexed reads and writes.
 * Arithmetic, comparison, matrix multiplication, and boolean logic remain
 * operator hooks; transpose and shape queries belong to the dense package.
 */

static tdarr *tdarr_slice(const tdarr *arr, size_t row, size_t col,
			  size_t rows, size_t cols)
{
	if (row > arr->rows || rows > arr->rows - row ||
	    col > arr->cols || cols > arr->cols - col)
		twarn(ErrRuntime_IdxOutRange, "tdarr_slice", "slice out of range");
	tdarr *out = tdarr_new_uninitialized(rows, cols);
	if (cols)
		for (size_t r = 0; r < rows; r++)
			memcpy(out->data + r * cols,
			       arr->data + (row + r) * arr->cols + col,
			       cols * sizeof(double));
	return out;
}

static tbarr *tbarr_slice(const tbarr *arr, size_t row, size_t col,
			  size_t rows, size_t cols)
{
	if (row > arr->rows || rows > arr->rows - row ||
	    col > arr->cols || cols > arr->cols - col)
		twarn(ErrRuntime_IdxOutRange, "tbarr_slice", "slice out of range");
	tbarr *out = tbarr_new_uninitialized(rows, cols);
	if (cols)
		for (size_t r = 0; r < rows; r++)
			memcpy(out->data + r * cols,
			       arr->data + (row + r) * arr->cols + col, cols);
	return out;
}

static int range_param(const tobj *value, size_t limit,
		       size_t *begin, size_t *end)
{
	if (value->type == tint) {
		long index = value->val.v_tint;
		if (index < 0)
			index += (long)limit;
		if (index < 0 || (size_t)index >= limit)
			twarn(ErrRuntime_IdxOutRange, "array index",
			      "array index out of range");
		*begin = (size_t)index;
		*end = *begin + 1;
		return 0;
	}
	if (value->type != tcompo || tobj_compo_type(value) != compo_tpair)
		twarn(ErrRuntime_ParamsType, "array index",
		      "index must be int or pair");
	tpair *pair = (tpair *)value->val.v_tcompo;
	if (pair->first.type != tint || pair->second.type != tint)
		twarn(ErrRuntime_ParamsType, "array index",
		      "slice bounds must be integers");
	long first = pair->first.val.v_tint;
	long last = pair->second.val.v_tint;
	if (first < 0)
		first += (long)limit;
	if (last < 0)
		last += (long)limit;
	if (first < 0 || last < first || (size_t)last > limit)
		twarn(ErrRuntime_IdxOutRange, "array index",
		      "invalid array slice");
	*begin = (size_t)first;
	*end = (size_t)last;
	return 1;
}

static void tarr_idx(void *self, const tobj *arguments,
		     uint_regs argument_count, tobj *result)
{
	tcompo_v *arr = (tcompo_v *)self;
	if (argument_count != 2)
		twarn(ErrRuntime_ParamsCtr, "array index", "two indices required");
	size_t row_begin, row_end, column_begin, column_end;
	int row_slice = range_param(&arguments[0], array_rows(arr),
				    &row_begin, &row_end);
	int column_slice = range_param(&arguments[1], array_cols(arr),
				       &column_begin, &column_end);
	tcompo_type code = arr->vtable->get_compo_type_code();
	if (!row_slice && !column_slice) {
		if (code == compo_tdarr)
			tobj_set_float(result, tdarr_at((tdarr *)arr,
						 row_begin, column_begin));
		else
			tobj_set_bool(result, tbarr_at((tbarr *)arr,
					       row_begin, column_begin));
		return;
	}
	if (code == compo_tdarr)
		tobj_set_compo(result, (tcompo_v *)tdarr_slice((tdarr *)arr,
			row_begin, column_begin, row_end - row_begin,
			column_end - column_begin));
	else
		tobj_set_compo(result, (tcompo_v *)tbarr_slice((tbarr *)arr,
			row_begin, column_begin, row_end - row_begin,
			column_end - column_begin));
}

static void tarr_iset(void *self, const tobj *arguments,
		      uint_regs argument_count, const tobj *value)
{
	tcompo_v *arr = (tcompo_v *)self;
	if (argument_count != 2)
		twarn(ErrRuntime_ParamsCtr, "array index assignment",
		      "two indices required");
	size_t row_begin, row_end, column_begin, column_end;
	int row_slice = range_param(&arguments[0], array_rows(arr),
				    &row_begin, &row_end);
	int column_slice = range_param(&arguments[1], array_cols(arr),
				       &column_begin, &column_end);
	tcompo_type code = arr->vtable->get_compo_type_code();
	if (!row_slice && !column_slice) {
		if (code == compo_tdarr &&
		    (value->type == tint || value->type == tfloat))
			tdarr_set((tdarr *)arr, row_begin, column_begin,
				  value->type == tint ? (double)value->val.v_tint
						      : value->val.v_tfloat);
		else if (code == compo_tbarr && value->type == tbool)
			tbarr_set((tbarr *)arr, row_begin, column_begin,
				  value->val.v_tbool);
		else
			twarn(ErrRuntime_ParamsType, "array index assignment",
			      "scalar type does not match array");
		return;
	}
	if (value->type != tcompo || tobj_compo_type(value) != code)
		twarn(ErrRuntime_ParamsType, "array index assignment",
		      "slice assignment needs matching array");
	if (array_rows(value->val.v_tcompo) != row_end - row_begin ||
	    array_cols(value->val.v_tcompo) != column_end - column_begin)
		twarn(ErrRuntime_LenInconsis, "array index assignment",
		      "slice shapes differ");
	size_t width = column_end - column_begin;
	if (!width)
		return;
	for (size_t row = 0; row < row_end - row_begin; row++) {
		if (code == compo_tdarr)
			memmove(((tdarr *)arr)->data +
				(row_begin + row) * array_cols(arr) + column_begin,
				((tdarr *)value->val.v_tcompo)->data +
				row * width, width * sizeof(double));
		else
			memmove(((tbarr *)arr)->data +
				(row_begin + row) * array_cols(arr) + column_begin,
				((tbarr *)value->val.v_tcompo)->data +
				row * width, width);
	}
}

static const tcompo_capabilities array_capabilities = {
	.indexable = tarr_idx,
	.index_settable = tarr_iset
};

tcompo_vtable tdarr_vtable = {
	.get_type = tdarr_type,
	.get_compo_type_code = tdarr_code,
	.len = tdarr_len,
	.copy = tdarr_copy_impl,
	.free = tdarr_free,
	.identical = tdarr_identical,
	.tostring_abbr = array_abbr,
	.tostring_full = tdarr_string,
	.op_neg = tdarr_neg,
	.op_add = tdarr_add,
	.op_sub = tdarr_sub,
	.op_mul = tdarr_mul,
	.op_div = tdarr_div,
	.op_mod = tdarr_mod,
	.op_pow = tdarr_pow,
	.op_mmul = tdarr_mmul,
	.op_eq = tdarr_eq,
	.op_ne = tdarr_ne,
	.op_sg = tdarr_gt,
	.op_sl = tdarr_lt,
	.op_ge = tdarr_ge,
	.op_le = tdarr_le,
	.capabilities = &array_capabilities
};

tcompo_vtable tbarr_vtable = {
	.get_type = tbarr_type,
	.get_compo_type_code = tbarr_code,
	.len = tbarr_len,
	.copy = tbarr_copy_impl,
	.free = tbarr_free,
	.identical = tbarr_identical,
	.tostring_abbr = array_abbr,
	.tostring_full = tbarr_string,
	.op_and = tbarr_and,
	.op_or = tbarr_or,
	.capabilities = &array_capabilities
};
