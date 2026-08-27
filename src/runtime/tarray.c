#include "tapas/runtime/tarray.h"

#include "tapas/runtime/tlist.h"
#include "tapas/runtime/tpair.h"
#include "tapas/tblas.h"

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
	if (!arr || !arr->vtable || !is_array_code(arr->vtable->get_compo_type_code()))
		return 0;
	return array_rows(arr);
}

size_t tarr_cols(const tcompo_v *arr)
{
	if (!arr || !arr->vtable || !is_array_code(arr->vtable->get_compo_type_code()))
		return 0;
	return array_cols(arr);
}

static size_t offset(size_t rows, size_t cols, size_t row, size_t col,
		     const char *where)
{
	if (row >= rows || col >= cols)
		twarn(ErrRuntime_IdxOutRange, where, "array index out of range");
	return row * cols + col;
}

tdarr *tdarr_new(size_t rows, size_t cols, double value)
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
		for (size_t i = 0; i < count; i++)
			arr->data[i] = value;
	}
	return arr;
}

tbarr *tbarr_new(size_t rows, size_t cols, int value)
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
		memset(arr->data, value ? 1 : 0, count);
	}
	return arr;
}

tdarr *tdarr_from_list(size_t rows, size_t cols, const tlist *values)
{
	size_t count = checked_count(rows, cols, sizeof(double), "tdarr_from_list");
	if (!values || tlist_size(values) != count)
		twarn(ErrRuntime_LenInconsis, "tdarr_from_list", "shape and list length differ");
	tdarr *arr = tdarr_new(rows, cols, 0.0);
	for (size_t i = 0; i < count; i++) {
		const tobj *v = tlist_at(values, (uint_objs)i);
		if (v->type == tint)
			arr->data[i] = (double)v->val.v_tint;
		else if (v->type == tfloat)
			arr->data[i] = v->val.v_tfloat;
		else if (v->type == tbool)
			arr->data[i] = (double)v->val.v_tbool;
		else
			twarn(ErrRuntime_ParamsType, "tdarr_from_list", "numeric list required");
	}
	return arr;
}

tbarr *tbarr_from_list(size_t rows, size_t cols, const tlist *values)
{
	size_t count = checked_count(rows, cols, 1, "tbarr_from_list");
	if (!values || tlist_size(values) != count)
		twarn(ErrRuntime_LenInconsis, "tbarr_from_list", "shape and list length differ");
	tbarr *arr = tbarr_new(rows, cols, 0);
	for (size_t i = 0; i < count; i++) {
		const tobj *v = tlist_at(values, (uint_objs)i);
		if (v->type != tbool)
			twarn(ErrRuntime_ParamsType, "tbarr_from_list", "boolean list required");
		arr->data[i] = v->val.v_tbool ? 1 : 0;
	}
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

static const char *tdarr_type(void) { return "Real Array"; }
static const char *tbarr_type(void) { return "Boolean Array"; }
static tcompo_type tdarr_code(void) { return compo_tdarr; }
static tcompo_type tbarr_code(void) { return compo_tbarr; }
static long tdarr_len(void *self) { return (long)(((tdarr *)self)->rows * ((tdarr *)self)->cols); }
static long tbarr_len(void *self) { return (long)(((tbarr *)self)->rows * ((tbarr *)self)->cols); }

static void *tdarr_copy_impl(void *self)
{
	tdarr *src = (tdarr *)self;
	tdarr *dst = tdarr_new(src->rows, src->cols, 0.0);
	memcpy(dst->data, src->data, src->rows * src->cols * sizeof(double));
	return dst;
}

static void *tbarr_copy_impl(void *self)
{
	tbarr *src = (tbarr *)self;
	tbarr *dst = tbarr_new(src->rows, src->cols, 0);
	memcpy(dst->data, src->data, src->rows * src->cols);
	return dst;
}

static void array_free(void *self)
{
	tdarr *arr = (tdarr *)self;
	free(arr->data);
	free(arr);
}

static int tdarr_identical(void *self, void *other)
{
	tdarr *a = (tdarr *)self, *b = (tdarr *)other;
	return a->rows == b->rows && a->cols == b->cols &&
	       memcmp(a->data, b->data, a->rows * a->cols * sizeof(double)) == 0;
}

static int tbarr_identical(void *self, void *other)
{
	tbarr *a = (tbarr *)self, *b = (tbarr *)other;
	return a->rows == b->rows && a->cols == b->cols &&
	       memcmp(a->data, b->data, a->rows * a->cols) == 0;
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

tdarr *tdarr_slice(const tdarr *arr, size_t row, size_t col, size_t rows, size_t cols)
{
	if (row > arr->rows || rows > arr->rows - row ||
	    col > arr->cols || cols > arr->cols - col)
		twarn(ErrRuntime_IdxOutRange, "tdarr_slice", "slice out of range");
	tdarr *out = tdarr_new(rows, cols, 0.0);
	for (size_t r = 0; r < rows; r++)
		memcpy(out->data + r * cols, arr->data + (row + r) * arr->cols + col,
		       cols * sizeof(double));
	return out;
}

tbarr *tbarr_slice(const tbarr *arr, size_t row, size_t col, size_t rows, size_t cols)
{
	if (row > arr->rows || rows > arr->rows - row ||
	    col > arr->cols || cols > arr->cols - col)
		twarn(ErrRuntime_IdxOutRange, "tbarr_slice", "slice out of range");
	tbarr *out = tbarr_new(rows, cols, 0);
	for (size_t r = 0; r < rows; r++)
		memcpy(out->data + r * cols, arr->data + (row + r) * arr->cols + col, cols);
	return out;
}

tdarr *tdarr_transpose(const tdarr *arr)
{
	tdarr *out = tdarr_new(arr->cols, arr->rows, 0.0);
	for (size_t r = 0; r < arr->rows; r++)
		for (size_t c = 0; c < arr->cols; c++)
			out->data[c * out->cols + r] = arr->data[r * arr->cols + c];
	return out;
}

tbarr *tbarr_transpose(const tbarr *arr)
{
	tbarr *out = tbarr_new(arr->cols, arr->rows, 0);
	for (size_t r = 0; r < arr->rows; r++)
		for (size_t c = 0; c < arr->cols; c++)
			out->data[c * out->cols + r] = arr->data[r * arr->cols + c];
	return out;
}

tdarr *tdarr_matmul(const tdarr *left, const tdarr *right)
{
	if (left->cols != right->rows)
		twarn(ErrRuntime_LenInconsis, "tdarr_matmul", "incompatible shapes");
	tdarr *out = tdarr_new(left->rows, right->cols, 0.0);
	tblas_gemm(left->rows, left->cols, right->cols,
		    left->data, right->data, out->data);
	return out;
}

tdarr *tdarr_neg(const tdarr *arr)
{
	size_t count = arr->rows * arr->cols;
	tdarr *out = tdarr_new(arr->rows, arr->cols, 0.0);
	tblas_copy(count, arr->data, out->data);
	tblas_scal(count, -1.0, out->data);
	return out;
}

static int range_param(const tobj *v, size_t limit, size_t *begin, size_t *end)
{
	if (v->type == tint) {
		long idx = v->val.v_tint;
		if (idx < 0) idx += (long)limit;
		if (idx < 0 || (size_t)idx >= limit)
			twarn(ErrRuntime_IdxOutRange, "tarr_idx", "array index out of range");
		*begin = (size_t)idx;
		*end = *begin + 1;
		return 0;
	}
	if (v->type != tcompo || tobj_compo_type(v) != compo_tpair)
		twarn(ErrRuntime_ParamsType, "tarr_idx", "index must be int or pair");
	tpair *pair = (tpair *)v->val.v_tcompo;
	if (pair->first.type != tint || pair->second.type != tint)
		twarn(ErrRuntime_ParamsType, "tarr_idx", "slice bounds must be integers");
	long first = pair->first.val.v_tint, last = pair->second.val.v_tint;
	if (first < 0) first += (long)limit;
	if (last < 0) last += (long)limit;
	if (first < 0 || last < first || (size_t)last > limit)
		twarn(ErrRuntime_IdxOutRange, "tarr_idx", "invalid array slice");
	*begin = (size_t)first;
	*end = (size_t)last;
	return 1;
}

void tarr_idx(tcompo_v *arr, const tobj *params, uint_regs np, tobj *vre)
{
	if (np != 2)
		twarn(ErrRuntime_ParamsCtr, "tarr_idx", "two indices required");
	size_t rb, re, cb, ce;
	int rs = range_param(&params[0], array_rows(arr), &rb, &re);
	int cs = range_param(&params[1], array_cols(arr), &cb, &ce);
	tcompo_type code = arr->vtable->get_compo_type_code();
	if (!rs && !cs) {
		if (code == compo_tdarr)
			tobj_set_float(vre, tdarr_at((tdarr *)arr, rb, cb));
		else
			tobj_set_bool(vre, tbarr_at((tbarr *)arr, rb, cb));
		return;
	}
	if (code == compo_tdarr)
		tobj_set_compo(vre, (tcompo_v *)tdarr_slice((tdarr *)arr, rb, cb, re - rb, ce - cb));
	else
		tobj_set_compo(vre, (tcompo_v *)tbarr_slice((tbarr *)arr, rb, cb, re - rb, ce - cb));
}

void tarr_iset(tcompo_v *arr, const tobj *params, uint_regs np, const tobj *value)
{
	if (np != 2)
		twarn(ErrRuntime_ParamsCtr, "tarr_iset", "two indices required");
	size_t rb, re, cb, ce;
	int rs = range_param(&params[0], array_rows(arr), &rb, &re);
	int cs = range_param(&params[1], array_cols(arr), &cb, &ce);
	tcompo_type code = arr->vtable->get_compo_type_code();
	if (!rs && !cs) {
		if (code == compo_tdarr && (value->type == tint || value->type == tfloat))
			tdarr_set((tdarr *)arr, rb, cb, value->type == tint ? (double)value->val.v_tint : value->val.v_tfloat);
		else if (code == compo_tbarr && value->type == tbool)
			tbarr_set((tbarr *)arr, rb, cb, value->val.v_tbool);
		else
			twarn(ErrRuntime_ParamsType, "tarr_iset", "scalar type does not match array");
		return;
	}
	if (value->type != tcompo || tobj_compo_type(value) != code)
		twarn(ErrRuntime_ParamsType, "tarr_iset", "slice assignment needs matching array");
	if (array_rows(value->val.v_tcompo) != re - rb || array_cols(value->val.v_tcompo) != ce - cb)
		twarn(ErrRuntime_LenInconsis, "tarr_iset", "slice shapes differ");
	for (size_t r = 0; r < re - rb; r++) {
		if (code == compo_tdarr)
			memmove(((tdarr *)arr)->data + (rb + r) * array_cols(arr) + cb,
				((tdarr *)value->val.v_tcompo)->data + r * (ce - cb),
				(ce - cb) * sizeof(double));
		else
			memmove(((tbarr *)arr)->data + (rb + r) * array_cols(arr) + cb,
				((tbarr *)value->val.v_tcompo)->data + r * (ce - cb), ce - cb);
	}
}

static double numeric_value(const tobj *v, const char *where)
{
	if (v->type == tint) return (double)v->val.v_tint;
	if (v->type == tfloat) return v->val.v_tfloat;
	twarn(ErrRuntime_ParamsType, where, "number or real array required");
	return 0.0;
}

static void tdarr_binary(void *self, const tobj *other, int rhs, tobj *out, tarr_op op)
{
	tdarr *a = (tdarr *)self;
	if (other->type == tcompo && tobj_compo_type(other) == compo_tdarr) {
		tdarr *b = (tdarr *)other->val.v_tcompo;
		if (a->rows != b->rows || a->cols != b->cols)
			twarn(ErrRuntime_LenInconsis, "tdarr_binary", "array shapes differ");
		if (op >= TARR_EQ) {
			tbarr *r = tbarr_new(a->rows, a->cols, 0);
			for (size_t i = 0; i < a->rows * a->cols; i++) {
				double x = rhs ? b->data[i] : a->data[i], y = rhs ? a->data[i] : b->data[i];
				r->data[i] = op == TARR_EQ ? x == y : op == TARR_NE ? x != y :
					op == TARR_GT ? x > y : op == TARR_LT ? x < y : op == TARR_GE ? x >= y : x <= y;
			}
			tobj_set_compo(out, (tcompo_v *)r);
			return;
		}
		size_t count = a->rows * a->cols;
		tdarr *r = tdarr_new(a->rows, a->cols, 0.0);
		if (op == TARR_ADD || op == TARR_SUB) {
			const double *left = rhs ? b->data : a->data;
			const double *right = rhs ? a->data : b->data;
			tblas_copy(count, left, r->data);
			tblas_axpy(count, op == TARR_ADD ? 1.0 : -1.0,
				    right, r->data);
			tobj_set_compo(out, (tcompo_v *)r);
			return;
		}
		for (size_t i = 0; i < count; i++) {
			double x = rhs ? b->data[i] : a->data[i], y = rhs ? a->data[i] : b->data[i];
			r->data[i] = op == TARR_ADD ? x + y : op == TARR_SUB ? x - y :
				op == TARR_MUL ? x * y : op == TARR_DIV ? x / y :
				op == TARR_MOD ? fmod(x, y) : pow(x, y);
		}
		tobj_set_compo(out, (tcompo_v *)r);
		return;
	}
	double scalar = numeric_value(other, "tdarr_binary");
	if (op >= TARR_EQ) {
		tbarr *r = tbarr_new(a->rows, a->cols, 0);
		for (size_t i = 0; i < a->rows * a->cols; i++) {
			double x = rhs ? scalar : a->data[i], y = rhs ? a->data[i] : scalar;
			r->data[i] = op == TARR_EQ ? x == y : op == TARR_NE ? x != y :
				op == TARR_GT ? x > y : op == TARR_LT ? x < y : op == TARR_GE ? x >= y : x <= y;
		}
		tobj_set_compo(out, (tcompo_v *)r);
		return;
	}
	tdarr *r = tdarr_new(a->rows, a->cols, 0.0);
	if (op == TARR_MUL) {
		size_t count = a->rows * a->cols;
		tblas_copy(count, a->data, r->data);
		tblas_scal(count, scalar, r->data);
		tobj_set_compo(out, (tcompo_v *)r);
		return;
	}
	for (size_t i = 0; i < a->rows * a->cols; i++) {
		double x = rhs ? scalar : a->data[i], y = rhs ? a->data[i] : scalar;
		r->data[i] = op == TARR_ADD ? x + y : op == TARR_SUB ? x - y :
			op == TARR_MUL ? x * y : op == TARR_DIV ? x / y :
			op == TARR_MOD ? fmod(x, y) : pow(x, y);
	}
	tobj_set_compo(out, (tcompo_v *)r);
}

#define DEF_ARR_OP(name, op) static void tdarr_##name(void *s, const tobj *v, int r, tobj *o) { tdarr_binary(s, v, r, o, op); }
DEF_ARR_OP(add, TARR_ADD) DEF_ARR_OP(sub, TARR_SUB) DEF_ARR_OP(mul, TARR_MUL)
DEF_ARR_OP(div, TARR_DIV) DEF_ARR_OP(mod, TARR_MOD) DEF_ARR_OP(pow, TARR_POW)
DEF_ARR_OP(eq, TARR_EQ)
DEF_ARR_OP(ne, TARR_NE) DEF_ARR_OP(gt, TARR_GT) DEF_ARR_OP(lt, TARR_LT)
DEF_ARR_OP(ge, TARR_GE) DEF_ARR_OP(le, TARR_LE)

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
	tbarr *a = (tbarr *)self, *r = tbarr_new(a->rows, a->cols, 0);
	if (other->type == tbool) {
		for (size_t i = 0; i < a->rows * a->cols; i++)
			r->data[i] = is_and ? (a->data[i] && other->val.v_tbool) : (a->data[i] || other->val.v_tbool);
	} else if (other->type == tcompo && tobj_compo_type(other) == compo_tbarr) {
		tbarr *b = (tbarr *)other->val.v_tcompo;
		if (a->rows != b->rows || a->cols != b->cols)
			twarn(ErrRuntime_LenInconsis, "tbarr_logic", "array shapes differ");
		for (size_t i = 0; i < a->rows * a->cols; i++)
			r->data[i] = is_and ? (a->data[i] && b->data[i]) : (a->data[i] || b->data[i]);
	} else
		twarn(ErrRuntime_ParamsType, "tbarr_logic", "boolean or boolean array required");
	tobj_set_compo(out, (tcompo_v *)r);
}

static void tbarr_and(void *s, const tobj *v, int r, tobj *o) { tbarr_logic(s, v, r, o, 1); }
static void tbarr_or(void *s, const tobj *v, int r, tobj *o) { tbarr_logic(s, v, r, o, 0); }

tcompo_vtable tdarr_vtable = {
	tdarr_type, tdarr_code, tdarr_len, tdarr_copy_impl, array_free,
	tdarr_identical, array_abbr, tdarr_string,
	tdarr_add, tdarr_sub, tdarr_mul, tdarr_div, tdarr_mod, tdarr_pow, tdarr_mmul,
	tdarr_eq, tdarr_ne, tdarr_gt, tdarr_lt, tdarr_ge, tdarr_le, NULL, NULL
};

tcompo_vtable tbarr_vtable = {
	tbarr_type, tbarr_code, tbarr_len, tbarr_copy_impl, array_free,
	tbarr_identical, array_abbr, tbarr_string,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, tbarr_and, tbarr_or
};
