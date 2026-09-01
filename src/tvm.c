#include "tapas/tvm.h"

#include "tapas/runtime/tarray.h"
#include "tapas/runtime/tdict.h"
#include "tapas/runtime/titer.h"
#include "tapas/runtime/tlist.h"
#include "tapas/runtime/tcfn.h"
#include "tapas/runtime/tpair.h"
#include "tapas/runtime/tstr.h"
#include "tapas/runtime/ttime.h"
#include "tapas/runtime/ttype.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/*===========================================================================*
 * 1. Binary Operator Function Type
 *===========================================================================*/

typedef void (*binopf)(const tobj *v1, const tobj *v2, tobj *vre);

/*===========================================================================*
 * 2. Operators
 *===========================================================================*/

void operator_pair(const tobj *v1, const tobj *v2, tobj *vre)
{
	tobj_set_compo(vre, (tcompo_v *)tpair_new(v1, v2));
}

void operator_to(const tobj *v1, const tobj *v2, tobj *vre)
{
	if (v1->type != tint || v2->type != tint)
		twarn(ErrRuntime_ParamsType, "operator_to", "");
	tobj_set_compo(vre,
		       (tcompo_v *)titer_new(v1->val.v_tint, v2->val.v_tint));
}

void operator_in(const tobj *v1, const tobj *v2, tobj *vre)
{
	if (v2->type == tcompo && v2->val.v_tcompo) {
		tcompo_type ct =
			v2->val.v_tcompo->vtable->get_compo_type_code();
		if (ct == compo_titer) {
			tobj_set_bool(vre,
				      titer_in((titer *)v2->val.v_tcompo, v1));
			return;
		}
		if (ct == compo_tlist) {
			tobj_set_bool(vre,
				      tlist_in((tlist *)v2->val.v_tcompo, v1));
			return;
		}
		if (ct == compo_tdict) {
			tobj_set_bool(vre,
				      tdict_contains((tdict *)v2->val.v_tcompo, v1));
			return;
		}
	}
	tobj_set_bool(vre, 0);
}

static inline void vm_set_int_result(tobj *value, long result)
{
	value->type = tint;
	value->name_loc = (uint_csts)UNDEF_NAMELOC;
	value->val.v_tint = result;
}

static inline void vm_set_float_result(tobj *value, double result)
{
	value->type = tfloat;
	value->name_loc = (uint_csts)UNDEF_NAMELOC;
	value->val.v_tfloat = result;
}

static inline void vm_set_bool_result(tobj *value, int result)
{
	value->type = tbool;
	value->name_loc = (uint_csts)UNDEF_NAMELOC;
	value->val.v_tbool = result;
}

#define DEF_ALGO(fn_name, fll, fld, fdl, fdd)                                  \
	static inline int fn_name##_impl(ttypes t1,                            \
					 ttypes t2,                            \
					 const tobj *v1,                       \
					 const tobj *v2,                       \
					 tobj *vre)                            \
	{                                                                      \
		if (t1 == tint && t2 == tint) {                                \
			vm_set_int_result(vre,                                 \
				     fll(v1->val.v_tint, v2->val.v_tint));     \
			return 1;                                              \
		}                                                              \
		if (t1 == tint && t2 == tfloat) {                              \
			vm_set_float_result(                                   \
				vre, fld(v1->val.v_tint, v2->val.v_tfloat));   \
			return 1;                                              \
		}                                                              \
		if (t1 == tfloat && t2 == tint) {                              \
			vm_set_float_result(                                   \
				vre, fdl(v1->val.v_tfloat, v2->val.v_tint));   \
			return 1;                                              \
		}                                                              \
		if (t1 == tfloat && t2 == tfloat) {                            \
			vm_set_float_result(                                   \
				vre,                                           \
				fdd(v1->val.v_tfloat, v2->val.v_tfloat));      \
			return 1;                                              \
		}                                                              \
		return 0;                                                      \
	}

long add_ll(long a, long b)
{
	return a + b;
}

double add_ld(long a, double b)
{
	return (double)a + b;
}

double add_dl(double a, long b)
{
	return a + (double)b;
}

double add_dd(double a, double b)
{
	return a + b;
}

DEF_ALGO(add, add_ll, add_ld, add_dl, add_dd)

long sub_ll(long a, long b)
{
	return a - b;
}

double sub_ld(long a, double b)
{
	return (double)a - b;
}

double sub_dl(double a, long b)
{
	return a - (double)b;
}

double sub_dd(double a, double b)
{
	return a - b;
}

DEF_ALGO(sub, sub_ll, sub_ld, sub_dl, sub_dd)

long mul_ll(long a, long b)
{
	return a * b;
}

double mul_ld(long a, double b)
{
	return (double)a * b;
}

double mul_dl(double a, long b)
{
	return a * (double)b;
}

double mul_dd(double a, double b)
{
	return a * b;
}

DEF_ALGO(mul, mul_ll, mul_ld, mul_dl, mul_dd)

long div_ll(long a, long b)
{
	if (b == 0)
		twarn(ErrRuntime_DivIntZero, "div", "");
	return a / b;
}

double div_ld(long a, double b)
{
	return (double)a / b;
}

double div_dl(double a, long b)
{
	return a / (double)b;
}

double div_dd(double a, double b)
{
	return a / b;
}

DEF_ALGO(div, div_ll, div_ld, div_dl, div_dd)

#undef DEF_ALGO

static void operator_add_slow(const tobj *v1, const tobj *v2, tobj *vre)
{
	if (v1->type == tcompo && v1->val.v_tcompo->vtable->op_add) {
		v1->val.v_tcompo->vtable->op_add(
			v1->val.v_tcompo, v2, 0, vre);
		return;
	}
	if (v2->type == tcompo && v2->val.v_tcompo->vtable->op_add) {
		v2->val.v_tcompo->vtable->op_add(
			v2->val.v_tcompo, v1, 1, vre);
		return;
	}
	twarn(ErrRuntime_ParamsType, "operator_add",
	      "unsupported type for +");
}

static void operator_add(
		const tobj *v1, const tobj *v2, tobj *vre)
{
	if (add_impl(v1->type, v2->type, v1, v2, vre))
		return;
	operator_add_slow(v1, v2, vre);
}

static void operator_sub_slow(const tobj *v1, const tobj *v2, tobj *vre)
{
	if (v1->type == tcompo && v1->val.v_tcompo->vtable->op_sub) {
		v1->val.v_tcompo->vtable->op_sub(
			v1->val.v_tcompo, v2, 0, vre);
		return;
	}
	if (v2->type == tcompo && v2->val.v_tcompo->vtable->op_sub) {
		v2->val.v_tcompo->vtable->op_sub(
			v2->val.v_tcompo, v1, 1, vre);
		return;
	}
	twarn(ErrRuntime_ParamsType, "operator_sub",
	      "unsupported type for -");
}

static void operator_sub(
		const tobj *v1, const tobj *v2, tobj *vre)
{
	if (sub_impl(v1->type, v2->type, v1, v2, vre))
		return;
	operator_sub_slow(v1, v2, vre);
}

static void operator_mul_slow(const tobj *v1, const tobj *v2, tobj *vre)
{
	if (v1->type == tcompo && v1->val.v_tcompo->vtable->op_mul) {
		v1->val.v_tcompo->vtable->op_mul(
			v1->val.v_tcompo, v2, 0, vre);
		return;
	}
	if (v2->type == tcompo && v2->val.v_tcompo->vtable->op_mul) {
		v2->val.v_tcompo->vtable->op_mul(
			v2->val.v_tcompo, v1, 1, vre);
		return;
	}
	twarn(ErrRuntime_ParamsType, "operator_mul",
	      "unsupported type for *");
}

static void operator_mul(
		const tobj *v1, const tobj *v2, tobj *vre)
{
	if (mul_impl(v1->type, v2->type, v1, v2, vre))
		return;
	operator_mul_slow(v1, v2, vre);
}

void operator_mmul(const tobj *v1, const tobj *v2, tobj *vre)
{
	/* Attempt composite fallback */
	if (v1->type == tcompo && v1->val.v_tcompo->vtable->op_mmul) {
		v1->val.v_tcompo->vtable->op_mmul(
			v1->val.v_tcompo, v2, 0, vre);
		return;
	}
	if (v2->type == tcompo && v2->val.v_tcompo->vtable->op_mmul) {
		v2->val.v_tcompo->vtable->op_mmul(
			v2->val.v_tcompo, v1, 1, vre);
		return;
	}
	twarn(ErrRuntime_ParamsType,
	      "operator_mmul",
	      "matrix multiplication is not supported for these types");
}

static void operator_div_slow(const tobj *v1, const tobj *v2, tobj *vre)
{
	if (v1->type == tcompo && v1->val.v_tcompo->vtable->op_div) {
		v1->val.v_tcompo->vtable->op_div(
			v1->val.v_tcompo, v2, 0, vre);
		return;
	}
	if (v2->type == tcompo && v2->val.v_tcompo->vtable->op_div) {
		v2->val.v_tcompo->vtable->op_div(
			v2->val.v_tcompo, v1, 1, vre);
		return;
	}
	twarn(ErrRuntime_ParamsType, "operator_div",
	      "unsupported type for /");
}

static void operator_div(
		const tobj *v1, const tobj *v2, tobj *vre)
{
	if (div_impl(v1->type, v2->type, v1, v2, vre))
		return;
	operator_div_slow(v1, v2, vre);
}

void operator_mod(const tobj *v1, const tobj *v2, tobj *vre)
{
	if (v1->type == tcompo && v1->val.v_tcompo->vtable->op_mod) {
		v1->val.v_tcompo->vtable->op_mod(v1->val.v_tcompo, v2, 0, vre);
		return;
	}
	if (v2->type == tcompo && v2->val.v_tcompo->vtable->op_mod) {
		v2->val.v_tcompo->vtable->op_mod(v2->val.v_tcompo, v1, 1, vre);
		return;
	}
	if ((v1->type != tint && v1->type != tfloat) ||
	    (v2->type != tint && v2->type != tfloat))
		twarn(ErrRuntime_ParamsType, "operator_mod",
		      "unsupported type for %");
	if (v1->type == tint && v2->type == tint) {
		if (v2->val.v_tint == 0)
			twarn(ErrRuntime_DivIntZero, "operator_mod", "");
		tobj_set_int(vre, v1->val.v_tint % v2->val.v_tint);
	} else {
		double a = (v1->type == tint) ? (double)v1->val.v_tint
					      : v1->val.v_tfloat;
		double b = (v2->type == tint) ? (double)v2->val.v_tint
					      : v2->val.v_tfloat;
		tobj_set_float(vre, fmod(a, b));
	}
}

void operator_pow(const tobj *v1, const tobj *v2, tobj *vre)
{
	if (v1->type == tcompo && v1->val.v_tcompo->vtable->op_pow) {
		v1->val.v_tcompo->vtable->op_pow(v1->val.v_tcompo, v2, 0, vre);
		return;
	}
	if (v2->type == tcompo && v2->val.v_tcompo->vtable->op_pow) {
		v2->val.v_tcompo->vtable->op_pow(v2->val.v_tcompo, v1, 1, vre);
		return;
	}
	if ((v1->type != tint && v1->type != tfloat) ||
	    (v2->type != tint && v2->type != tfloat))
		twarn(ErrRuntime_ParamsType, "operator_pow", "unsupported type for ^");
	double a =
		(v1->type == tint) ? (double)v1->val.v_tint : v1->val.v_tfloat;
	double b =
		(v2->type == tint) ? (double)v2->val.v_tint : v2->val.v_tfloat;
	tobj_set_float(vre, pow(a, b));
}

static void operator_pos(tobj *value)
{
	if (value->type != tint && value->type != tfloat)
		twarn(ErrRuntime_ParamsType, "operator_pos",
		      "unary + requires a number");
}

static void operator_neg(tobj *value)
{
	if (value->type == tint) {
		value->val.v_tint = -value->val.v_tint;
		return;
	}
	if (value->type == tfloat) {
		value->val.v_tfloat = -value->val.v_tfloat;
		return;
	}
	if (value->type == tcompo &&
	    tobj_compo_type(value) == compo_tdarr) {
		tdarr *result = tdarr_neg((tdarr *)value->val.v_tcompo);
		tobj_set_compo(value, (tcompo_v *)result);
		return;
	}
	twarn(ErrRuntime_ParamsType, "operator_neg",
	      "unary - requires a number or real array");
}

#define DEF_CMP(fn_name, op)                                                   \
	static inline int fn_name##_impl(ttypes t1,                            \
					 ttypes t2,                            \
					 const tobj *v1,                       \
					 const tobj *v2,                       \
					 tobj *vre)                            \
	{                                                                      \
		if (t1 == tint && t2 == tint) {                                \
			vm_set_bool_result(                                    \
				vre, (int)(v1->val.v_tint op v2->val.v_tint)); \
			return 1;                                              \
		}                                                              \
		if (t1 == tint && t2 == tfloat) {                              \
			vm_set_bool_result(vre,                                \
				      (int)((double)v1->val.v_tint op          \
						    v2->val.v_tfloat));        \
			return 1;                                              \
		}                                                              \
		if (t1 == tfloat && t2 == tint) {                              \
			vm_set_bool_result(vre,                                \
				      (int)(v1->val.v_tfloat op(double)        \
						    v2->val.v_tint));          \
			return 1;                                              \
		}                                                              \
		if (t1 == tfloat && t2 == tfloat) {                            \
			vm_set_bool_result(vre,                                \
				      (int)(v1->val.v_tfloat op                \
						    v2->val.v_tfloat));        \
			return 1;                                              \
		}                                                              \
		return 0;                                                      \
	}

DEF_CMP(eq, ==)
DEF_CMP(ne, !=)
DEF_CMP(sg, >)
DEF_CMP(sl, <)
DEF_CMP(ge, >=)
DEF_CMP(le, <=)

static void operator_eq_slow(const tobj *v1, const tobj *v2, tobj *vre)
{
	if (v1->type == tcompo && v1->val.v_tcompo->vtable->op_eq) {
		v1->val.v_tcompo->vtable->op_eq(v1->val.v_tcompo, v2, 0, vre); return;
	}
	if (v2->type == tcompo && v2->val.v_tcompo->vtable->op_eq) {
		v2->val.v_tcompo->vtable->op_eq(v2->val.v_tcompo, v1, 1, vre); return;
	}
	tobj_set_bool(vre, tobj_identical(v1, v2));
}

static void operator_eq(
		const tobj *v1, const tobj *v2, tobj *vre)
{
	if (eq_impl(v1->type, v2->type, v1, v2, vre)) return;
	operator_eq_slow(v1, v2, vre);
}

static void operator_ne_slow(const tobj *v1, const tobj *v2, tobj *vre)
{
	if (v1->type == tcompo && v1->val.v_tcompo->vtable->op_ne) {
		v1->val.v_tcompo->vtable->op_ne(v1->val.v_tcompo, v2, 0, vre); return;
	}
	if (v2->type == tcompo && v2->val.v_tcompo->vtable->op_ne) {
		v2->val.v_tcompo->vtable->op_ne(v2->val.v_tcompo, v1, 1, vre); return;
	}
	tobj_set_bool(vre, !tobj_identical(v1, v2));
}

static void operator_ne(
		const tobj *v1, const tobj *v2, tobj *vre)
{
	if (ne_impl(v1->type, v2->type, v1, v2, vre)) return;
	operator_ne_slow(v1, v2, vre);
}

#define DEF_COMPO_CMP_OPERATOR(fn, field, impl, opname)                     \
static void fn##_slow(const tobj *v1, const tobj *v2, tobj *vre)            \
{                                                                            \
	if (v1->type == tcompo && v1->val.v_tcompo->vtable->field) { \
		v1->val.v_tcompo->vtable->field(v1->val.v_tcompo, v2, 0, vre); return; \
	} \
	if (v2->type == tcompo && v2->val.v_tcompo->vtable->field) { \
		v2->val.v_tcompo->vtable->field(v2->val.v_tcompo, v1, 1, vre); return; \
	} \
	twarn(ErrRuntime_ParamsType, opname, "unsupported comparison");            \
}                                                                            \
static void fn(const tobj *v1, const tobj *v2, tobj *vre)                    \
{                                                                            \
	if (impl(v1->type, v2->type, v1, v2, vre)) return;                     \
	fn##_slow(v1, v2, vre);                                                 \
}

DEF_COMPO_CMP_OPERATOR(operator_sg, op_sg, sg_impl, "operator_sg")
DEF_COMPO_CMP_OPERATOR(operator_sl, op_sl, sl_impl, "operator_sl")
DEF_COMPO_CMP_OPERATOR(operator_ge, op_ge, ge_impl, "operator_ge")
DEF_COMPO_CMP_OPERATOR(operator_le, op_le, le_impl, "operator_le")

void operator_and(const tobj *v1, const tobj *v2, tobj *vre)
{
	if (v1->type == tbool && v2->type == tbool)
		tobj_set_bool(vre, v1->val.v_tbool && v2->val.v_tbool);
	else if (v1->type == tcompo && v1->val.v_tcompo->vtable->op_and)
		v1->val.v_tcompo->vtable->op_and(v1->val.v_tcompo, v2, 0, vre);
	else if (v2->type == tcompo && v2->val.v_tcompo->vtable->op_and)
		v2->val.v_tcompo->vtable->op_and(v2->val.v_tcompo, v1, 1, vre);
	else twarn(ErrRuntime_ParamsType, "operator_and", "");
}

void operator_or(const tobj *v1, const tobj *v2, tobj *vre)
{
	if (v1->type == tbool && v2->type == tbool)
		tobj_set_bool(vre, v1->val.v_tbool || v2->val.v_tbool);
	else if (v1->type == tcompo && v1->val.v_tcompo->vtable->op_or)
		v1->val.v_tcompo->vtable->op_or(v1->val.v_tcompo, v2, 0, vre);
	else if (v2->type == tcompo && v2->val.v_tcompo->vtable->op_or)
		v2->val.v_tcompo->vtable->op_or(v2->val.v_tcompo, v1, 1, vre);
	else twarn(ErrRuntime_ParamsType, "operator_or", "");
}

#undef DEF_COMPO_CMP_OPERATOR
#undef DEF_CMP

/*===========================================================================*
 * 3. Virtual Machine
 *===========================================================================*/

static void gen_check_nparams(const char *fname, uint_regs len, uint_regs expected)
{
	if (expected != UNDEF_NPARAMS && len != expected)
		twarn(ErrRuntime_ParamsCtr, fname, "");
}

static void gen_print(tobj *params, uint_regs len, tobj *vre)
{
	uint_regs i;
	for (i = 0; i < len; i++) {
		tstring *s = tobj_tostring_abbr(&params[i]);
		printf("%s", tstring_cstr(s));
		tstring_free(s);
	}
	printf("\n");
	tobj_set_nil(vre);
}

static void gen_sprint(tobj *params, uint_regs len, tobj *vre)
{
	uint_regs i;
	for (i = 0; i < len; i++) {
		tstring *s = tobj_tostring_full(&params[i]);
		printf("%s", tstring_cstr(s));
		tstring_free(s);
	}
	printf("\n");
	tobj_set_nil(vre);
}

static void gen_clock(tobj *params, uint_regs len, tobj *vre)
{
	(void)params;
	gen_check_nparams("clock", len, 0);
	tobj_set_float(vre, (double)clock() / (double)CLOCKS_PER_SEC);
}

static void gen_clock_ns(tobj *params, uint_regs len, tobj *vre)
{
	struct timespec value;
	long seconds;

	(void)params;
	gen_check_nparams("clock_ns", len, 0);
	if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &value) != 0)
		twarn(ErrRuntime_Other, "clock_ns", "process CPU clock unavailable");
	seconds = (long)value.tv_sec;
	if (seconds > (LONG_MAX - value.tv_nsec) / 1000000000L)
		twarn(ErrRuntime_IntOutOfRange, "clock_ns", "");
	tobj_set_int(vre, seconds * 1000000000L + value.tv_nsec);
}

static void gen_int(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("int", len, 1);
	switch (params[0].type) {
	case tint:
		tobj_set_int(vre, params[0].val.v_tint);
		return;
	case tfloat:
		tobj_set_int(vre, (long)params[0].val.v_tfloat);
		return;
	case tbool:
		tobj_set_int(vre, params[0].val.v_tbool ? 1 : 0);
		return;
	case tcompo:
		if (tobj_compo_type(&params[0]) == compo_tstr) {
			tstr *s = (tstr *)params[0].val.v_tcompo;
			char *end = NULL;
			errno = 0;
			long v = strtol(tstring_cstr(s->data), &end, 10);
			if (errno == 0 && end && *end == '\0') {
				tobj_set_int(vre, v);
				return;
			}
		}
		break;
	case tnil:
		break;
	}
	twarn(ErrRuntime_ParamsType, "int", "");
}

static void gen_float(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("float", len, 1);
	switch (params[0].type) {
	case tint:
		tobj_set_float(vre, (double)params[0].val.v_tint);
		return;
	case tfloat:
		tobj_set_float(vre, params[0].val.v_tfloat);
		return;
	case tbool:
		tobj_set_float(vre, params[0].val.v_tbool ? 1.0 : 0.0);
		return;
	case tcompo:
		if (tobj_compo_type(&params[0]) == compo_tstr) {
			tstr *s = (tstr *)params[0].val.v_tcompo;
			char *end = NULL;
			errno = 0;
			double v = strtod(tstring_cstr(s->data), &end);
			if (errno == 0 && end && *end == '\0') {
				tobj_set_float(vre, v);
				return;
			}
		}
		break;
	case tnil:
		break;
	}
	twarn(ErrRuntime_ParamsType, "float", "");
}

static void gen_bool(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("bool", len, 1);
	switch (params[0].type) {
	case tnil:
		tobj_set_bool(vre, 0);
		return;
	case tbool:
		tobj_set_bool(vre, params[0].val.v_tbool);
		return;
	case tint:
		tobj_set_bool(vre, params[0].val.v_tint != 0);
		return;
	case tfloat:
		tobj_set_bool(vre, params[0].val.v_tfloat != 0.0);
		return;
	case tcompo:
		if (tobj_compo_type(&params[0]) == compo_tstr) {
			tstr *s = (tstr *)params[0].val.v_tcompo;
			const char *v = tstring_cstr(s->data);
			if (strcmp(v, "true") == 0) {
				tobj_set_bool(vre, 1);
				return;
			}
			if (strcmp(v, "false") == 0) {
				tobj_set_bool(vre, 0);
				return;
			}
		}
		tobj_set_bool(vre, params[0].val.v_tcompo->vtable->len(
				      params[0].val.v_tcompo) != 0);
		return;
	}
	twarn(ErrRuntime_ParamsType, "bool", "");
}

static void gen_str(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("str", len, 1);
	tstring *s = tobj_tostring_full(&params[0]);
	tobj_set_compo(vre, (tcompo_v *)tstr_new(tstring_cstr(s)));
	tstring_free(s);
}

static void gen_list(tobj *params, uint_regs len, tobj *vre)
{
	tlist *l = tlist_new();
	uint_regs i;
	for (i = 0; i < len; i++)
		tlist_push(l, &params[i]);
	tobj_set_compo(vre, (tcompo_v *)l);
}

static void gen_len(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("len", len, 1);
	if (params[0].type == tnil) {
		tobj_set_int(vre, 0);
		return;
	}
	if (params[0].type != tcompo || !params[0].val.v_tcompo) {
		tobj_set_int(vre, 1);
		return;
	}
	tobj_set_int(vre, params[0].val.v_tcompo->vtable->len(params[0].val.v_tcompo));
}

static void gen_type(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("type", len, 1);
	const char *name = "nil";
	switch (params[0].type) {
	case tnil:
		name = "nil";
		break;
	case tbool:
		name = "bool";
		break;
	case tint:
		name = "int";
		break;
	case tfloat:
		name = "float";
		break;
	case tcompo:
		name = params[0].val.v_tcompo->vtable->get_type();
		break;
	}
	tobj_set_compo(vre, (tcompo_v *)tstr_new(name));
}

static void gen_push(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("push", len, 2);
	if (params[0].type != tcompo ||
	    tobj_compo_type(&params[0]) != compo_tlist)
		twarn(ErrRuntime_ParamsType, "push", "");
	tlist_push((tlist *)params[0].val.v_tcompo, &params[1]);
	tobj_set_nil(vre);
}

static void gen_pop(tobj *params, uint_regs len, tobj *vre)
{
	long idx;
	if (len != 1 && len != 2)
		twarn(ErrRuntime_ParamsCtr, "pop", "");
	if (params[0].type != tcompo ||
	    tobj_compo_type(&params[0]) != compo_tlist)
		twarn(ErrRuntime_ParamsType, "pop", "");
	tlist *l = (tlist *)params[0].val.v_tcompo;
	idx = (long)tlist_size(l) - 1;
	if (len == 2) {
		if (params[1].type != tint)
			twarn(ErrRuntime_ParamsType, "pop", "");
		idx = params[1].val.v_tint;
	}
	if (idx < 0)
		idx += (long)tlist_size(l);
	tlist_pop(l, (uint_objs)idx);
	tobj_set_nil(vre);
}

static void gen_idx(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("idx", len, 2);
	if (params[0].type != tcompo)
		twarn(ErrRuntime_ParamsType, "idx", "");
	tcompo_v *v = params[0].val.v_tcompo;
	switch (v->vtable->get_compo_type_code()) {
	case compo_tstr:
		tstr_idx((tstr *)v, &params[1], 1, vre);
		return;
	case compo_tlist:
		tlist_idx((tlist *)v, &params[1], 1, vre);
		return;
	case compo_tpair:
		tpair_idx((tpair *)v, &params[1], 1, vre);
		return;
	case compo_tdict:
		tdict_idx((tdict *)v, &params[1], 1, vre);
		return;
	case compo_ttypeval:
		ttypeval_idx((ttypeval *)v, &params[1], 1, vre);
		return;
	case compo_tdarr:
	case compo_tbarr:
		tarr_idx(v, &params[1], 1, vre);
		return;
	default:
		break;
	}
	twarn(ErrRuntime_RefType, "idx", "");
}

static void gen_keys(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("keys", len, 1);
	if (params[0].type != tcompo)
		twarn(ErrRuntime_ParamsType, "keys", "");
	if (tobj_compo_type(&params[0]) == compo_tdict)
		tobj_set_compo(
			vre,
			(tcompo_v *)tdict_keys((tdict *)params[0].val.v_tcompo));
	else if (tobj_compo_type(&params[0]) == compo_ttypeval) {
		tlist *keys = tlist_new();
		long position = 0;
		tobj key;
		tobj_set_nil(&key);
		while (ttypeval_next_key(
			(ttypeval *)params[0].val.v_tcompo, &position, &key)) {
			tlist_push(keys, &key);
			tobj_ddc_ref_clear(&key);
		}
		tobj_set_compo(vre, (tcompo_v *)keys);
	} else
		twarn(ErrRuntime_ParamsType, "keys", "Dictionary or Type required");
}

static void gen_pair(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("pair", len, 2);
	tobj_set_compo(vre, (tcompo_v *)tpair_new(&params[0], &params[1]));
}

static int sort_compare_tobj(const void *a, const void *b)
{
	const tobj *va = (const tobj *)a;
	const tobj *vb = (const tobj *)b;
	if (va->type == tint && vb->type == tint)
		return (va->val.v_tint > vb->val.v_tint) -
		       (va->val.v_tint < vb->val.v_tint);
	if ((va->type == tint || va->type == tfloat) &&
	    (vb->type == tint || vb->type == tfloat)) {
		double da = va->type == tint ? (double)va->val.v_tint : va->val.v_tfloat;
		double db = vb->type == tint ? (double)vb->val.v_tint : vb->val.v_tfloat;
		return (da > db) - (da < db);
	}
	return (int)va->type - (int)vb->type;
}

static void gen_sort(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("sort", len, 1);
	if (params[0].type != tcompo ||
	    tobj_compo_type(&params[0]) != compo_tlist)
		twarn(ErrRuntime_ParamsType, "sort", "");
	tlist *l = (tlist *)params[0].val.v_tcompo;
	tlist_sort(l, sort_compare_tobj);
	tobj_set_nil(vre);
}

static void gen_copy(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("copy", len, 1);
	if (params[0].type == tcompo && params[0].val.v_tcompo) {
		tobj_set_compo(
			vre, (tcompo_v *)params[0].val.v_tcompo->vtable->copy(
				     params[0].val.v_tcompo));
		return;
	}
	tobj_copy(vre, &params[0]);
}

static void gen_union(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("union", len, 2);
	if (params[0].type != tcompo || params[1].type != tcompo ||
	    tobj_compo_type(&params[0]) != compo_tlist ||
	    tobj_compo_type(&params[1]) != compo_tlist)
		twarn(ErrRuntime_ParamsType, "union", "");

	tlist *left = (tlist *)params[0].val.v_tcompo;
	tlist *right = (tlist *)params[1].val.v_tcompo;
	tlist *out = tlist_new();
	for (uint_objs i = 0; i < tlist_size(left); i++)
		tlist_push(out, tlist_at(left, i));
	for (uint_objs i = 0; i < tlist_size(right); i++)
		tlist_push(out, tlist_at(right, i));
	tobj_set_compo(vre, (tcompo_v *)out);
}

static void gen_identical(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("identical", len, 2);
	tobj_set_bool(vre, tobj_identical(&params[0], &params[1]));
}

static void gen_iter(tobj *params, uint_regs len, tobj *vre)
{
	long start, step, end;
	if (len != 2 && len != 3)
		twarn(ErrRuntime_ParamsCtr, "iter", "");
	if (params[0].type != tint || params[len - 1].type != tint)
		twarn(ErrRuntime_ParamsType, "iter", "");
	start = params[0].val.v_tint;
	end = params[len - 1].val.v_tint;
	step = (end >= start) ? 1 : -1;
	if (len == 3) {
		if (params[1].type != tint)
			twarn(ErrRuntime_ParamsType, "iter", "");
		step = params[1].val.v_tint;
	}
	tobj_set_compo(vre, (tcompo_v *)titer_new_step(start, step, end));
}

static size_t array_dimension(const tobj *v, const char *where)
{
	if (v->type != tint || v->val.v_tint < 0)
		twarn(ErrRuntime_ParamsType, where, "array dimensions must be non-negative integers");
	return (size_t)v->val.v_tint;
}

static void gen_array(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("array", len, 3);
	size_t rows = array_dimension(&params[0], "array");
	size_t cols = array_dimension(&params[1], "array");
	const tobj *value = &params[2];
	if (value->type == tbool)
		tobj_set_compo(vre, (tcompo_v *)tbarr_new(rows, cols, value->val.v_tbool));
	else if (value->type == tint || value->type == tfloat)
		tobj_set_compo(vre, (tcompo_v *)tdarr_new(rows, cols,
			value->type == tint ? (double)value->val.v_tint : value->val.v_tfloat));
	else if (value->type == tcompo && tobj_compo_type(value) == compo_tlist) {
		tlist *list = (tlist *)value->val.v_tcompo;
		const tobj *first = tlist_size(list) ? tlist_at(list, 0) : NULL;
		if (first && first->type == tbool)
			tobj_set_compo(vre, (tcompo_v *)tbarr_from_list(rows, cols, list));
		else
			tobj_set_compo(vre, (tcompo_v *)tdarr_from_list(rows, cols, list));
	} else
		twarn(ErrRuntime_ParamsType, "array", "value must be scalar or list");
}

static tcompo_v *array_param(tobj *params, uint_regs len, const char *where)
{
	gen_check_nparams(where, len, 1);
	if (params[0].type != tcompo ||
	    (tobj_compo_type(&params[0]) != compo_tdarr &&
	     tobj_compo_type(&params[0]) != compo_tbarr))
		twarn(ErrRuntime_ParamsType, where, "array required");
	return params[0].val.v_tcompo;
}

static void gen_array_rows(tobj *p, uint_regs n, tobj *r)
{
	tobj_set_int(r, (long)tarr_rows(array_param(p, n, "array::rows")));
}

static void gen_array_cols(tobj *p, uint_regs n, tobj *r)
{
	tobj_set_int(r, (long)tarr_cols(array_param(p, n, "array::cols")));
}

static void gen_array_transpose(tobj *p, uint_regs n, tobj *r)
{
	tcompo_v *arr = array_param(p, n, "array::transpose");
	if (arr->vtable->get_compo_type_code() == compo_tdarr)
		tobj_set_compo(r, (tcompo_v *)tdarr_transpose((tdarr *)arr));
	else
		tobj_set_compo(r, (tcompo_v *)tbarr_transpose((tbarr *)arr));
}

static void gen_now(tobj *params, uint_regs len, tobj *vre)
{
	(void)params;
	gen_check_nparams("now", len, 0);
	tobj_set_compo(vre, (tcompo_v *)ttime_new());
}

static ttime *time_param(const tobj *value, const char *where)
{
	if (value->type != tcompo || tobj_compo_type(value) != compo_time)
		twarn(ErrRuntime_ParamsType, where, "time value required");
	return (ttime *)value->val.v_tcompo;
}

static void gen_time_from_unix(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("time::from_unix", len, 1);
	if (params[0].type != tint)
		twarn(ErrRuntime_ParamsType, "time::from_unix",
		      "integer Unix seconds required");
	tobj_set_compo(vre,
		      (tcompo_v *)ttime_from_unix(params[0].val.v_tint));
}

static void gen_time_unix(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("time::unix", len, 1);
	tobj_set_int(vre, ttime_unix(time_param(&params[0], "time::unix")));
}

static void gen_time_format(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("time::format", len, 2);
	ttime *value = time_param(&params[0], "time::format");
	if (params[1].type != tcompo ||
	    tobj_compo_type(&params[1]) != compo_tstr)
		twarn(ErrRuntime_ParamsType, "time::format",
		      "format string required");
	tstr *pattern = (tstr *)params[1].val.v_tcompo;
	tstring *formatted = ttime_format(value, tstring_cstr(pattern->data));
	tobj_set_compo(vre,
		      (tcompo_v *)tstr_new(tstring_cstr(formatted)));
	tstring_free(formatted);
}

static void gen_append(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("append", len, 2);
	if (params[0].type != tcompo)
		twarn(ErrRuntime_ParamsType, "append", "");
	switch (tobj_compo_type(&params[0])) {
	case compo_tlist:
		tlist_push((tlist *)params[0].val.v_tcompo, &params[1]);
		break;
	case compo_tdict:
		tdict_set_append((tdict *)params[0].val.v_tcompo, &params[1]);
		break;
	case compo_tstr: {
		if (params[1].type == tcompo &&
		    tobj_compo_type(&params[1]) == compo_tstr) {
			tstr *s = (tstr *)params[1].val.v_tcompo;
			tstring_append_ts(((tstr *)params[0].val.v_tcompo)->data, s->data);
		} else {
			tstring *s = tobj_tostring_full(&params[1]);
			tstring_append_ts(((tstr *)params[0].val.v_tcompo)->data, s);
			tstring_free(s);
		}
		break;
	}
	default:
		twarn(ErrRuntime_ParamsType, "append", "");
	}
	tobj_set_nil(vre);
}

static void gen_insert(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("insert", len, 3);
	if (params[0].type != tcompo || params[2].type != tint)
		twarn(ErrRuntime_ParamsType, "insert", "");
	if (tobj_compo_type(&params[0]) != compo_tlist)
		twarn(ErrRuntime_ParamsType, "insert", "only list insert is implemented");
	tlist *l = (tlist *)params[0].val.v_tcompo;
	long idx = params[2].val.v_tint;
	if (idx < 0)
		idx += (long)tlist_size(l);
	if (idx < 0 || (uint_objs)idx > tlist_size(l))
		twarn(ErrRuntime_IdxOutRange, "insert", "");
	tlist_insert(l, (uint_objs)idx, &params[1]);
	tobj_set_nil(vre);
}

static void gen_delete(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("delete", len, 2);
	if (params[0].type != tcompo)
		twarn(ErrRuntime_ParamsType, "delete", "");
	switch (tobj_compo_type(&params[0])) {
	case compo_tlist:
		if (params[1].type != tint)
			twarn(ErrRuntime_ParamsType, "delete", "");
		tlist_pop((tlist *)params[0].val.v_tcompo, (uint_objs)params[1].val.v_tint);
		break;
	case compo_tdict: {
		tdict *d = (tdict *)params[0].val.v_tcompo;
		if (!tdict_delete(d, &params[1]))
			twarn(ErrRuntime_ObjUnfound, "delete", "");
	} break;
	default:
		twarn(ErrRuntime_ParamsType, "delete", "");
	}
	tobj_set_nil(vre);
}

static void gen_dvalues(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("dvalues", len, 1);
	if (params[0].type != tcompo || tobj_compo_type(&params[0]) != compo_tdict)
		twarn(ErrRuntime_ParamsType, "dvalues", "");
	tdict *d = (tdict *)params[0].val.v_tcompo;
	tobj_set_compo(vre, (tcompo_v *)tdict_values(d));
}

static double math_arg_double(const tobj *v, const char *fname)
{
	if (v->type == tint)
		return (double)v->val.v_tint;
	if (v->type == tfloat)
		return v->val.v_tfloat;
	twarn(ErrRuntime_ParamsType, fname, "");
	return 0.0;
}

static long math_arg_long(const tobj *v, const char *fname)
{
	if (v->type == tint)
		return v->val.v_tint;
	if (v->type == tfloat)
		return (long)v->val.v_tfloat;
	twarn(ErrRuntime_ParamsType, fname, "");
	return 0;
}

#define DEF_MATH_UNARY_FLOAT(name, fn)                                            \
	static void gen_math_##name(tobj *params, uint_regs len, tobj *vre)       \
	{                                                                         \
		gen_check_nparams("math::" #name, len, 1);                        \
		tobj_set_float(vre, fn(math_arg_double(&params[0], "math::" #name))); \
	}

#define DEF_MATH_UNARY_INT(name, fn)                                             \
	static void gen_math_##name(tobj *params, uint_regs len, tobj *vre)      \
	{                                                                        \
		gen_check_nparams("math::" #name, len, 1);                       \
		tobj_set_int(vre, (long)fn(math_arg_double(&params[0], "math::" #name))); \
	}

#define DEF_MATH_BINARY_FLOAT(name, fn)                                           \
	static void gen_math_##name(tobj *params, uint_regs len, tobj *vre)       \
	{                                                                         \
		gen_check_nparams("math::" #name, len, 2);                        \
		tobj_set_float(vre, fn(math_arg_double(&params[0], "math::" #name), \
				       math_arg_double(&params[1], "math::" #name))); \
	}

#define DEF_MATH_BINARY_INT(name, fn)                                             \
	static void gen_math_##name(tobj *params, uint_regs len, tobj *vre)       \
	{                                                                         \
		gen_check_nparams("math::" #name, len, 2);                        \
		tobj_set_bool(vre, fn(math_arg_double(&params[0], "math::" #name), \
				      math_arg_double(&params[1], "math::" #name)));  \
	}

DEF_MATH_UNARY_FLOAT(fabs, fabs)
DEF_MATH_UNARY_FLOAT(sqrt, sqrt)
DEF_MATH_UNARY_FLOAT(cbrt, cbrt)
DEF_MATH_BINARY_FLOAT(pow, pow)
DEF_MATH_BINARY_FLOAT(hypot, hypot)
DEF_MATH_UNARY_FLOAT(sin, sin)
DEF_MATH_UNARY_FLOAT(cos, cos)
DEF_MATH_UNARY_FLOAT(tan, tan)
DEF_MATH_UNARY_FLOAT(asin, asin)
DEF_MATH_UNARY_FLOAT(acos, acos)
DEF_MATH_UNARY_FLOAT(atan, atan)
DEF_MATH_BINARY_FLOAT(atan2, atan2)
DEF_MATH_UNARY_FLOAT(sinh, sinh)
DEF_MATH_UNARY_FLOAT(cosh, cosh)
DEF_MATH_UNARY_FLOAT(tanh, tanh)
DEF_MATH_UNARY_FLOAT(asinh, asinh)
DEF_MATH_UNARY_FLOAT(acosh, acosh)
DEF_MATH_UNARY_FLOAT(atanh, atanh)
DEF_MATH_UNARY_FLOAT(exp, exp)
DEF_MATH_UNARY_FLOAT(exp2, exp2)
DEF_MATH_UNARY_FLOAT(expm1, expm1)
DEF_MATH_UNARY_FLOAT(log, log)
DEF_MATH_UNARY_FLOAT(log2, log2)
DEF_MATH_UNARY_FLOAT(log10, log10)
DEF_MATH_UNARY_FLOAT(log1p, log1p)
DEF_MATH_UNARY_FLOAT(logb, logb)
DEF_MATH_UNARY_INT(ilogb, ilogb)
DEF_MATH_UNARY_FLOAT(erf, erf)
DEF_MATH_UNARY_FLOAT(erfc, erfc)
DEF_MATH_UNARY_FLOAT(lgamma, lgamma)
DEF_MATH_UNARY_FLOAT(tgamma, tgamma)
DEF_MATH_UNARY_FLOAT(ceil, ceil)
DEF_MATH_UNARY_FLOAT(floor, floor)
DEF_MATH_UNARY_FLOAT(nearbyint, nearbyint)
DEF_MATH_UNARY_FLOAT(rint, rint)
DEF_MATH_UNARY_INT(lrint, lrint)
DEF_MATH_UNARY_INT(llrint, llrint)
DEF_MATH_UNARY_FLOAT(round, round)
DEF_MATH_UNARY_INT(lround, lround)
DEF_MATH_UNARY_INT(llround, llround)
DEF_MATH_UNARY_FLOAT(trunc, trunc)
DEF_MATH_BINARY_FLOAT(fmod, fmod)
DEF_MATH_BINARY_FLOAT(remainder, remainder)
DEF_MATH_BINARY_FLOAT(copysign, copysign)
DEF_MATH_BINARY_FLOAT(nextafter, nextafter)
DEF_MATH_BINARY_FLOAT(fdim, fdim)
DEF_MATH_BINARY_FLOAT(fmax, fmax)
DEF_MATH_BINARY_FLOAT(fmin, fmin)
DEF_MATH_BINARY_INT(isgreater, isgreater)
DEF_MATH_BINARY_INT(isgreaterequal, isgreaterequal)
DEF_MATH_BINARY_INT(isless, isless)
DEF_MATH_BINARY_INT(islessequal, islessequal)
DEF_MATH_BINARY_INT(islessgreater, islessgreater)
DEF_MATH_BINARY_INT(isunordered, isunordered)

static void gen_math_abs(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("math::abs", len, 1);
	if (params[0].type == tint)
		tobj_set_int(vre, labs(params[0].val.v_tint));
	else
		tobj_set_float(vre, fabs(math_arg_double(&params[0], "math::abs")));
}

static void gen_math_rsqrt(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("math::rsqrt", len, 1);
	tobj_set_float(vre, 1.0 / sqrt(math_arg_double(&params[0], "math::rsqrt")));
}

static void gen_math_fma(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("math::fma", len, 3);
	tobj_set_float(vre, fma(math_arg_double(&params[0], "math::fma"),
				math_arg_double(&params[1], "math::fma"),
				math_arg_double(&params[2], "math::fma")));
}

static void gen_math_ldexp(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("math::ldexp", len, 2);
	tobj_set_float(vre, ldexp(math_arg_double(&params[0], "math::ldexp"),
				  (int)math_arg_long(&params[1], "math::ldexp")));
}

static void gen_math_scalbn(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("math::scalbn", len, 2);
	tobj_set_float(vre, scalbn(math_arg_double(&params[0], "math::scalbn"),
				   (int)math_arg_long(&params[1], "math::scalbn")));
}

static void gen_math_scalbln(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("math::scalbln", len, 2);
	tobj_set_float(vre, scalbln(math_arg_double(&params[0], "math::scalbln"),
				    math_arg_long(&params[1], "math::scalbln")));
}

static void gen_math_eleinv(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("math::eleinv", len, 1);
	tobj_set_float(vre, 1.0 / math_arg_double(&params[0], "math::eleinv"));
}

static void gen_math_make_nan(tobj *params, uint_regs len, tobj *vre)
{
	(void)params;
	gen_check_nparams("math::make_nan", len, 0);
	tobj_set_float(vre, NAN);
}

static void gen_math_frexp(tobj *params, uint_regs len, tobj *vre)
{
	int expv = 0;
	double mant;
	tobj a, b;
	gen_check_nparams("math::frexp", len, 1);
	mant = frexp(math_arg_double(&params[0], "math::frexp"), &expv);
	tobj_set_nil(&a);
	tobj_set_nil(&b);
	tobj_set_float(&a, mant);
	tobj_set_int(&b, expv);
	tobj_set_compo(vre, (tcompo_v *)tpair_new(&a, &b));
}

static void gen_math_modf(tobj *params, uint_regs len, tobj *vre)
{
	double intpart = 0.0;
	double frac;
	tobj a, b;
	gen_check_nparams("math::modf", len, 1);
	frac = modf(math_arg_double(&params[0], "math::modf"), &intpart);
	tobj_set_nil(&a);
	tobj_set_nil(&b);
	tobj_set_float(&a, frac);
	tobj_set_float(&b, intpart);
	tobj_set_compo(vre, (tcompo_v *)tpair_new(&a, &b));
}

static void gen_math_remquo(tobj *params, uint_regs len, tobj *vre)
{
	int quo = 0;
	double rem;
	tobj a, b;
	gen_check_nparams("math::remquo", len, 2);
	rem = remquo(math_arg_double(&params[0], "math::remquo"),
		     math_arg_double(&params[1], "math::remquo"),
		     &quo);
	tobj_set_nil(&a);
	tobj_set_nil(&b);
	tobj_set_float(&a, rem);
	tobj_set_int(&b, quo);
	tobj_set_compo(vre, (tcompo_v *)tpair_new(&a, &b));
}

static void gen_math_isfinite(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("math::isfinite", len, 1);
	tobj_set_bool(vre, isfinite(math_arg_double(&params[0], "math::isfinite")));
}

static void gen_math_isinf(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("math::isinf", len, 1);
	tobj_set_bool(vre, isinf(math_arg_double(&params[0], "math::isinf")));
}

static void gen_math_isnan(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("math::isnan", len, 1);
	tobj_set_bool(vre, isnan(math_arg_double(&params[0], "math::isnan")));
}

static void gen_math_isnormal(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("math::isnormal", len, 1);
	tobj_set_bool(vre, isnormal(math_arg_double(&params[0], "math::isnormal")));
}

static void gen_math_fpclassify(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("math::fpclassify", len, 1);
	tobj_set_int(vre, fpclassify(math_arg_double(&params[0], "math::fpclassify")));
}

static void gen_math_signbit(tobj *params, uint_regs len, tobj *vre)
{
	gen_check_nparams("math::signbit", len, 1);
	tobj_set_bool(vre, signbit(math_arg_double(&params[0], "math::signbit")));
}

static void require_count(const char *name, uint_regs actual, uint_regs expected)
{
	if (actual != expected)
		twarn(ErrRuntime_ParamsCtr, name, "incorrect parameter count");
}

static ttypeval *require_type(const tobj *value, const char *name)
{
	if (!value || value->type != tcompo ||
	    tobj_compo_type(value) != compo_ttypeval)
		twarn(ErrRuntime_ParamsType, name, "Type value required");
	return (ttypeval *)value->val.v_tcompo;
}

static void return_type(tobj *result, ttypeval *type)
{
	tobj_set_compo(result, (tcompo_v *)type);
}

static void gen_types_make_type(tobj *params, uint_regs len, tobj *result)
{
	if (len == 0)
		twarn(ErrRuntime_ParamsCtr, "types::make_type",
		      "at least one field is required");
	ttype_field *fields = (ttype_field *)calloc(len, sizeof(ttype_field));
	if (!fields)
		twarn(ErrRuntime_Other, "types::make_type", "out of memory");
	for (uint_regs i = 0; i < len; i++) {
		if (params[i].type != tcompo ||
		    tobj_compo_type(&params[i]) != compo_tpair)
			twarn(ErrRuntime_ParamsType, "types::make_type",
			      "field Pair required");
		tpair *pair = (tpair *)params[i].val.v_tcompo;
		if (pair->first.type != tcompo ||
		    tobj_compo_type(&pair->first) != compo_tstr)
			twarn(ErrRuntime_ParamsType, "types::make_type",
			      "field name must be String");
		fields[i].name = ((tstr *)pair->first.val.v_tcompo)->data;
		fields[i].type = require_type(&pair->second, "types::make_type");
	}
	ttypeval *type = ttypeval_new_fields(fields, len);
	free(fields);
	return_type(result, type);
}

static void gen_types_union(tobj *params, uint_regs len, tobj *result)
{
	if (len < 2)
		twarn(ErrRuntime_ParamsCtr, "types::union",
		      "at least two Type values are required");
	ttypeval **members = (ttypeval **)calloc(len, sizeof(ttypeval *));
	if (!members)
		twarn(ErrRuntime_Other, "types::union", "out of memory");
	for (uint_regs i = 0; i < len; i++)
		members[i] = require_type(&params[i], "types::union");
	ttypeval *type = ttypeval_new_union(members, len);
	free(members);
	return_type(result, type);
}

static void gen_types_list(tobj *params, uint_regs len, tobj *result)
{
	require_count("types::list", len, 1);
	return_type(result,
		    ttypeval_new_list(require_type(&params[0], "types::list")));
}

static void gen_types_pair(tobj *params, uint_regs len, tobj *result)
{
	require_count("types::pair", len, 2);
	return_type(result,
		    ttypeval_new_pair(require_type(&params[0], "types::pair"),
				      require_type(&params[1], "types::pair")));
}

static void gen_types_dictionary(tobj *params, uint_regs len, tobj *result)
{
	require_count("types::dictionary", len, 2);
	return_type(result,
		    ttypeval_new_dictionary(
			    require_type(&params[0], "types::dictionary"),
			    require_type(&params[1], "types::dictionary")));
}

static ttypeval *type_of_value(const tobj *value)
{
	switch (value->type) {
	case tnil:
		return ttypeval_builtin(ttype_builtin_nil);
	case tbool:
		return ttypeval_builtin(ttype_builtin_bool);
	case tint:
		return ttypeval_builtin(ttype_builtin_int);
	case tfloat:
		return ttypeval_builtin(ttype_builtin_float);
	case tcompo:
		break;
	}
	switch (tobj_compo_type(value)) {
	case compo_tstr:
		return ttypeval_builtin(ttype_builtin_string);
	case compo_tlist:
		return ttypeval_builtin(ttype_builtin_list);
	case compo_tpair:
		return ttypeval_builtin(ttype_builtin_pair);
	case compo_tdict:
		return ttypeval_builtin(ttype_builtin_dictionary);
	case compo_titer:
		return ttypeval_builtin(ttype_builtin_iterator);
	case compo_tfunc:
	case compo_cppfunc:
	case compo_sessfunc:
		return ttypeval_builtin(ttype_builtin_function);
	case compo_tlib:
		return ttypeval_builtin(ttype_builtin_library);
	case compo_tdarr:
		return ttypeval_builtin(ttype_builtin_real_array);
	case compo_tbarr:
		return ttypeval_builtin(ttype_builtin_bool_array);
	case compo_time:
		return ttypeval_builtin(ttype_builtin_time);
	case compo_ttypeval:
		return ttypeval_builtin(ttype_builtin_type);
	default:
		twarn(ErrRuntime_ParamsType, "types::of", "unsupported value type");
	}
	return NULL;
}

static void gen_types_of(tobj *params, uint_regs len, tobj *result)
{
	require_count("types::of", len, 1);
	return_type(result, type_of_value(&params[0]));
}

static void gen_types_matches(tobj *params, uint_regs len, tobj *result)
{
	require_count("types::matches", len, 2);
	tobj_set_bool(result,
		      ttypeval_matches(&params[0],
				       require_type(&params[1], "types::matches")));
}

static void copy_definition_entry(const tobj *key, const tobj *value,
				  void *context)
{
	tdict_set((tdict *)context, key, value);
}

static tdict *definition_copy(const ttypeval *type)
{
	tdict *copy = tdict_new();
	thashtbl_each(ttypeval_definition(type), copy_definition_entry, copy);
	return copy;
}

static void gen_types_fields(tobj *params, uint_regs len, tobj *result)
{
	require_count("types::fields", len, 1);
	ttypeval *type = require_type(&params[0], "types::fields");
	tdict *fields = tdict_new();
	for (uint_objs i = 0; i < ttypeval_field_count(type); i++) {
		const tobj *name;
		ttypeval *field;
		ttypeval_field_at(type, i, &name, &field);
		tobj value;
		tobj_set_nil(&value);
		tobj_set_compo(&value, (tcompo_v *)field);
		tdict_set(fields, name, &value);
	}
	tobj_set_compo(result, (tcompo_v *)fields);
}

static void gen_types_members(tobj *params, uint_regs len, tobj *result)
{
	require_count("types::members", len, 1);
	ttypeval *type = require_type(&params[0], "types::members");
	tlist *members = tlist_new();
	uint_objs count = ttypeval_member_count(type);
	if (count == 0) {
		tlist_push(members, &params[0]);
	} else {
		for (uint_objs i = 0; i < count; i++) {
			tobj value;
			tobj_set_nil(&value);
			tobj_set_compo(
				&value, (tcompo_v *)ttypeval_member_at(type, i));
			tlist_push(members, &value);
		}
	}
	tobj_set_compo(result, (tcompo_v *)members);
}

static void gen_types_base(tobj *params, uint_regs len, tobj *result)
{
	require_count("types::base", len, 1);
	return_type(result,
		    ttypeval_base(require_type(&params[0], "types::base")));
}

static void set_parameter(tdict *parameters, const char *name, ttypeval *type)
{
	if (!type)
		return;
	tobj key;
	tobj value;
	tobj_set_nil(&key);
	tobj_set_nil(&value);
	tobj_set_compo(&key, (tcompo_v *)tstr_new(name));
	tobj_set_compo(&value, (tcompo_v *)type);
	tdict_set(parameters, &key, &value);
	tobj_try_clear(&key);
}

static void gen_types_parameters(tobj *params, uint_regs len, tobj *result)
{
	require_count("types::parameters", len, 1);
	ttypeval *type = require_type(&params[0], "types::parameters");
	tdict *parameters = tdict_new();
	if (type->kind == ttype_kind_list)
		set_parameter(parameters, "item", ttypeval_parameter(type, "item"));
	else if (type->kind == ttype_kind_pair) {
		set_parameter(parameters, "first", ttypeval_parameter(type, "first"));
		set_parameter(parameters, "second", ttypeval_parameter(type, "second"));
	} else if (type->kind == ttype_kind_dictionary) {
		set_parameter(parameters, "key", ttypeval_parameter(type, "key"));
		set_parameter(parameters, "value", ttypeval_parameter(type, "value"));
	}
	tobj_set_compo(result, (tcompo_v *)parameters);
}

static void gen_types_definition(tobj *params, uint_regs len, tobj *result)
{
	require_count("types::definition", len, 1);
	tobj_set_compo(
		result,
		(tcompo_v *)definition_copy(
			require_type(&params[0], "types::definition")));
}

static void tdict_add_cppf(tdict *pkg, const char *name, genf_t f, uint_regs nparams_sig)
{
	tobj key, val;
	tobj_set_nil(&key);
	tobj_set_nil(&val);
	tobj_set_compo(&key, (tcompo_v *)tstr_new(name));
	tobj_set_compo(&val, (tcompo_v *)tcppgenf_new(f, name, nparams_sig));
	tdict_set(pkg, &key, &val);
	tobj_try_clear(&key);
	tobj_try_clear(&val);
}

#define ADD_MATH(pkg, name, nparams) \
	tdict_add_cppf((pkg), #name, gen_math_##name, (nparams))

static void tvm_drop_call_frames(tvm *vm);

typedef enum {
	tins_cache_empty,
	tins_cache_loop_iter,
	tins_cache_loop_list,
	tins_cache_loop_type,
	tins_cache_idxr_list_int,
	tins_cache_idxr_str_int,
	tins_cache_idxr_dense_int2,
	tins_cache_idxl_list_int,
	tins_cache_idxl_dense_int2,
	tins_cache_eval_tfunc,
	tins_cache_polymorphic
} tins_cache_kind;

typedef struct {
	tcompo_vtable *guard;
	uint_cmds state_slot;
	uint8_t kind;
} tins_cache;

struct tvm_code_cache {
	const twrapper *wrapper;
	tins_cache *entries;
	uint_cmds entry_count;
};

static tvm_code_cache *tvm_get_code_cache(tvm *vm, const twrapper *wrapper)
{
	for (uint32_t i = 0; i < vm->code_cache_len; i++) {
		if (vm->code_caches[i].wrapper == wrapper)
			return &vm->code_caches[i];
	}

	if (vm->code_cache_len >= vm->code_cache_cap) {
		uint32_t newcap = vm->code_cache_cap ? vm->code_cache_cap * 2 : 4;
		tvm_code_cache *grown = (tvm_code_cache *)realloc(
			vm->code_caches, newcap * sizeof(tvm_code_cache));
		if (!grown)
			twarn(ErrRuntime_Other, "tvm_get_code_cache", "out of memory");
		vm->code_caches = grown;
		vm->code_cache_cap = newcap;
	}

	tvm_code_cache *cache = &vm->code_caches[vm->code_cache_len++];
	cache->wrapper = wrapper;
	cache->entry_count = wrapper->ncmds;
	cache->entries = NULL;
	if (wrapper->ncmds == 0)
		return cache;

	cache->entries = (tins_cache *)calloc(
		cache->entry_count, sizeof(tins_cache));
	if (!cache->entries)
		twarn(ErrRuntime_Other, "tvm_get_code_cache", "out of memory");
	uint_cmds loop_slot = 0;
	for (uint_cmds i = 0; i < wrapper->ncmds; i++) {
		tins instruction = tbycode_ins(wrapper->cmdarr[i]);
		if (instruction == OP_LOOPAS)
			cache->entries[i].state_slot = loop_slot++;
	}
	return cache;
}

static inline tins_cache *tvm_instruction_cache(
		tvm_code_cache *cache, uint_cmds instruction)
{
	if (!cache || instruction >= cache->entry_count)
		return NULL;
	return &cache->entries[instruction];
}

void tvm_init(tvm *vm, uint_objs tmpmax)
{
	tobj_array_init(&vm->tmps, tmpmax);
	vm->regmax = 0;
	vm->stk = NULL;
	vm->stklen = 0;
	tobj_set_nil(&vm->rev);
	vm->loop_states = NULL;
	vm->loop_state_len = 0;
	vm->loop_state_cap = 0;
	vm->code_caches = NULL;
	vm->code_cache_len = 0;
	vm->code_cache_cap = 0;
	vm->frames = NULL;
	vm->frame_len = 0;
	vm->frame_cap = 0;
	vm->error_source_locs = NULL;
	vm->error_source_loc_count = 0;
	vm->error_instruction = NULL;
	vm->execution_depth = 0;
}

void tvm_set_tmpmax(tvm *vm, uint_objs m)
{
	tobj_array_try_expand(&vm->tmps, m);
}

void tvm_set_vmstack(tvm *vm, tobj *s, uint_regs n)
{
	vm->stk = s;
	vm->regmax = n;
	vm->stklen = 0;
}

tobj *tvm_get_vre(tvm *vm)
{
	return &vm->rev;
}

void tvm_set_rev_empty(tvm *vm)
{
	tobj_set_nil(&vm->rev);
}

void tvm_clean(tvm *vm)
{
	tobj_ddc_ref_clear(&vm->rev);
	tvm_drop_call_frames(vm);
	uint_regs i;
	for (i = 0; i < vm->stklen; i++)
		tobj_ddc_ref_clear(&vm->stk[i]);
	vm->stklen = 0;
	free(vm->loop_states);
	vm->loop_states = NULL;
	vm->loop_state_len = 0;
	vm->loop_state_cap = 0;
	for (uint32_t i = 0; i < vm->code_cache_len; i++) {
		free(vm->code_caches[i].entries);
	}
	free(vm->code_caches);
	vm->code_caches = NULL;
	vm->code_cache_len = 0;
	vm->code_cache_cap = 0;
	for (uint32_t i = 0; i < vm->frame_cap; i++) {
		tcall_frame *frame = vm->frames[i];
		if (frame->initialized) {
			tobj_array_free(&frame->tmps);
			tobj_array_free(&frame->tail_args);
			tobj_array_free(&frame->env.base.objs);
			free(frame->env.vmstack);
			free(frame->loop_states);
		}
		free(vm->frames[i]);
	}
	free(vm->frames);
	vm->frames = NULL;
	vm->frame_len = 0;
	vm->frame_cap = 0;
}

static tloop_state *tvm_loop_state(tvm *vm, uint_cmds state_slot)
{
	if (state_slot >= vm->loop_state_cap) {
		uint_cmds newcap = vm->loop_state_cap ? vm->loop_state_cap * 2 : 16;
		while (newcap <= state_slot)
			newcap *= 2;
		tloop_state *grown = (tloop_state *)realloc(
			vm->loop_states, newcap * sizeof(tloop_state));
		if (!grown)
			twarn(ErrRuntime_Other, "tvm_loop_state", "out of memory");
		vm->loop_states = grown;
		vm->loop_state_cap = newcap;
	}
	while (vm->loop_state_len <= state_slot) {
		vm->loop_states[vm->loop_state_len].pos = 0;
		vm->loop_states[vm->loop_state_len].iterator_slot = NULL;
		vm->loop_states[vm->loop_state_len].iterator_kind = tins_cache_empty;
		vm->loop_state_len++;
	}
	return &vm->loop_states[state_slot];
}

static void tvm_release_loop_iterator(tvm *vm, const tobj *stack_slot)
{
	for (uint_cmds i = 0; i < vm->loop_state_len; i++) {
		if (vm->loop_states[i].iterator_slot != stack_slot)
			continue;
		vm->loop_states[i].pos = 0;
		vm->loop_states[i].iterator_slot = NULL;
		vm->loop_states[i].iterator_kind = tins_cache_empty;
	}
}

static void tcall_frame_release(tcall_frame *fr, tvm *vm)
{
	uint_regs i;

	fr->tmps = vm->tmps;
	for (i = 0; i < vm->stklen; i++)
		tobj_ddc_ref_clear(&vm->stk[i]);
	vm->stklen = 0;
	tobj_array_set_len(&fr->tmps, 0);
	tobj_array_set_len(&fr->tail_args, 0);
	tobj_array_set_len(&fr->env.base.objs, 0);
	fr->env.params = NULL;
	fr->env.dynamic_nparams = 0;
	fr->env.owner_func = NULL;
	fr->loop_states = vm->loop_states;
	fr->loop_state_len = 0;
	fr->loop_state_cap = vm->loop_state_cap;
}

static void tcall_frame_prepare(tcall_frame *fr, tfunc *f)
{
	if (fr->initialized && fr->func == f) {
		fr->env.owner_func = f;
		return;
	}
	if (fr->initialized &&
	    fr->env.base.father_env == f->env.base.father_env &&
	    fr->env.base.objs.capacity >= f->env.base.objs.capacity &&
	    fr->tmps.capacity >= f->env.tmpmax &&
	    fr->regcap >= f->env.regmax &&
	    fr->env.tmpmax == f->env.tmpmax &&
	    fr->env.regmax == f->env.regmax &&
	    fr->env.nparams == f->env.nparams) {
		fr->func = f;
		fr->env.owner_func = f;
		return;
	}

	tcompo_env *prototype = &f->env;
	uint_objs object_capacity = prototype->base.objs.capacity;
	uint_objs temporary_capacity = prototype->tmpmax;
	uint_regs register_capacity = prototype->regmax;
	tcompo_env *father = (tcompo_env *)prototype->base.father_env;

	if (!fr->initialized) {
		tcompo_env_init(&fr->env,
				object_capacity,
				father,
				register_capacity,
				temporary_capacity,
				prototype->nparams,
				compo_tfunc);
		tobj_array_init(&fr->tmps, temporary_capacity);
		tobj_array_init(&fr->tail_args,
				prototype->nparams == UNDEF_NPARAMS ? 0 :
				prototype->nparams);
		fr->regcap = register_capacity;
		fr->initialized = 1;
	} else {
		tobj_array_try_expand(&fr->env.base.objs, object_capacity);
		tobj_array_try_expand(&fr->tmps, temporary_capacity);
		if (register_capacity > fr->regcap) {
			tobj *grown = (tobj *)realloc(
				fr->env.vmstack, register_capacity * sizeof(tobj));
			if (!grown)
				twarn(ErrRuntime_Other,
				      "tcall_frame_prepare", "out of memory");
			fr->env.vmstack = grown;
			for (uint_regs i = fr->regcap; i < register_capacity; i++)
				tobj_set_nil(&fr->env.vmstack[i]);
			fr->regcap = register_capacity;
		}
	}

	fr->func = f;
	fr->env.base.father_env = father ? &father->base : NULL;
	fr->env.base.loc_in_father_env = father ?
		tobj_array_get_len(&father->base.objs) : 0;
	fr->env.tmpmax = temporary_capacity;
	fr->env.regmax = register_capacity;
	fr->env.nparams = prototype->nparams;
	fr->env.dynamic_nparams = 0;
	fr->env.compo_type = compo_tfunc;
	fr->env.owner_func = f;
}

static inline void tcall_frame_assign_params(
		tcall_frame *frame, tobj *params, uint_regs nparams)
{
	tobj_array *locals = &frame->env.base.objs;
	if (nparams > locals->capacity)
		tobj_array_try_expand(locals, nparams);
	locals->len = nparams;
	for (uint_regs i = 0; i < nparams; i++)
		tobj_array_set_obj(locals, i, &params[i]);
}

static tcall_frame *tvm_push_call_frame(
		tvm *vm, tfunc *f, tobj *params, uint_regs nparams)
{
	if (vm->frame_len >= vm->frame_cap) {
		uint32_t oldcap = vm->frame_cap;
		uint32_t newcap = oldcap ? oldcap * 2 : 16;
		tcall_frame **grown = (tcall_frame **)realloc(
			vm->frames, newcap * sizeof(tcall_frame *));
		if (!grown)
			twarn(ErrRuntime_Other, "tvm_push_call_frame", "out of memory");
		vm->frames = grown;
		for (uint32_t i = oldcap; i < newcap; i++) {
			vm->frames[i] = (tcall_frame *)calloc(1, sizeof(tcall_frame));
			if (!vm->frames[i])
				twarn(ErrRuntime_Other, "tvm_push_call_frame", "out of memory");
		}
		vm->frame_cap = newcap;
	}

	tcall_frame *fr = vm->frames[vm->frame_len++];
	if (fr->initialized && fr->func == f)
		fr->env.owner_func = f;
	else
		tcall_frame_prepare(fr, f);
	fr->saved_tmps = vm->tmps;
	fr->saved_stk = vm->stk;
	fr->saved_regmax = vm->regmax;
	fr->saved_stklen = vm->stklen;
	fr->saved_loop_states = vm->loop_states;
	fr->saved_loop_state_len = vm->loop_state_len;
	fr->saved_loop_state_cap = vm->loop_state_cap;

	if (fr->env.nparams != UNDEF_NPARAMS) {
		tcall_frame_assign_params(fr, params, nparams);
		fr->env.params = fr->env.base.objs.data;
	} else {
		fr->env.params = params;
	}
	fr->env.dynamic_nparams = nparams;

	vm->tmps = fr->tmps;
	vm->stk = fr->env.vmstack;
	vm->regmax = fr->env.regmax;
	vm->stklen = 0;
	vm->loop_states = fr->loop_states;
	vm->loop_state_len = 0;
	vm->loop_state_cap = fr->loop_state_cap;
	return fr;
}

static tcall_frame *tvm_replace_current_call_frame(
		tvm *vm, tfunc *function, tobj *params, uint_regs nparams)
{
	if (vm->frame_len == 0)
		twarn(ErrRuntime_Other,
		      "tvm_replace_current_call_frame", "empty frame stack");
	tcall_frame *frame = vm->frames[vm->frame_len - 1];

	tobj_array_set_len(&frame->tail_args, 0);
	for (uint_regs i = 0; i < nparams; i++) {
		tobj_array_add_obj(&frame->tail_args, UNDEF_NAMELOC);
		tobj_array_set_obj(&frame->tail_args, i, &params[i]);
	}

	for (uint_regs i = 0; i < vm->stklen; i++)
		tobj_ddc_ref_clear(&vm->stk[i]);
	vm->stklen = 0;
	frame->tmps = vm->tmps;
	tobj_array_set_len(&frame->tmps, 0);
	tobj_array_set_len(&frame->env.base.objs, 0);
	frame->loop_states = vm->loop_states;
	frame->loop_state_cap = vm->loop_state_cap;
	frame->loop_state_len = 0;
	vm->loop_state_len = 0;

	if (frame->initialized && frame->func == function)
		frame->env.owner_func = function;
	else
		tcall_frame_prepare(frame, function);
	if (frame->env.nparams != UNDEF_NPARAMS) {
		tcall_frame_assign_params(
			frame, frame->tail_args.data, nparams);
		frame->env.params = frame->env.base.objs.data;
	} else {
		frame->env.params = frame->tail_args.data;
	}
	frame->env.dynamic_nparams = nparams;
	frame->func = function;

	vm->tmps = frame->tmps;
	vm->stk = frame->env.vmstack;
	vm->regmax = frame->env.regmax;
	vm->stklen = 0;
	return frame;
}

static void tvm_pop_call_frame(tvm *vm)
{
	if (vm->frame_len == 0)
		twarn(ErrRuntime_Other, "tvm_pop_call_frame", "empty frame stack");
	tcall_frame *fr = vm->frames[vm->frame_len - 1];
	tobj_array saved_tmps = fr->saved_tmps;
	tobj *saved_stk = fr->saved_stk;
	uint_regs saved_regmax = fr->saved_regmax;
	uint_regs saved_stklen = fr->saved_stklen;
	tloop_state *saved_loop_states = fr->saved_loop_states;
	uint_cmds saved_loop_state_len = fr->saved_loop_state_len;
	uint_cmds saved_loop_state_cap = fr->saved_loop_state_cap;

	tcall_frame_release(fr, vm);
	vm->frame_len--;

	vm->tmps = saved_tmps;
	vm->stk = saved_stk;
	vm->regmax = saved_regmax;
	vm->stklen = saved_stklen;
	vm->loop_states = saved_loop_states;
	vm->loop_state_len = saved_loop_state_len;
	vm->loop_state_cap = saved_loop_state_cap;
}

static void tvm_drop_call_frames(tvm *vm)
{
	while (vm->frame_len > 0)
		tvm_pop_call_frame(vm);
}

/* Stack helpers */
static inline tobj *stk_at(tvm *vm, uint_regs loc)
{
	return &vm->stk[vm->stklen - loc - 1];
}

static inline tobj *stk_top(tvm *vm)
{
	return &vm->stk[vm->stklen - 1];
}

static inline tobj *stk_topn(tvm *vm, uint_regs n)
{
	return vm->stk + vm->stklen - n;
}

static inline tobj *stk_free(tvm *vm)
{
	return &vm->stk[vm->stklen];
}

static inline void stk_fill(tvm *vm)
{
	vm->stklen++;
}

static inline uint_regs stk_len(tvm *vm)
{
	return vm->stklen;
}

static inline void stk_pop(tvm *vm)
{
	vm->stklen--;
}

static inline void stk_popc(tvm *vm)
{
	tobj *value = stk_top(vm);
	if (value->type == tcompo)
		tobj_ddc_ref_clear(value);
	vm->stklen--;
}

static inline void stk_popcn(tvm *vm, uint_regs n)
{
	uint_regs i;
	for (i = 0; i < n; i++)
		stk_popc(vm);
}

static inline void stk_push(tvm *vm, const tobj *v)
{
	vm->stk[vm->stklen] = *v;
	if (v->type == tcompo && v->val.v_tcompo)
		v->val.v_tcompo->refctr++;
	vm->stklen++;
}

static inline tobj *vm_array_slot(tobj_array *array, uint_objs slot)
{
	if (slot >= array->len)
		twarn(ErrRuntime_ObjUnfound, "vm_array_slot", "");
	return &array->data[slot];
}

tobj *tmp_obj(tvm *vm, uint_objs loc)
{
	return vm_array_slot(&vm->tmps, loc);
}

static tobj *vm_pushx_source(tvm *vm, tcompo_env *env, uint_objs slot, uint16_t addr)
{
	if (!tpushx_isenv(addr))
		return vm_array_slot(&vm->tmps, slot);

	if (!tpushx_is_upval(addr))
		return vm_array_slot(&env->base.objs, slot);

	tcompo_env_abstract *cur = &env->base;
	uint16_t depth = tpushx_depth(addr);
	for (uint16_t i = 0; i < depth; i++) {
		if (!cur->father_env)
			twarn(ErrRuntime_ObjUnfound, "OP_PUSHX", "");
		cur = cur->father_env;
	}
	return vm_array_slot(&cur->objs, slot);
}

typedef struct {
	const tobj *left;
	const tobj *right;
	tobj *result;
	int pop_top;
	int fill_stack;
} tbinop_operands;

#if defined(__GNUC__) || defined(__clang__)
__attribute__((always_inline))
#endif
static inline tbinop_operands vm_binop_operands(
		tvm *vm, tbycode code, uint_regs type, tcompo_env *env)
{
	uint16_t left = tbycode_get_L(code);
	uint16_t right = tbycode_get_R(code);
	tbinop_operands operands = { 0 };

	switch (type) {
	case 0: /* value value */
		operands.left = stk_at(vm, (uint_regs)left);
		operands.right = stk_at(vm, (uint_regs)right);
		operands.result = stk_at(vm, (uint_regs)right);
		operands.pop_top = 1;
		break;
	case 1: /* env value */
		operands.left = tcompo_env_get_obj(env, left);
		operands.right = stk_at(vm, (uint_regs)right);
		operands.result = stk_at(vm, (uint_regs)right);
		break;
	case 2: /* value env */
		operands.left = stk_at(vm, (uint_regs)left);
		operands.right = tcompo_env_get_obj(env, right);
		operands.result = stk_at(vm, (uint_regs)left);
		break;
	case 3: /* env env */
		operands.left = tcompo_env_get_obj(env, left);
		operands.right = tcompo_env_get_obj(env, right);
		operands.result = stk_free(vm);
		tobj_set_nil(operands.result);
		operands.fill_stack = 1;
		break;
	case 4: /* tmp value */
		operands.left = tmp_obj(vm, left);
		operands.right = stk_at(vm, (uint_regs)right);
		operands.result = stk_at(vm, (uint_regs)right);
		break;
	case 5: /* value tmp */
		operands.left = stk_at(vm, (uint_regs)left);
		operands.right = tmp_obj(vm, right);
		operands.result = stk_at(vm, (uint_regs)left);
		break;
	case 6: /* tmp tmp */
		operands.left = tmp_obj(vm, left);
		operands.right = tmp_obj(vm, right);
		operands.result = stk_free(vm);
		tobj_set_nil(operands.result);
		operands.fill_stack = 1;
		break;
	case 7: /* env tmp */
		operands.left = tcompo_env_get_obj(env, left);
		operands.right = tmp_obj(vm, right);
		operands.result = stk_free(vm);
		tobj_set_nil(operands.result);
		operands.fill_stack = 1;
		break;
	case 8: /* tmp env */
		operands.left = tmp_obj(vm, left);
		operands.right = tcompo_env_get_obj(env, right);
		operands.result = stk_free(vm);
		tobj_set_nil(operands.result);
		operands.fill_stack = 1;
		break;
	default:
		twarn(ErrRuntime_Other, "vm_parse_binop", "invalid binop type");
	}
	return operands;
}

static inline void vm_finish_binop(tvm *vm, tbinop_operands operands)
{
	if (operands.pop_top)
		stk_popc(vm);
	else if (operands.fill_stack)
		stk_fill(vm);
}

#define VM_PARSE_BINOP(vm, fn, code, type, env)                             \
	do {                                                                 \
		tbinop_operands operands__ =                                  \
			vm_binop_operands((vm), (code), (type), (env));         \
		(fn)(operands__.left, operands__.right, operands__.result);     \
		vm_finish_binop((vm), operands__);                              \
	} while (0)

static inline int vm_try_fused_binop(
		tvm *vm, tbycode code, uint_regs type, tcompo_env *env)
{
	tins instruction = tbycode_ins(code);
	if (instruction < OP_ADD || instruction > OP_OR)
		return 0;

	tbinop_operands operands = vm_binop_operands(vm, code, type, env);
	const tobj *left = operands.left;
	const tobj *right = operands.right;
	tobj *result = operands.result;
	switch (instruction) {
	case OP_ADD:
		if (!add_impl(left->type, right->type, left, right, result))
			operator_add_slow(left, right, result);
		break;
	case OP_SUB:
		if (!sub_impl(left->type, right->type, left, right, result))
			operator_sub_slow(left, right, result);
		break;
	case OP_MUL:
		if (!mul_impl(left->type, right->type, left, right, result))
			operator_mul_slow(left, right, result);
		break;
	case OP_DIV:
		if (!div_impl(left->type, right->type, left, right, result))
			operator_div_slow(left, right, result);
		break;
	case OP_MOD:
		operator_mod(left, right, result);
		break;
	case OP_POW:
		operator_pow(left, right, result);
		break;
	case OP_MMUL:
		operator_mmul(left, right, result);
		break;
	case OP_EQ:
		if (!eq_impl(left->type, right->type, left, right, result))
			operator_eq_slow(left, right, result);
		break;
	case OP_NE:
		if (!ne_impl(left->type, right->type, left, right, result))
			operator_ne_slow(left, right, result);
		break;
	case OP_SG:
		if (!sg_impl(left->type, right->type, left, right, result))
			operator_sg_slow(left, right, result);
		break;
	case OP_SL:
		if (!sl_impl(left->type, right->type, left, right, result))
			operator_sl_slow(left, right, result);
		break;
	case OP_GE:
		if (!ge_impl(left->type, right->type, left, right, result))
			operator_ge_slow(left, right, result);
		break;
	case OP_LE:
		if (!le_impl(left->type, right->type, left, right, result))
			operator_le_slow(left, right, result);
		break;
	case OP_AND:
		operator_and(left, right, result);
		break;
	case OP_OR:
		operator_or(left, right, result);
		break;
	default:
		return 0;
	}
	vm_finish_binop(vm, operands);
	return 1;
}

void tmp_add(tvm *vm)
{
	tobj_array_add_obj(&vm->tmps, UNDEF_NAMELOC);
}

void tmp_del(tvm *vm, uint_objs n)
{
	tobj_array_del_obj(&vm->tmps, n);
}

static inline void vm_add_slot(tobj_array *array, uint_csts name)
{
	if (array->len >= array->capacity) {
		tobj_array_add_obj(array, name);
		return;
	}
	tobj *slot = &array->data[array->len++];
	tobj_set_nil(slot);
	slot->name_loc = (int)name;
}

static inline void vm_del_slots(tobj_array *array, uint_objs count)
{
	while (count > 0 && array->len > 0) {
		tobj *slot = &array->data[--array->len];
		if (slot->type == tcompo)
			tobj_ddc_ref_clear(slot);
		else
			tobj_set_nil(slot);
		count--;
	}
}

static void tins_cache_observe(tins_cache *cache, tins_cache_kind kind,
			       tcompo_vtable *guard)
{
	if (!cache || cache->kind == tins_cache_polymorphic)
		return;
	if (cache->kind == tins_cache_empty) {
		cache->kind = (uint8_t)kind;
		cache->guard = guard;
		return;
	}
	if (cache->kind != kind || cache->guard != guard) {
		cache->kind = tins_cache_polymorphic;
		cache->guard = NULL;
	}
}

static void vm_list_idx_int(tlist *list, long index, tobj *result)
{
	uint_objs len = list->items.len;
	if (index < 0)
		index += (long)len;
	if (index < 0 || (uint_objs)index >= len)
		twarn(ErrRuntime_IdxOutRange, "list index", "");
	*result = list->items.data[index];
	if (result->type == tcompo && result->val.v_tcompo)
		result->val.v_tcompo->refctr++;
}

static void vm_str_idx_int(tstr *str, long index, tobj *result)
{
	size_t len = tstring_len(str->data);
	if (index < 0)
		index += (long)len;
	if (index < 0 || (size_t)index >= len)
		twarn(ErrRuntime_IdxOutRange, "string index", "");
	tobj_set_compo(
		result,
		(tcompo_v *)tstr_new_len(tstring_cstr(str->data) + index, 1));
}

static size_t vm_dense_int2_offset(tcompo_v *array, const tobj *params)
{
	long row = params[0].val.v_tint;
	long column = params[1].val.v_tint;
	size_t rows;
	size_t columns;
	if (array->vtable == &tdarr_vtable) {
		rows = ((tdarr *)array)->rows;
		columns = ((tdarr *)array)->cols;
	} else {
		rows = ((tbarr *)array)->rows;
		columns = ((tbarr *)array)->cols;
	}
	if (row < 0)
		row += (long)rows;
	if (column < 0)
		column += (long)columns;
	if (row < 0 || column < 0 || (size_t)row >= rows ||
	    (size_t)column >= columns)
		twarn(ErrRuntime_IdxOutRange, "array index",
		      "array index out of range");
	return (size_t)row * columns + (size_t)column;
}

static void vm_dense_idx_int2(tcompo_v *array, const tobj *params,
			      tobj *result)
{
	size_t offset = vm_dense_int2_offset(array, params);
	if (array->vtable == &tdarr_vtable)
		vm_set_float_result(result, ((tdarr *)array)->data[offset]);
	else
		vm_set_bool_result(result, ((tbarr *)array)->data[offset] != 0);
}

static void vm_dense_iset_int2(tcompo_v *array, const tobj *params,
			       const tobj *value)
{
	size_t offset = vm_dense_int2_offset(array, params);
	if (array->vtable == &tdarr_vtable) {
		if (value->type == tint)
			((tdarr *)array)->data[offset] = (double)value->val.v_tint;
		else if (value->type == tfloat)
			((tdarr *)array)->data[offset] = value->val.v_tfloat;
		else
			twarn(ErrRuntime_ParamsType, "array assignment",
			      "real array requires a number");
	} else {
		if (value->type != tbool)
			twarn(ErrRuntime_ParamsType, "array assignment",
			      "boolean array requires a boolean");
		((tbarr *)array)->data[offset] =
			(unsigned char)(value->val.v_tbool != 0);
	}
}

/* Index right */
static void vm_idxr(tvm *vm, uint_regs nparams, tins_cache *cache)
{
	tobj *obj = stk_top(vm);
	if (obj->type != tcompo)
		twarn(ErrRuntime_RefType, "vm_idxr", "");
	tcompo_v *arr = obj->val.v_tcompo;
	tobj *params = stk_topn(vm, nparams + 1);
	if (cache && cache->kind == tins_cache_idxr_list_int &&
	    cache->guard == arr->vtable && nparams == 1 &&
	    params[0].type == tint) {
		vm_list_idx_int((tlist *)arr, params[0].val.v_tint, &vm->rev);
		goto finish;
	}
	if (cache && cache->kind == tins_cache_idxr_str_int &&
	    cache->guard == arr->vtable && nparams == 1 &&
	    params[0].type == tint) {
		vm_str_idx_int((tstr *)arr, params[0].val.v_tint, &vm->rev);
		goto finish;
	}
	if (cache && cache->kind == tins_cache_idxr_dense_int2 &&
	    cache->guard == arr->vtable && nparams == 2 &&
	    params[0].type == tint && params[1].type == tint) {
		vm_dense_idx_int2(arr, params, &vm->rev);
		goto finish;
	}
	switch (arr->vtable->get_compo_type_code()) {
	case compo_tstr:
		if (nparams == 1 && params[0].type == tint) {
			tins_cache_observe(cache, tins_cache_idxr_str_int,
					   arr->vtable);
			vm_str_idx_int(
				(tstr *)arr, params[0].val.v_tint, &vm->rev);
		} else {
			if (cache && cache->kind == tins_cache_empty)
				cache->kind = tins_cache_polymorphic;
			tstr_idx((tstr *)arr, params, nparams, &vm->rev);
		}
		break;
	case compo_tlist:
		if (nparams == 1 && params[0].type == tint) {
			tins_cache_observe(cache, tins_cache_idxr_list_int,
					   arr->vtable);
			vm_list_idx_int(
				(tlist *)arr, params[0].val.v_tint, &vm->rev);
		} else {
			if (cache && cache->kind == tins_cache_empty)
				cache->kind = tins_cache_polymorphic;
			tlist_idx((tlist *)arr, params, nparams, &vm->rev);
		}
		break;
	case compo_tpair:
		tpair_idx((tpair *)arr, params, nparams, &vm->rev);
		break;
	case compo_tdict:
		tdict_idx((tdict *)arr, params, nparams, &vm->rev);
		break;
	case compo_ttypeval:
		ttypeval_idx((ttypeval *)arr, params, nparams, &vm->rev);
		break;
	case compo_tlib:
		tlib_idx((tlib *)arr, params, nparams, &vm->rev);
		break;
	case compo_tdarr:
	case compo_tbarr:
		if (nparams == 2 && params[0].type == tint &&
		    params[1].type == tint) {
			tins_cache_observe(cache, tins_cache_idxr_dense_int2,
					   arr->vtable);
			vm_dense_idx_int2(arr, params, &vm->rev);
		} else {
			if (cache && cache->kind == tins_cache_empty)
				cache->kind = tins_cache_polymorphic;
			tarr_idx(arr, params, nparams, &vm->rev);
		}
		break;
	default:
		twarn(ErrRuntime_RefType, "vm_idxr", "unindexable type");
	}
finish:
	stk_popcn(vm, 1 + nparams);
	stk_push(vm, &vm->rev);
	tvm_set_rev_empty(vm);
}

/* Index left */
static void vm_idxl(tvm *vm, uint_objs loc, uint_regs nparams, int isenv,
		    tcompo_env *env, tins_cache *cache)
{
	tobj *objp = isenv ? tcompo_env_get_obj(env, loc) : tmp_obj(vm, loc);
	if (objp->type != tcompo)
		twarn(ErrRuntime_RefType, "vm_idxl", "");
	tcompo_v *arr = objp->val.v_tcompo;
	tobj *rv = stk_at(vm, nparams);
	tobj *params = stk_topn(vm, nparams);
	if (cache && cache->kind == tins_cache_idxl_list_int &&
	    cache->guard == arr->vtable && nparams == 1 &&
	    params[0].type == tint) {
		long index = params[0].val.v_tint;
		uint_objs len = ((tlist *)arr)->items.len;
		if (index < 0)
			index += (long)len;
		if (index < 0 || (uint_objs)index >= len)
			twarn(ErrRuntime_IdxOutRange, "list assignment", "");
		tlist_set_at((tlist *)arr, (uint_objs)index, rv);
		goto finish;
	}
	if (cache && cache->kind == tins_cache_idxl_dense_int2 &&
	    cache->guard == arr->vtable && nparams == 2 &&
	    params[0].type == tint && params[1].type == tint) {
		vm_dense_iset_int2(arr, params, rv);
		goto finish;
	}
	switch (arr->vtable->get_compo_type_code()) {
	case compo_tpair:
		tpair_iset((tpair *)arr, params, nparams, rv);
		break;
	case compo_tstr:
		tstr_iset((tstr *)arr, params, nparams, rv);
		break;
	case compo_tdict:
		tdict_iset((tdict *)arr, params, nparams, rv);
		break;
	case compo_tlist:
		if (nparams == 1 && params[0].type == tint) {
			tins_cache_observe(cache, tins_cache_idxl_list_int,
					   arr->vtable);
			long index = params[0].val.v_tint;
			uint_objs len = ((tlist *)arr)->items.len;
			if (index < 0)
				index += (long)len;
			if (index < 0 || (uint_objs)index >= len)
				twarn(ErrRuntime_IdxOutRange,
				      "list assignment", "");
			tlist_set_at((tlist *)arr, (uint_objs)index, rv);
		} else {
			if (cache && cache->kind == tins_cache_empty)
				cache->kind = tins_cache_polymorphic;
			tlist_iset((tlist *)arr, params, nparams, rv);
		}
		break;
	case compo_tdarr:
	case compo_tbarr:
		if (nparams == 2 && params[0].type == tint &&
		    params[1].type == tint) {
			tins_cache_observe(cache, tins_cache_idxl_dense_int2,
					   arr->vtable);
			vm_dense_iset_int2(arr, params, rv);
		} else {
			if (cache && cache->kind == tins_cache_empty)
				cache->kind = tins_cache_polymorphic;
			tarr_iset(arr, params, nparams, rv);
		}
		break;
	default:
		twarn(ErrRuntime_RefType, "vm_idxl", "");
	}
finish:
	stk_popcn(vm, 1 + nparams);
}

static tobj *vm_loop_target(tvm *vm, uint_objs idx, int isenv, tcompo_env *env)
{
	return isenv ? tcompo_env_get_obj(env, idx) : tmp_obj(vm, idx);
}

static void vm_loop_push_cond(tvm *vm, int has_next)
{
	vm_set_bool_result(stk_free(vm), has_next);
	stk_fill(vm);
}

static void vm_loopias(tvm *vm, tloop_state *state, uint_objs idx,
		       int isenv, tcompo_env *env)
{
	titer *p = (titer *)stk_top(vm)->val.v_tcompo;
	long val = p->current;
	int has_next = (p->step > 0 && val < p->end) ||
		       (p->step < 0 && val > p->end);
	if (has_next) {
		tobj_set_int(vm_loop_target(vm, idx, isenv, env), val);
		p->current += p->step;
	} else {
		p->current = p->start;
		if (state) {
			state->iterator_slot = NULL;
			state->iterator_kind = tins_cache_empty;
		}
	}
	vm_loop_push_cond(vm, has_next);
}

static void vm_looplas(tvm *vm, tloop_state *state, uint_objs idx,
		       int isenv, tcompo_env *env)
{
	tobj *viter = stk_top(vm);
	tobj v;
	tobj_set_nil(&v);
	int has_next = tlist_next_at(
		(tlist *)viter->val.v_tcompo, &state->pos, &v);
	if (has_next) {
		if (isenv)
			tcompo_env_set_obj(env, idx, &v);
		else
			tobj_array_set_obj(&vm->tmps, idx, &v);
		tobj_ddc_ref_clear(&v);
	} else {
		state->iterator_slot = NULL;
		state->iterator_kind = tins_cache_empty;
	}
	vm_loop_push_cond(vm, has_next);
}

static tins_cache_kind vm_loop_cache_kind(tcompo_type type)
{
	switch (type) {
	case compo_titer:
		return tins_cache_loop_iter;
	case compo_tlist:
		return tins_cache_loop_list;
	case compo_ttypeval:
		return tins_cache_loop_type;
	default:
		return tins_cache_empty;
	}
}

static void vm_loop_dispatch(tvm *vm, tins_cache_kind kind,
			     tloop_state *state, uint_objs idx,
			     int isenv, tcompo_env *env)
{
	tobj *viter = stk_top(vm);
	tcompo_v *it = viter->val.v_tcompo;
	switch (kind) {
	case tins_cache_loop_iter:
		vm_loopias(vm, state, idx, isenv, env);
		return;
	case tins_cache_loop_list:
		vm_looplas(vm, state, idx, isenv, env);
		return;
	case tins_cache_loop_type: {
		tobj key;
		tobj_set_nil(&key);
		int has_next = ttypeval_next_key(
			(ttypeval *)it, &state->pos, &key);
		if (has_next) {
			if (isenv)
				tcompo_env_set_obj(env, idx, &key);
			else
				tobj_array_set_obj(&vm->tmps, idx, &key);
			tobj_ddc_ref_clear(&key);
		} else {
			state->pos = 0;
			state->iterator_slot = NULL;
			state->iterator_kind = tins_cache_empty;
		}
		vm_loop_push_cond(vm, has_next);
		return;
	}
	default:
		twarn(ErrRuntime_RefType, "vm_loopas", "unsupported iterator");
	}
}

/* The bytecode describes one loop operation. This VM-local cache selects the
 * concrete iterator implementation without rewriting the instruction. */
static void vm_loopas(tvm *vm, tins_cache *cache, uint_objs idx,
		      int isenv, tcompo_env *env)
{
	tobj *viter = stk_top(vm);
	if (viter->type != tcompo || !viter->val.v_tcompo)
		twarn(ErrRuntime_RefType, "vm_loopas", "");
	tcompo_v *it = viter->val.v_tcompo;
	if (!cache)
		twarn(ErrRuntime_Other, "vm_loopas", "missing instruction cache");
	tloop_state *state = tvm_loop_state(vm, cache->state_slot);
	if (state->iterator_slot == viter &&
	    state->iterator_kind != tins_cache_empty) {
		vm_loop_dispatch(vm, (tins_cache_kind)state->iterator_kind,
				 state, idx, isenv, env);
		return;
	}
	if (cache->kind != tins_cache_empty &&
	    cache->kind != tins_cache_polymorphic && cache->guard == it->vtable) {
		state->pos = 0;
		state->iterator_slot = viter;
		state->iterator_kind = cache->kind;
		vm_loop_dispatch(vm, (tins_cache_kind)cache->kind,
				 state, idx, isenv, env);
		return;
	}

	tins_cache_kind observed = vm_loop_cache_kind(
		it->vtable->get_compo_type_code());
	if (observed == tins_cache_empty)
		twarn(ErrRuntime_RefType, "vm_loopas", "unsupported iterator");
	if (cache->kind == tins_cache_empty) {
		cache->kind = (uint8_t)observed;
		cache->guard = it->vtable;
	} else if (cache->guard != it->vtable) {
		cache->kind = tins_cache_polymorphic;
		cache->guard = NULL;
	}
	state->pos = 0;
	state->iterator_slot = viter;
	state->iterator_kind = (uint8_t)observed;
	vm_loop_dispatch(vm, observed, state, idx, isenv, env);
}

/* Forward declare exec_tins (defined later) */
void exec_tins(tvm *vm, uint_cmds from, uint_cmds ncmds, tcompo_env *env);

/* Eval helper */
void vm_eval(tvm *vm, tbycode *iter, tcompo_env *env)
{
	uint_regs nparams = (uint_regs)tbycode_get_U(*iter);
	tobj *obj = stk_top(vm);
	tobj *params = stk_topn(vm, nparams + 1);
	if (obj->type != tcompo)
		twarn(ErrRuntime_RefType, "vm_eval", "");
	tcompo_v *v = obj->val.v_tcompo;

	switch (v->vtable->get_compo_type_code()) {
	case compo_tfunc: {
		tfunc *f = (tfunc *)v;
		tcall_frame *fr = tvm_push_call_frame(vm, f, params, nparams);
		exec_tins(vm, f->cmdloc, f->ncmds, &fr->env);
		tvm_pop_call_frame(vm);
	} break;
	case compo_cppfunc: {
		tcppgenf *g = (tcppgenf *)v;
		if (!g->f)
			twarn(ErrRuntime_Other,
			      tstring_cstr(g->name),
			      "function is not implemented");
		g->f(params, nparams, &vm->rev);
	} break;
	case compo_sessfunc: {
		tcppsessf *s = (tcppsessf *)v;
		s->f(params, nparams, &vm->rev, env);
	} break;
	default:
		twarn(ErrRuntime_RefType, "vm_eval", "");
	}
	stk_popcn(vm, 1 + nparams);
	stk_push(vm, &vm->rev);
	tvm_set_rev_empty(vm);
}

/* Import */
void
vm_import(tvm *vm, uint_csts cloc, tstring **cstrlsts, tcompo_env *env)
{
	tlib *current_lib = tlib_from_env(env);
	tlib *lib = tlib_recreate(current_lib);
	tstring *file_ts = cstrlsts[cloc];
	const char *file = tstring_cstr(file_ts);
	char *dot = strrchr(file, '.');
	size_t baselen = dot ? (size_t)(dot - file) : strlen(file);
	char *binf = (char *)malloc(baselen + 6);
	memcpy(binf, file, baselen);
	strcpy(binf + baselen, ".tapc");
	twrapper *w = tanalyser_load_bin_file(binf);
	if (!w) {
		free(binf);
		twarn(ErrSession_IO, "vm_import", file);
		return;
	}
	free(binf);
	tlib_set_wrapper(lib, w);
	tvm vm2;
	tvm_init(&vm2, w->info.tmp_max);
	tvm_set_vmstack(
		&vm2, tcompo_env_get_vmstack(&lib->env), w->info.reg_max);
	exec_tins(&vm2, 0, w->ncmds, &lib->env);
	tobj returned = *tvm_get_vre(&vm2);
	tvm_get_vre(&vm2)->type = tnil;
	tvm_clean(&vm2);
	tvm_set_vmstack(&vm2, NULL, 0);
	if (returned.type == tcompo &&
	    tobj_compo_type(&returned) == compo_tdict)
		tlib_set_exposed(lib, (tdict *)returned.val.v_tcompo);
	else if (returned.type == tcompo) {
		returned.val.v_tcompo->vtable->free(returned.val.v_tcompo);
		tlib_set_exposed(lib, tdict_new());
	} else
		tlib_set_exposed(lib, tdict_new());
	tobj_set_compo(stk_free(vm), (tcompo_v *)lib);
	stk_fill(vm);
}

/* Binop dispatch */
void
vm_binop(tvm *vm, binopf f, tbycode *iter, uint_regs type, tcompo_env *env)
{
	uint16_t left = tbycode_get_L(*iter), right = tbycode_get_R(*iter);
	tobj lv, rv;
	switch (type) {
	case 0:
		f(stk_at(vm, left), stk_at(vm, right), stk_at(vm, right));
		stk_pop(vm);
		break;
	case 1:
		tobj_set_nil(&lv);
		tobj_copy(&lv, tcompo_env_get_obj(env, left));
		f(&lv, stk_at(vm, right), stk_at(vm, right));
		tobj_ddc_ref_clear(&lv);
		break;
	case 2:
		tobj_set_nil(&rv);
		tobj_copy(&rv, tcompo_env_get_obj(env, right));
		f(stk_at(vm, left), &rv, stk_at(vm, left));
		tobj_ddc_ref_clear(&rv);
		break;
	case 3:
		tobj_set_nil(&lv);
		tobj_set_nil(&rv);
		tobj_copy(&lv, tcompo_env_get_obj(env, left));
		tobj_copy(&rv, tcompo_env_get_obj(env, right));
		f(&lv, &rv, stk_free(vm));
		stk_fill(vm);
		tobj_ddc_ref_clear(&lv);
		tobj_ddc_ref_clear(&rv);
		break;
	case 4:
		tobj_set_nil(&lv);
		tobj_copy(&lv, tmp_obj(vm, left));
		f(&lv, stk_at(vm, right), stk_at(vm, right));
		tobj_ddc_ref_clear(&lv);
		break;
	case 5:
		tobj_set_nil(&rv);
		tobj_copy(&rv, tmp_obj(vm, right));
		f(stk_at(vm, left), &rv, stk_at(vm, left));
		tobj_ddc_ref_clear(&rv);
		break;
	case 6:
		tobj_set_nil(&lv);
		tobj_set_nil(&rv);
		tobj_copy(&lv, tmp_obj(vm, left));
		tobj_copy(&rv, tmp_obj(vm, right));
		f(&lv, &rv, stk_free(vm));
		stk_fill(vm);
		tobj_ddc_ref_clear(&lv);
		tobj_ddc_ref_clear(&rv);
		break;
	case 7:
		tobj_set_nil(&lv);
		tobj_set_nil(&rv);
		tobj_copy(&lv, tcompo_env_get_obj(env, left));
		tobj_copy(&rv, tmp_obj(vm, right));
		f(&lv, &rv, stk_free(vm));
		stk_fill(vm);
		tobj_ddc_ref_clear(&lv);
		tobj_ddc_ref_clear(&rv);
		break;
	case 8:
		tobj_set_nil(&lv);
		tobj_set_nil(&rv);
		tobj_copy(&lv, tmp_obj(vm, left));
		tobj_copy(&rv, tcompo_env_get_obj(env, right));
		f(&lv, &rv, stk_free(vm));
		stk_fill(vm);
		tobj_ddc_ref_clear(&lv);
		tobj_ddc_ref_clear(&rv);
		break;
	}
}

/* Single ins execution */
typedef enum {
	texec_normal,
	texec_call,
	texec_return
} texec_action;

typedef struct {
	tfunc *function;
	tobj *params;
	uint_regs nparams;
	uint_regs return_stack_values;
	int current_function;
} tcall_request;

#if defined(__GNUC__) || defined(__clang__)
__attribute__((always_inline))
#endif
static inline texec_action exec_tin(tvm *vm,
			    tbycode *iter,
			    tins ins,
			    tvm_code_cache *code_cache,
			    tcall_request *call,
			    uint_cmds *idx,
			    uint_cmds end,
			    tcompo_env *env,
			    long *cints,
			    double *cflts,
			    tstring **cstrs)
{
	(void)end;
	switch (ins) {
	case OP_PASS:
		break;
	case OP_VCRT: {
		int isenv = (int)tbycode_get_R(*iter);
		uint_csts nl = tbycode_get_L(*iter);
		if (isenv)
			vm_add_slot(&env->base.objs, nl);
		else
			vm_add_slot(&vm->tmps, UNDEF_NAMELOC);
	} break;
	case OP_TMPDEL:
		vm_del_slots(&vm->tmps, (uint_objs)tbycode_get_U(*iter));
		break;
		case OP_THIS: {
			tobj current;
			tobj_set_nil(&current);
			tcompo_env_copy_to_obj(env, &current);
			stk_push(vm, &current);
			tobj_set_nil(&current);
		} break;
		case OP_BASE: {
			tobj v;
			tobj_set_nil(&v);
			tcompo_env_copy_to_obj(tcompo_env_get_father(env), &v);
			stk_push(vm, &v);
			tobj_set_nil(&v);
	} break;
	case OP_RET:
		if (stk_len(vm) > 0) {
			if (stk_top(vm)->type == tcompo &&
			    tobj_compo_type(stk_top(vm)) == compo_tfunc)
				tfunc_close_over(
					(tfunc *)stk_top(vm)->val.v_tcompo,
					env);
			vm->rev = *stk_top(vm);
			stk_pop(vm);
		} else
			tvm_set_rev_empty(vm);
		stk_popcn(vm, stk_len(vm));
		*idx = end;
		return texec_return;
	case OP_IN: {
		tobj r;
		tobj_set_nil(&r);
		operator_in(stk_top(vm), stk_at(vm, 1), &r);
		stk_popcn(vm, 2);
		stk_push(vm, &r);
		tobj_set_nil(&r);
	} break;
	case OP_PAIR: {
		tobj r;
		tobj_set_nil(&r);
		operator_pair(stk_top(vm), stk_at(vm, 1), &r);
		stk_popcn(vm, 2);
		stk_push(vm, &r);
		tobj_set_nil(&r);
	} break;
	case OP_TO: {
		tobj r;
		tobj_set_nil(&r);
		operator_to(stk_top(vm), stk_at(vm, 1), &r);
		stk_popcn(vm, 2);
		stk_push(vm, &r);
		tobj_set_nil(&r);
	} break;
	case OP_POPN: {
		int print = tbycode_get_R(*iter);
		uint_regs i, n = (uint_regs)tbycode_get_L(*iter);
		for (i = 0; i < n; i++) {
			tvm_release_loop_iterator(vm, stk_top(vm));
			if (print && stk_top(vm)->type != tnil) {
				tstring *s = tobj_tostring_full(stk_top(vm));
				printf("%s\n", tstring_cstr(s));
				tstring_free(s);
			}
			stk_popc(vm);
		}
	} break;
	case OP_POPCOV:
		if (stk_top(vm)->type == tnil)
			twarn(ErrRuntime_AssignNil, "OP_POPCOV", "");
		if (tbycode_get_R(*iter))
			tcompo_env_set_obj(
				env, tbycode_get_L(*iter), stk_top(vm));
		else
			tobj_array_set_obj(
				&vm->tmps, tbycode_get_L(*iter), stk_top(vm));
		stk_pop(vm);
		break;
	case OP_TYPEFWD: {
		tobj value;
		tobj_set_nil(&value);
		tobj_set_compo(&value, (tcompo_v *)ttypeval_new_recursive());
		stk_push(vm, &value);
		tobj_set_nil(&value);
	} break;
	case OP_TYPEDEFINE: {
		tobj *target = tbycode_get_R(*iter) ?
			tcompo_env_get_obj(env, tbycode_get_L(*iter)) :
			tmp_obj(vm, tbycode_get_L(*iter));
		tobj *body = stk_top(vm);
		if (!target || target->type != tcompo ||
		    tobj_compo_type(target) != compo_ttypeval ||
		    !body || body->type != tcompo ||
		    tobj_compo_type(body) != compo_ttypeval ||
		    !ttypeval_define_recursive(
			(ttypeval *)target->val.v_tcompo,
			(ttypeval *)body->val.v_tcompo))
			twarn(ErrRuntime_Other, "recursive Type",
			      "invalid or repeated Type definition");
		stk_popc(vm);
	} break;
	case OP_LOOPAS:
		vm_loopas(vm,
			  tvm_instruction_cache(code_cache, *idx),
			  tbycode_get_L(*iter),
			  tbycode_get_R(*iter),
			  env);
		break;
	case OP_JPF:
		*idx += tbycode_get_U(*iter);
		iter += tbycode_get_U(*iter);
		break;
	case OP_JPB:
		*idx -= tbycode_get_U(*iter);
		iter -= tbycode_get_U(*iter);
		break;
	case OP_CJPFPOP:
		if (stk_top(vm)->type != tbool)
			twarn(ErrRuntime_ParamsType, "OP_CJPFPOP", "");
		if (stk_top(vm)->val.v_tbool == 0) {
			uint_cmds u = tbycode_get_U(*iter);
			*idx += u;
			iter += u;
			stk_pop(vm);
		} else
			stk_popc(vm);
		break;
	case OP_CJPBPOP:
		if (stk_top(vm)->type != tbool)
			twarn(ErrRuntime_ParamsType, "OP_CJPBPOP", "");
		if (stk_top(vm)->val.v_tbool != 0) {
			uint_cmds u = tbycode_get_U(*iter);
			*idx += u;
			iter += u;
			stk_pop(vm);
		} else
			stk_popc(vm);
		break;
	case OP_PUSHX: {
		uint_objs loc = tbycode_get_L(*iter);
		uint16_t addr = tbycode_get_R(*iter);
		tobj *src = vm_pushx_source(vm, env, loc, addr);
		vm->stk[vm->stklen] = *src;
		if (src->type == tcompo && src->val.v_tcompo)
			src->val.v_tcompo->refctr++;
		vm->stklen++;
	} break;
	case OP_PUSHI: {
		tobj v;
		tobj_set_nil(&v);
		vm_set_int_result(&v, cints[tbycode_get_U(*iter)]);
		stk_push(vm, &v);
	} break;
	case OP_PUSHFLT: {
		tobj v;
		tobj_set_nil(&v);
		vm_set_float_result(&v, cflts[tbycode_get_U(*iter)]);
		stk_push(vm, &v);
	} break;
	case OP_PUSHB: {
		tobj v;
		tobj_set_nil(&v);
		vm_set_bool_result(&v, (int)tbycode_get_U(*iter));
		stk_push(vm, &v);
	} break;
	case OP_PUSHS: {
		tobj v;
		tobj_set_nil(&v);
		tobj_set_compo(
			&v, (tcompo_v *)tstr_new(tstring_cstr(cstrs[tbycode_get_U(*iter)])));
		stk_push(vm, &v);
		tobj_set_nil(&v);
	} break;
	case OP_PUSHDICT: {
		tdict *d = tdict_new();
		uint_regs i, n = (uint_regs)tbycode_get_U(*iter);
		tobj *ps = stk_topn(vm, n);
		for (i = 0; i < n; i++)
			tdict_set_append(d, &ps[i]);
		stk_popcn(vm, n);
		tobj v;
		tobj_set_nil(&v);
		tobj_set_compo(&v, (tcompo_v *)d);
		stk_push(vm, &v);
		tobj_set_nil(&v);
	} break;
	case OP_PUSHINFO: {
		tobj v;
		tobj_set_nil(&v);
		vm_set_int_result(&v, (long)tbycode_get_U(*iter));
		stk_push(vm, &v);
	} break;
	case OP_IMPORT:
		vm_import(vm, (uint_csts)tbycode_get_U(*iter), cstrs, env);
		break;
	case OP_IDXR:
		vm_idxr(vm, (uint_regs)tbycode_get_U(*iter),
			tvm_instruction_cache(code_cache, *idx));
		break;
	case OP_EVAL:
	case OP_EVALCF:
	case OP_EVALSF: {
		uint_regs nparams = (uint_regs)tbycode_get_U(*iter);
		tobj *callable = stk_top(vm);
		tins_cache *cache = tvm_instruction_cache(code_cache, *idx);
		if (cache && cache->kind == tins_cache_eval_tfunc &&
		    callable->type == tcompo &&
		    callable->val.v_tcompo &&
		    callable->val.v_tcompo->vtable == cache->guard) {
			call->function = (tfunc *)callable->val.v_tcompo;
			call->params = stk_topn(vm, nparams + 1);
			call->nparams = nparams;
			call->return_stack_values = nparams + 1;
			call->current_function = 0;
			return texec_call;
		}
		if (callable->type == tcompo && callable->val.v_tcompo &&
		    callable->val.v_tcompo->vtable->get_compo_type_code() ==
			    compo_tfunc) {
			if (cache && cache->kind == tins_cache_empty) {
				cache->kind = tins_cache_eval_tfunc;
				cache->guard = callable->val.v_tcompo->vtable;
			} else if (cache &&
				   (cache->kind != tins_cache_eval_tfunc ||
				    cache->guard != callable->val.v_tcompo->vtable)) {
				cache->kind = tins_cache_polymorphic;
				cache->guard = NULL;
			}
			call->function = (tfunc *)callable->val.v_tcompo;
			call->params = stk_topn(vm, nparams + 1);
			call->nparams = nparams;
			call->return_stack_values = nparams + 1;
			call->current_function = 0;
			return texec_call;
		}
		if (cache && cache->kind == tins_cache_empty)
			cache->kind = tins_cache_polymorphic;
		vm_eval(vm, iter, env);
	} break;
	case OP_EVALTF: {
		uint_regs nparams = (uint_regs)tbycode_get_U(*iter);
		if (!env->owner_func)
			twarn(ErrRuntime_RefType,
			      "exec_tin", "missing current function");
		call->function = env->owner_func;
		call->params = stk_topn(vm, nparams);
		call->nparams = nparams;
		call->return_stack_values = nparams;
		call->current_function = 1;
		return texec_call;
	}
	case OP_IDXL:
		vm_idxl(vm,
			tbycode_get_L(*iter),
			tbycode_get_b(*iter),
			tbycode_get_i(*iter),
			env,
			tvm_instruction_cache(code_cache, *idx));
		break;
	case OP_PUSHF: {
		uint_cmds ncmds = tbycode_get_U(*iter);
		uint_regs nparams = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		uint_regs fregmax = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		uint_objs ntmps = (uint_objs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		uint_objs nobjs = (uint_objs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		tfunc *f = tfunc_new(
			nobjs, env, fregmax, ntmps, nparams, *idx + 1, ncmds);
		*idx += ncmds;
		iter += ncmds;
		tobj v;
		tobj_set_nil(&v);
		tobj_set_compo(&v, (tcompo_v *)f);
		stk_push(vm, &v);
		tobj_set_nil(&v);
	} break;
	case OP_ADD: {
		uint_regs type = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		VM_PARSE_BINOP(vm, operator_add, *iter, type, env);
	} break;
	case OP_SUB: {
		uint_regs type = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		VM_PARSE_BINOP(vm, operator_sub, *iter, type, env);
	} break;
	case OP_MUL: {
		uint_regs type = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		VM_PARSE_BINOP(vm, operator_mul, *iter, type, env);
	} break;
	case OP_DIV: {
		uint_regs type = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		VM_PARSE_BINOP(vm, operator_div, *iter, type, env);
	} break;
	case OP_MOD: {
		uint_regs type = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		VM_PARSE_BINOP(vm, operator_mod, *iter, type, env);
	} break;
	case OP_POW: {
		uint_regs type = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		VM_PARSE_BINOP(vm, operator_pow, *iter, type, env);
	} break;
	case OP_MMUL: {
		uint_regs type = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		VM_PARSE_BINOP(vm, operator_mmul, *iter, type, env);
	} break;
	case OP_EQ: {
		uint_regs type = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		VM_PARSE_BINOP(vm, operator_eq, *iter, type, env);
	} break;
	case OP_NE: {
		uint_regs type = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		VM_PARSE_BINOP(vm, operator_ne, *iter, type, env);
	} break;
	case OP_GE: {
		uint_regs type = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		VM_PARSE_BINOP(vm, operator_ge, *iter, type, env);
	} break;
	case OP_SG: {
		uint_regs type = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		VM_PARSE_BINOP(vm, operator_sg, *iter, type, env);
	} break;
	case OP_LE: {
		uint_regs type = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		VM_PARSE_BINOP(vm, operator_le, *iter, type, env);
	} break;
	case OP_SL: {
		uint_regs type = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		VM_PARSE_BINOP(vm, operator_sl, *iter, type, env);
	} break;
	case OP_AND: {
		uint_regs type = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		VM_PARSE_BINOP(vm, operator_and, *iter, type, env);
	} break;
	case OP_OR: {
		uint_regs type = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		VM_PARSE_BINOP(vm, operator_or, *iter, type, env);
	} break;
	case OP_BAND: {
		uint_regs type = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		VM_PARSE_BINOP(vm, operator_and, *iter, type, env);
	} break;
	case OP_BOR: {
		uint_regs type = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		VM_PARSE_BINOP(vm, operator_or, *iter, type, env);
	} break;
	case OP_POS:
		operator_pos(stk_top(vm));
		break;
	case OP_NEG:
		operator_neg(stk_top(vm));
		break;
	default:
		twarn(ErrRuntime_Other, "exec_tin", "invalid bytecode instruction");
	}
	return texec_normal;
}

#undef VM_PARSE_BINOP

static void tvm_resolve_error_context(
		void *opaque, const char **source, const char **file,
		uint64_t *line, uint64_t *column, uint_cmds *instruction)
{
	tvm *vm = (tvm *)opaque;
	if (!vm->error_instruction)
		return;
	*instruction = *vm->error_instruction;
	if (!vm->error_source_locs ||
	    *instruction >= vm->error_source_loc_count)
		return;
	tsource_loc *location = &vm->error_source_locs[*instruction];
	*source = location->source ? tstring_cstr(location->source) : NULL;
	*file = location->file ? tstring_cstr(location->file) : NULL;
	*line = location->line;
	*column = location->column;
}

void exec_tins(tvm *vm, uint_cmds from, uint_cmds ncmds, tcompo_env *env)
{
	twrapper *wrapper = tfunc_get_wrapper_from_env(env);
	tvm_code_cache *code_cache = tvm_get_code_cache(vm, wrapper);
	tbycode *cmdarr = wrapper->cmdarr;
	long *cints = wrapper->consts.cints;
	double *cflts = wrapper->consts.cflts;
	tstring **cstrs = wrapper->consts.cstrs;
	uint_cmds i = from;
	uint_cmds end = from + ncmds;
	tsource_loc *previous_source_locs = vm->error_source_locs;
	uint_cmds previous_source_loc_count = vm->error_source_loc_count;
	uint_cmds *previous_instruction = vm->error_instruction;
	int outermost = vm->execution_depth++ == 0;
	terror_runtime_context_state previous_error_context = { 0 };
	vm->error_source_locs = wrapper->source_locs;
	vm->error_source_loc_count = wrapper->ncmds;
	vm->error_instruction = &i;
	if (outermost)
		previous_error_context = terror_set_runtime_context_resolver(
			tvm_resolve_error_context, vm);

	uint32_t base_frame_depth = vm->frame_len;
	for (;;) {
		if (i >= end) {
			if (vm->frame_len == base_frame_depth)
				break;
			tvm_set_rev_empty(vm);
			goto resume_caller;
		}

		tins instruction = tbycode_ins(cmdarr[i]);
		if (instruction == OP_PUSHINFO && i + 1 < end) {
			uint_cmds metadata_instruction = i;
			i++;
			if (vm_try_fused_binop(vm,
						   cmdarr[i],
						   (uint_regs)tbycode_get_U(
							   cmdarr[metadata_instruction]),
						   env)) {
				i++;
				continue;
			}
			i = metadata_instruction;
		}
		tcall_request call;
		texec_action action = exec_tin(vm, &cmdarr[i], instruction,
				       code_cache, &call,
				       &i, end, env, cints, cflts, cstrs);
		if (action == texec_normal) {
			i++;
			continue;
		}
		if (action == texec_return) {
			if (vm->frame_len == base_frame_depth)
				break;
			goto resume_caller;
		}
		{
			tfunc *function = call.function;
			twrapper *callee_wrapper =
				tfunc_get_wrapper_from_env(&function->env);
			int tail_recursion = call.current_function &&
				i + 1 < end &&
				tbycode_ins(cmdarr[i + 1]) == OP_RET &&
				vm->frame_len > base_frame_depth;
			if (tail_recursion) {
				tvm_replace_current_call_frame(
					vm, function, call.params, call.nparams);
				env = &vm->frames[vm->frame_len - 1]->env;
			} else {
				tcall_frame *frame = tvm_push_call_frame(
					vm, function, call.params, call.nparams);
				frame->return_pc = i;
				frame->return_end = end;
				frame->return_stack_values =
					call.return_stack_values;
				frame->return_env = env;
				frame->return_wrapper = wrapper;
				env = &frame->env;
			}

			if (callee_wrapper != wrapper)
				code_cache = tvm_get_code_cache(vm, callee_wrapper);
			wrapper = callee_wrapper;
			cmdarr = wrapper->cmdarr;
			cints = wrapper->consts.cints;
			cflts = wrapper->consts.cflts;
			cstrs = wrapper->consts.cstrs;
			i = function->cmdloc;
			end = function->cmdloc + function->ncmds;
			vm->error_source_locs = wrapper->source_locs;
			vm->error_source_loc_count = wrapper->ncmds;
			continue;
		}

resume_caller: {
			tcall_frame *frame = vm->frames[vm->frame_len - 1];
			uint_cmds return_pc = frame->return_pc;
			uint_cmds return_end = frame->return_end;
			uint_regs return_stack_values = frame->return_stack_values;
			tcompo_env *return_env = frame->return_env;
			twrapper *return_wrapper = frame->return_wrapper;

			tvm_pop_call_frame(vm);
			stk_popcn(vm, return_stack_values);
			stk_push(vm, &vm->rev);
			tvm_set_rev_empty(vm);

			env = return_env;
			if (return_wrapper != wrapper)
				code_cache = tvm_get_code_cache(vm, return_wrapper);
			wrapper = return_wrapper;
			cmdarr = wrapper->cmdarr;
			cints = wrapper->consts.cints;
			cflts = wrapper->consts.cflts;
			cstrs = wrapper->consts.cstrs;
			i = return_pc + 1;
			end = return_end;
			vm->error_source_locs = wrapper->source_locs;
			vm->error_source_loc_count = wrapper->ncmds;
		}
	}
	vm->execution_depth--;
	vm->error_source_locs = previous_source_locs;
	vm->error_source_loc_count = previous_source_loc_count;
	vm->error_instruction = previous_instruction;
	if (outermost)
		terror_restore_runtime_context_resolver(previous_error_context);
}

void eval_bycodes(tvm *vm, uint_cmds from, tlib *lib)
{
	twrapper *wrapper = tlib_get_wrapper(lib);
	if (!wrapper)
		return;
	if (from > wrapper->ncmds)
		twarn(ErrRuntime_Other, "eval_bycodes", "invalid bytecode offset");
	exec_tins(vm, from, wrapper->ncmds - from, &lib->env);
}

/*===========================================================================*
 * 7. Built-in C Functions Registration
 *===========================================================================*/

void register_cppfuncs(tlib *lib)
{
	tdict *math_pkg;
	tdict *dense_pkg;
	tdict *time_pkg;
	tdict *types_pkg;

#define TAPAS_ROOT(name, implementation, arity, signature) \
	tlib_add_cppf(lib, #name, implementation, arity);
#define TAPAS_SESSION(name, implementation, signature)
#define TAPAS_PACKAGE(name) name##_pkg = tlib_add_pkg(lib, #name);
#define TAPAS_MEMBER(package, name, implementation, arity, signature) \
	tdict_add_cppf(package##_pkg, #name, implementation, arity);
#define TAPAS_TYPE(name, builtin, signature) do { \
		tobj key, value; \
		tobj_set_nil(&key); \
		tobj_set_nil(&value); \
		tobj_set_compo(&key, (tcompo_v *)tstr_new(#name)); \
		tobj_set_compo(&value, (tcompo_v *)ttypeval_builtin(builtin)); \
		tdict_set(types_pkg, &key, &value); \
		tobj_try_clear(&key); \
		tobj_set_nil(&value); \
	} while (0);
#include "stdlib.def"
#undef TAPAS_ROOT
#undef TAPAS_SESSION
#undef TAPAS_PACKAGE
#undef TAPAS_MEMBER
#undef TAPAS_TYPE
}
