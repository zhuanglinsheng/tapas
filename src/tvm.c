#include "Tapas/tvm.h"

#include "tapas/tcompile.h"
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

#define DEF_ALGO(fn_name, fll, fld, fdl, fdd)                                  \
	static inline int fn_name##_impl(ttypes t1,                            \
					 ttypes t2,                            \
					 const tobj *v1,                       \
					 const tobj *v2,                       \
					 tobj *vre)                            \
	{                                                                      \
		if (t1 == tint && t2 == tint) {                                \
			tobj_set_int(vre,                                      \
				     fll(v1->val.v_tint, v2->val.v_tint));     \
			return 1;                                              \
		}                                                              \
		if (t1 == tint && t2 == tfloat) {                             \
			tobj_set_float(                                       \
				vre, fld(v1->val.v_tint, v2->val.v_tfloat));  \
			return 1;                                              \
		}                                                              \
		if (t1 == tfloat && t2 == tint) {                             \
			tobj_set_float(                                       \
				vre, fdl(v1->val.v_tfloat, v2->val.v_tint));  \
			return 1;                                              \
		}                                                              \
		if (t1 == tfloat && t2 == tfloat) {                          \
			tobj_set_float(                                       \
				vre,                                           \
				fdd(v1->val.v_tfloat, v2->val.v_tfloat));    \
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

void operator_add(const tobj *v1, const tobj *v2, tobj *vre)
{
	if (add_impl(v1->type, v2->type, v1, v2, vre))
		return;
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

void operator_sub(const tobj *v1, const tobj *v2, tobj *vre)
{
	if (sub_impl(v1->type, v2->type, v1, v2, vre))
		return;
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

void operator_mul(const tobj *v1, const tobj *v2, tobj *vre)
{
	if (mul_impl(v1->type, v2->type, v1, v2, vre))
		return;
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

void operator_div(const tobj *v1, const tobj *v2, tobj *vre)
{
	if (div_impl(v1->type, v2->type, v1, v2, vre))
		return;
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
			tobj_set_bool(                                         \
				vre, (int)(v1->val.v_tint op v2->val.v_tint)); \
			return 1;                                              \
		}                                                              \
		if (t1 == tint && t2 == tfloat) {                             \
			tobj_set_bool(vre,                                     \
				      (int)((double)v1->val.v_tint op          \
						    v2->val.v_tfloat));       \
			return 1;                                              \
		}                                                              \
		if (t1 == tfloat && t2 == tint) {                             \
			tobj_set_bool(vre,                                     \
				      (int)(v1->val.v_tfloat op(double)       \
						    v2->val.v_tint));          \
			return 1;                                              \
		}                                                              \
		if (t1 == tfloat && t2 == tfloat) {                          \
			tobj_set_bool(vre,                                     \
				      (int)(v1->val.v_tfloat op               \
						    v2->val.v_tfloat));       \
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

void operator_eq(const tobj *v1, const tobj *v2, tobj *vre)
{
	if (eq_impl(v1->type, v2->type, v1, v2, vre)) return;
	if (v1->type == tcompo && v1->val.v_tcompo->vtable->op_eq) {
		v1->val.v_tcompo->vtable->op_eq(v1->val.v_tcompo, v2, 0, vre); return;
	}
	if (v2->type == tcompo && v2->val.v_tcompo->vtable->op_eq) {
		v2->val.v_tcompo->vtable->op_eq(v2->val.v_tcompo, v1, 1, vre); return;
	}
	tobj_set_bool(vre, tobj_identical(v1, v2));
}

void operator_ne(const tobj *v1, const tobj *v2, tobj *vre)
{
	if (ne_impl(v1->type, v2->type, v1, v2, vre)) return;
	if (v1->type == tcompo && v1->val.v_tcompo->vtable->op_ne) {
		v1->val.v_tcompo->vtable->op_ne(v1->val.v_tcompo, v2, 0, vre); return;
	}
	if (v2->type == tcompo && v2->val.v_tcompo->vtable->op_ne) {
		v2->val.v_tcompo->vtable->op_ne(v2->val.v_tcompo, v1, 1, vre); return;
	}
	tobj_set_bool(vre, !tobj_identical(v1, v2));
}

#define COMPO_CMP_BODY(field, impl, opname) do { \
	if (impl(v1->type, v2->type, v1, v2, vre)) return; \
	if (v1->type == tcompo && v1->val.v_tcompo->vtable->field) { \
		v1->val.v_tcompo->vtable->field(v1->val.v_tcompo, v2, 0, vre); return; \
	} \
	if (v2->type == tcompo && v2->val.v_tcompo->vtable->field) { \
		v2->val.v_tcompo->vtable->field(v2->val.v_tcompo, v1, 1, vre); return; \
	} \
	twarn(ErrRuntime_ParamsType, opname, "unsupported comparison"); \
} while (0)

void operator_sg(const tobj *v1, const tobj *v2, tobj *vre)
{
	COMPO_CMP_BODY(op_sg, sg_impl, "operator_sg");
}

void operator_sl(const tobj *v1, const tobj *v2, tobj *vre)
{
	COMPO_CMP_BODY(op_sl, sl_impl, "operator_sl");
}

void operator_ge(const tobj *v1, const tobj *v2, tobj *vre)
{
	COMPO_CMP_BODY(op_ge, ge_impl, "operator_ge");
}

void operator_le(const tobj *v1, const tobj *v2, tobj *vre)
{
	COMPO_CMP_BODY(op_le, le_impl, "operator_le");
}

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

#undef COMPO_CMP_BODY
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
	vm->frames = NULL;
	vm->frame_len = 0;
	vm->frame_cap = 0;
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
	for (uint32_t i = 0; i < vm->frame_cap; i++)
		free(vm->frames[i]);
	free(vm->frames);
	vm->frames = NULL;
	vm->frame_len = 0;
	vm->frame_cap = 0;
}

static tloop_state *tvm_loop_state(tvm *vm, uint_cmds ins_idx)
{
	uint_cmds i;
	for (i = 0; i < vm->loop_state_len; i++) {
		if (vm->loop_states[i].ins_idx == ins_idx)
			return &vm->loop_states[i];
	}
	if (vm->loop_state_len >= vm->loop_state_cap) {
		uint_cmds newcap = vm->loop_state_cap ? vm->loop_state_cap * 2 : 16;
		tloop_state *grown = (tloop_state *)realloc(
			vm->loop_states, newcap * sizeof(tloop_state));
		if (!grown)
			twarn(ErrRuntime_Other, "tvm_loop_state", "out of memory");
		vm->loop_states = grown;
		vm->loop_state_cap = newcap;
	}
	vm->loop_states[vm->loop_state_len].ins_idx = ins_idx;
	vm->loop_states[vm->loop_state_len].pos = 0;
	vm->loop_state_len++;
	return &vm->loop_states[vm->loop_state_len - 1];
}

static void tvm_reset_loop_states(tvm *vm)
{
	uint_cmds i;
	for (i = 0; i < vm->loop_state_len; i++)
		vm->loop_states[i].pos = 0;
}

static void tcall_frame_release(tcall_frame *fr, tvm *vm)
{
	uint_regs i;

	fr->tmps = vm->tmps;
	for (i = 0; i < vm->stklen; i++)
		tobj_ddc_ref_clear(&vm->stk[i]);
	vm->stklen = 0;

	tobj_array_free(&fr->tmps);
	tcompo_env_set_regmax(&fr->env, 0);
	tobj_array_free(&fr->env.base.objs);
	if (fr->params) {
		for (i = 0; i < fr->nparams; i++)
			tobj_ddc_ref_clear(&fr->params[i]);
		free(fr->params);
	}
	memset(fr, 0, sizeof(*fr));
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
	memset(fr, 0, sizeof(*fr));
	fr->func = f;
	fr->saved_tmps = vm->tmps;
	fr->saved_stk = vm->stk;
	fr->saved_regmax = vm->regmax;
	fr->saved_stklen = vm->stklen;
	fr->nparams = nparams;

	tcompo_env_init(&fr->env,
			tobj_array_get_cap(&tfunc_get_env(f)->base.objs),
			tcompo_env_get_father(tfunc_get_env(f)),
			tcompo_env_get_regmax(tfunc_get_env(f)),
			tcompo_env_get_tmpmax(tfunc_get_env(f)),
			tcompo_env_get_nparams(tfunc_get_env(f)),
			compo_tfunc);
	fr->env.owner_func = f;
	tobj_array_init(&fr->tmps, tcompo_env_get_tmpmax(tfunc_get_env(f)));

	if (nparams > 0) {
		fr->params = (tobj *)calloc(nparams, sizeof(tobj));
		if (!fr->params)
			twarn(ErrRuntime_Other, "tvm_push_call_frame", "out of memory");
		for (uint_regs i = 0; i < nparams; i++) {
			tobj_set_nil(&fr->params[i]);
			tobj_copy(&fr->params[i], &params[i]);
		}
	}

	if (tcompo_env_get_nparams(&fr->env) != UNDEF_NPARAMS) {
		for (uint_regs i = 0; i < nparams; i++) {
			tcompo_env_add_obj(&fr->env, UNDEF_NAMELOC);
			tcompo_env_set_obj(&fr->env, i, &fr->params[i]);
		}
	}
	tcompo_env_set_dynamic_nparams(&fr->env, nparams);
	tcompo_env_set_params(&fr->env, fr->params);

	vm->tmps = fr->tmps;
	vm->stk = tcompo_env_get_vmstack(&fr->env);
	vm->regmax = tcompo_env_get_regmax(&fr->env);
	vm->stklen = 0;
	return fr;
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

	tcall_frame_release(fr, vm);
	vm->frame_len--;

	vm->tmps = saved_tmps;
	vm->stk = saved_stk;
	vm->regmax = saved_regmax;
	vm->stklen = saved_stklen;
}

static void tvm_drop_call_frames(tvm *vm)
{
	while (vm->frame_len > 0)
		tvm_pop_call_frame(vm);
}

/* Stack helpers */
tobj *stk_at(tvm *vm, uint_regs loc)
{
	return &vm->stk[vm->stklen - loc - 1];
}

tobj *stk_top(tvm *vm)
{
	return &vm->stk[vm->stklen - 1];
}

tobj *stk_topn(tvm *vm, uint_regs n)
{
	return vm->stk + vm->stklen - n;
}

tobj *stk_free(tvm *vm)
{
	return &vm->stk[vm->stklen];
}

void stk_fill(tvm *vm)
{
	vm->stklen++;
}

uint_regs stk_len(tvm *vm)
{
	return vm->stklen;
}

void stk_pop(tvm *vm)
{
	vm->stklen--;
}

void stk_popc(tvm *vm)
{
	tobj_ddc_ref_clear(stk_top(vm));
	vm->stklen--;
}

void stk_popcn(tvm *vm, uint_regs n)
{
	uint_regs i;
	for (i = 0; i < n; i++)
		stk_popc(vm);
}

void stk_push(tvm *vm, const tobj *v)
{
	vm->stk[vm->stklen] = *v;
	if (v->type == tcompo && v->val.v_tcompo)
		v->val.v_tcompo->refctr++;
	vm->stklen++;
}

tobj *tmp_obj(tvm *vm, uint_objs loc)
{
	return tobj_array_get_obj(&vm->tmps, loc);
}

static tobj *vm_pushx_source(tvm *vm, tcompo_env *env, uint_objs slot, uint16_t addr)
{
	if (!tpushx_isenv(addr))
		return tobj_array_get_obj(&vm->tmps, slot);

	if (!tpushx_is_upval(addr))
		return tobj_array_get_obj(&env->base.objs, slot);

	tcompo_env_abstract *cur = &env->base;
	uint16_t depth = tpushx_depth(addr);
	for (uint16_t i = 0; i < depth; i++) {
		if (!cur->father_env)
			twarn(ErrRuntime_ObjUnfound, "OP_PUSHX", "");
		cur = cur->father_env;
	}
	return tobj_array_get_obj(&cur->objs, slot);
}

typedef void (*tbinop_fn)(const tobj *, const tobj *, tobj *);

static void vm_push_binop_result(tvm *vm,
				 tbinop_fn fn,
				 const tobj *left,
				 const tobj *right)
{
	tobj r;
	tobj_set_nil(&r);
	fn(left, right, &r);
	stk_push(vm, &r);
	tobj_try_clear(&r);
}

static void vm_parse_binop(
		tvm *vm, tbinop_fn fn, tbycode code, uint_regs type, tcompo_env *env)
{
	uint16_t left = tbycode_get_L(code);
	uint16_t right = tbycode_get_R(code);

	switch (type) {
	case 0: /* value value */
		fn(stk_at(vm, (uint_regs)left),
		   stk_at(vm, (uint_regs)right),
		   stk_at(vm, (uint_regs)right));
		stk_popc(vm);
		break;
	case 1: /* env value */
		fn(tcompo_env_get_obj(env, left),
		   stk_at(vm, (uint_regs)right),
		   stk_at(vm, (uint_regs)right));
		break;
	case 2: /* value env */
		fn(stk_at(vm, (uint_regs)left),
		   tcompo_env_get_obj(env, right),
		   stk_at(vm, (uint_regs)left));
		break;
	case 3: /* env env */
		vm_push_binop_result(vm,
				     fn,
				     tcompo_env_get_obj(env, left),
				     tcompo_env_get_obj(env, right));
		break;
	case 4: /* tmp value */
		fn(tmp_obj(vm, left),
		   stk_at(vm, (uint_regs)right),
		   stk_at(vm, (uint_regs)right));
		break;
	case 5: /* value tmp */
		fn(stk_at(vm, (uint_regs)left),
		   tmp_obj(vm, right),
		   stk_at(vm, (uint_regs)left));
		break;
	case 6: /* tmp tmp */
		vm_push_binop_result(vm, fn, tmp_obj(vm, left), tmp_obj(vm, right));
		break;
	case 7: /* env tmp */
		vm_push_binop_result(
			vm, fn, tcompo_env_get_obj(env, left), tmp_obj(vm, right));
		break;
	case 8: /* tmp env */
		vm_push_binop_result(
			vm, fn, tmp_obj(vm, left), tcompo_env_get_obj(env, right));
		break;
	default:
		twarn(ErrRuntime_Other, "vm_parse_binop", "invalid binop type");
	}
}

void tmp_add(tvm *vm)
{
	tobj_array_add_obj(&vm->tmps, UNDEF_NAMELOC);
}

void tmp_del(tvm *vm, uint_objs n)
{
	tobj_array_del_obj(&vm->tmps, n);
}

/* Index right */
void vm_idxr(tvm *vm, uint_regs nparams)
{
	tobj *obj = stk_top(vm);
	stk_pop(vm);
	if (obj->type != tcompo)
		twarn(ErrRuntime_RefType, "vm_idxr", "");
	tcompo_v *arr = obj->val.v_tcompo;
	tobj *params = stk_topn(vm, nparams);
	switch (arr->vtable->get_compo_type_code()) {
	case compo_tstr:
		tstr_idx((tstr *)arr, params, nparams, &vm->rev);
		break;
	case compo_tlist:
		tlist_idx((tlist *)arr, params, nparams, &vm->rev);
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
		tarr_idx(arr, params, nparams, &vm->rev);
		break;
	default:
		twarn(ErrRuntime_RefType, "vm_idxr", "unindexable type");
	}
	stk_push(vm, obj);
	stk_popcn(vm, 1 + nparams);
	stk_push(vm, &vm->rev);
	tvm_set_rev_empty(vm);
}

/* Index left */
void
vm_idxl(tvm *vm, uint_objs loc, uint_regs nparams, int isenv, tcompo_env *env)
{
	tobj *objp = isenv ? tcompo_env_get_obj(env, loc) : tmp_obj(vm, loc);
	if (objp->type != tcompo)
		twarn(ErrRuntime_RefType, "vm_idxl", "");
	tcompo_v *arr = objp->val.v_tcompo;
	tobj *rv = stk_at(vm, nparams);
	tobj *params = stk_topn(vm, nparams);
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
		tlist_iset((tlist *)arr, params, nparams, rv);
		break;
	case compo_tdarr:
	case compo_tbarr:
		tarr_iset(arr, params, nparams, rv);
		break;
	default:
		twarn(ErrRuntime_RefType, "vm_idxl", "");
	}
	stk_popcn(vm, 1 + nparams);
}

static tobj *vm_loop_target(tvm *vm, uint_objs idx, int isenv, tcompo_env *env)
{
	return isenv ? tcompo_env_get_obj(env, idx) : tmp_obj(vm, idx);
}

static void vm_loop_push_cond(tvm *vm, int has_next)
{
	tobj_set_bool(stk_free(vm), has_next);
	stk_fill(vm);
}

static void vm_loopias(tvm *vm, uint_cmds ins_idx, uint_objs idx, int isenv, tcompo_env *env)
{
	tobj *viter = stk_top(vm);
	if (viter->type != tcompo)
		twarn(ErrRuntime_RefType, "vm_loopias", "");
	tcompo_v *it = viter->val.v_tcompo;
	if (it->vtable->get_compo_type_code() != compo_titer)
		twarn(ErrRuntime_RefType, "vm_loopias", "");
	titer *p = (titer *)it;
	(void)ins_idx;
	long val = p->current;
	int has_next = (p->step > 0 && val < p->end) ||
		       (p->step < 0 && val > p->end);
	if (has_next) {
		tobj_set_int(vm_loop_target(vm, idx, isenv, env), val);
		p->current += p->step;
	} else {
		p->current = p->start;
	}
	vm_loop_push_cond(vm, has_next);
}

static void vm_looplas(tvm *vm, uint_cmds ins_idx, uint_objs idx, int isenv, tcompo_env *env)
{
	tobj *viter = stk_top(vm);
	if (viter->type != tcompo)
		twarn(ErrRuntime_RefType, "vm_looplas", "");
	tcompo_v *it = viter->val.v_tcompo;
	if (it->vtable->get_compo_type_code() != compo_tlist)
		twarn(ErrRuntime_RefType, "vm_looplas", "");
	tloop_state *state = tvm_loop_state(vm, ins_idx);
	tobj v;
	tobj_set_nil(&v);
	int has_next = tlist_next_at((tlist *)it, &state->pos, &v);
	if (has_next) {
		if (isenv)
			tcompo_env_set_obj(env, idx, &v);
		else
			tobj_array_set_obj(&vm->tmps, idx, &v);
		tobj_ddc_ref_clear(&v);
	}
	vm_loop_push_cond(vm, has_next);
}

/* Loop assignment */
void vm_loopas(tvm *vm, tbycode *iter, uint_cmds ins_idx, uint_objs idx, int isenv, tcompo_env *env)
{
	tobj *viter = stk_top(vm);
	if (viter->type != tcompo)
		twarn(ErrRuntime_RefType, "vm_loopas", "");
	tcompo_v *it = viter->val.v_tcompo;
	switch (it->vtable->get_compo_type_code()) {
	case compo_titer:
		*iter = tbycode_make_lr(OP_LOOPIAS, (uint16_t)idx, (uint16_t)isenv);
		vm_loopias(vm, ins_idx, idx, isenv, env);
		return;
	case compo_tlist:
		*iter = tbycode_make_lr(OP_LOOPLAS, (uint16_t)idx, (uint16_t)isenv);
		vm_looplas(vm, ins_idx, idx, isenv, env);
		return;
	case compo_ttypeval: {
		tloop_state *state = tvm_loop_state(vm, ins_idx);
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
		}
		vm_loop_push_cond(vm, has_next);
		return;
	}
	default:
		twarn(ErrRuntime_RefType, "vm_loopas", "unsupported iterator");
	}
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
void exec_tin(tvm *vm,
			    tbycode *iter,
			    uint_cmds *idx,
			    uint_cmds end,
			    tcompo_env *env,
			    long *cints,
			    double *cflts,
			    tstring **cstrs)
{
	tins ins = tbycode_ins(*iter);
	(void)end;
	switch (ins) {
	case OP_PASS:
		break;
	case OP_VCRT: {
		int isenv = (int)tbycode_get_R(*iter);
		uint_csts nl = tbycode_get_L(*iter);
		if (isenv)
			tcompo_env_add_obj(env, nl);
		else
			tmp_add(vm);
	} break;
	case OP_TMPDEL:
		tmp_del(vm, (uint_objs)tbycode_get_U(*iter));
		break;
		case OP_THIS: {
			tobj v;
			tobj_set_nil(&v);
			tcompo_env_copy_to_obj(env, &v);
			stk_push(vm, &v);
			tobj_set_nil(&v);
		} break;
		case OP_BASE: {
			tobj v;
			tobj_set_nil(&v);
			tcompo_env_copy_to_obj(tcompo_env_get_father(env), &v);
			stk_push(vm, &v);
			tobj_set_nil(&v);
	} break;
	case OP_BREAK:
		tvm_reset_loop_states(vm);
		while (tbycode_ins(*iter) != OP_JPB && *idx < end) {
			(*idx)++;
			iter++;
		}
		break;
	case OP_CONTI:
		while (tbycode_ins(*iter) != OP_JPB && *idx < end) {
			(*idx)++;
			iter++;
		}
		if (tbycode_ins(*iter) == OP_JPB) {
			(*idx)--;
			iter--;
		}
		break;
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
		return;
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
	case OP_LOOPAS:
		vm_loopas(vm,
			  iter,
			  *idx,
			  tbycode_get_L(*iter),
			  tbycode_get_R(*iter),
			  env);
		break;
	case OP_LOOPIAS:
		vm_loopias(vm,
			   *idx,
			   tbycode_get_L(*iter),
			   tbycode_get_R(*iter),
			   env);
		break;
	case OP_LOOPLAS:
		vm_looplas(vm,
			   *idx,
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
		tobj_set_int(&v, cints[tbycode_get_U(*iter)]);
		stk_push(vm, &v);
	} break;
	case OP_PUSHFLT: {
		tobj v;
		tobj_set_nil(&v);
		tobj_set_float(&v, cflts[tbycode_get_U(*iter)]);
		stk_push(vm, &v);
	} break;
	case OP_PUSHB: {
		tobj v;
		tobj_set_nil(&v);
		tobj_set_bool(&v, (int)tbycode_get_U(*iter));
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
		tobj_set_int(&v, (long)tbycode_get_U(*iter));
		stk_push(vm, &v);
	} break;
	case OP_IMPORT:
		vm_import(vm, (uint_csts)tbycode_get_U(*iter), cstrs, env);
		break;
	case OP_IDXR:
		vm_idxr(vm, (uint_regs)tbycode_get_U(*iter));
		break;
	case OP_EVAL:
	case OP_EVALCF:
	case OP_EVALSF:
		vm_eval(vm, iter, env);
		break;
	case OP_IDXL:
		vm_idxl(vm,
			tbycode_get_L(*iter),
			tbycode_get_b(*iter),
			tbycode_get_i(*iter),
			env);
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
		vm_parse_binop(vm, operator_add, *iter, type, env);
	} break;
	case OP_SUB: {
		uint_regs type = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		vm_parse_binop(vm, operator_sub, *iter, type, env);
	} break;
	case OP_MUL: {
		uint_regs type = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		vm_parse_binop(vm, operator_mul, *iter, type, env);
	} break;
	case OP_DIV: {
		uint_regs type = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		vm_parse_binop(vm, operator_div, *iter, type, env);
	} break;
	case OP_MOD: {
		uint_regs type = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		vm_parse_binop(vm, operator_mod, *iter, type, env);
	} break;
	case OP_POW: {
		uint_regs type = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		vm_parse_binop(vm, operator_pow, *iter, type, env);
	} break;
	case OP_MMUL: {
		uint_regs type = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		vm_parse_binop(vm, operator_mmul, *iter, type, env);
	} break;
	case OP_EQ: {
		uint_regs type = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		vm_parse_binop(vm, operator_eq, *iter, type, env);
	} break;
	case OP_NE: {
		uint_regs type = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		vm_parse_binop(vm, operator_ne, *iter, type, env);
	} break;
	case OP_GE: {
		uint_regs type = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		vm_parse_binop(vm, operator_ge, *iter, type, env);
	} break;
	case OP_SG: {
		uint_regs type = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		vm_parse_binop(vm, operator_sg, *iter, type, env);
	} break;
	case OP_LE: {
		uint_regs type = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		vm_parse_binop(vm, operator_le, *iter, type, env);
	} break;
	case OP_SL: {
		uint_regs type = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		vm_parse_binop(vm, operator_sl, *iter, type, env);
	} break;
	case OP_AND: {
		uint_regs type = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		vm_parse_binop(vm, operator_and, *iter, type, env);
	} break;
	case OP_OR: {
		uint_regs type = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		vm_parse_binop(vm, operator_or, *iter, type, env);
	} break;
	case OP_BAND: {
		uint_regs type = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		vm_parse_binop(vm, operator_and, *iter, type, env);
	} break;
	case OP_BOR: {
		uint_regs type = (uint_regs)stk_top(vm)->val.v_tint;
		stk_popc(vm);
		vm_parse_binop(vm, operator_or, *iter, type, env);
	} break;
	case OP_POS:
		operator_pos(stk_top(vm));
		break;
	case OP_NEG:
		operator_neg(stk_top(vm));
		break;
	default:
		break;
	}
}

void exec_tins(tvm *vm, uint_cmds from, uint_cmds ncmds, tcompo_env *env)
{
	twrapper *wrapper = tfunc_get_wrapper_from_env(env);
	tbycode *cmdarr = wrapper->cmdarr;
	long *cints = wrapper->consts.cints;
	double *cflts = wrapper->consts.cflts;
	tstring **cstrs = wrapper->consts.cstrs;

	uint_cmds i;
	for (i = from; i < from + ncmds; i++) {
		if (wrapper->source_locs) {
			tsource_loc *loc = &wrapper->source_locs[i];
			if (loc->source)
				terror_set_source_context_borrowed(tstring_cstr(loc->source));
			if (loc->file || loc->line || loc->column)
				terror_set_file_context_borrowed(
					loc->file ? tstring_cstr(loc->file) : NULL,
					loc->line,
					loc->column);
		}
		terror_set_instruction_context(i);
		exec_tin(vm, &cmdarr[i], &i, from + ncmds, env, cints, cflts, cstrs);
	}
	terror_clear_instruction_context();
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
