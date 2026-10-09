#include "tvm.h"
#include "compile/cache.h"
#include "compile/compiler.h"
#include "tapas/dsa/tstring.h"
#include "../dsa/thashtbl_internal.h"
#include "tapas/objects/tarray.h"
#include "tapas/objects/tdict.h"
#include "tenv.h"
#include "tapas/objects/titer.h"
#include "tapas/objects/tlist.h"
#include "tapas/objects/tcfn.h"
#include "tapas/objects/tpair.h"
#include "tapas/objects/tstr.h"
#include "tapas/objects/ttype.h"
#include "tapas/objects/trule.h"
#include "tapas/objects/trule_ir.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#define VM_UNLIKELY(condition) __builtin_expect(!!(condition), 0)
#else
#define VM_UNLIKELY(condition) (condition)
#endif

/* Comparison-aware OP_IDXR may keep a one-byte String unboxed until EQ/NE.
 * No other instruction observes this private representation. */
#define VM_INDEXED_BYTE_NAMELOC ((uint_csts)(UNDEF_NAMELOC + 1u))

static inline int vm_is_indexed_byte(const tobj *value)
{
	return value->type == tint &&
	       (uint_csts)value->name_loc == VM_INDEXED_BYTE_NAMELOC;
}

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
	tobj_set_compo(vre, (tcompo_v *)titer_new(v1->val.v_tint, v2->val.v_tint));
}

void operator_in(const tobj *v1, const tobj *v2, tobj *vre)
{
	int left_term = v1->type == tcompo && tobj_compo_type(v1) == compo_trule_term;
	int right_term = v2->type == tcompo && tobj_compo_type(v2) == compo_trule_term;
	if (left_term || right_term) {
		trule_term *left = left_term ? (trule_term *)v1->val.v_tcompo :
		                                trule_term_constant_new(v1);
		trule_term *right = right_term ? (trule_term *)v2->val.v_tcompo :
		                                  trule_term_constant_new(v2);
		tobj_set_compo(vre, (tcompo_v *)trule_term_in_new(left, right));
		return;
	}
	tobj_set_bool(vre, v2->type == tcompo && v2->val.v_tcompo
		&& tcompo_contains(v2->val.v_tcompo, v1));
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

#if defined(__GNUC__) || defined(__clang__)
#define TVM_ALWAYS_INLINE __attribute__((always_inline))
#else
#define TVM_ALWAYS_INLINE
#endif

#define DEF_ALGO(fn_name, fll, fld, fdl, fdd)                                  \
	static inline TVM_ALWAYS_INLINE int fn_name##_impl(                    \
				 ttypes t1,                                    \
				 ttypes t2,                                    \
				 const tobj *v1,                               \
				 const tobj *v2,                               \
				 tobj *vre)                                    \
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
	twarn(ErrRuntime_ParamsType, "operator_add", "unsupported type for +");
}

static inline TVM_ALWAYS_INLINE void operator_add(const tobj *v1, const tobj *v2,
						  tobj *vre)
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
	twarn(ErrRuntime_ParamsType, "operator_sub", "unsupported type for -");
}

static inline TVM_ALWAYS_INLINE void operator_sub(const tobj *v1, const tobj *v2,
						  tobj *vre)
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
	twarn(ErrRuntime_ParamsType, "operator_mul", "unsupported type for *");
}

static inline TVM_ALWAYS_INLINE void operator_mul(const tobj *v1, const tobj *v2,
						  tobj *vre)
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
	twarn(ErrRuntime_ParamsType, "operator_div", "unsupported type for /");
}

static inline TVM_ALWAYS_INLINE void operator_div(const tobj *v1, const tobj *v2,
						  tobj *vre)
{
	if (div_impl(v1->type, v2->type, v1, v2, vre))
		return;
	operator_div_slow(v1, v2, vre);
}

static inline TVM_ALWAYS_INLINE void operator_mod(const tobj *v1, const tobj *v2,
						  tobj *vre)
{
	/* 整数取模是条件表达式与寻址计算的热路径。 */
	if (v1->type == tint && v2->type == tint) {
		if (v2->val.v_tint == 0)
			twarn(ErrRuntime_DivIntZero, "operator_mod", "");
		tobj_set_int(vre, v1->val.v_tint % v2->val.v_tint);
		return;
	}
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
		twarn(ErrRuntime_ParamsType, "operator_pos", "unary + requires a number");
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
	if (value->type == tcompo && value->val.v_tcompo &&
	    value->val.v_tcompo->vtable->op_neg) {
		tcompo_v *self = value->val.v_tcompo;
		self->vtable->op_neg(self, value);
		return;
	}
	twarn(ErrRuntime_ParamsType, "operator_neg", "unsupported operand");
}

#define DEF_CMP(fn_name, op)                                                   \
	static inline TVM_ALWAYS_INLINE int fn_name##_impl(                    \
				 ttypes t1,                                    \
				 ttypes t2,                                    \
				 const tobj *v1,                               \
				 const tobj *v2,                               \
				 tobj *vre)                                    \
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
	if (vm_is_indexed_byte(v1) || vm_is_indexed_byte(v2)) {
		if (vm_is_indexed_byte(v1) && vm_is_indexed_byte(v2)) {
			vm_set_bool_result(vre, v1->val.v_tint == v2->val.v_tint);
			return;
		}
		const tobj *byte_value = vm_is_indexed_byte(v1) ? v1 : v2;
		const tobj *other = vm_is_indexed_byte(v1) ? v2 : v1;
		char byte = (char)byte_value->val.v_tint;
		tobj string = { 0 };
		tobj_set_compo(&string, (tcompo_v *)tstr_new_len(&byte, 1));
		operator_eq_slow(vm_is_indexed_byte(v1) ? &string : other,
			vm_is_indexed_byte(v1) ? other : &string, vre);
		tobj_try_clear(&string);
		return;
	}
	if (v1->type == tcompo && v2->type == tcompo &&
	    v1->val.v_tcompo && v2->val.v_tcompo &&
	    v1->val.v_tcompo->vtable == &tstr_vtable &&
	    v2->val.v_tcompo->vtable == &tstr_vtable) {
		vm_set_bool_result(vre, tstring_eq(
			((tstr *)v1->val.v_tcompo)->data,
			((tstr *)v2->val.v_tcompo)->data));
		return;
	}
	if (v1->type == tcompo && v1->val.v_tcompo->vtable->op_eq) {
		v1->val.v_tcompo->vtable->op_eq(v1->val.v_tcompo, v2, 0, vre); return;
	}
	if (v2->type == tcompo && v2->val.v_tcompo->vtable->op_eq) {
		v2->val.v_tcompo->vtable->op_eq(v2->val.v_tcompo, v1, 1, vre); return;
	}
	tobj_set_bool(vre, tobj_identical(v1, v2));
}

static inline TVM_ALWAYS_INLINE void operator_eq(const tobj *v1, const tobj *v2,
						  tobj *vre)
{
	/* 两个索引字节直接按值比较；单边字节交给保字符串语义的慢路径。 */
	if (vm_is_indexed_byte(v1) && vm_is_indexed_byte(v2)) {
		vm_set_bool_result(vre, v1->val.v_tint == v2->val.v_tint);
		return;
	}
	if (vm_is_indexed_byte(v1) || vm_is_indexed_byte(v2)) {
		operator_eq_slow(v1, v2, vre);
		return;
	}
	if (eq_impl(v1->type, v2->type, v1, v2, vre))
		return;
	operator_eq_slow(v1, v2, vre);
}

static void operator_ne_slow(const tobj *v1, const tobj *v2, tobj *vre)
{
	if (vm_is_indexed_byte(v1) || vm_is_indexed_byte(v2)) {
		if (vm_is_indexed_byte(v1) && vm_is_indexed_byte(v2)) {
			vm_set_bool_result(vre, v1->val.v_tint != v2->val.v_tint);
			return;
		}
		const tobj *byte_value = vm_is_indexed_byte(v1) ? v1 : v2;
		const tobj *other = vm_is_indexed_byte(v1) ? v2 : v1;
		char byte = (char)byte_value->val.v_tint;
		tobj string = { 0 };
		tobj_set_compo(&string, (tcompo_v *)tstr_new_len(&byte, 1));
		operator_ne_slow(vm_is_indexed_byte(v1) ? &string : other,
			vm_is_indexed_byte(v1) ? other : &string, vre);
		tobj_try_clear(&string);
		return;
	}
	if (v1->type == tcompo && v2->type == tcompo &&
	    v1->val.v_tcompo && v2->val.v_tcompo &&
	    v1->val.v_tcompo->vtable == &tstr_vtable &&
	    v2->val.v_tcompo->vtable == &tstr_vtable) {
		vm_set_bool_result(vre, !tstring_eq(
			((tstr *)v1->val.v_tcompo)->data,
			((tstr *)v2->val.v_tcompo)->data));
		return;
	}
	if (v1->type == tcompo && v1->val.v_tcompo->vtable->op_ne) {
		v1->val.v_tcompo->vtable->op_ne(v1->val.v_tcompo, v2, 0, vre); return;
	}
	if (v2->type == tcompo && v2->val.v_tcompo->vtable->op_ne) {
		v2->val.v_tcompo->vtable->op_ne(v2->val.v_tcompo, v1, 1, vre); return;
	}
	tobj_set_bool(vre, !tobj_identical(v1, v2));
}

static inline TVM_ALWAYS_INLINE void operator_ne(
		const tobj *v1, const tobj *v2, tobj *vre)
{
	if (vm_is_indexed_byte(v1) && vm_is_indexed_byte(v2)) {
		vm_set_bool_result(vre, v1->val.v_tint != v2->val.v_tint);
		return;
	}
	if (vm_is_indexed_byte(v1) || vm_is_indexed_byte(v2)) {
		operator_ne_slow(v1, v2, vre);
		return;
	}
	if (ne_impl(v1->type, v2->type, v1, v2, vre))
		return;
	operator_ne_slow(v1, v2, vre);
}

#define DEF_COMPO_CMP_OPERATOR(fn, field, impl, opname)                        \
	static void fn##_slow(const tobj *v1, const tobj *v2, tobj *vre)               \
	{                                                                              \
		if (v1->type == tcompo && v1->val.v_tcompo->vtable->field) {           \
			v1->val.v_tcompo->vtable->field(v1->val.v_tcompo, v2, 0, vre); \
			return;                                                        \
		} \
		if (v2->type == tcompo && v2->val.v_tcompo->vtable->field) {           \
			v2->val.v_tcompo->vtable->field(v2->val.v_tcompo, v1, 1, vre); \
			return;                                                        \
		} \
		twarn(ErrRuntime_ParamsType, opname, "unsupported comparison");        \
	}                                                                              \
	static inline TVM_ALWAYS_INLINE void fn(const tobj *v1, const tobj *v2,       \
						tobj *vre)                     \
	{                                                                              \
		if (impl(v1->type, v2->type, v1, v2, vre))                             \
			return;                                                        \
		fn##_slow(v1, v2, vre);                                                \
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
	else
		twarn(ErrRuntime_ParamsType, "operator_and", "");
}

void operator_or(const tobj *v1, const tobj *v2, tobj *vre)
{
	if (v1->type == tbool && v2->type == tbool)
		tobj_set_bool(vre, v1->val.v_tbool || v2->val.v_tbool);
	else if (v1->type == tcompo && v1->val.v_tcompo->vtable->op_or)
		v1->val.v_tcompo->vtable->op_or(v1->val.v_tcompo, v2, 0, vre);
	else if (v2->type == tcompo && v2->val.v_tcompo->vtable->op_or)
		v2->val.v_tcompo->vtable->op_or(v2->val.v_tcompo, v1, 1, vre);
	else
		twarn(ErrRuntime_ParamsType, "operator_or", "");
}

#undef DEF_COMPO_CMP_OPERATOR
#undef DEF_CMP


/*===========================================================================*
 * 3. Virtual Machine
 *===========================================================================*/

static void tvm_drop_call_frames(tvm *vm);

typedef enum {
	tins_cache_empty,
	tins_cache_loop_iter,
	tins_cache_loop_list,
	tins_cache_loop_str,
	tins_cache_idxr_list_int,
	tins_cache_idxr_str_int,
	tins_cache_idxr_dict_str,
	tins_cache_idxr_dense_int2,
	tins_cache_idxl_list_int,
	tins_cache_idxl_dense_int2,
	tins_cache_eval_tfunc,
	tins_cache_eval_cppfunc,
	tins_cache_eval_sessfunc,
	tins_cache_pushx_plain,
	tins_cache_polymorphic
} tins_cache_kind;

typedef struct {
	tcompo_vtable *guard;
	uint_cmds state_slot;
	uint8_t kind;
	const tcompo_v *key;   /* dict_str: the interned key object */
	uint_count slot;       /* dict_str: learned entry slot */
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
	cache->entries = nullptr;
	if (wrapper->ncmds == 0)
		return cache;

	cache->entries = (tins_cache *)calloc(
		cache->entry_count, sizeof(tins_cache));
	if (!cache->entries)
		twarn(ErrRuntime_Other, "tvm_get_code_cache", "out of memory");
	uint_cmds loop_slot = 0;
	for (uint_cmds i = 0; i < wrapper->ncmds; i++) {
		tins instruction = tbycode_ins(wrapper->cmdarr[i]);
		if (instruction == OP_LOOPAS || instruction == OP_LOOPRANGE)
			cache->entries[i].state_slot = loop_slot++;
	}
	return cache;
}

static inline tins_cache *tvm_instruction_cache(
		tvm_code_cache *cache, uint_cmds instruction)
{
	if (!cache || instruction >= cache->entry_count)
		return nullptr;
	return &cache->entries[instruction];
}

void tvm_init(tvm *vm, uint_objs tmpmax)
{
	tobj_array_init(&vm->tmps, tmpmax);
	vm->regmax = 0;
	vm->stk = nullptr;
	vm->stklen = 0;
	tobj_set_nil(&vm->rev);
	vm->loop_states = nullptr;
	vm->loop_state_len = 0;
	vm->loop_state_cap = 0;
	vm->code_caches = nullptr;
	vm->code_cache_len = 0;
	vm->code_cache_cap = 0;
	vm->frames = nullptr;
	vm->frame_len = 0;
	vm->frame_cap = 0;
	vm->error_source_locs = nullptr;
	vm->error_source_loc_count = 0;
	vm->error_instruction = nullptr;
	vm->execution_depth = 0;
	vm->antecedent_depth = 0;
	vm->rule_logic_values = nullptr;
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
	vm->loop_states = nullptr;
	vm->loop_state_len = 0;
	vm->loop_state_cap = 0;
	for (uint32_t i = 0; i < vm->code_cache_len; i++) {
		free(vm->code_caches[i].entries);
	}
	free(vm->code_caches);
	vm->code_caches = nullptr;
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
	vm->frames = nullptr;
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
		vm->loop_states[vm->loop_state_len].range_start = 0;
		vm->loop_states[vm->loop_state_len].range_step = 1;
		vm->loop_states[vm->loop_state_len].range_end = 0;
		vm->loop_states[vm->loop_state_len].iterator_slot = nullptr;
		vm->loop_state_len++;
	}
	return &vm->loop_states[state_slot];
}

/* 函数是否含循环指令：首次调用时扫描其指令区间并缓存在函数对象上。
 * 无循环的被调函数在帧交换时不需要保存/恢复调用方的循环状态。 */
static int vm_function_has_loops(tfunc *f)
{
	if (f->loop_state_kind == 0) {
		twrapper *wrapper = f->library ? f->library->wrapper :
			tfunc_get_wrapper_from_env(&f->env);
		int has = 0;
		if (wrapper) {
			uint_cmds end = f->cmdloc + f->ncmds;
			for (uint_cmds k = f->cmdloc; k < end; k++) {
				tins ins = tbycode_ins(wrapper->cmdarr[k]);
				if (ins == OP_LOOPAS || ins == OP_LOOPRANGE) {
					has = 1;
					break;
				}
			}
		}
		f->loop_state_kind = has ? 2 : 1;
	}
	return f->loop_state_kind == 2;
}

static void tvm_release_loop_iterator(tvm *vm, const tobj *stack_slot)
{
	for (uint_cmds i = 0; i < vm->loop_state_len; i++) {
		if (vm->loop_states[i].iterator_slot != stack_slot)
			continue;
		vm->loop_states[i].pos = 0;
		vm->loop_states[i].iterator_slot = nullptr;
	}
}

/* Mirrors tobj_array_set_len(array, 0) exactly (top-down release). */
static inline TVM_ALWAYS_INLINE void tvm_array_clear(tobj_array *array)
{
	while (array->len > 0) {
		array->len--;
		tobj *slot = &array->data[array->len];
		if (slot->type == tcompo)
			tobj_ddc_ref_clear(slot);
		else
			tobj_set_nil(slot);
	}
}

static inline TVM_ALWAYS_INLINE void tcall_frame_release(
	tcall_frame *fr, tvm *vm)
{
	uint_regs i;

	fr->tmps = vm->tmps;
	for (i = 0; i < vm->stklen; i++)
		tobj_ddc_ref_clear(&vm->stk[i]);
	vm->stklen = 0;
	tvm_array_clear(&fr->tmps);
	tvm_array_clear(&fr->tail_args);
	tvm_array_clear(&fr->env.base.objs);
	tobj_ddc_ref_clear(&fr->retained_callable);
	if (fr->func && vm_function_has_loops(fr->func)) {
		fr->loop_states = vm->loop_states;
		fr->loop_state_len = 0;
		fr->loop_state_cap = vm->loop_state_cap;
	}
}

static void tcall_frame_prepare(tcall_frame *fr, tfunc *f)
{
	if (fr->initialized && fr->func == f &&
	    fr->env.base.father_env == f->env.base.father_env) {
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
	fr->env.base.father_env = father ? &father->base : nullptr;
	fr->env.base.loc_in_father_env = father ?
		tobj_array_get_len(&father->base.objs) : 0;
	fr->env.tmpmax = temporary_capacity;
	fr->env.regmax = register_capacity;
	fr->env.nparams = prototype->nparams;
	fr->env.dynamic_nparams = 0;
	fr->env.compo_type = compo_tfunc;
	fr->env.owner_func = f;
}

/* A pooled frame is cleared before it is returned to the free portion of the
 * frame stack, so its local array has no live occupants here. Copy callers
 * retain their arguments; bytecode calls use the move variant below and
 * transfer the caller stack's ownership into the callee instead. */
static inline TVM_ALWAYS_INLINE void tcall_frame_transfer_params(
	tobj_array *locals, const tobj *params, uint_regs nparams)
{
	if (nparams == 0) {
		locals->len = 0;
		return;
	}
	if (nparams == 1) {
		locals->data[0] = params[0];
		if (locals->data[0].type == tcompo &&
		    locals->data[0].val.v_tcompo)
			locals->data[0].val.v_tcompo->refctr++;
		locals->len = 1;
		return;
	}
	if (nparams == 2) {
		locals->data[0] = params[0];
		locals->data[1] = params[1];
		if (locals->data[0].type == tcompo &&
		    locals->data[0].val.v_tcompo)
			locals->data[0].val.v_tcompo->refctr++;
		if (locals->data[1].type == tcompo &&
		    locals->data[1].val.v_tcompo)
			locals->data[1].val.v_tcompo->refctr++;
		locals->len = 2;
		return;
	}
	if (nparams == 3) {
		locals->data[0] = params[0];
		locals->data[1] = params[1];
		locals->data[2] = params[2];
		for (uint_regs i = 0; i < 3; i++)
			if (locals->data[i].type == tcompo &&
			    locals->data[i].val.v_tcompo)
				locals->data[i].val.v_tcompo->refctr++;
		locals->len = 3;
		return;
	}
	if (nparams == 4) {
		locals->data[0] = params[0];
		locals->data[1] = params[1];
		locals->data[2] = params[2];
		locals->data[3] = params[3];
		for (uint_regs i = 0; i < 4; i++)
			if (locals->data[i].type == tcompo &&
			    locals->data[i].val.v_tcompo)
				locals->data[i].val.v_tcompo->refctr++;
		locals->len = 4;
		return;
	}
	memcpy(locals->data, params, nparams * sizeof(tobj));
	for (uint_regs i = 0; i < nparams; i++)
		if (locals->data[i].type == tcompo &&
		    locals->data[i].val.v_tcompo)
			locals->data[i].val.v_tcompo->refctr++;
	locals->len = nparams;
}

static inline TVM_ALWAYS_INLINE void tcall_frame_move_params(
	tobj_array *locals, tobj *params, uint_regs nparams)
{
	if (nparams == 0) {
		locals->len = 0;
		return;
	}
	if (nparams == 1) {
		locals->data[0] = params[0];
		tobj_set_nil(&params[0]);
		locals->len = 1;
		return;
	}
	if (nparams == 2) {
		locals->data[0] = params[0];
		locals->data[1] = params[1];
		tobj_set_nil(&params[0]);
		tobj_set_nil(&params[1]);
		locals->len = 2;
		return;
	}
	if (nparams == 3) {
		locals->data[0] = params[0];
		locals->data[1] = params[1];
		locals->data[2] = params[2];
		tobj_set_nil(&params[0]);
		tobj_set_nil(&params[1]);
		tobj_set_nil(&params[2]);
		locals->len = 3;
		return;
	}
	if (nparams == 4) {
		locals->data[0] = params[0];
		locals->data[1] = params[1];
		locals->data[2] = params[2];
		locals->data[3] = params[3];
		tobj_set_nil(&params[0]);
		tobj_set_nil(&params[1]);
		tobj_set_nil(&params[2]);
		tobj_set_nil(&params[3]);
		locals->len = 4;
		return;
	}
	memcpy(locals->data, params, nparams * sizeof(tobj));
	for (uint_regs i = 0; i < nparams; i++)
		tobj_set_nil(&params[i]);
	locals->len = nparams;
}

static inline void tcall_frame_assign_params(
	tcall_frame *frame, tobj *params, uint_regs nparams,
	int move_params)
{
	tobj_array *locals = &frame->env.base.objs;
	if (nparams > locals->capacity)
		tobj_array_try_expand(locals, nparams);
	if (move_params)
		tcall_frame_move_params(locals, params, nparams);
	else
		tcall_frame_transfer_params(locals, params, nparams);
}

static tcall_frame *tvm_push_call_frame(
	tvm *vm, tfunc *f, tobj *params, uint_regs nparams,
	int move_params)
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
			tobj_set_nil(&vm->frames[i]->retained_callable);
		}
		vm->frame_cap = newcap;
	}

	tcall_frame *fr = vm->frames[vm->frame_len++];
	/* The func pointer alone is not identity: a freed function's memory
	 * may be reused by an unrelated function, so the frame's cached
	 * environment must also match before it can be reused unchanged. */
	if (fr->initialized && fr->func == f &&
	    fr->env.base.father_env == f->env.base.father_env)
		fr->env.owner_func = f;
	else
		tcall_frame_prepare(fr, f);
	fr->saved_tmps = vm->tmps;
	fr->saved_stk = vm->stk;
	fr->saved_stklen = vm->stklen;
	if (vm_function_has_loops(f)) {
		fr->saved_loop_states = vm->loop_states;
		fr->saved_loop_state_len = vm->loop_state_len;
		fr->saved_loop_state_cap = vm->loop_state_cap;
	}

	if (fr->env.nparams != UNDEF_NPARAMS) {
		tcall_frame_assign_params(fr, params, nparams, move_params);
		fr->env.params = fr->env.base.objs.data;
	} else {
		fr->env.params = params;
	}
	fr->env.dynamic_nparams = nparams;

	vm->tmps = fr->tmps;
	vm->stk = fr->env.vmstack;
	vm->regmax = fr->env.regmax;
	vm->stklen = 0;
	if (vm_function_has_loops(f)) {
		vm->loop_states = fr->loop_states;
		vm->loop_state_len = 0;
		vm->loop_state_cap = fr->loop_state_cap;
	}
	return fr;
}

/* Inline fast path for the steady state of hot calls: the pooled frame
 * already serves this exact function with fixed arity. Anything else
 * (pool growth, prepare miss, dynamic arity, tight capacity) falls back
 * to tvm_push_call_frame. */
static inline TVM_ALWAYS_INLINE tcall_frame *tvm_push_call_frame_fast(
	tvm *vm, tfunc *f, tobj *params, uint_regs nparams)
{
	if (vm->frame_len >= vm->frame_cap)
		return nullptr;
	tcall_frame *fr = vm->frames[vm->frame_len];
	if (!fr->initialized || fr->func != f ||
	    fr->env.base.father_env != f->env.base.father_env ||
	    fr->env.nparams == UNDEF_NPARAMS ||
	    fr->env.nparams != nparams ||
	    fr->env.base.objs.capacity < nparams)
		return nullptr;
	fr->env.owner_func = f;
	fr->saved_tmps = vm->tmps;
	fr->saved_stk = vm->stk;
	fr->saved_stklen = vm->stklen;
	if (vm_function_has_loops(f)) {
		fr->saved_loop_states = vm->loop_states;
		fr->saved_loop_state_len = vm->loop_state_len;
		fr->saved_loop_state_cap = vm->loop_state_cap;
		vm->loop_states = fr->loop_states;
		vm->loop_state_len = 0;
		vm->loop_state_cap = fr->loop_state_cap;
	}
	tcall_frame_move_params(&fr->env.base.objs, params, nparams);
	fr->env.params = fr->env.base.objs.data;
	fr->env.dynamic_nparams = nparams;
	vm->tmps = fr->tmps;
	vm->stk = fr->env.vmstack;
	vm->regmax = fr->env.regmax;
	vm->stklen = 0;
	vm->frame_len++;
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

	if (frame->initialized && frame->func == function &&
	    frame->env.base.father_env == function->env.base.father_env)
		frame->env.owner_func = function;
	else
		tcall_frame_prepare(frame, function);
	if (frame->env.nparams != UNDEF_NPARAMS) {
		tcall_frame_assign_params(
			frame, frame->tail_args.data, nparams, 1);
		frame->tail_args.len = 0;
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

static inline TVM_ALWAYS_INLINE void tvm_pop_call_frame(tvm *vm)
{
	if (vm->frame_len == 0)
		twarn(ErrRuntime_Other, "tvm_pop_call_frame", "empty frame stack");
	tcall_frame *fr = vm->frames[vm->frame_len - 1];
	tobj_array saved_tmps = fr->saved_tmps;
	tobj *saved_stk = fr->saved_stk;
	uint_regs saved_stklen = fr->saved_stklen;
	tloop_state *saved_loop_states = fr->saved_loop_states;
	uint_cmds saved_loop_state_len = fr->saved_loop_state_len;
	uint_cmds saved_loop_state_cap = fr->saved_loop_state_cap;

	tcall_frame_release(fr, vm);
	vm->frame_len--;

	vm->tmps = saved_tmps;
	vm->stk = saved_stk;
	vm->stklen = saved_stklen;
	if (vm_function_has_loops(fr->func)) {
		vm->loop_states = saved_loop_states;
		vm->loop_state_len = saved_loop_state_len;
		vm->loop_state_cap = saved_loop_state_cap;
	}
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

/* Bytecode calls move their arguments into the callee frame. On return those
 * caller slots are therefore nil; only a generic call's callable still owns a
 * reference. Replace the whole call expression with the returned value in one
 * step instead of popping every argument and retaining the result again. */
static inline TVM_ALWAYS_INLINE void stk_finish_tapas_call(
	tvm *vm, uint_regs stack_values, int has_callable)
{
	uint_regs result_slot = vm->stklen - stack_values;
	if (has_callable) {
		tobj *callable = &vm->stk[vm->stklen - 1];
		if (callable->type == tcompo)
			tobj_ddc_ref_clear(callable);
		else
			tobj_set_nil(callable);
	}
	vm->stk[result_slot] = vm->rev;
	tobj_set_nil(&vm->rev);
	vm->stklen = result_slot + 1;
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

static inline TVM_ALWAYS_INLINE tobj *vm_pushx_source(tvm *vm, tcompo_env *env,
						      uint_objs slot,
						      uint16_t addr)
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

static inline tobj *vm_direct_slot(tcompo_env *env, uint_objs slot, uint16_t depth)
{
	tcompo_env_abstract *owner = &env->base;
	for (uint16_t i = 0; i < depth; i++) {
		if (!owner->father_env)
			twarn(ErrRuntime_ObjUnfound, "direct native call", "");
		owner = owner->father_env;
	}
	return vm_array_slot(&owner->objs, slot);
}

/* Composite operator hooks (RealArray/BoolArray arithmetic, comparisons and
 * logic) deliver a fresh result at refctr zero; the stack slot that receives
 * it must own one reference so its later popc releases exactly one. */
static inline TVM_ALWAYS_INLINE void vm_own_result(tobj *slot)
{
	if (slot->type == tcompo && slot->val.v_tcompo &&
	    slot->val.v_tcompo->refctr == 0)
		slot->val.v_tcompo->refctr++;
}

/* Scalar-only fast dispatch for the borrowed-left peephole. The impls
 * carry the full type matrix and error paths, so this re-dispatches rather
 * than re-implements; indexed bytes keep their string-aware EQ/NE slow path. */
static inline TVM_ALWAYS_INLINE int vm_binop_fast(
		tins instruction, const tobj *left, const tobj *right, tobj *result)
{
	switch (instruction) {
	case OP_ADD:
		return add_impl(left->type, right->type, left, right, result);
	case OP_SUB:
		return sub_impl(left->type, right->type, left, right, result);
	case OP_MUL:
		return mul_impl(left->type, right->type, left, right, result);
	case OP_DIV:
		return div_impl(left->type, right->type, left, right, result);
	case OP_EQ:
		if (vm_is_indexed_byte(left) || vm_is_indexed_byte(right))
			return 0;
		return eq_impl(left->type, right->type, left, right, result);
	case OP_NE:
		if (vm_is_indexed_byte(left) || vm_is_indexed_byte(right))
			return 0;
		return ne_impl(left->type, right->type, left, right, result);
	case OP_SG:
		return sg_impl(left->type, right->type, left, right, result);
	case OP_SL:
		return sl_impl(left->type, right->type, left, right, result);
	case OP_GE:
		return ge_impl(left->type, right->type, left, right, result);
	case OP_LE:
		return le_impl(left->type, right->type, left, right, result);
	default:
		return 0;
	}
}

static inline int vm_is_binop_instruction(tins instruction)
{
	return (instruction >= OP_ADD && instruction <= OP_OR) ||
		instruction == OP_BAND || instruction == OP_BOR;
}

static inline int vm_apply_binop(tins instruction, const tobj *left,
				 const tobj *right, tobj *result)
{
	if (!vm_is_binop_instruction(instruction))
		return 0;
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
		if (vm_is_indexed_byte(left) || vm_is_indexed_byte(right))
			operator_eq_slow(left, right, result);
		else if (!eq_impl(left->type, right->type, left, right, result))
			operator_eq_slow(left, right, result);
		break;
	case OP_NE:
		if (vm_is_indexed_byte(left) || vm_is_indexed_byte(right))
			operator_ne_slow(left, right, result);
		else if (!ne_impl(left->type, right->type, left, right, result))
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
	case OP_BAND:
		operator_and(left, right, result);
		break;
	case OP_OR:
	case OP_BOR:
		operator_or(left, right, result);
		break;
	default:
		return 0;
	}
	return 1;
}

static inline int vm_try_borrowed_left_binop(
		tvm *vm, const tobj *left, tbycode operation)
{
	if (!vm_is_binop_instruction(tbycode_ins(operation)) ||
	    tbycode_get_L(operation) != 0 || tbycode_get_R(operation) != 1 ||
	    stk_len(vm) == 0)
		return 0;
	tins instruction = tbycode_ins(operation);
	int done = vm_binop_fast(instruction, left, stk_top(vm), stk_top(vm)) ||
		   vm_apply_binop(instruction, left, stk_top(vm), stk_top(vm));
	if (done)
		vm_own_result(stk_top(vm));
	return done;
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
		cache->guard = nullptr;
	}
}

static void vm_list_idx_int(tlist *list, long index, tobj *result)
{
	uint_count len = list->items.len;
	if (index < 0)
		index += (long)len;
	if (index < 0 || (uint_objs)index >= len)
		twarn(ErrRuntime_IdxOutRange, "list index", "");
	*result = list->items.data[index];
	if (result->type == tcompo && result->val.v_tcompo)
		result->val.v_tcompo->refctr++;
}

/* Direct vtable dispatch for the dominant single-int reads. Unlike the
 * tins_cache path this needs no cache slot access and never degrades; the
 * bounds and error behaviour mirror vm_list_idx_int / vm_str_idx_int. */
static inline TVM_ALWAYS_INLINE int vm_idxr_fast(
		tcompo_v *arr, tobj *params, uint_regs nparams,
		int comparison_hint, tobj *result)
{
	if (nparams != 1 || params[0].type != tint)
		return 0;
	long index = params[0].val.v_tint;
	if (arr->vtable == &tlist_vtable) {
		tlist *list = (tlist *)arr;
		uint_count len = list->items.len;
		if (index < 0)
			index += (long)len;
		if (index < 0 || (uint_objs)index >= len)
			twarn(ErrRuntime_IdxOutRange, "list index", "");
		*result = list->items.data[index];
		/* Own the result before the receiver is popped: the receiver's
		 * release may free the container holding this value. */
		if (result->type == tcompo && result->val.v_tcompo)
			result->val.v_tcompo->refctr++;
		return 1;
	}
	if (arr->vtable == &tstr_vtable) {
		tstr *str = (tstr *)arr;
		size_t len = tstring_len(str->data);
		if (index < 0)
			index += (long)len;
		if (index < 0 || (size_t)index >= len)
			twarn(ErrRuntime_IdxOutRange, "string index", "");
		if (comparison_hint) {
			*result = (tobj){
				.type = tint,
				.name_loc = (int)VM_INDEXED_BYTE_NAMELOC,
				.val.v_tint =
					(unsigned char)tstring_cstr(str->data)[index]
			};
			return 1;
		}
		tobj_set_compo(
			result,
			(tcompo_v *)tstr_new_len(
				tstring_cstr(str->data) + index, 1));
		return 1;
	}
	if (arr->vtable == &tdict_vtable) {
		/* Integer keys dominate numeric maps. Probe the collision chain
		 * inline all the way; an empty bucket proves absence and raises
		 * the same error the general path would. */
		thashtbl *items = ((tdict *)arr)->items;
		if (items->capacity == 0)
			twarn(ErrRuntime_ObjUnfound, "tdict_get", "");
		uint64_t hash = thashtbl_hash_int((uint64_t)index);
		uint_count mask = items->capacity - 1;
		uint_count idx = (uint_count)(hash & mask);
		for (;;) {
			thash_entry *entry = &items->entries[idx];
			if (entry->hash == 0)
				twarn(ErrRuntime_ObjUnfound, "tdict_get", "");
			if (entry->hash == hash && entry->key.type == tint &&
			    entry->key.val.v_tint == index) {
				*result = entry->value;
				if (result->type == tcompo &&
				    result->val.v_tcompo)
					result->val.v_tcompo->refctr++;
				return 1;
			}
			idx = (idx + 1) & mask;
		}
	}
	return 0;
}

static void vm_str_idx_int(tstr *str, long index, int comparison_hint, tobj *result)
{
	size_t len = tstring_len(str->data);
	if (index < 0)
		index += (long)len;
	if (index < 0 || (size_t)index >= len)
		twarn(ErrRuntime_IdxOutRange, "string index", "");
	if (comparison_hint) {
		*result = (tobj){
			.type = tint,
			.name_loc = (int)VM_INDEXED_BYTE_NAMELOC,
			.val.v_tint = (unsigned char)tstring_cstr(str->data)[index]
		};
		return;
	}
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

static void vm_dense_idx_int2(tcompo_v *array, const tobj *params, tobj *result)
{
	size_t offset = vm_dense_int2_offset(array, params);
	if (array->vtable == &tdarr_vtable)
		vm_set_float_result(result, ((tdarr *)array)->data[offset]);
	else
		vm_set_bool_result(result, ((tbarr *)array)->data[offset] != 0);
}

static void vm_dense_iset_int2(tcompo_v *array, const tobj *params, const tobj *value)
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

static inline TVM_ALWAYS_INLINE void vm_slot_move(tobj *slot,
						 const tobj *owned);
static inline TVM_ALWAYS_INLINE void vm_consume_cjpop(
	tvm *vm, const tbycode *cmdarr, uint_cmds *pc, uint_cmds end);
static inline TVM_ALWAYS_INLINE void vm_consume_popcov(
	tvm *vm, tcompo_env *env, tbycode *cmdarr, uint_cmds *pc,
	uint_cmds end);

/* After a fusion consumes instructions up to `next`, a trailing OP_JPB (the
 * loop back-edge) can be consumed as well. Returns the index to store in the
 * program counter, given that the dispatch loop increments it once more. */
static inline TVM_ALWAYS_INLINE uint_cmds vm_fused_next(
		const tbycode *cmdarr, uint_cmds end, uint_cmds next)
{
	if (next < end) {
		tins kind = tbycode_ins(cmdarr[next]);
		if (kind == OP_JPB)
			return next - (uint_cmds)tbycode_get_U(cmdarr[next]);
		if (kind == OP_JPF)
			return next + (uint_cmds)tbycode_get_U(cmdarr[next]);
	}
	return next - 1;
}

/* Jump threading: a taken conditional jump whose landing instruction is
 * itself an unconditional jump can be followed immediately, since the
 * compiler routinely emits "skip body -> loop back-edge" shapes. Returns
 * the program counter to store, given the dispatch loop's final i++. */
static inline TVM_ALWAYS_INLINE uint_cmds vm_jump_landing(
	const tbycode *cmdarr, uint_cmds end, uint_cmds landing)
{
	if (landing < end) {
		tins kind = tbycode_ins(cmdarr[landing]);
		if (kind == OP_JPB)
			return landing - (uint_cmds)tbycode_get_U(cmdarr[landing]);
		if (kind == OP_JPF)
			return landing + (uint_cmds)tbycode_get_U(cmdarr[landing]);
	}
	return landing - 1;
}

/* Fused "compare with a freshly produced operand, then branch": handles a
 * named-left comparison whose right operand the current instruction just
 * produced, and consumes the following conditional jump, all without stack
 * traffic. Returns 0 when the operand types or the pattern need the general
 * path. Indexed bytes keep their string-aware EQ/NE semantics there. */
static inline TVM_ALWAYS_INLINE int vm_cmp_branch_fused(
	tvm *vm, tcompo_env *env, tbycode *cmdarr, uint_cmds *pc,
	uint_cmds end, const tobj *right)
{
	if (*pc + 2 >= end)
		return 0;
	tbycode cmp = cmdarr[*pc + 1];
	tbycode jump = cmdarr[*pc + 2];
	tins jk = tbycode_ins(jump);
	tins op = tbycode_ins(cmp);
	/* 先校验操作码再读 named 标志：PUSHI 立即数的标志位恰好落在
	 * binop 的 named 位上，非比较指令不得进入该字段的解释。 */
	if ((op != OP_EQ && op != OP_NE && op != OP_SG && op != OP_SL &&
	     op != OP_GE && op != OP_LE) ||
	    !tbycode_binop_named(cmp) ||
	    (jk != OP_CJPFPOP && jk != OP_CJPBPOP))
		return 0;
	const tobj *left = vm_pushx_source(
		vm, env, tbycode_get_L(cmp), tbycode_binop_address(cmp));
	tobj result;
	int ok;
	switch (op) {
	case OP_EQ:
		ok = !vm_is_indexed_byte(left) && !vm_is_indexed_byte(right) &&
			eq_impl(left->type, right->type, left, right, &result);
		break;
	case OP_NE:
		ok = !vm_is_indexed_byte(left) && !vm_is_indexed_byte(right) &&
			ne_impl(left->type, right->type, left, right, &result);
		break;
	case OP_SG:
		ok = sg_impl(left->type, right->type, left, right, &result);
		break;
	case OP_SL:
		ok = sl_impl(left->type, right->type, left, right, &result);
		break;
	case OP_GE:
		ok = ge_impl(left->type, right->type, left, right, &result);
		break;
	case OP_LE:
		ok = le_impl(left->type, right->type, left, right, &result);
		break;
	default:
		return 0;
	}
	if (!ok)
		return 0;
	int taken = jk == OP_CJPFPOP ? !result.val.v_tbool :
		result.val.v_tbool;
	if (taken)
		*pc = vm_jump_landing(cmdarr, end,
				      *pc + 3u + (uint_cmds)tbycode_get_U(jump));
	else
		*pc += 2;
	return 1;
}

/* Fused named arithmetic with a freshly produced right operand:
 * `slot = slot OP value` stores straight into the POPCOV target, while the
 * bare form leaves the result on the stack. Both skip the operand's stack
 * round trip; the store form also consumes trailing jumps (loop back-edges,
 * branch-chain skips). Returns 0 for the general path. Numeric results are
 * never nil, so the POPCOV nil check cannot fire on this path. */
static inline TVM_ALWAYS_INLINE int vm_arith_named_fused(
	tvm *vm, tcompo_env *env, tbycode *cmdarr, uint_cmds *pc,
	uint_cmds end, const tobj *right)
{
	if (*pc + 1 >= end)
		return 0;
	tbycode arith = cmdarr[*pc + 1];
	tins op = tbycode_ins(arith);
	/* 同上：操作码先于 named 标志。 */
	if ((op != OP_ADD && op != OP_SUB && op != OP_MUL && op != OP_DIV) ||
	    !tbycode_binop_named(arith))
		return 0;
	const tobj *left = vm_pushx_source(
		vm, env, tbycode_get_L(arith), tbycode_binop_address(arith));
	tobj result;
	int ok;
	switch (op) {
	case OP_ADD:
		ok = add_impl(left->type, right->type, left, right, &result);
		break;
	case OP_SUB:
		ok = sub_impl(left->type, right->type, left, right, &result);
		break;
	case OP_MUL:
		ok = mul_impl(left->type, right->type, left, right, &result);
		break;
	case OP_DIV:
		ok = div_impl(left->type, right->type, left, right, &result);
		break;
	default:
		return 0;
	}
	if (!ok)
		return 0;
	if (*pc + 2 < end && tbycode_ins(cmdarr[*pc + 2]) == OP_POPCOV) {
		tbycode store = cmdarr[*pc + 2];
		tobj_array *slots =
			tbycode_get_R(store) ? &env->base.objs : &vm->tmps;
		uint_objs dloc = (uint_objs)tbycode_get_L(store);
		if (dloc >= slots->len)
			return 0;
		vm_slot_move(&slots->data[dloc], &result);
		*pc = vm_fused_next(cmdarr, end, *pc + 3);
		return 1;
	}
	/* `recv[slot +/- c]`：索引常量偏移惯用法，读出结果直接入栈。 */
	if (*pc + 2 < end && result.type == tint &&
	    tbycode_ins(cmdarr[*pc + 2]) == OP_IDXR &&
	    tbycode_idxr_named(cmdarr[*pc + 2]) &&
	    tbycode_idxr_count(cmdarr[*pc + 2]) == 1 &&
	    !tbycode_idxr_slice(cmdarr[*pc + 2])) {
		tbycode idxr = cmdarr[*pc + 2];
		const tobj *object = vm_pushx_source(
			vm, env, tbycode_idxr_named_slot(idxr),
			tbycode_idxr_named_address(idxr));
		if (object->type == tcompo && object->val.v_tcompo) {
			tobj *dest = stk_free(vm);
			if (vm_idxr_fast(object->val.v_tcompo, &result, 1,
					 tbycode_idxr_compare(idxr), dest)) {
				vm->stklen++;
				*pc += 2;
				vm_consume_cjpop(vm, cmdarr, pc, end);
				vm_consume_popcov(vm, env, cmdarr, pc, end);
				return 1;
			}
		}
	}
	/* Immediate results carry no reference; place them directly. */
	vm->stk[vm->stklen] = result;
	vm->stklen++;
	*pc += 1;
	return 1;
}

/* Store an owned value through a following POPCOV (existing slot) or
 * VCRT-init (fresh slot) instruction. Returns nonzero when consumed. */
static inline TVM_ALWAYS_INLINE int vm_store_owned_consumed(
	tvm *vm, tcompo_env *env, tbycode *cmdarr, uint_cmds *pc,
	uint_cmds end, const tobj *owned)
{
	if (*pc + 1 >= end)
		return 0;
	tbycode store = cmdarr[*pc + 1];
	tins kind = tbycode_ins(store);
	if (kind == OP_POPCOV) {
		tobj_array *slots =
			tbycode_get_R(store) ? &env->base.objs : &vm->tmps;
		uint_objs dloc = (uint_objs)tbycode_get_L(store);
		if (dloc >= slots->len)
			return 0;
		if (owned->type == tnil)
			twarn(ErrRuntime_AssignNil, "OP_POPCOV", "");
		vm_slot_move(&slots->data[dloc], owned);
		*pc = vm_fused_next(cmdarr, end, *pc + 2);
		return 1;
	}
	if (kind == OP_VCRT && (tbycode_get_R(store) & TVCRT_INIT_FLAG)) {
		if (owned->type == tnil)
			twarn(ErrRuntime_AssignNil, "OP_VCRT", "");
		tobj_array *slots = (tbycode_get_R(store) & TVCRT_ENV_FLAG) ?
			&env->base.objs : &vm->tmps;
		vm_add_slot(slots, (uint_csts)tbycode_get_L(store));
		slots->data[slots->len - 1] = *owned;
		*pc = vm_fused_next(cmdarr, end, *pc + 2);
		return 1;
	}
	return 0;
}

/* A produced value followed by POPCOV stores into the target slot directly,
 * skipping the store instruction's dispatch; the slot takes over the stack
 * top's owned reference. Falls back (returns without progress) when the
 * target needs the father-environment walk. */
static inline TVM_ALWAYS_INLINE void vm_consume_popcov(
	tvm *vm, tcompo_env *env, tbycode *cmdarr, uint_cmds *pc, uint_cmds end)
{
	if (*pc + 1 >= end)
		return;
	tins kind = tbycode_ins(cmdarr[*pc + 1]);
	if (kind != OP_POPCOV && kind != OP_VCRT)
		return;
	tobj *top = stk_top(vm);
	if (vm_store_owned_consumed(vm, env, cmdarr, pc, end, top))
		stk_pop(vm);
}

#define VM_NATIVE_FUSED_MAX_PARAMS 4

/* Fused direct native call whose arguments come from consecutive
 * PUSHX/PUSHI instructions: resolve the slot/immediate operands into a
 * local array (native callbacks borrow their parameters for the call's
 * duration only) and invoke without any stack traffic. The result is
 * consumed by a following POPN discard, conditional jump, or POPCOV store;
 * otherwise it lands on the stack with its reference moved over. Returns 0
 * when the pattern or the callable does not fit. */
/* Materialize one owned reference for a composite sitting in vm->rev.
 * Fresh producers deliver refctr zero; borrowers deliver an uncounted
 * alias. The idxr pipeline always hands out exactly one owned reference,
 * so both states become refctr >= 1 here. */
static inline TVM_ALWAYS_INLINE void vm_own_rev(tvm *vm)
{
	if (vm->rev.type == tcompo && vm->rev.val.v_tcompo &&
	    vm->rev.val.v_tcompo->refctr == 0)
		vm->rev.val.v_tcompo->refctr++;
}

/* Index right */
static void vm_idxr_value(tvm *vm, tcompo_v *arr, tobj *params,
			  uint_regs nparams, int comparison_hint,
			  int slice_form, tins_cache *cache)
{
	if (slice_form) {
		if (nparams == 2 && params[0].type == tint &&
		    params[1].type == tint) {
			if (arr->vtable == &tlist_vtable) {
				tlist_slice_index((tlist *)arr, params, &vm->rev);
				vm_own_rev(vm);
				return;
			}
			if (arr->vtable == &tstr_vtable) {
				tstr_slice_index((tstr *)arr, params, &vm->rev);
				vm_own_rev(vm);
				return;
			}
		}
		/* Other indexables keep the Pair protocol. */
		tobj pair;
		tobj_set_nil(&pair);
		tobj_set_compo(&pair,
			(tcompo_v *)tpair_new(&params[1], &params[0]));
		tcompo_index(arr, &pair, 1, &vm->rev);
		tobj_try_clear(&pair);
		vm_own_rev(vm);
		return;
	}
	/* Dictionaries and libraries dominate struct-like member access;
	 * read them directly instead of through the index capability. */
	if (nparams == 1 && arr->vtable == &tdict_vtable) {
		tdict *dict = (tdict *)arr;
		const tobj *key = &params[0];
		/* Constant-string keys (interned pool objects) hit the same
		 * entry slot across structurally identical dictionaries, so
		 * remember the slot per call site and re-validate it. */
		if (cache && cache->kind == tins_cache_idxr_dict_str &&
		    key->type == tcompo &&
		    key->val.v_tcompo == cache->key &&
		    cache->slot < thashtbl_capacity(dict->items)) {
			const tobj *value = thashtbl_get_entry_at(
				dict->items, cache->slot, key);
			if (value) {
				vm->rev = *value;
				if (vm->rev.type == tcompo &&
				    vm->rev.val.v_tcompo)
					vm->rev.val.v_tcompo->refctr++;
				return;
			}
		}
		uint_count slot = 0;
		const tobj *value = thashtbl_find(dict->items, key, &slot);
		if (value) {
			if (cache && cache->kind == tins_cache_empty) {
				cache->kind = tins_cache_idxr_dict_str;
				cache->guard = arr->vtable;
				cache->key = key->val.v_tcompo;
				cache->slot = slot;
			} else if (cache &&
				   (cache->kind != tins_cache_idxr_dict_str ||
				    cache->guard != arr->vtable ||
				    cache->key != key->val.v_tcompo)) {
				cache->kind = tins_cache_polymorphic;
				cache->guard = nullptr;
				cache->key = nullptr;
			}
			vm->rev = *value;
			if (vm->rev.type == tcompo && vm->rev.val.v_tcompo)
				vm->rev.val.v_tcompo->refctr++;
			return;
		}
		twarn(ErrRuntime_ObjUnfound, "tdict_get", "");
	}
	if (nparams == 1 && arr->vtable == &tlib_vtable) {
		tlib_idx((tlib *)arr, &params[0], 1, &vm->rev);
		vm_own_rev(vm);
		return;
	}
	if (cache && cache->kind == tins_cache_idxr_list_int &&
	    cache->guard == arr->vtable && nparams == 1 &&
	    params[0].type == tint) {
		vm_list_idx_int((tlist *)arr, params[0].val.v_tint, &vm->rev);
		return;
	}
	if (cache && cache->kind == tins_cache_idxr_str_int &&
	    cache->guard == arr->vtable && nparams == 1 &&
	    params[0].type == tint) {
		vm_str_idx_int((tstr *)arr, params[0].val.v_tint,
			       comparison_hint, &vm->rev);
		return;
	}
	if (cache && cache->kind == tins_cache_idxr_dense_int2 &&
	    cache->guard == arr->vtable && nparams == 2 &&
	    params[0].type == tint && params[1].type == tint) {
		vm_dense_idx_int2(arr, params, &vm->rev);
		return;
	}
	if (arr->vtable == &tstr_vtable && nparams == 1 &&
	    params[0].type == tint) {
		tins_cache_observe(cache, tins_cache_idxr_str_int, arr->vtable);
		vm_str_idx_int((tstr *)arr, params[0].val.v_tint,
			       comparison_hint, &vm->rev);
	} else if (arr->vtable == &tlist_vtable && nparams == 1 &&
		   params[0].type == tint) {
		tins_cache_observe(cache, tins_cache_idxr_list_int, arr->vtable);
		vm_list_idx_int((tlist *)arr, params[0].val.v_tint, &vm->rev);
	} else if ((arr->vtable == &tdarr_vtable || arr->vtable == &tbarr_vtable) &&
		   nparams == 2 && params[0].type == tint &&
		   params[1].type == tint) {
		tins_cache_observe(cache, tins_cache_idxr_dense_int2, arr->vtable);
		vm_dense_idx_int2(arr, params, &vm->rev);
	} else {
		if (cache && cache->kind == tins_cache_empty)
			cache->kind = tins_cache_polymorphic;
		tcompo_index(arr, params, nparams, &vm->rev);
		vm_own_rev(vm);
	}
}

/* Dispatch-site hot prefix for indexed reads. Resolves the dominant
 * single-int reads on List and Dictionary without entering the general
 * out-of-line handlers, so the fast path carries no call frame and no
 * operand re-decoding. The result overwrites the key's stack slot directly:
 * the key is an immediate that owns no reference, and the fast reader leaves
 * an owned value behind, so the slot simply becomes the result. Returns zero
 * for anything it does not resolve; the caller falls back to the general
 * handler. */
static inline TVM_ALWAYS_INLINE int vm_idxr_slot_hot(tvm *vm, const tobj *obj,
						     tbycode instruction)
{
	if (tbycode_idxr_count(instruction) != 1 ||
	    tbycode_idxr_slice(instruction) ||
	    obj->type != tcompo || !obj->val.v_tcompo)
		return 0;
	tobj *key_slot = stk_top(vm);
	/* vm_idxr_fast reads the key before writing the result and already
	 * owns the composite result, so the slot simply takes it over. */
	return vm_idxr_fast(obj->val.v_tcompo, key_slot, 1,
			    tbycode_idxr_compare(instruction), key_slot);
}

/* Stack form: the receiver itself occupies the top stack slot, so it must be
 * released after the read; the result takes over the key's slot below it. */
static inline TVM_ALWAYS_INLINE int vm_idxr_stack_hot(tvm *vm,
						      tbycode instruction)
{
	if (tbycode_idxr_count(instruction) != 1 ||
	    tbycode_idxr_slice(instruction))
		return 0;
	tobj *obj = stk_top(vm);
	if (obj->type != tcompo || !obj->val.v_tcompo)
		return 0;
	tobj *key_slot = stk_at(vm, 1);
	if (!vm_idxr_fast(obj->val.v_tcompo, key_slot, 1,
			  tbycode_idxr_compare(instruction), key_slot))
		return 0;
	stk_popc(vm);
	return 1;
}

static void vm_idxr(tvm *vm, tbycode instruction, tins_cache *cache)
{
	uint_regs nparams = (uint_regs)tbycode_idxr_count(instruction);
	tobj *obj = stk_top(vm);
	if (obj->type != tcompo || !obj->val.v_tcompo)
		twarn(ErrRuntime_RefType, "vm_idxr", "");
	if (!vm_idxr_fast(obj->val.v_tcompo, stk_topn(vm, nparams + 1), nparams,
			  tbycode_idxr_compare(instruction), &vm->rev))
		vm_idxr_value(vm, obj->val.v_tcompo, stk_topn(vm, nparams + 1),
			      nparams, tbycode_idxr_compare(instruction),
			      tbycode_idxr_slice(instruction), cache);
	stk_popcn(vm, 1 + nparams);
	/* The reader owns one protective reference (the receiver's release
	 * may free the container holding this value). Push uniformizes the
	 * stack convention (+1, like every other producer); releasing the
	 * reader's reference afterwards leaves exactly one owned count. */
	stk_push(vm, &vm->rev);
	if (vm->rev.type == tcompo && vm->rev.val.v_tcompo)
		vm->rev.val.v_tcompo->refctr--;
	tobj_set_nil(&vm->rev);
}

static void vm_idxr_borrowed(tvm *vm, const tobj *obj,
			      tbycode instruction, tins_cache *cache)
{
	uint_regs nparams = (uint_regs)tbycode_idxr_count(instruction);
	if (obj->type != tcompo || !obj->val.v_tcompo)
		twarn(ErrRuntime_RefType, "vm_idxr", "");
	if (!vm_idxr_fast(obj->val.v_tcompo, stk_topn(vm, nparams), nparams,
			  tbycode_idxr_compare(instruction), &vm->rev))
		vm_idxr_value(vm, obj->val.v_tcompo, stk_topn(vm, nparams),
			      nparams, tbycode_idxr_compare(instruction),
			      tbycode_idxr_slice(instruction), cache);
	stk_popcn(vm, nparams);
	stk_push(vm, &vm->rev);
	if (vm->rev.type == tcompo && vm->rev.val.v_tcompo)
		vm->rev.val.v_tcompo->refctr--;
	tobj_set_nil(&vm->rev);
}

/* Mirrors thashtbl's stored-value identity check: composites compare by
 * pointer, immediates by payload. */
static inline TVM_ALWAYS_INLINE int vm_same_stored_value(const tobj *a,
							 const tobj *b)
{
	if (a->type != b->type)
		return 0;
	switch (a->type) {
	case tcompo:
		return a->val.v_tcompo == b->val.v_tcompo;
	case tint:
		return a->val.v_tint == b->val.v_tint;
	case tfloat:
		return a->val.v_tfloat == b->val.v_tfloat;
	case tbool:
		return a->val.v_tbool == b->val.v_tbool;
	default:
		return a->type == tnil;
	}
}

static inline TVM_ALWAYS_INLINE void vm_retain(const tobj *obj)
{
	if (obj->type == tcompo && obj->val.v_tcompo)
		obj->val.v_tcompo->refctr++;
}

/* Move an owned value into an existing slot, releasing the old content.
 * The incoming reference becomes the slot's own reference. */
static inline TVM_ALWAYS_INLINE void vm_slot_move(tobj *slot, const tobj *owned)
{
	/* 定态下目标槽与来值同为 Int：类型与名字已就位，只搬运负载。 */
	if (slot->type == tint && owned->type == tint) {
		slot->val.v_tint = owned->val.v_tint;
		return;
	}
	if (slot->type == tcompo)
		tobj_ddc_ref_clear(slot);
	*slot = *owned;
}

/* Integer-key dictionary write shared by the IDXL dispatch prefix, the
 * push-free store fusion and the general vm_idxl handler: replace on hit,
 * insert on a free slot when no rehash is due, and defer to thashtbl_set
 * for growth and empty tables. */
static inline TVM_ALWAYS_INLINE void vm_dict_set_int(thashtbl *items,
						     const tobj *key,
						     const tobj *rv)
{
	if (items->capacity == 0) {
		thashtbl_set(items, key, rv);
		return;
	}
	uint64_t hash = thashtbl_hash_int((uint64_t)key->val.v_tint);
	uint_count mask = items->capacity - 1;
	uint_count idx = (uint_count)(hash & mask);
	uint_count first_deleted = items->capacity;
	thash_entry *hit = nullptr;
	for (;;) {
		thash_entry *candidate = &items->entries[idx];
		if (candidate->hash == 0)
			break;
		if (candidate->hash == 1) {
			if (first_deleted == items->capacity)
				first_deleted = idx;
		} else if (candidate->hash == hash &&
			   candidate->key.type == tint &&
			   candidate->key.val.v_tint == key->val.v_tint) {
			hit = candidate;
			break;
		}
		idx = (idx + 1) & mask;
	}
	if (hit) {
		if (!vm_same_stored_value(&hit->value, rv)) {
			vm_retain(rv);
			tobj_ddc_ref_clear(&hit->value);
			hit->value = *rv;
		}
		return;
	}
	if (!thashtbl_needs_rehash(items)) {
		thash_entry *target = &items->entries
			[first_deleted != items->capacity ?
				 first_deleted :
				 idx];
		if (target->hash == 0)
			items->used++;
		target->key = *key;
		target->value = *rv;
		target->hash = hash;
		vm_retain(key);
		vm_retain(rv);
		items->len++;
		return;
	}
	thashtbl_set(items, key, rv);
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
	/* Dictionary writes are struct-field assignments in the common case;
	 * set the key directly instead of through the capability dispatch.
	 * Integer keys resolve inline: replace on hit, insert on a free slot
	 * when no rehash is due, and fall back to thashtbl_set otherwise.
	 * Constant-string keys hit the same entry slot across structurally
	 * identical dictionaries, so remember the slot per call site. */
	if (nparams == 1 && arr->vtable == &tdict_vtable) {
		tdict *dict = (tdict *)arr;
		const tobj *key = &params[0];
		thashtbl *items = dict->items;
		if (key->type == tint) {
			vm_dict_set_int(items, key, rv);
			goto finish;
		}
		if (cache && cache->kind == tins_cache_idxr_dict_str &&
		    key->type == tcompo &&
		    key->val.v_tcompo == cache->key &&
		    cache->slot < thashtbl_capacity(dict->items) &&
		    thashtbl_set_at(dict->items, cache->slot, key, rv)) {
			goto finish;
		}
		uint_count slot = thashtbl_set(dict->items, key, rv);
		if (cache && cache->kind == tins_cache_empty) {
			cache->kind = tins_cache_idxr_dict_str;
			cache->guard = arr->vtable;
			cache->key = key->type == tcompo ?
				key->val.v_tcompo : nullptr;
			cache->slot = slot;
		} else if (cache &&
			   (cache->kind != tins_cache_idxr_dict_str ||
			    cache->guard != arr->vtable ||
			    cache->key != (key->type == tcompo ?
					key->val.v_tcompo : nullptr))) {
			cache->kind = tins_cache_polymorphic;
			cache->guard = nullptr;
			cache->key = nullptr;
		}
		goto finish;
	}
	if (cache && cache->kind == tins_cache_idxl_list_int &&
	    cache->guard == arr->vtable && nparams == 1 &&
	    params[0].type == tint) {
		long index = params[0].val.v_tint;
		uint_count len = ((tlist *)arr)->items.len;
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
	switch (arr->vtable->compo_code) {
	case compo_tlist:
		if (nparams == 1 && params[0].type == tint) {
			tins_cache_observe(cache, tins_cache_idxl_list_int,
					   arr->vtable);
			long index = params[0].val.v_tint;
			uint_count len = ((tlist *)arr)->items.len;
			if (index < 0)
				index += (long)len;
			if (index < 0 || (uint_objs)index >= len)
				twarn(ErrRuntime_IdxOutRange,
				      "list assignment", "");
			tlist_set_at((tlist *)arr, (uint_objs)index, rv);
		} else {
			if (cache && cache->kind == tins_cache_empty)
				cache->kind = tins_cache_polymorphic;
			tcompo_index_set(arr, params, nparams, rv);
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
			tcompo_index_set(arr, params, nparams, rv);
		}
		break;
	default:
		if (cache && cache->kind == tins_cache_empty)
			cache->kind = tins_cache_polymorphic;
		tcompo_index_set(arr, params, nparams, rv);
	}
	finish:
	stk_popcn(vm, 1 + nparams);
}

static void vm_loop_push_cond(tvm *vm, int has_next)
{
	vm_set_bool_result(stk_free(vm), has_next);
	stk_fill(vm);
}

static inline TVM_ALWAYS_INLINE tobj *vm_loop_target(tvm *vm, uint_objs idx, int isenv, tcompo_env *env)
{
	/* The common case targets a live temporary slot; resolve it without
	 * the bounds-checking helper calls. */
	if (!isenv)
		return idx < vm->tmps.len ? &vm->tmps.data[idx] :
			tmp_obj(vm, idx);
	if (idx < env->base.objs.len)
		return &env->base.objs.data[idx];
	return tcompo_env_get_obj(env, idx);
}

static void vm_loop_range(tvm *vm, tloop_state *state, uint_objs idx,
			  int isenv, tcompo_env *env, titer *iterator)
{
	long value = iterator->start + state->pos * iterator->step;
	int has_next = (iterator->step > 0 && value < iterator->end) ||
		       (iterator->step < 0 && value > iterator->end);
	if (has_next) {
		vm_set_int_result(vm_loop_target(vm, idx, isenv, env), value);
		state->pos++;
	} else {
		state->pos = 0;
		state->iterator_slot = nullptr;
	}
	vm_loop_push_cond(vm, has_next);
}

/* Advance an inline range loop by one step and report whether the body
 * should run. The condition value itself is left to the caller: the fused
 * dispatch consumes it as a jump decision without a stack round trip. */
static inline TVM_ALWAYS_INLINE int vm_loop_range_step(tvm *vm,
		tins_cache *cache, uint_objs idx, int isenv, tcompo_env *env)
{
	tloop_state *state = cache->state_slot < vm->loop_state_len ?
		&vm->loop_states[cache->state_slot] :
		tvm_loop_state(vm, cache->state_slot);
	if (!state->iterator_slot) {
		if (stk_len(vm) < 2 || stk_top(vm)->type != tint ||
		    stk_at(vm, 1)->type != tint)
			twarn(ErrRuntime_ParamsType, "range loop", "Int bounds required");
		state->range_start = stk_top(vm)->val.v_tint;
		state->range_step = 1;
		state->range_end = stk_at(vm, 1)->val.v_tint;
		state->pos = 0;
		stk_popcn(vm, 2);
		tobj_set_nil(stk_free(vm));
		stk_fill(vm);
		state->iterator_slot = stk_top(vm);
	}
	long value = state->range_start + state->pos * state->range_step;
	int has_next = value < state->range_end;
	if (has_next) {
		/* 定态下循环变量槽已是 Int；只更新负载。 */
		tobj *target = vm_loop_target(vm, idx, isenv, env);
		if (target->type == tint)
			target->val.v_tint = value;
		else
			vm_set_int_result(target, value);
		state->pos++;
	} else {
		/* Reset on exhaustion instead of relying on the exit POPN to
		 * recognize the iterator slot by address; re-entry always
		 * re-reads the bounds. */
		state->pos = 0;
		state->iterator_slot = nullptr;
	}
	return has_next;
}

static void vm_loop_list(tvm *vm, tloop_state *state, uint_objs idx,
			 int isenv, tcompo_env *env, tlist *list)
{
	if ((uint_objs)state->pos < list->items.len) {
		tobj *target = vm_loop_target(vm, idx, isenv, env);
		tobj_copy(target, &list->items.data[state->pos++]);
		vm_loop_push_cond(vm, 1);
		return;
	}
	state->pos = 0;
	state->iterator_slot = nullptr;
	vm_loop_push_cond(vm, 0);
}

static void vm_loop_string(tvm *vm, tloop_state *state, uint_objs idx,
			   int isenv, tcompo_env *env, tstr *string)
{
	size_t length = tstring_len(string->data);
	if (state->pos >= 0 && (size_t)state->pos < length) {
		tobj *target = vm_loop_target(vm, idx, isenv, env);
		tobj_set_compo(target, (tcompo_v *)tstr_new_len(
			tstring_cstr(string->data) + state->pos, 1));
		state->pos++;
		vm_loop_push_cond(vm, 1);
		return;
	}
	state->pos = 0;
	state->iterator_slot = nullptr;
	vm_loop_push_cond(vm, 0);
}

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
	if (state->iterator_slot != viter) {
		state->pos = 0;
		state->iterator_slot = viter;
	}
	if (cache->kind == tins_cache_loop_iter &&
	    it->vtable == cache->guard) {
		vm_loop_range(vm, state, idx, isenv, env, (titer *)it);
		return;
	}
	if (cache->kind == tins_cache_loop_list &&
	    it->vtable == cache->guard) {
		vm_loop_list(vm, state, idx, isenv, env, (tlist *)it);
		return;
	}
	if (cache->kind == tins_cache_loop_str &&
	    it->vtable == cache->guard) {
		vm_loop_string(vm, state, idx, isenv, env, (tstr *)it);
		return;
	}
	if (it->vtable == &titer_vtable) {
		tins_cache_observe(cache, tins_cache_loop_iter, it->vtable);
		vm_loop_range(vm, state, idx, isenv, env, (titer *)it);
		return;
	}
	if (it->vtable == &tlist_vtable) {
		tins_cache_observe(cache, tins_cache_loop_list, it->vtable);
		vm_loop_list(vm, state, idx, isenv, env, (tlist *)it);
		return;
	}
	if (it->vtable == &tstr_vtable) {
		tins_cache_observe(cache, tins_cache_loop_str, it->vtable);
		vm_loop_string(vm, state, idx, isenv, env, (tstr *)it);
		return;
	}
	if (cache->kind == tins_cache_empty)
		cache->kind = tins_cache_polymorphic;
	tobj value;
	tobj_set_nil(&value);
	int has_next = tcompo_next(it, &state->pos, &value);
	if (has_next) {
		if (isenv)
			tcompo_env_set_obj(env, idx, &value);
		else
			tobj_array_set_obj(&vm->tmps, idx, &value);
		tobj_ddc_ref_clear(&value);
	} else {
		state->pos = 0;
		state->iterator_slot = nullptr;
	}
	vm_loop_push_cond(vm, has_next);
}

/* Forward declare exec_tins (defined later) */
void exec_tins(tvm *vm, uint_cmds from, uint_cmds ncmds, tcompo_env *env);

static void rule_result_field(tdict *result, const char *name, const tobj *value)
{
	tobj key;
	tobj_set_nil(&key);
	tobj_set_compo(&key, (tcompo_v *)tstr_new(name));
	tdict_set(result, &key, value);
	tobj_try_clear(&key);
}

static trule_instance *rule_instance_argument(const tobj *value)
{
	if (!value || value->type != tcompo || !value->val.v_tcompo)
		return nullptr;
	if (tobj_compo_type(value) == compo_trule_instance)
		return (trule_instance *)value->val.v_tcompo;
	if (tobj_compo_type(value) == compo_trule) {
		trule *rule = (trule *)value->val.v_tcompo;
		if (!rule->ir || rule->ir->parameters.len != 0)
			return nullptr;
		return trule_bind(rule, nullptr, 0);
	}
	return nullptr;
}

static trule_instance *rule_bind_call(trule *rule, const tobj *arguments,
				      uint_regs argument_count);

static void vm_invoke(tvm *vm, const tobj *callable, tobj *arguments,
		      uint_regs argument_count, tcompo_env *environment,
		      tobj *result);

static int rule_parameter_index(const trule_ir *ir, const trule_term *term)
{
	for (uint_count i = 0; i < ir->parameters.len; i++)
		if (((trule_term *)ir->parameters.data[i].val.v_tcompo)->id ==
		    term->id)
			return (int)i;
	return -1;
}

static void rule_eval_source_item(tvm *vm, trule_instance *instance,
				  const trule_term *term, tobj *result)
{
	trule *rule = (trule *)instance->rule.val.v_tcompo;
	if (rule->checker.type != tcompo ||
	    tobj_compo_type(&rule->checker) != compo_tfunc)
		twarn(ErrRuntime_Other, "Rule", "missing source checker");
	tfunc *checker = (tfunc *)rule->checker.val.v_tcompo;
	tlist *output = tlist_new();
	tlist *saved = vm->rule_output;
	tlist *negations = tlist_new();
	tlist *saved_negations = vm->rule_logic_values;
	vm->rule_logic_values = negations;
	vm->rule_output = output;
	tcall_frame *frame = tvm_push_call_frame(vm, checker,
		instance->arguments.data, (uint_regs)instance->arguments.len, 0);
	exec_tins(vm, checker->cmdloc, checker->ncmds, &frame->env);
	tvm_pop_call_frame(vm);
	tobj_try_clear(&vm->rev);
	vm->rule_output = saved;
	vm->rule_logic_values = saved_negations;
	const char *role = tstring_cstr(term->provider_kind);
	if (strcmp(role, "negation-value") == 0
	 || strcmp(role, "negation-operand") == 0
	 || strcmp(role, "expression-value") == 0) {
		const tpair *record = nullptr;
		for (uint_count i = 0; i < tlist_size(negations); i++) {
			const tpair *entry = (tpair *)tlist_at(negations, i)->val.v_tcompo;
			if (entry->first.val.v_tint == term->provider_version)
				record = (tpair *)entry->second.val.v_tcompo;
		}
		if (!record)
			twarn(ErrRuntime_Other, "Rule",
			      "logical Term was not evaluated (short-circuited)");
		tobj_copy(result, strcmp(role, "negation-value") == 0 ? &record->second : &record->first);
	} else {
		long item_index = term->provider_version;
		if (item_index < 0 || (uint_objs)item_index >= tlist_size(output))
			twarn(ErrRuntime_Other, "Rule", "invalid source Term index");
		const tobj *item = tlist_at(output, (uint_objs)item_index);
		if (item->type == tcompo && tobj_compo_type(item) == compo_tpair)
			tobj_copy(result, &((tpair *)item->val.v_tcompo)->second);
		else
			tobj_copy(result, item);
	}
	tobj owner;
	tobj_set_nil(&owner);
	tobj_set_compo(&owner, (tcompo_v *)output);
	tobj_try_clear(&owner);
	tobj_set_compo(&owner, (tcompo_v *)negations);
	tobj_try_clear(&owner);
}

static int rule_antecedent_truth(tvm *vm, const tobj *value, tcompo_env *environment);

static void rule_eval_term(tvm *vm, trule_instance *instance,
			   trule_term *term, tcompo_env *environment, tobj *result)
{
	tobj_set_nil(result);
	if (!strcmp(tstring_cstr(term->provider), "tapas.source") &&
	    !strcmp(tstring_cstr(term->provider_kind), "expression-value")) {
		rule_eval_source_item(vm, instance, term, result);
		return;
	}
	if (term->kind == trule_term_in) {
		if (!strcmp(tstring_cstr(term->provider), "tapas.source")) {
			rule_eval_source_item(vm, instance, term, result);
			return;
		}
		if (term->arguments.len != 2)
			twarn(ErrRuntime_Other, "Rule", "In requires two operands");
		tobj left, right;
		tobj_set_nil(&left);
		tobj_set_nil(&right);
		/* Match ordinary Tapas membership evaluation order. */
		rule_eval_term(vm, instance, (trule_term *)term->arguments.data[1].val.v_tcompo, environment, &right);
		rule_eval_term(vm, instance, (trule_term *)term->arguments.data[0].val.v_tcompo, environment, &left);
		operator_in(&left, &right, result);
		tobj_try_clear(&left);
		tobj_try_clear(&right);
		return;
	}
	if (term->kind == trule_term_and || term->kind == trule_term_or) {
		if (strcmp(tstring_cstr(term->provider), "tapas.source") == 0) {
			rule_eval_source_item(vm, instance, term, result);
			return;
		}
		if (term->arguments.len != 2)
			twarn(ErrRuntime_Other, "Rule", "And/Or require two operands");
		tobj operand;
		tobj_set_nil(&operand);
		rule_eval_term(vm, instance,
			(trule_term *)term->arguments.data[0].val.v_tcompo,
			environment, &operand);
		int truth = rule_antecedent_truth(vm, &operand, environment);
		tobj_try_clear(&operand);
		if ((term->kind == trule_term_and && truth)
		 || (term->kind == trule_term_or && !truth)) {
			rule_eval_term(vm, instance,
				(trule_term *)term->arguments.data[1].val.v_tcompo,
				environment, &operand);
			truth = rule_antecedent_truth(vm, &operand, environment);
			tobj_try_clear(&operand);
		}
		tobj_set_bool(result, truth);
		return;
	}
	if (term->kind == trule_term_not) {
		if (strcmp(tstring_cstr(term->provider), "tapas.source") == 0) {
			rule_eval_source_item(vm, instance, term, result);
			return;
		}
		if (term->arguments.len != 1)
			twarn(ErrRuntime_Other, "Rule", "Not requires one operand");
		tobj operand;
		tobj_set_nil(&operand);
		rule_eval_term(vm, instance,
			(trule_term *)term->arguments.data[0].val.v_tcompo, environment, &operand);
		tobj_set_bool(result, !rule_antecedent_truth(vm, &operand, environment));
		tobj_try_clear(&operand);
		return;
	}
	if (term->kind == trule_term_constant) {
		tobj_copy(result, &term->payload);
		return;
	}
	if (term->kind == trule_term_parameter) {
		trule *rule = (trule *)instance->rule.val.v_tcompo;
		int index = rule_parameter_index(rule->ir, term);
		if (index < 0 || (uint_objs)index >= instance->arguments.len)
			twarn(ErrRuntime_Other, "Rule", "invalid Parameter Term");
		tobj_copy(result, &instance->arguments.data[index]);
		return;
	}
	if (term->kind == trule_term_capture) {
		if (!trule_read_capture((trule *)instance->rule.val.v_tcompo, term, result))
			twarn(ErrRuntime_Other, "Rule", "unbound Capture Term");
		return;
	}
	if (term->kind == trule_term_extension)
		twarn(ErrRuntime_Other, "Rule",
		      "Extension Term requires a supporting evaluator");
	if (term->kind == trule_term_call) {
		trule_term *function =
			term->payload.type == tcompo &&
			tobj_compo_type(&term->payload) == compo_trule_term ?
			(trule_term *)term->payload.val.v_tcompo : nullptr;
		if (!function)
			twarn(ErrRuntime_Other, "Rule", "invalid Call Term");
		tobj callable;
		tobj_set_nil(&callable);
		rule_eval_term(vm, instance, function, environment, &callable);
		uint_regs count = (uint_regs)term->arguments.len;
		tobj *arguments = count ? calloc(count, sizeof(*arguments)) : nullptr;
		for (uint_regs i = 0; i < count; i++) {
			tobj_set_nil(&arguments[i]);
			rule_eval_term(vm, instance,
				(trule_term *)term->arguments.data[i].val.v_tcompo,
				environment, &arguments[i]);
		}
		vm_invoke(vm, &callable, arguments, count, environment, result);
		for (uint_regs i = 0; i < count; i++)
			tobj_try_clear(&arguments[i]);
		free(arguments);
		tobj_try_clear(&callable);
		return;
	}
	if (term->kind == trule_term_construct
	 || term->kind == trule_term_convert) {
		if (term->kind == trule_term_construct
		 && strcmp(tstring_cstr(term->provider), "tapas.source") == 0) {
			rule_eval_source_item(vm, instance, term, result);
			return;
		}
		if (term->arguments.len != 1)
			twarn(ErrRuntime_Other, "Rule", "invalid unary Term");
		rule_eval_term(vm, instance,
			(trule_term *)term->arguments.data[0].val.v_tcompo,
			environment, result);
		return;
	}
	if (term->kind != trule_term_intrinsic)
		twarn(ErrRuntime_Other, "Rule", "unsupported Term kind");
	const char *operation =
		term->payload.type == tcompo &&
		tobj_compo_type(&term->payload) == compo_tstr ?
		tstring_cstr(((tstr *)term->payload.val.v_tcompo)->data) : "";
	if (term->arguments.len == 1 && strcmp(operation, "neg") == 0) {
		rule_eval_term(vm, instance,
			(trule_term *)term->arguments.data[0].val.v_tcompo,
			environment, result);
		operator_neg(result);
		return;
	}
	if (term->arguments.len != 2)
		twarn(ErrRuntime_Other, "Rule", "invalid Intrinsic Term");
	tobj left;
	tobj right;
	tobj_set_nil(&left);
	tobj_set_nil(&right);
	rule_eval_term(vm, instance,
		(trule_term *)term->arguments.data[0].val.v_tcompo,
		environment, &left);
	rule_eval_term(vm, instance,
		(trule_term *)term->arguments.data[1].val.v_tcompo,
		environment, &right);
	if (strcmp(operation, "+") == 0) operator_add(&left, &right, result);
	else if (strcmp(operation, "-") == 0) operator_sub(&left, &right, result);
	else if (strcmp(operation, "*") == 0) operator_mul(&left, &right, result);
	else if (strcmp(operation, "/") == 0) operator_div(&left, &right, result);
	else if (strcmp(operation, "%") == 0) operator_mod(&left, &right, result);
	else if (strcmp(operation, "^") == 0) operator_pow(&left, &right, result);
	else if (strcmp(operation, "@") == 0) operator_mmul(&left, &right, result);
	else if (strcmp(operation, "==") == 0) operator_eq(&left, &right, result);
	else if (strcmp(operation, "!=") == 0) operator_ne(&left, &right, result);
	else if (strcmp(operation, ">") == 0) operator_sg(&left, &right, result);
	else if (strcmp(operation, "<") == 0) operator_sl(&left, &right, result);
	else if (strcmp(operation, ">=") == 0) operator_ge(&left, &right, result);
	else if (strcmp(operation, "<=") == 0) operator_le(&left, &right, result);
	else if (strcmp(operation, "and") == 0) operator_and(&left, &right, result);
	else if (strcmp(operation, "or") == 0) operator_or(&left, &right, result);
	else twarn(ErrRuntime_Other, "Rule", "unknown intrinsic operation");
	tobj_try_clear(&left);
	tobj_try_clear(&right);
}

static void rule_violation(tlist *violations, trule_item *condition,
			   trule_instance **path, trule_item **requirement_path,
			   uint32_t depth)
{
	tdict *violation = tdict_new();
	tobj value;
	tobj_set_nil(&value);
	tobj_set_compo(&value, (tcompo_v *)condition);
	rule_result_field(violation, "condition", &value);
	tobj_set_compo(&value, (tcompo_v *)tstr_new(tstring_cstr(condition->description)));
	rule_result_field(violation, "description", &value);
	tobj_try_clear(&value);
	tlist *arguments = tlist_new();
	if (depth) {
		trule_instance *current = path[depth - 1];
		for (uint_count i = 0; i < current->arguments.len; i++)
			tobj_vec_push(&arguments->items, &current->arguments.data[i]);
	}
	tobj_set_compo(&value, (tcompo_v *)arguments);
	rule_result_field(violation, "arguments", &value);
	tobj_try_clear(&value);
	tlist *requirements = tlist_new();
	for (uint32_t i = 0; i + 1 < depth; i++) {
		if (!requirement_path[i])
			continue;
		tobj requirement;
		tobj_set_nil(&requirement);
		tobj_set_compo(&requirement, (tcompo_v *)requirement_path[i]);
		tobj_vec_push(&requirements->items, &requirement);
	}
	tobj_set_compo(&value, (tcompo_v *)requirements);
	rule_result_field(violation, "requirement_path", &value);
	tobj_try_clear(&value);
	tdict *origin = tdict_new();
	tobj_set_int(&value, condition->origin_start);
	rule_result_field(origin, "start", &value);
	tobj_set_int(&value, condition->origin_end);
	rule_result_field(origin, "end", &value);
	if (depth) {
		trule *owner = (trule *)path[depth - 1]->rule.val.v_tcompo;
		tobj_set_compo(&value, (tcompo_v *)tstr_new(tstring_cstr(owner->source)));
	} else
		tobj_set_nil(&value);
	rule_result_field(origin, "source", &value);
	tobj_try_clear(&value);
	tobj_set_compo(&value, (tcompo_v *)origin);
	rule_result_field(violation, "source", &value);
	tobj_try_clear(&value);
	tobj_set_compo(&value, (tcompo_v *)violation);
	tobj_vec_push(&violations->items, &value);
	tobj_try_clear(&value);
}

/* Rule evaluator logic: no implication-specific VM instruction is needed. */
static void rule_check(tvm *vm, const tobj *value, tobj *result, int fatal,
		       tcompo_env *environment);

static int rule_antecedent_truth(tvm *vm, const tobj *value, tcompo_env *environment)
{
	if (value->type == tbool)
		return value->val.v_tbool;
	if (value->type != tcompo || tobj_compo_type(value) != compo_trule_instance)
		twarn(ErrRuntime_ParamsType, "Rule truth", "Bool or RuleInstance required");
	/* Bound source and dynamic recursion even though each nested check starts
	 * a new requirement path. */
	if (vm->antecedent_depth >= 128)
		twarn(ErrRuntime_Other, "Rule truth",
		      "antecedent evaluation depth exceeded (cyclic or deeply nested Rule check)");
	tobj checked;
	tobj_set_nil(&checked);
	vm->antecedent_depth++;
	rule_check(vm, value, &checked, 0, environment);
	vm->antecedent_depth--;
	tobj key;
	tobj_set_nil(&key);
	tobj_set_compo(&key, (tcompo_v *)tstr_new("passed"));
	tobj result;
	tobj_set_nil(&result);
	tdict_get((tdict *)checked.val.v_tcompo, &key, &result);
	int passed = result.val.v_tbool;
	tobj_try_clear(&result);
	tobj_try_clear(&key);
	tobj_try_clear(&checked);
	return passed;
}

static int rule_bool_record(const tobj *record)
{
	if (!record
	 || record->type != tcompo
	 || tobj_compo_type(record) != compo_tpair
	 || ((tpair *)record->val.v_tcompo)->second.type != tbool)
		twarn(ErrRuntime_Other, "Rule", "invalid implication checker record");
	return ((tpair *)record->val.v_tcompo)->second.val.v_tbool;
}

static void rule_check_implication(
		tvm *vm, trule_instance *instance, trule_item *item,
		tlist *violations, tlist *implications, trule_instance **path,
		trule_item **requirement_path, uint32_t depth,
		tcompo_env *environment, tlist *records, uint_count *record_index)
{
	tobj guard;
	tobj_set_nil(&guard);
	if (records && (((trule *)instance->rule.val.v_tcompo)->ir->version >= 7
	 || item->term->kind == trule_term_not
	 || item->term->kind == trule_term_in
	 || item->term->kind == trule_term_and
	 || item->term->kind == trule_term_or
	 || strcmp(tstring_cstr(item->term->provider_kind), "antecedent-value") == 0))
		(*record_index)++; /* use the already checked truth, not a second evaluation */
	if (records)
		tobj_set_bool(&guard, rule_bool_record(tlist_at(records, *record_index)));
	else
		rule_eval_term(vm, instance, item->term, environment, &guard);
	int triggered = rule_antecedent_truth(vm, &guard, environment);
	tobj_try_clear(&guard);
	int passed = 1;
	for (uint_objs j = 0; j < item->arguments.len; j++) {
		trule_term *term = (trule_term *)item->arguments.data[j].val.v_tcompo;
		tobj value;
		tobj_set_nil(&value);
		if (records) {
			(*record_index)++;
			if (*record_index >= tlist_size(records))
				twarn(ErrRuntime_Other, "implies", "truncated checker records");
			if (triggered)
				tobj_set_bool(&value, rule_bool_record(tlist_at(records, *record_index)));
		} else if (triggered)
			rule_eval_term(vm, instance, term, environment, &value);
		if (!triggered)
			continue;
		if (value.type != tbool)
			twarn(ErrRuntime_ParamsType, "implies", "Bool consequent required");
		if (!value.val.v_tbool) {
			passed = 0;
			trule_item *condition = trule_condition_new(term, tstring_cstr(item->description));
			condition->origin_start = term->origin_start;
			condition->origin_end = term->origin_end;
			rule_violation(violations, condition, path, requirement_path, depth);
		}
		tobj_try_clear(&value);
	}
	tdict *event = tdict_new();
	tobj field;
	tobj_set_nil(&field);
	tobj_set_bool(&field, triggered);
	rule_result_field(event, "triggered", &field);
	tobj_set_bool(&field, passed);
	rule_result_field(event, "passed", &field);
	tobj_set_int(&field, triggered ? (long)item->arguments.len : 0);
	rule_result_field(event, "consequents_checked", &field);
	tobj_set_compo(&field, (tcompo_v *)item);
	rule_result_field(event, "item", &field);
	tobj_try_clear(&field);
	tobj_set_compo(&field, (tcompo_v *)instance);
	rule_result_field(event, "instance", &field);
	tobj_try_clear(&field);
	tobj_set_compo(&field, (tcompo_v *)event);
	tobj_vec_push(&implications->items, &field);
	tobj_try_clear(&field);
}

static void rule_collect(tvm *vm, trule_instance *instance,
			 tlist *violations, trule_instance **path, uint32_t depth,
			 trule_item **requirement_path,
			 tcompo_env *environment, tlist *implications)
{
	if (depth >= 128)
		twarn(ErrRuntime_Other, "Rule", "requirement depth exceeded");
	for (uint32_t i = 0; i < depth; i++) {
		int same = path[i] == instance ||
			(path[i]->rule.val.v_tcompo == instance->rule.val.v_tcompo &&
			 path[i]->arguments.len == instance->arguments.len);
		for (uint_objs j = 0; same && j < instance->arguments.len; j++)
			same = tobj_identical(
				&path[i]->arguments.data[j],
				&instance->arguments.data[j]);
		if (same)
			twarn(ErrRuntime_Other, "Rule", "cyclic requirement");
	}
	path[depth] = instance;
	trule *rule = (trule *)instance->rule.val.v_tcompo;
	if (rule->evaluate_ir) {
		for (uint_count i = 0; i < rule->ir->items.len; i++) {
			trule_item *item =
				(trule_item *)rule->ir->items.data[i].val.v_tcompo;
			if (item->kind == trule_item_implication) {
				rule_check_implication(
					vm, instance, item, violations, implications,
					path, requirement_path, depth + 1,
					environment, nullptr, nullptr);
				continue;
			}
			if (item->kind == trule_item_condition) {
				tobj value;
				tobj_set_nil(&value);
				rule_eval_term(vm, instance, item->term, environment, &value);
				if (value.type != tbool)
					twarn(ErrRuntime_ParamsType, "Rule condition",
					      "Bool result required");
				if (!value.val.v_tbool)
					rule_violation(violations, item, path,
						requirement_path, depth + 1);
				tobj_try_clear(&value);
				continue;
			}
			tobj rule_value;
			tobj_set_nil(&rule_value);
			rule_eval_term(vm, instance, item->rule, environment, &rule_value);
			if (rule_value.type != tcompo
			 || tobj_compo_type(&rule_value) != compo_trule)
				twarn(ErrRuntime_ParamsType, "Rule dependency",
				      "Rule Term required");
			uint_regs count = (uint_regs)item->arguments.len;
			tobj *arguments = count ? calloc(count, sizeof(*arguments)) : nullptr;
			for (uint_regs j = 0; j < count; j++) {
				tobj_set_nil(&arguments[j]);
				rule_eval_term(vm, instance,
					(trule_term *)item->arguments.data[j].val.v_tcompo,
					environment, &arguments[j]);
			}
			trule_instance *required = trule_bind(
				(trule *)rule_value.val.v_tcompo, arguments, count);
			requirement_path[depth] = item;
			rule_collect(vm, required, violations, path, depth + 1,
				requirement_path, environment, implications);
			for (uint_regs j = 0; j < count; j++) tobj_try_clear(&arguments[j]);
			free(arguments);
			tobj_try_clear(&rule_value);
			if (required->base.refctr == 0)
				required->base.vtable->free(required);
		}
		return;
	}
	tfunc *checker = (tfunc *)rule->checker.val.v_tcompo;
	tlist *output = tlist_new();
	tlist *saved = vm->rule_output;
	tlist *negations = tlist_new();
	tlist *saved_negations = vm->rule_logic_values;
	vm->rule_logic_values = negations;
	vm->rule_output = output;
	tcall_frame *frame = tvm_push_call_frame(
		vm, checker, instance->arguments.data,
		(uint_regs)instance->arguments.len, 0);
	exec_tins(vm, checker->cmdloc, checker->ncmds, &frame->env);
	tvm_pop_call_frame(vm);
	tobj_try_clear(&vm->rev);
	vm->rule_output = saved;
	vm->rule_logic_values = saved_negations;

	uint_objs metadata_index = 0;
	for (uint_count i = 0; i < tlist_size(output); i++) {
		const tobj *item = tlist_at(output, i);
		trule_item *metadata = metadata_index < rule->ir->items.len ?
			(trule_item *)rule->ir->items.data[metadata_index++]
				.val.v_tcompo : nullptr;
		if (metadata && metadata->kind == trule_item_implication) {
			rule_check_implication(vm, instance, metadata, violations, implications,
				path, requirement_path, depth + 1, environment, output, &i);
			continue;
		}
		if (item->type != tcompo || !item->val.v_tcompo)
			twarn(ErrRuntime_Other, "Rule", "invalid checker output");
		if (tobj_compo_type(item) == compo_tpair) {
			tpair *condition = (tpair *)item->val.v_tcompo;
			if (condition->second.type != tbool)
				twarn(ErrRuntime_ParamsType, "Rule condition",
				      "Bool result required");
			if (!condition->second.val.v_tbool) {
				if (metadata && metadata->kind == trule_item_condition)
					rule_violation(violations, metadata, path,
						requirement_path, depth + 1);
				else tobj_vec_push(&violations->items,
					&condition->first);
			}
		} else if (tobj_compo_type(item) == compo_trule_instance) {
			/* Dynamic source expressions may acquire their dependency role only
			 * after evaluation. Preserve that role in the violation path too. */
			trule_item *dependency = metadata;
			if (metadata && metadata->kind != trule_item_requirement) {
				dependency = trule_requirement_new(metadata->term, nullptr, 0);
				tstring_free(dependency->description);
				dependency->description = tstring_dup(metadata->description);
				dependency->origin_start = metadata->origin_start;
				dependency->origin_end = metadata->origin_end;
			}
			requirement_path[depth] = dependency;
			rule_collect(vm, (trule_instance *)item->val.v_tcompo,
				violations, path, depth + 1,
				requirement_path, environment, implications);
			if (dependency && dependency != metadata && dependency->base.refctr == 0)
				dependency->base.vtable->free(dependency);
		} else
			twarn(ErrRuntime_ParamsType, "Rule dependency", "RuleInstance required");
	}
	tobj owner;
	tobj_set_nil(&owner);
	tobj_set_compo(&owner, (tcompo_v *)output);
	tobj_try_clear(&owner);
	tobj_set_compo(&owner, (tcompo_v *)negations);
	tobj_try_clear(&owner);
}

static void rule_check(tvm *vm, const tobj *value, tobj *result, int fatal,
		       tcompo_env *environment)
{
	trule_instance *instance = rule_instance_argument(value);
	if (!instance)
		twarn(ErrRuntime_ParamsType, fatal ? "assert" : "Rule check",
		      "RuleInstance or zero-argument Rule required");
	int temporary = tobj_compo_type(value) == compo_trule;
	tlist *violations = tlist_new();
	tlist *implications = tlist_new();
	trule_instance *path[128];
	trule_item *requirement_path[128] = { 0 };
	rule_collect(vm, instance, violations, path, 0,
		requirement_path, environment, implications);
	if (temporary) {
		tobj owner;
		tobj_set_nil(&owner);
		tobj_set_compo(&owner, (tcompo_v *)instance);
		tobj_try_clear(&owner);
	}
	if (fatal && tlist_size(violations)) {
		const tobj *first = tlist_at(violations, 0);
		const char *message = "rule condition failed";
		if (first->type == tcompo && tobj_compo_type(first) == compo_tstr)
			message = tstring_cstr(((tstr *)first->val.v_tcompo)->data);
		else if (first->type == tcompo &&
			 tobj_compo_type(first) == compo_tdict) {
			tobj key;
			tobj description;
			tobj_set_nil(&key);
			tobj_set_nil(&description);
			tobj_set_compo(&key, (tcompo_v *)tstr_new("description"));
			tdict_get((tdict *)first->val.v_tcompo, &key, &description);
			if (description.type == tcompo &&
			    tobj_compo_type(&description) == compo_tstr &&
			    tstring_len(((tstr *)description.val.v_tcompo)->data))
				message = tstring_cstr(
					((tstr *)description.val.v_tcompo)->data);
			tobj_try_clear(&key);
		}
		twarn(ErrRuntime_Other, "assert", message);
	}
	if (fatal) {
		tobj owner;
		tobj_set_nil(&owner);
		tobj_set_compo(&owner, (tcompo_v *)implications);
		tobj_try_clear(&owner);
		tobj_set_compo(&owner, (tcompo_v *)violations);
		tobj_try_clear(&owner);
		tobj_set_nil(result);
		return;
	}
	tdict *check = tdict_new();
	tobj field;
	tobj_set_nil(&field);
	tobj_set_bool(&field, tlist_size(violations) == 0);
	rule_result_field(check, "passed", &field);
	tobj_set_compo(&field, (tcompo_v *)tstr_new(
		"Success"));
	rule_result_field(check, "status", &field);
	tobj_try_clear(&field);
	tobj_set_compo(&field, (tcompo_v *)violations);
	rule_result_field(check, "violations", &field);
	tobj_try_clear(&field);
	tobj_set_compo(&field, (tcompo_v *)implications);
	rule_result_field(check, "implications", &field);
	tobj_try_clear(&field);
	tobj_set_compo(&field, (tcompo_v *)tlist_new());
	rule_result_field(check, "diagnostics", &field);
	tobj_try_clear(&field);
	tobj_set_compo(result, (tcompo_v *)check);
}

static void vm_invoke(tvm *vm, const tobj *callable, tobj *arguments,
		      uint_regs argument_count, tcompo_env *environment,
		      tobj *result)
{
	if (!callable || callable->type != tcompo || !callable->val.v_tcompo)
		twarn(ErrRuntime_RefType, "VM invoke", "Function required");
	switch (tobj_compo_type(callable)) {
	case compo_trule:
		tobj_set_compo(result, (tcompo_v *)rule_bind_call(
			(trule *)callable->val.v_tcompo, arguments, argument_count));
		break;
	case compo_tfunc: {
		tfunc *function = (tfunc *)callable->val.v_tcompo;
		tcall_frame *frame = tvm_push_call_frame(
			vm, function, arguments, argument_count, 0);
		exec_tins(vm, function->cmdloc, function->ncmds, &frame->env);
		tvm_pop_call_frame(vm);
		tobj returned = vm->rev;
		tobj_set_nil(&vm->rev);
		*result = returned;
	} break;
	case compo_cppfunc: {
		tcppgenf *function = (tcppgenf *)callable->val.v_tcompo;
		if (!tcppgenf_accepts(function, argument_count))
			twarn(ErrRuntime_ParamsCtr, "VM invoke",
			      "incorrect parameter count");
		function->f(arguments, argument_count, result);
	} break;
	case compo_sessfunc: {
		tcppsessf *function = (tcppsessf *)callable->val.v_tcompo;
		if (!tcppsessf_accepts(function, argument_count))
			twarn(ErrRuntime_ParamsCtr, "VM invoke",
			      "incorrect parameter count");
		function->f(arguments, argument_count, result, environment);
	} break;
	default:
		twarn(ErrRuntime_RefType, "VM invoke", "Function required");
	}
}

static void vm_service_invoke(void *context, const tobj *callable,
			      tobj *arguments, uint_regs argument_count,
			      tcompo_env *environment, tobj *result)
{
	vm_invoke((tvm *)context, callable, arguments, argument_count,
		environment, result);
}

static void vm_service_evaluate_rule_term(void *context,
					  const tobj *instance,
					  const tobj *term,
					  tcompo_env *environment,
					  tobj *result)
{
	if (!instance || instance->type != tcompo ||
	    tobj_compo_type(instance) != compo_trule_instance ||
	    !term || term->type != tcompo ||
	    tobj_compo_type(term) != compo_trule_term)
		twarn(ErrRuntime_ParamsType, "VM Rule service",
		      "RuleInstance and Term required");
	rule_eval_term((tvm *)context,
		(trule_instance *)instance->val.v_tcompo,
		(trule_term *)term->val.v_tcompo, environment, result);
}

static void vm_service_check_rule(void *context, const tobj *rule,
				  tcompo_env *environment, tobj *result)
{
	rule_check((tvm *)context, rule, result, 0, environment);
}

static const char *rule_parameter_name(const trule_term *parameter)
{
	if (!parameter || parameter->payload.type != tcompo ||
	    tobj_compo_type(&parameter->payload) != compo_tstr)
		twarn(ErrRuntime_Other, "Rule", "parameter has no stable name");
	return tstring_cstr(
		((tstr *)parameter->payload.val.v_tcompo)->data);
}

static int rule_uses_named_arguments(trule *rule, const tobj *arguments,
				     uint_regs argument_count)
{
	if (argument_count != 1 || arguments[0].type != tcompo ||
	    tobj_compo_type(&arguments[0]) != compo_tdict)
		return 0;
	uint_count count = rule->ir ? rule->ir->parameters.len : 0;
	if (count != 1)
		return 1;
	trule_term *parameter = (trule_term *)
		rule->ir->parameters.data[0].val.v_tcompo;
	tobj key;
	tobj_set_nil(&key);
	tobj_set_compo(&key, (tcompo_v *)tstr_new(
		rule_parameter_name(parameter)));
	int named = tdict_contains((tdict *)arguments[0].val.v_tcompo, &key);
	tobj_try_clear(&key);
	return named;
}

static trule_instance *rule_bind_named(trule *rule, tdict *arguments)
{
	uint_count count = rule->ir ? rule->ir->parameters.len : 0;
	if (thashtbl_len(arguments->items) != count)
		twarn(ErrRuntime_ParamsCtr, "Rule",
		      "argument Dictionary must contain every parameter exactly once");
	tobj *ordered = count ? calloc(count, sizeof(*ordered)) : nullptr;
	if (count && !ordered)
		abort();
	for (uint_count i = 0; i < count; i++) {
		tobj_set_nil(&ordered[i]);
		trule_term *parameter = (trule_term *)
			rule->ir->parameters.data[i].val.v_tcompo;
		tobj key;
		tobj_set_nil(&key);
		tobj_set_compo(&key, (tcompo_v *)tstr_new(
			rule_parameter_name(parameter)));
		if (!tdict_contains(arguments, &key))
			twarn(ErrRuntime_ParamsType, "Rule",
			      "argument Dictionary key does not match a parameter");
		tdict_get(arguments, &key, &ordered[i]);
		tobj_try_clear(&key);
	}
	trule_instance *instance = trule_bind(
		rule, ordered, (uint_regs)count);
	for (uint_count i = 0; i < count; i++)
		tobj_ddc_ref_clear(&ordered[i]);
	free(ordered);
	return instance;
}

static trule_instance *rule_bind_call(trule *rule, const tobj *arguments,
				      uint_regs argument_count)
{
	return rule_uses_named_arguments(rule, arguments, argument_count) ?
		rule_bind_named(rule, (tdict *)arguments[0].val.v_tcompo) :
		trule_bind(rule, arguments, argument_count);
}

static const tvm_services core_vm_services = {
	.invoke = vm_service_invoke,
	.evaluate_rule_term = vm_service_evaluate_rule_term,
	.check_rule = vm_service_check_rule
};

static inline void vm_eval_cppfunc(tvm *vm, tcppgenf *function,
				   tobj *params, uint_regs nparams)
{
	if (!function->f)
		twarn(ErrRuntime_Other, tstring_cstr(function->name),
		      "function is not implemented");
	if (!tcppgenf_accepts(function, nparams))
		twarn(ErrRuntime_ParamsCtr, tstring_cstr(function->name),
		      "incorrect parameter count");
	function->f(params, nparams, &vm->rev);
}

static inline void vm_eval_sessfunc(tvm *vm, tcppsessf *function,
				    tobj *params, uint_regs nparams,
				    tcompo_env *env)
{
	if (!tcppsessf_accepts(function, nparams))
		twarn(ErrRuntime_ParamsCtr, tstring_cstr(function->name),
		      "incorrect parameter count");
	tcompo_env_set_vm_services(env, &core_vm_services, vm);
	function->f(params, nparams, &vm->rev, env);
}

static inline void vm_finish_eval(tvm *vm, uint_regs stack_values)
{
	stk_popcn(vm, stack_values);
	stk_push(vm, &vm->rev);
	tvm_set_rev_empty(vm);
}

/* Statement-level native calls are followed by a lone POPN that discards the
 * result. Consume that POPN at the call site: pop the arguments, print the
 * result when the POPN would have, and release a fresh unreferenced result
 * exactly as the push/pop pair would. The result slot can never alias a loop
 * iterator placeholder, so the release scan POPN performs cannot fire. */
static inline TVM_ALWAYS_INLINE int vm_try_discard_eval_result(
		tvm *vm, const tbycode *cmdarr, uint_cmds i, uint_cmds end,
		uint_regs stack_values)
{
	if (i + 1 >= end || tbycode_ins(cmdarr[i + 1]) != OP_POPN ||
	    tbycode_get_L(cmdarr[i + 1]) != 1 ||
	    tbycode_popn_temporary_count(cmdarr[i + 1]) != 0)
		return 0;
	if (tbycode_popn_print(cmdarr[i + 1]) && vm->rev.type != tnil) {
		tstring *s = tobj_tostring_full(&vm->rev);
		printf("%s\n", tstring_cstr(s));
		tstring_free(s);
	}
	stk_popcn(vm, stack_values);
	tobj_try_clear(&vm->rev);
	return 1;
}

/* Eval helper */
void vm_eval(tvm *vm, tbycode *iter, tcompo_env *env)
{
	/* Services follow the active call, not the closure's lexical library.
	 * Binding them to the executing frame keeps imported closures connected to
	 * the VM that invoked them. */
	tcompo_env_set_vm_services(env, &core_vm_services, vm);
	uint_regs nparams = (uint_regs)tbycode_get_U(*iter);
	tobj *obj = stk_top(vm);
	tobj *params = stk_topn(vm, nparams + 1);
	if (obj->type != tcompo)
		twarn(ErrRuntime_RefType, "vm_eval", "");
	tcompo_v *v = obj->val.v_tcompo;

	switch (v->vtable->compo_code) {
	case compo_trule: {
		trule *rule = (trule *)v;
		tobj_set_compo(&vm->rev,
			(tcompo_v *)rule_bind_call(rule, params, nparams));
	} break;
	case compo_tfunc: {
		tfunc *f = (tfunc *)v;
		tcall_frame *fr = tvm_push_call_frame(vm, f, params, nparams, 0);
		exec_tins(vm, f->cmdloc, f->ncmds, &fr->env);
		tvm_pop_call_frame(vm);
	} break;
	case compo_cppfunc: {
		vm_eval_cppfunc(vm, (tcppgenf *)v, params, nparams);
	} break;
	case compo_sessfunc: {
		vm_eval_sessfunc(vm, (tcppsessf *)v, params, nparams, env);
	} break;
	case compo_trule_builtin:
		switch (((trule_builtin *)v)->kind) {
		case trule_builtin_assert:
			if (nparams != 1) twarn(ErrRuntime_ParamsCtr,
				"assert", "one argument required");
			rule_check(vm, &params[0], &vm->rev, 1, env);
			break;
		}
		break;
	default:
		twarn(ErrRuntime_RefType, "vm_eval", "");
	}
	vm_finish_eval(vm, 1 + nparams);
}

void tvm_call(tvm *vm, const tobj *callable, tobj *arguments,
	      uint_regs argument_count, tcompo_env *environment,
	      tobj *result)
{
	const tvm_services *previous_services = environment->vm_services;
	void *previous_context = environment->vm_services_context;
	tcompo_env_set_vm_services(environment, &core_vm_services, vm);
	/* The VM stack owns retained references and releases them during OP_EVAL.
	 * Protect the caller's borrowed values so a zero-refcount composite is not
	 * destroyed merely because it was passed through this convenience API. */
	if (callable->type == tcompo && callable->val.v_tcompo)
		callable->val.v_tcompo->refctr++;
	for (uint_regs i = 0; i < argument_count; i++)
		if (arguments[i].type == tcompo && arguments[i].val.v_tcompo)
			arguments[i].val.v_tcompo->refctr++;
	for (uint_regs i = 0; i < argument_count; i++)
		stk_push(vm, &arguments[i]);
	stk_push(vm, callable);
	tbycode call = tbycode_make_u(OP_EVAL, argument_count);
	vm_eval(vm, &call, environment);
	if (callable->type == tcompo && callable->val.v_tcompo)
		callable->val.v_tcompo->refctr--;
	for (uint_regs i = 0; i < argument_count; i++)
		if (arguments[i].type == tcompo && arguments[i].val.v_tcompo)
			arguments[i].val.v_tcompo->refctr--;
	tobj_copy(result, stk_top(vm));
	stk_popc(vm);
	tcompo_env_set_vm_services(environment, previous_services,
		previous_context);
}

/* Import */
void
vm_import(tvm *vm, uint_csts cloc, tstring **cstrlsts, tcompo_env *env)
{
	tlib *current_lib = tlib_from_env(env);
	tlib *lib = tlib_recreate(current_lib);
	tstring *file_ts = cstrlsts[cloc];
	const char *file = tstring_cstr(file_ts);
	if (!tlib_get_build_root(current_lib))
		tcompile_cache_configure(current_lib, file_ts,
			tlib_get_build_interactive(current_lib));
	if (!tlib_get_build_root(lib))
		tcompile_cache_configure(lib, file_ts,
			tlib_get_build_interactive(current_lib));
	twrapper *w = tcompile_cache_load_current(current_lib, file_ts);
	if (!w) {
		tcp *compiler = tcp_new_library(
			current_lib, tlib_get_build_interactive(current_lib));
		w = compile_file(compiler, file_ts,
			tlib_get_paths(current_lib),
			tlib_get_npaths(current_lib));
		tcp_delete(compiler);
		if (!w) {
			twarn(ErrSession_IO, "vm_import", file);
			return;
		}
		(void)tcompile_cache_save(current_lib, file_ts, w);
	}
	tlib_set_wrapper(lib, w);
	tvm vm2;
	tvm_init(&vm2, w->info.tmp_max);
	tvm_set_vmstack(
		&vm2, tcompo_env_get_vmstack(&lib->env), w->info.reg_max);
	exec_tins(&vm2, 0, w->ncmds, &lib->env);
	tobj returned = *tvm_get_vre(&vm2);
	tvm_get_vre(&vm2)->type = tnil;
	tvm_clean(&vm2);
	tvm_set_vmstack(&vm2, nullptr, 0);
	if (returned.type == tcompo &&
	    tobj_compo_type(&returned) == compo_tdict)
		tlib_set_exposed(lib, (tdict *)returned.val.v_tcompo);
	else if (returned.type == tcompo) {
		returned.val.v_tcompo->vtable->free(returned.val.v_tcompo);
		tlib_set_exposed(lib, tdict_new());
	} else
		tlib_set_exposed(lib, tdict_new());
	((tcompo_v *)lib)->refctr++;
	tobj_set_compo(stk_free(vm), (tcompo_v *)lib);
	stk_fill(vm);
}

/* Single ins execution */
typedef struct {
	tfunc *function;
	tobj *params;
	uint_regs nparams;
	uint_regs return_stack_values;
	int stack_has_callable;
	int tail_self_call;
	tcompo_v *retain_callable;
} tcall_request;

static inline int vm_eval_generic_instruction(
		tvm *vm, tbycode *instruction, tins_cache *cache,
		tcall_request *call, tcompo_env *env)
{
	uint_regs nparams = (uint_regs)tbycode_get_U(*instruction);
	tobj *callable = stk_top(vm);
	if (cache && callable->type == tcompo &&
	    callable->val.v_tcompo &&
	    callable->val.v_tcompo->vtable == cache->guard) {
		if (cache->kind == tins_cache_eval_cppfunc) {
			vm_eval_cppfunc(vm, (tcppgenf *)callable->val.v_tcompo,
				stk_topn(vm, nparams + 1), nparams);
			vm_finish_eval(vm, nparams + 1);
			return 0;
		}
		if (cache->kind == tins_cache_eval_sessfunc) {
			vm_eval_sessfunc(vm, (tcppsessf *)callable->val.v_tcompo,
				 stk_topn(vm, nparams + 1), nparams, env);
			vm_finish_eval(vm, nparams + 1);
			return 0;
		}
	}
	if (cache && cache->kind == tins_cache_eval_tfunc &&
	    callable->type == tcompo && callable->val.v_tcompo &&
	    callable->val.v_tcompo->vtable == cache->guard) {
		call->function = (tfunc *)callable->val.v_tcompo;
		call->params = stk_topn(vm, nparams + 1);
		call->nparams = nparams;
		call->return_stack_values = nparams + 1;
		call->stack_has_callable = 1;
		call->tail_self_call = 0;
		call->retain_callable = nullptr;
		return 1;
	}
	if (callable->type == tcompo && callable->val.v_tcompo &&
	    callable->val.v_tcompo->vtable->compo_code ==
		    compo_tfunc) {
		if (cache && cache->kind == tins_cache_empty) {
			cache->kind = tins_cache_eval_tfunc;
			cache->guard = callable->val.v_tcompo->vtable;
		} else if (cache &&
			   (cache->kind != tins_cache_eval_tfunc ||
			    cache->guard != callable->val.v_tcompo->vtable)) {
			cache->kind = tins_cache_polymorphic;
			cache->guard = nullptr;
		}
		call->function = (tfunc *)callable->val.v_tcompo;
		call->params = stk_topn(vm, nparams + 1);
		call->nparams = nparams;
		call->return_stack_values = nparams + 1;
		call->stack_has_callable = 1;
		call->tail_self_call = 0;
		call->retain_callable = nullptr;
		return 1;
	}
	if (callable->type == tcompo && callable->val.v_tcompo) {
		tcompo_type kind = tobj_compo_type(callable);
		if (kind == compo_cppfunc || kind == compo_sessfunc) {
			tins_cache_observe(
				cache,
				kind == compo_cppfunc ? tins_cache_eval_cppfunc :
						       tins_cache_eval_sessfunc,
				callable->val.v_tcompo->vtable);
			if (kind == compo_cppfunc)
				vm_eval_cppfunc(
					vm, (tcppgenf *)callable->val.v_tcompo,
					stk_topn(vm, nparams + 1), nparams);
			else
				vm_eval_sessfunc(
					vm, (tcppsessf *)callable->val.v_tcompo,
					stk_topn(vm, nparams + 1), nparams, env);
			vm_finish_eval(vm, nparams + 1);
			return 0;
		}
	}
	if (cache && cache->kind == tins_cache_empty)
		cache->kind = tins_cache_polymorphic;
	vm_eval(vm, instruction, env);
	return 0;
}

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
	*source = location->source ? tstring_cstr(location->source) : nullptr;
	*file = location->file ? tstring_cstr(location->file) : nullptr;
	*line = location->line;
	*column = location->column;
}

#define VM_PARSE_BINOP(vm, fn, code, env)                                  \
	do {                                                                 \
		uint_regs left__ = (uint_regs)tbycode_get_L(code);          \
		uint16_t right__ = tbycode_get_R(code);                     \
		if (tbycode_binop_named(code)) {                             \
			const tobj *left_value__ = vm_pushx_source(            \
				(vm), (env), left__, tbycode_binop_address(code)); \
			(fn)(left_value__, stk_top(vm), stk_top(vm));           \
			vm_own_result(stk_top(vm));                             \
		} else {                                                     \
			const tobj *left_value__ = stk_at((vm), left__);       \
			const tobj *right_value__ = stk_at((vm), right__);     \
			(fn)(left_value__, right_value__, stk_at((vm), right__)); \
			vm_own_result(stk_at((vm), right__));                   \
			stk_popc(vm);                                           \
		}                                                            \
	} while (0)

/* After a comparison (or membership test) pushed its Bool, an immediately
 * following conditional jump can be consumed in place: read the predicate,
 * drop the value, and redirect the program counter exactly as the jump
 * instruction would. The caller's VM_NEXT() accounts for the final step. */
static inline TVM_ALWAYS_INLINE void vm_consume_cjpop(
	tvm *vm, const tbycode *cmdarr, uint_cmds *pc, uint_cmds end)
{
	if (*pc + 1 >= end)
		return;
	tbycode jump = cmdarr[*pc + 1];
	tins kind = tbycode_ins(jump);
	if (kind != OP_CJPFPOP && kind != OP_CJPBPOP)
		return;
	tobj *top = stk_top(vm);
	if (top->type != tbool)
		twarn(ErrRuntime_ParamsType,
		      kind == OP_CJPFPOP ? "OP_CJPFPOP" : "OP_CJPBPOP", "");
	int taken = kind == OP_CJPFPOP ? !top->val.v_tbool :
		top->val.v_tbool;
	stk_pop(vm);
	/* CJPFPOP 在假时前跳，CJPBPOP 在真时前跳；两者都只向前。 */
	if (taken)
		*pc = vm_jump_landing(cmdarr, end,
				      *pc + 2u + (uint_cmds)tbycode_get_U(jump));
	else
		*pc += 1;
}


/* Named-receiver indexed read with a slot key: store form
 * (`dst = recv[key]`) and expression form share one body; the shape is
 * learned per call site so later dispatches skip the pattern checks. */
static inline TVM_ALWAYS_INLINE int vm_pushx_idxr_named_fused(
	tvm *vm, tcompo_env *env, tbycode *cmdarr, uint_cmds *pc,
	uint_cmds end, tobj *src)
{
	tbycode idxr = cmdarr[*pc + 1];
	const tobj *object = vm_pushx_source(
		vm, env, tbycode_idxr_named_slot(idxr),
		tbycode_idxr_named_address(idxr));
	if (object->type != tcompo || !object->val.v_tcompo)
		return 0;
	if (*pc + 2 < end &&
	    (tbycode_ins(cmdarr[*pc + 2]) == OP_POPCOV ||
	     (tbycode_ins(cmdarr[*pc + 2]) == OP_VCRT &&
	      (tbycode_get_R(cmdarr[*pc + 2]) & TVCRT_INIT_FLAG)))) {
		tobj result;
		if (!vm_idxr_fast(object->val.v_tcompo, src, 1,
				  tbycode_idxr_compare(idxr), &result))
			return 0;
		*pc += 1;
		if (vm_store_owned_consumed(vm, env, cmdarr, pc, end,
					    &result))
			return 1;
		*pc -= 1;
		/* A father-environment target is rare; leave the owned
		 * result on the stack and let POPCOV walk the chain. */
		vm->stk[vm->stklen] = result;
		vm->stklen++;
		*pc += 1;
		return 1;
	}
	tobj *dest = stk_free(vm);
	if (!vm_idxr_fast(object->val.v_tcompo, src, 1,
			  tbycode_idxr_compare(idxr), dest))
		return 0;
	vm->stklen++;
	*pc += 1;
	if (*pc + 1 < end && tbycode_ins(cmdarr[*pc + 1]) == OP_NOT) {
		if (dest->type != tbool)
			twarn(ErrRuntime_ParamsType, "OP_NOT", "");
		dest->val.v_tbool = !dest->val.v_tbool;
		*pc += 1;
	}
	vm_consume_cjpop(vm, cmdarr, pc, end);
	return 1;
}

static inline TVM_ALWAYS_INLINE int vm_native_call_fused(
	tvm *vm, tcompo_env *env, tbycode *cmdarr, uint_cmds *pc,
	uint_cmds end, const long *cints, const tobj *first_arg,
	tcall_request *call, tobj *out_params)
{
	const tobj *args[VM_NATIVE_FUSED_MAX_PARAMS];
	tobj immediates[VM_NATIVE_FUSED_MAX_PARAMS];
	uint_regs nargs = 1;
	args[0] = first_arg;
	uint_cmds j = *pc + 1;
	while (nargs < VM_NATIVE_FUSED_MAX_PARAMS && j < end) {
		tbycode a = cmdarr[j];
		if (tbycode_ins(a) == OP_PUSHX) {
			args[nargs++] = vm_pushx_source(
				vm, env, tbycode_get_L(a), tbycode_get_R(a));
			j++;
			continue;
		}
		if (tbycode_ins(a) == OP_PUSHI) {
			vm_set_int_result(&immediates[nargs],
				tbycode_pushi_is_immediate(a) ?
					tbycode_pushi_immediate_value(a) :
					cints[tbycode_get_U(a)]);
			args[nargs] = &immediates[nargs];
			nargs++;
			j++;
			continue;
		}
		break;
	}
	if (j >= end)
		return 0;
	tins callop = tbycode_ins(cmdarr[j]);
	/* Tapas 函数调用（自调用或槽位函数）：参数搬入局部数组，补一次
	 * 引用供被调帧 move_params 转移所有权；返回 2 由调用方进入
	 * make_call。返回值落到空栈上（return_stack_values 为零）。 */
	if (callop == OP_EVALTF &&
	    (uint_regs)tbycode_get_U(cmdarr[j]) == nargs) {
		if (!env->owner_func)
			return 0;
		for (uint_regs k = 0; k < nargs; k++) {
			out_params[k] = *args[k];
			vm_retain(&out_params[k]);
		}
		call->function = env->owner_func;
		call->params = out_params;
		call->nparams = nargs;
		call->return_stack_values = 0;
		call->stack_has_callable = 0;
		call->tail_self_call = 1;
		call->retain_callable = nullptr;
		*pc = j;
		return 2;
	}
	if (callop == OP_EVALDF && tbycode_get_b(cmdarr[j]) == nargs) {
		uint8_t encoded_address = tbycode_get_i(cmdarr[j]);
		uint8_t address = encoded_address & 0x0f;
		tobj *callable = address ?
			vm_direct_slot(env, tbycode_get_L(cmdarr[j]),
				       address - 1) :
			tmp_obj(vm, tbycode_get_L(cmdarr[j]));
		if (callable->type != tcompo || !callable->val.v_tcompo ||
		    callable->val.v_tcompo->vtable != &tfunc_vtable)
			return 0;
		for (uint_regs k = 0; k < nargs; k++) {
			out_params[k] = *args[k];
			vm_retain(&out_params[k]);
		}
		call->function = (tfunc *)callable->val.v_tcompo;
		call->params = out_params;
		call->nparams = nargs;
		call->return_stack_values = 0;
		call->stack_has_callable = 0;
		call->tail_self_call = 0;
		call->retain_callable = (encoded_address & 0x10) ?
			callable->val.v_tcompo : nullptr;
		*pc = j;
		return 2;
	}
	if (callop != OP_EVALCF || tbycode_get_i(cmdarr[j]) == 0 ||
	    tbycode_get_b(cmdarr[j]) != nargs)
		return 0;
	tobj *callable = vm_direct_slot(env, tbycode_get_L(cmdarr[j]),
					tbycode_get_i(cmdarr[j]) - 1);
	if (callable->type != tcompo || !callable->val.v_tcompo ||
	    callable->val.v_tcompo->vtable != &tcppgenf_vtable)
		return 0;
	tcppgenf *fn = (tcppgenf *)callable->val.v_tcompo;
	if (!fn->f || !tcppgenf_accepts(fn, nargs))
		return 0;
	tobj params[VM_NATIVE_FUSED_MAX_PARAMS];
	for (uint_regs k = 0; k < nargs; k++)
		params[k] = *args[k];
	fn->f(params, nargs, &vm->rev);
	*pc = j;
	if (j + 1 < end && tbycode_ins(cmdarr[j + 1]) == OP_POPN &&
	    tbycode_get_L(cmdarr[j + 1]) == 1 &&
	    tbycode_popn_temporary_count(cmdarr[j + 1]) == 0) {
		if (tbycode_popn_print(cmdarr[j + 1]) && vm->rev.type != tnil) {
			tstring *text = tobj_tostring_full(&vm->rev);
			printf("%s\n", tstring_cstr(text));
			tstring_free(text);
		}
		tobj_try_clear(&vm->rev);
		*pc = vm_fused_next(cmdarr, end, j + 2);
		return 1;
	}
	vm->stk[vm->stklen] = vm->rev;
	vm->stklen++;
	tvm_set_rev_empty(vm);
	/* 内建回调交出的是未计数的初始所有权；栈槽按计数引用管理。 */
	vm_own_result(stk_top(vm));
	vm_consume_cjpop(vm, cmdarr, pc, end);
	vm_consume_popcov(vm, env, cmdarr, pc, end);
	return 1;
}


void exec_tins(tvm *vm, uint_cmds from, uint_cmds ncmds, tcompo_env *env)
{
	twrapper *wrapper = tfunc_get_wrapper_from_env(env);
	/* Nested Rule/evaluator execution may grow vm->code_caches. Keep the
	 * descriptor by value; its separately allocated entries remain stable. */
	tvm_code_cache code_cache = *tvm_get_code_cache(vm, wrapper);
	tbycode *cmdarr = wrapper->cmdarr;
	long *cints = wrapper->consts.cints;
	double *cflts = wrapper->consts.cflts;
	tstring **cstrs = wrapper->consts.cstrs;
	tstr **csobjs = wrapper->consts.csobjs;
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
#if (defined(__GNUC__) || defined(__clang__)) && !defined(TAPAS_VM_NO_THREADED)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wgnu-label-as-value"
#pragma GCC diagnostic ignored "-Wgnu-designator"
#if defined(__clang__)
#pragma GCC diagnostic ignored "-Winitializer-overrides"
#else
#pragma GCC diagnostic ignored "-Woverride-init"
#endif
#define TAPAS_VM_THREADED 1
#define VM_CASE(op) vm_lbl_##op
#define VM_DEFAULT vm_lbl_invalid
#define VM_NEXT() goto vm_advance
#define VM_SWITCH_BEGIN goto *vm_dispatch_table[tbycode_ins(*iter)];
#define VM_SWITCH_END
#else
#define VM_CASE(op) case op
#define VM_DEFAULT default
#define VM_NEXT() break
#define VM_SWITCH_BEGIN switch (tbycode_ins(cmdarr[i])) {
#define VM_SWITCH_END }
#endif

#ifdef TAPAS_VM_THREADED
	static const void *const vm_dispatch_table[64] = {
		[0 ... 63] = &&vm_lbl_invalid,
		[OP_PASS] = &&vm_lbl_OP_PASS,
		[OP_VCRT] = &&vm_lbl_OP_VCRT,
		[OP_TMPDEL] = &&vm_lbl_OP_TMPDEL,
		[OP_THIS] = &&vm_lbl_OP_THIS,
		[OP_BASE] = &&vm_lbl_OP_BASE,
		[OP_RET] = &&vm_lbl_OP_RET,
		[OP_IN] = &&vm_lbl_OP_IN,
		[OP_PAIR] = &&vm_lbl_OP_PAIR,
		[OP_TO] = &&vm_lbl_OP_TO,
		[OP_POPN] = &&vm_lbl_OP_POPN,
		[OP_POPCOV] = &&vm_lbl_OP_POPCOV,
		[OP_TYPEFWD] = &&vm_lbl_OP_TYPEFWD,
		[OP_TYPEDEFINE] = &&vm_lbl_OP_TYPEDEFINE,
		[OP_LOOPAS] = &&vm_lbl_OP_LOOPAS,
		[OP_LOOPRANGE] = &&vm_lbl_OP_LOOPRANGE,
		[OP_JPF] = &&vm_lbl_OP_JPF,
		[OP_JPB] = &&vm_lbl_OP_JPB,
		[OP_CJPFPOP] = &&vm_lbl_OP_CJPFPOP,
		[OP_CJPBPOP] = &&vm_lbl_OP_CJPBPOP,
		[OP_PUSHX] = &&vm_lbl_OP_PUSHX,
		[OP_PUSHI] = &&vm_lbl_OP_PUSHI,
		[OP_PUSHFLT] = &&vm_lbl_OP_PUSHFLT,
		[OP_PUSHB] = &&vm_lbl_OP_PUSHB,
		[OP_PUSHS] = &&vm_lbl_OP_PUSHS,
		[OP_PUSHDICT] = &&vm_lbl_OP_PUSHDICT,
		[OP_PUSHLIST] = &&vm_lbl_OP_PUSHLIST,
		[OP_PUSHINFO] = &&vm_lbl_OP_PUSHINFO,
		[OP_IMPORT] = &&vm_lbl_OP_IMPORT,
		[OP_IDXR] = &&vm_lbl_OP_IDXR,
		[OP_EVALSF] = &&vm_lbl_OP_EVALSF,
		[OP_EVALCF] = &&vm_lbl_OP_EVALCF,
		[OP_EVALTF] = &&vm_lbl_OP_EVALTF,
		[OP_EVALDF] = &&vm_lbl_OP_EVALDF,
		[OP_EVAL] = &&vm_lbl_OP_EVAL,
		[OP_IDXL] = &&vm_lbl_OP_IDXL,
		[OP_PUSHF] = &&vm_lbl_OP_PUSHF,
		[OP_PUSHRULE] = &&vm_lbl_OP_PUSHRULE,
		[OP_RULEVALUE] = &&vm_lbl_OP_RULEVALUE,
		[OP_RULENOT] = &&vm_lbl_OP_RULENOT,
		[OP_RULETRUTH] = &&vm_lbl_OP_RULETRUTH,
		[OP_RULEITEM] = &&vm_lbl_OP_RULEITEM,
		[OP_RULEIMPLY] = &&vm_lbl_OP_RULEIMPLY,
		[OP_ADD] = &&vm_lbl_OP_ADD,
		[OP_SUB] = &&vm_lbl_OP_SUB,
		[OP_MUL] = &&vm_lbl_OP_MUL,
		[OP_DIV] = &&vm_lbl_OP_DIV,
		[OP_MOD] = &&vm_lbl_OP_MOD,
		[OP_POW] = &&vm_lbl_OP_POW,
		[OP_MMUL] = &&vm_lbl_OP_MMUL,
		[OP_EQ] = &&vm_lbl_OP_EQ,
		[OP_NE] = &&vm_lbl_OP_NE,
		[OP_GE] = &&vm_lbl_OP_GE,
		[OP_SG] = &&vm_lbl_OP_SG,
		[OP_LE] = &&vm_lbl_OP_LE,
		[OP_SL] = &&vm_lbl_OP_SL,
		[OP_AND] = &&vm_lbl_OP_AND,
		[OP_OR] = &&vm_lbl_OP_OR,
		[OP_BAND] = &&vm_lbl_OP_BAND,
		[OP_BOR] = &&vm_lbl_OP_BOR,
		[OP_POS] = &&vm_lbl_OP_POS,
		[OP_NEG] = &&vm_lbl_OP_NEG,
		[OP_NOT] = &&vm_lbl_OP_NOT
	};
#endif

	for (;;) {
	dispatch:
		if (i >= end) {
			if (vm->frame_len == base_frame_depth)
				break;
			tvm_set_rev_empty(vm);
			goto resume_caller;
		}

		tbycode *iter = &cmdarr[i];
		tcall_request call;
		VM_SWITCH_BEGIN
		VM_CASE(OP_PASS):
			VM_NEXT();
		VM_CASE(OP_VCRT): {
			uint16_t flags = tbycode_get_R(*iter);
			int isenv = (flags & TVCRT_ENV_FLAG) != 0;
			uint_csts nl = tbycode_get_L(*iter);
			tobj_array *slots;
			if (isenv) {
				vm_add_slot(&env->base.objs, nl);
				slots = &env->base.objs;
			} else {
				vm_add_slot(&vm->tmps, UNDEF_NAMELOC);
				slots = &vm->tmps;
			}
			if (flags & TVCRT_INIT_FLAG) {
				if (stk_len(vm) == 0 || stk_top(vm)->type == tnil)
					twarn(ErrRuntime_AssignNil, "OP_VCRT", "");
				tobj_array_set_obj(slots, slots->len - 1,
						   stk_top(vm));
				stk_popc(vm);
			} else if (i + 3 < end &&
				   (tbycode_ins(cmdarr[i + 1]) == OP_PUSHX ||
				    tbycode_ins(cmdarr[i + 1]) == OP_PUSHI) &&
				   tbycode_binop_named(cmdarr[i + 2]) &&
				   tbycode_ins(cmdarr[i + 3]) == OP_POPCOV) {
				/* `let x = a <op> b`：刚创建的槽位即 POPCOV
				 * 目标时，算术与初始化一次完成。 */
				tins op = tbycode_ins(cmdarr[i + 2]);
				if (op == OP_ADD || op == OP_SUB || op == OP_MUL ||
				    op == OP_DIV) {
					tbycode store = cmdarr[i + 3];
					tobj_array *dst =
						tbycode_get_R(store) ?
							&env->base.objs :
							&vm->tmps;
					uint_objs dloc =
						(uint_objs)tbycode_get_L(store);
					if (dst == slots && dloc == slots->len - 1 &&
					    dloc < dst->len) {
						tobj right;
						if (tbycode_ins(cmdarr[i + 1]) == OP_PUSHI) {
							vm_set_int_result(&right,
								tbycode_pushi_is_immediate(cmdarr[i + 1]) ?
									tbycode_pushi_immediate_value(cmdarr[i + 1]) :
									cints[tbycode_get_U(cmdarr[i + 1])]);
						} else {
							right = *vm_pushx_source(
								vm, env,
								tbycode_get_L(cmdarr[i + 1]),
								tbycode_get_R(cmdarr[i + 1]));
						}
						const tobj *left = vm_pushx_source(
							vm, env,
							tbycode_get_L(cmdarr[i + 2]),
							tbycode_binop_address(cmdarr[i + 2]));
						tobj result;
						int ok;
						switch (op) {
						case OP_ADD:
							ok = add_impl(left->type, right.type, left, &right, &result);
							break;
						case OP_SUB:
							ok = sub_impl(left->type, right.type, left, &right, &result);
							break;
						case OP_MUL:
							ok = mul_impl(left->type, right.type, left, &right, &result);
							break;
						default:
							ok = div_impl(left->type, right.type, left, &right, &result);
							break;
						}
						if (ok) {
							slots->data[dloc] = result;
							i = vm_fused_next(cmdarr, end,
									  i + 4);
							VM_NEXT();
						}
					}
				}
			} else if (i + 3 < end &&
				   tbycode_ins(cmdarr[i + 1]) == OP_PUSHX &&
				   tbycode_ins(cmdarr[i + 2]) == OP_IDXR &&
				   tbycode_idxr_named(cmdarr[i + 2]) &&
				   tbycode_idxr_count(cmdarr[i + 2]) == 1 &&
				   !tbycode_idxr_slice(cmdarr[i + 2]) &&
				   tbycode_ins(cmdarr[i + 3]) == OP_POPCOV) {
				/* `let x = recv[k]`：刚创建的槽位就是 POPCOV
				 * 目标时，索引读 + 初始化一次完成。 */
				tbycode idxr = cmdarr[i + 2];
				tbycode store = cmdarr[i + 3];
				tobj_array *dst =
					tbycode_get_R(store) ?
						&env->base.objs :
						&vm->tmps;
				uint_objs dloc = (uint_objs)tbycode_get_L(store);
				if (dst == slots &&
				    dloc == slots->len - 1 && dloc < dst->len) {
					const tobj *key = vm_pushx_source(
						vm, env,
						tbycode_get_L(cmdarr[i + 1]),
						tbycode_get_R(cmdarr[i + 1]));
					const tobj *object = vm_pushx_source(
						vm, env,
						tbycode_idxr_named_slot(idxr),
						tbycode_idxr_named_address(idxr));
					if (object->type == tcompo &&
					    object->val.v_tcompo &&
					    vm_idxr_fast(object->val.v_tcompo,
							 (tobj *)key, 1,
							 tbycode_idxr_compare(idxr),
							 &slots->data[dloc])) {
						if (slots->data[dloc].type == tnil)
							twarn(ErrRuntime_AssignNil,
							      "OP_POPCOV", "");
						i = vm_fused_next(cmdarr, end,
								  i + 4);
						VM_NEXT();
					}
				}
			}
		}
		VM_NEXT();
		VM_CASE(OP_TMPDEL):
			vm_del_slots(&vm->tmps, (uint_objs)tbycode_get_U(*iter));
			/* 循环体末尾的 TMPDEL 之后紧跟回边；一并消费。 */
			i = vm_fused_next(cmdarr, end, i + 1);
			VM_NEXT();
		VM_CASE(OP_THIS): {
			tobj current;
			tobj_set_nil(&current);
			tcompo_env_copy_to_obj(env, &current);
			stk_push(vm, &current);
			tobj_set_nil(&current);
		}
		VM_NEXT();
		VM_CASE(OP_BASE): {
			tobj v;
			tobj_set_nil(&v);
			tcompo_env_copy_to_obj(tcompo_env_get_father(env), &v);
			stk_push(vm, &v);
			tobj_set_nil(&v);
		}
		VM_NEXT();
		VM_CASE(OP_RET):
			if (stk_len(vm) > 0) {
				if (stk_top(vm)->type == tcompo &&
				    tobj_compo_type(stk_top(vm)) == compo_tfunc)
					tfunc_close_over(
						(tfunc *)stk_top(vm)->val.v_tcompo,
						env);
				else if (stk_top(vm)->type == tcompo &&
					 tobj_compo_type(stk_top(vm)) == compo_trule)
					trule_close_over(
						(trule *)stk_top(vm)->val.v_tcompo, env);
				vm->rev = *stk_top(vm);
				stk_pop(vm);
			} else
				tvm_set_rev_empty(vm);
			stk_popcn(vm, stk_len(vm));
			i = end;
			goto do_return;
		VM_CASE(OP_IN): {
			tobj r;
			tobj_set_nil(&r);
			const tobj *member = stk_top(vm);
			const tobj *collection = stk_at(vm, 1);
			if (collection->type == tcompo &&
			    collection->val.v_tcompo &&
			    collection->val.v_tcompo->vtable == &tdict_vtable &&
			    member->type != tcompo) {
				/* Non-composite members are never Rule terms, so
				 * dict membership can skip operator_in's term
				 * machinery. Integer members resolve at the first
				 * probe; an empty bucket proves absence. */
				thashtbl *items =
					((tdict *)collection->val.v_tcompo)
						->items;
				int hit = 0;
				if (items->capacity == 0)
					hit = 0;
				else if (member->type == tint) {
					uint64_t hash = thashtbl_hash_int(
						(uint64_t)member->val.v_tint);
					thash_entry *entry = &items->entries
						[hash & (items->capacity - 1)];
					hit = entry->hash == hash &&
					      entry->key.type == tint &&
					      entry->key.val.v_tint ==
						      member->val.v_tint;
					/* Any non-empty first bucket may hide a
					 * collision chain; probe exactly. */
					if (!hit && entry->hash != 0)
						hit = thashtbl_contains(
							items, member);
				} else
					hit = thashtbl_contains(items, member);
				tobj_set_bool(&r, hit);
			} else
				operator_in(member, collection, &r);
			stk_popcn(vm, 2);
			stk_push(vm, &r);
			tobj_set_nil(&r);
			vm_consume_cjpop(vm, cmdarr, &i, end);
			vm_consume_popcov(vm, env, cmdarr, &i, end);
		}
		VM_NEXT();
		VM_CASE(OP_PAIR): {
			tobj r;
			tobj_set_nil(&r);
			operator_pair(stk_top(vm), stk_at(vm, 1), &r);
			stk_popcn(vm, 2);
			stk_push(vm, &r);
			tobj_set_nil(&r);
		}
		VM_NEXT();
		VM_CASE(OP_TO): {
			tobj r;
			tobj_set_nil(&r);
			operator_to(stk_top(vm), stk_at(vm, 1), &r);
			stk_popcn(vm, 2);
			stk_push(vm, &r);
			tobj_set_nil(&r);
		}
		VM_NEXT();
		VM_CASE(OP_POPN): {
			int print = tbycode_popn_print(*iter);
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
			vm_del_slots(&vm->tmps,
				(uint_objs)tbycode_popn_temporary_count(*iter));
		}
		VM_NEXT();
		VM_CASE(OP_POPCOV):
			if (stk_top(vm)->type == tnil)
				twarn(ErrRuntime_AssignNil, "OP_POPCOV", "");
			if (tbycode_get_R(*iter))
				tcompo_env_set_obj(
					env, tbycode_get_L(*iter), stk_top(vm));
			else
				tobj_array_set_obj(
					&vm->tmps, tbycode_get_L(*iter),
					stk_top(vm));
			stk_popc(vm);
			VM_NEXT();
		VM_CASE(OP_TYPEFWD): {
			tobj value;
			tobj_set_nil(&value);
			tobj_set_compo(&value, (tcompo_v *)ttypeval_new_recursive());
			stk_push(vm, &value);
			tobj_set_nil(&value);
		}
		VM_NEXT();
		VM_CASE(OP_TYPEDEFINE): {
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
		}
		VM_NEXT();
		VM_CASE(OP_LOOPAS): {
			tins_cache *lcache = tvm_instruction_cache(&code_cache, i);
			/* List iteration guarded by CJPFPOP (the canonical loop
			 * shape): step and branch without the condition's
			 * stack round trip. */
			if (i + 1 < end &&
			    tbycode_ins(cmdarr[i + 1]) == OP_CJPFPOP) {
				tobj *viter = stk_top(vm);
				if (viter->type == tcompo && viter->val.v_tcompo &&
				    lcache &&
				    (viter->val.v_tcompo->vtable == &tlist_vtable ||
				     viter->val.v_tcompo->vtable == &tstr_vtable)) {
					tloop_state *state =
						lcache->state_slot <
							vm->loop_state_len ?
							&vm->loop_states
								[lcache->state_slot] :
							tvm_loop_state(vm,
								lcache->state_slot);
					if (state->iterator_slot != viter) {
						state->pos = 0;
						state->iterator_slot = viter;
					}
					int has_next;
					if (viter->val.v_tcompo->vtable == &tlist_vtable) {
						tlist *list = (tlist *)viter->val.v_tcompo;
						has_next = (uint_objs)state->pos <
							list->items.len;
						if (has_next)
							tobj_copy(
								vm_loop_target(
									vm,
									tbycode_get_L(*iter),
									tbycode_get_R(*iter),
									env),
								&list->items.data[state->pos++]);
					} else {
						tstr *str = (tstr *)viter->val.v_tcompo;
						size_t length = tstring_len(str->data);
						has_next = state->pos >= 0 &&
							(size_t)state->pos < length;
						if (has_next) {
							/* Single characters come from the
							 * shared byte-string cache. */
							tobj ch;
							tobj_set_nil(&ch);
							tobj_set_compo(&ch,
								(tcompo_v *)tstr_new_len(
									tstring_cstr(str->data) +
										state->pos,
									1));
							state->pos++;
							tobj *target = vm_loop_target(
								vm,
								tbycode_get_L(*iter),
								tbycode_get_R(*iter),
								env);
							tobj_copy(target, &ch);
							tobj_ddc_ref_clear(&ch);
						}
					}
					if (has_next) {
						i++;
						VM_NEXT();
					}
					state->pos = 0;
					state->iterator_slot = nullptr;
					i += 1u + tbycode_get_U(cmdarr[i + 1]);
					VM_NEXT();
				}
			}
			vm_loopas(vm,
				  lcache,
				  tbycode_get_L(*iter),
				  tbycode_get_R(*iter),
				  env);
			VM_NEXT();
		}
		VM_CASE(OP_LOOPRANGE): {
			int has_next = vm_loop_range_step(
				vm, tvm_instruction_cache(&code_cache, i),
				tbycode_get_L(*iter), tbycode_get_R(*iter), env);
			/* The compiler always guards the body with CJPFPOP;
			 * consume the condition directly as the jump decision
			 * and skip its stack round trip. */
			if (i + 1 < end &&
			    tbycode_ins(cmdarr[i + 1]) == OP_CJPFPOP) {
				if (has_next)
					i++;
				else
					i += 1u + tbycode_get_U(cmdarr[i + 1]);
				VM_NEXT();
			}
			vm_loop_push_cond(vm, has_next);
			VM_NEXT();
		}
		VM_CASE(OP_JPF):
			i += tbycode_get_U(*iter);
			iter += tbycode_get_U(*iter);
			VM_NEXT();
		VM_CASE(OP_JPB):
			i -= tbycode_get_U(*iter);
			iter -= tbycode_get_U(*iter);
			VM_NEXT();
		VM_CASE(OP_CJPFPOP):
			if (stk_top(vm)->type != tbool)
				twarn(ErrRuntime_ParamsType, "OP_CJPFPOP", "");
			if (stk_top(vm)->val.v_tbool == 0) {
				uint_cmds u = tbycode_get_U(*iter);
				i += u;
				iter += u;
				stk_pop(vm);
			} else
				stk_popc(vm);
			VM_NEXT();
		VM_CASE(OP_CJPBPOP):
			if (stk_top(vm)->type != tbool)
				twarn(ErrRuntime_ParamsType, "OP_CJPBPOP", "");
			if (stk_top(vm)->val.v_tbool != 0) {
				uint_cmds u = tbycode_get_U(*iter);
				i += u;
				iter += u;
				stk_pop(vm);
			} else
				stk_popc(vm);
			VM_NEXT();
		VM_CASE(OP_PUSHX): {
			uint_objs loc = tbycode_get_L(*iter);
			uint16_t addr = tbycode_get_R(*iter);
			tobj *src = vm_pushx_source(vm, env, loc, addr);
			/* 学过"无融合"的槽位推送直接跳过整条探测链。 */
			tins_cache *pcache = tvm_instruction_cache(&code_cache, i);
			if (pcache && pcache->kind == tins_cache_pushx_plain)
				goto pushx_plain;
			/* 按后继指令一次性判定融合形态；字节码不可变，
			 * 各形态的分支条件预测稳定，省掉整条顺序探测链。 */
			tins f1 = i + 1 < end ? tbycode_ins(cmdarr[i + 1]) : OP_PASS;
			if (f1 == OP_POPCOV || f1 == OP_VCRT) {
			/* `dst = src` 纯拷贝赋值：最高频的语句形态，直接
			 * 槽到槽转移。先保留后释放，`x = x` 与别名安全。 */
			tins store_kind =
				i + 1 < end ? tbycode_ins(cmdarr[i + 1]) : OP_PASS;
			if (store_kind == OP_POPCOV) {
				tbycode store = cmdarr[i + 1];
				tobj_array *slots =
					tbycode_get_R(store) ?
						&env->base.objs :
						&vm->tmps;
				uint_objs dloc = (uint_objs)tbycode_get_L(store);
				if (dloc < slots->len) {
					if (src->type == tnil)
						twarn(ErrRuntime_AssignNil,
						      "OP_POPCOV", "");
					tobj *dslot = &slots->data[dloc];
					if (dslot != src) {
						vm_retain(src);
						vm_slot_move(dslot, src);
					}
					i = vm_fused_next(cmdarr, end, i + 2);
					VM_NEXT();
				}
			} else if (store_kind == OP_VCRT &&
				   (tbycode_get_R(cmdarr[i + 1]) &
				    TVCRT_INIT_FLAG)) {
				/* `let x = y`：新建槽位并初始化。 */
				tbycode store = cmdarr[i + 1];
				if (src->type == tnil)
					twarn(ErrRuntime_AssignNil, "OP_VCRT", "");
				tobj_array *slots =
					(tbycode_get_R(store) & TVCRT_ENV_FLAG) ?
						&env->base.objs :
						&vm->tmps;
				vm_add_slot(slots, (uint_csts)tbycode_get_L(store));
				vm_retain(src);
				slots->data[slots->len - 1] = *src;
				i = vm_fused_next(cmdarr, end, i + 2);
				VM_NEXT();
			}

			} else if (f1 == OP_PUSHX) {
				tins f2 = i + 2 < end ? tbycode_ins(cmdarr[i + 2]) : OP_PASS;
				if (f2 == OP_IDXL) {
			/* `slot[key] = value` with both operands in slots: resolve
			 * the List/Dictionary write without the stack round trip
			 * of the two pushes. */
			if (i + 2 < end &&
			    tbycode_ins(cmdarr[i + 1]) == OP_PUSHX &&
			    tbycode_ins(cmdarr[i + 2]) == OP_IDXL &&
			    tbycode_get_b(cmdarr[i + 2]) == 1) {
				tbycode idxl = cmdarr[i + 2];
				/* `values[i] = i` 式写法中两个操作数来自同一
				 * 槽位，跳过第二次解析。 */
				const tobj *key =
					(tbycode_get_L(cmdarr[i + 1]) == loc &&
					 tbycode_get_R(cmdarr[i + 1]) == addr) ?
						src :
						vm_pushx_source(
							vm, env,
							tbycode_get_L(cmdarr[i + 1]),
							tbycode_get_R(cmdarr[i + 1]));
				uint_objs tloc = (uint_objs)tbycode_get_L(idxl);
				tobj *objp = nullptr;
				if (tbycode_get_i(idxl)) {
					if (tloc < env->base.objs.len)
						objp = &env->base.objs.data[tloc];
				} else if (tloc < vm->tmps.len)
					objp = &vm->tmps.data[tloc];
				if (key->type == tint && objp &&
				    objp->type == tcompo && objp->val.v_tcompo) {
					tcompo_v *arr = objp->val.v_tcompo;
					if (arr->vtable == &tdict_vtable) {
						vm_dict_set_int(
							((tdict *)arr)->items,
							key, src);
						i = vm_fused_next(cmdarr, end,
								  i + 3);
						VM_NEXT();
					}
					if (arr->vtable == &tlist_vtable) {
						tlist *list = (tlist *)arr;
						long index = key->val.v_tint;
						uint_count len = list->items.len;
						if (index < 0)
							index += (long)len;
						if (index < 0 ||
						    (uint_objs)index >= len)
							twarn(ErrRuntime_IdxOutRange,
							      "list assignment", "");
						/* Both-immediate stores keep the
						 * composite count untouched and
						 * need no reference traffic. */
						tobj *slotp =
							&list->items.data[index];
						if (slotp->type != tcompo &&
						    src->type != tcompo)
							*slotp = *src;
						else
							tobj_vec_set(&list->items,
								(uint_objs)index,
								src);
						i = vm_fused_next(cmdarr, end,
								  i + 3);
						VM_NEXT();
					}
				}
			}

				} else if (f2 == OP_IDXR &&
				    tbycode_idxr_named(cmdarr[i + 2])) {
					if (tbycode_idxr_count(cmdarr[i + 2]) == 2) {
			/* `matrix[row, column]` on a dense array with both indices
			 * in slots: resolve the 2D read without stack traffic. */
			if (i + 2 < end &&
			    tbycode_ins(cmdarr[i + 1]) == OP_PUSHX &&
			    tbycode_ins(cmdarr[i + 2]) == OP_IDXR &&
			    tbycode_idxr_named(cmdarr[i + 2]) &&
			    tbycode_idxr_count(cmdarr[i + 2]) == 2 &&
			    !tbycode_idxr_slice(cmdarr[i + 2])) {
				tbycode idxr = cmdarr[i + 2];
				const tobj *object = vm_pushx_source(
					vm, env, tbycode_idxr_named_slot(idxr),
					tbycode_idxr_named_address(idxr));
				if (object->type == tcompo && object->val.v_tcompo &&
				    (object->val.v_tcompo->vtable ==
					     &tdarr_vtable ||
				     object->val.v_tcompo->vtable ==
					     &tbarr_vtable)) {
					const tobj *second = vm_pushx_source(
						vm, env,
						tbycode_get_L(cmdarr[i + 1]),
						tbycode_get_R(cmdarr[i + 1]));
					if (src->type == tint &&
					    second->type == tint) {
						tobj params[2];
						params[0] = *src;
						params[1] = *second;
						tobj *dest = stk_free(vm);
						vm_dense_idx_int2(
							object->val.v_tcompo,
							params, dest);
						vm->stklen++;
						i += 2;
						vm_consume_cjpop(vm, cmdarr, &i,
								 end);
						vm_consume_popcov(vm, env,
								  cmdarr, &i,
								  end);
						VM_NEXT();
					}
				}
			}

					} else {
			/* `container[key2] <op> slot` 扫描惯用法：索引读 +
			 * 栈式比较 + 条件跳，一次完成，无栈往返。 */
			if (i + 4 < end &&
			    tbycode_ins(cmdarr[i + 1]) == OP_PUSHX &&
			    tbycode_ins(cmdarr[i + 2]) == OP_IDXR &&
			    tbycode_idxr_named(cmdarr[i + 2]) &&
			    tbycode_idxr_count(cmdarr[i + 2]) == 1 &&
			    !tbycode_idxr_slice(cmdarr[i + 2]) &&
			    !tbycode_binop_named(cmdarr[i + 3]) &&
			    tbycode_get_L(cmdarr[i + 3]) == 0 &&
			    tbycode_get_R(cmdarr[i + 3]) == 1 &&
			    (tbycode_ins(cmdarr[i + 4]) == OP_CJPFPOP ||
			     tbycode_ins(cmdarr[i + 4]) == OP_CJPBPOP)) {
				tbycode idxr = cmdarr[i + 2];
				tins op = tbycode_ins(cmdarr[i + 3]);
				tbycode jump = cmdarr[i + 4];
				const tobj *object = vm_pushx_source(
					vm, env, tbycode_idxr_named_slot(idxr),
					tbycode_idxr_named_address(idxr));
				const tobj *key = vm_pushx_source(
					vm, env, tbycode_get_L(cmdarr[i + 1]),
					tbycode_get_R(cmdarr[i + 1]));
				tobj readval;
				int ok = 0;
				if (object->type == tcompo && object->val.v_tcompo &&
				    vm_idxr_fast(object->val.v_tcompo,
						 (tobj *)key, 1,
						 tbycode_idxr_compare(idxr),
						 &readval) &&
				    !vm_is_indexed_byte(&readval)) {
					/* 栈式二元运算的左操作数在栈顶：这里是
					 * 刚读出的值，右操作数是首个槽位。 */
					switch (op) {
					case OP_EQ:
						ok = eq_impl(readval.type, src->type, &readval, src, &readval);
						break;
					case OP_NE:
						ok = ne_impl(readval.type, src->type, &readval, src, &readval);
						break;
					case OP_SG:
						ok = sg_impl(readval.type, src->type, &readval, src, &readval);
						break;
					case OP_SL:
						ok = sl_impl(readval.type, src->type, &readval, src, &readval);
						break;
					case OP_GE:
						ok = ge_impl(readval.type, src->type, &readval, src, &readval);
						break;
					case OP_LE:
						ok = le_impl(readval.type, src->type, &readval, src, &readval);
						break;
					default:
						break;
					}
				}
				if (ok) {
					int taken = tbycode_ins(jump) == OP_CJPFPOP ?
						!readval.val.v_tbool :
						readval.val.v_tbool;
					if (taken)
						i = vm_jump_landing(
							cmdarr, end,
							i + 5u + (uint_cmds)tbycode_get_U(jump));
					else
						i += 4;
					VM_NEXT();
				}
			}

					}
				} else
					goto pushx_native;
			} else if (f1 == OP_EVALCF || f1 == OP_EVALTF ||
				   f1 == OP_EVALDF) {
			pushx_native: {
				/* 直接调用（原生或 Tapas 函数）：连续槽位/立即数
				 * 推送的参数解析进借用数组，不经 VM 栈。 */
				tobj call_params[VM_NATIVE_FUSED_MAX_PARAMS];
				int fused = vm_native_call_fused(
					vm, env, cmdarr, &i, end, cints, src,
					&call, call_params);
				if (fused == 2)
					goto make_call;
				if (fused)
					VM_NEXT();
			}

			} else if (f1 == OP_IDXR &&
			    tbycode_idxr_named(cmdarr[i + 1]) &&
			    tbycode_idxr_count(cmdarr[i + 1]) == 1 &&
			    !tbycode_idxr_slice(cmdarr[i + 1])) {
			{
				if (vm_pushx_idxr_named_fused(vm, env, cmdarr, &i,
							      end, src)) {
					VM_NEXT();
				}
			}
			} else if (f1 == OP_IDXL) {
			/* `recv[key] = value` with the value already on the
			 * stack: the current push supplies only the key. */
			if (i + 1 < end &&
			    tbycode_ins(cmdarr[i + 1]) == OP_IDXL &&
			    tbycode_get_b(cmdarr[i + 1]) == 1 &&
			    stk_len(vm) >= 1 && src->type == tint) {
				tbycode idxl = cmdarr[i + 1];
				uint_objs tloc = (uint_objs)tbycode_get_L(idxl);
				tobj *objp = nullptr;
				if (tbycode_get_i(idxl)) {
					if (tloc < env->base.objs.len)
						objp = &env->base.objs.data[tloc];
				} else if (tloc < vm->tmps.len)
					objp = &vm->tmps.data[tloc];
				if (objp && objp->type == tcompo &&
				    objp->val.v_tcompo) {
					tcompo_v *arr = objp->val.v_tcompo;
					if (arr->vtable == &tdict_vtable) {
						vm_dict_set_int(
							((tdict *)arr)->items,
							src, stk_top(vm));
						stk_popc(vm);
						i = vm_fused_next(cmdarr, end,
								  i + 2);
						VM_NEXT();
					}
					if (arr->vtable == &tlist_vtable) {
						tlist *list = (tlist *)arr;
						long index = src->val.v_tint;
						uint_count len = list->items.len;
						if (index < 0)
							index += (long)len;
						if (index < 0 ||
						    (uint_objs)index >= len)
							twarn(ErrRuntime_IdxOutRange,
							      "list assignment", "");
						tobj_vec_set(&list->items,
							(uint_objs)index,
							stk_top(vm));
						stk_popc(vm);
						i = vm_fused_next(cmdarr, end,
								  i + 2);
						VM_NEXT();
					}
				}
			}

			} else if (vm_is_binop_instruction(f1)) {
			/* `x <op> slot` compare-branch and `slot = slot <op>
			 * slot` accumulate-into-slot resolve without stack
			 * traffic. */
			if (vm_cmp_branch_fused(vm, env, cmdarr, &i, end, src)) {
				VM_NEXT();
			}
			if (vm_arith_named_fused(vm, env, cmdarr, &i, end, src)) {
				VM_NEXT();
			}

			if (i + 1 < end &&
			    vm_try_borrowed_left_binop(vm, src, cmdarr[i + 1])) {
				i++;
				vm_consume_cjpop(vm, cmdarr, &i, end);
				vm_consume_popcov(vm, env, cmdarr, &i, end);
				VM_NEXT();
			}

			} else if (f1 == OP_IDXR) {
			if (i + 1 < end &&
			    tbycode_ins(cmdarr[i + 1]) == OP_IDXR &&
			    !tbycode_idxr_named(cmdarr[i + 1])) {
				i++;
				if (!vm_idxr_slot_hot(vm, src, cmdarr[i]))
					vm_idxr_borrowed(
						vm, src, cmdarr[i],
						tvm_instruction_cache(
							&code_cache, i));
				VM_NEXT();
			}

			} else {
			if (pcache && pcache->kind == tins_cache_empty &&
			    i + 1 < end) {
				/* 下一条指令不构成任何融合前缀时才学习，
				 * 以免把暂时类型不匹配的可融合槽位永久降级。 */
				tins follower = tbycode_ins(cmdarr[i + 1]);
				if (follower != OP_PUSHX && follower != OP_IDXR &&
				    follower != OP_IDXL && follower != OP_EVALCF &&
				    !vm_is_binop_instruction(follower))
					pcache->kind = tins_cache_pushx_plain;
			}
		
			}
		pushx_plain:

			vm->stk[vm->stklen] = *src;
			if (src->type == tcompo && src->val.v_tcompo)
				src->val.v_tcompo->refctr++;
			vm->stklen++;
		}
		VM_NEXT();
		VM_CASE(OP_PUSHI): {
			long value = tbycode_pushi_is_immediate(*iter) ?
				tbycode_pushi_immediate_value(*iter) :
				cints[tbycode_get_U(*iter)];
			tobj cst;
			vm_set_int_result(&cst, value);
			/* 按后继指令一次性判定融合形态。 */
			tins f1 = i + 1 < end ? tbycode_ins(cmdarr[i + 1]) : OP_PASS;
			if (vm_is_binop_instruction(f1)) {
				/* `x <op> const` compare-branch 与 `slot = slot
				 * <op> const` 累加落槽，均无栈往返。 */
				if (vm_cmp_branch_fused(vm, env, cmdarr, &i,
							end, &cst))
					VM_NEXT();
				if (vm_arith_named_fused(vm, env, cmdarr, &i,
							 end, &cst))
					VM_NEXT();
			} else if (f1 == OP_IDXR &&
			    tbycode_idxr_named(cmdarr[i + 1]) &&
			    tbycode_idxr_count(cmdarr[i + 1]) == 1 &&
			    !tbycode_idxr_slice(cmdarr[i + 1])) {
				/* `recv[constant]` on a named receiver: read
				 * through the fast path without
				 * materializing the key on the stack. */
				tbycode idxr = cmdarr[i + 1];
				const tobj *object = vm_pushx_source(
					vm, env, tbycode_idxr_named_slot(idxr),
					tbycode_idxr_named_address(idxr));
				if (object->type == tcompo &&
				    object->val.v_tcompo) {
					tobj key;
					vm_set_int_result(&key, value);
					tobj *dest = stk_free(vm);
					if (vm_idxr_fast(
						    object->val.v_tcompo, &key,
						    1, tbycode_idxr_compare(idxr),
						    dest)) {
						vm->stklen++;
						i++;
						vm_consume_cjpop(vm, cmdarr,
								 &i, end);
						vm_consume_popcov(vm, env,
								  cmdarr, &i,
								  end);
						VM_NEXT();
					}
				}
			} else {
				tobj call_params[VM_NATIVE_FUSED_MAX_PARAMS];
				int fused = vm_native_call_fused(
					vm, env, cmdarr, &i, end, cints, &cst,
					&call, call_params);
				if (fused == 2)
					goto make_call;
				if (fused)
					VM_NEXT();
			}
			tobj v;
			tobj_set_nil(&v);
			vm_set_int_result(&v, value);
			stk_push(vm, &v);
		}
		VM_NEXT();
		VM_CASE(OP_PUSHFLT): {
			tobj v;
			tobj_set_nil(&v);
			vm_set_float_result(&v, cflts[tbycode_get_U(*iter)]);
			stk_push(vm, &v);
		}
		VM_NEXT();
		VM_CASE(OP_PUSHB): {
			tobj v;
			tobj_set_nil(&v);
			vm_set_bool_result(&v, (int)tbycode_get_U(*iter));
			stk_push(vm, &v);
		}
		VM_NEXT();
		VM_CASE(OP_PUSHS): {
			/* `obj::field` / `obj['field']` 读取：常量字符串键 +
			 * 槽位接收者 + IDXR。字典实例直接探测，键的内弦对象
			 * 指针配合 IDXR 的槽位缓存免去了逐字符比较。 */
			if (i + 2 < end &&
			    tbycode_ins(cmdarr[i + 1]) == OP_PUSHX &&
			    tbycode_ins(cmdarr[i + 2]) == OP_IDXR &&
			    !tbycode_idxr_named(cmdarr[i + 2]) &&
			    tbycode_idxr_count(cmdarr[i + 2]) == 1 &&
			    !tbycode_idxr_slice(cmdarr[i + 2])) {
				tbycode idxr = cmdarr[i + 2];
				const tobj *recv = vm_pushx_source(
					vm, env, tbycode_get_L(cmdarr[i + 1]),
					tbycode_get_R(cmdarr[i + 1]));
				if (recv->type == tcompo && recv->val.v_tcompo &&
				    recv->val.v_tcompo->vtable == &tdict_vtable) {
					tobj key;
					tobj_set_nil(&key);
					tobj_set_compo(&key,
						(tcompo_v *)csobjs[tbycode_get_U(*iter)]);
					thashtbl *items =
						((tdict *)recv->val.v_tcompo)->items;
					tins_cache *gcache = tvm_instruction_cache(
						&code_cache, i + 2);
					const tobj *value = nullptr;
					if (gcache &&
					    gcache->kind == tins_cache_idxr_dict_str &&
					    gcache->key == key.val.v_tcompo)
						value = thashtbl_entry_value_at(
							items, gcache->slot,
							key.val.v_tcompo);
					if (value) {
						/* 内弦键指针直中：内联返回。 */
						tobj *dest = stk_free(vm);
						*dest = *value;
						if (dest->type == tcompo &&
						    dest->val.v_tcompo)
							dest->val.v_tcompo->refctr++;
						vm->stklen++;
						i += 2;
						vm_consume_cjpop(vm, cmdarr,
								 &i, end);
						vm_consume_popcov(vm, env,
								  cmdarr, &i,
								  end);
						VM_NEXT();
					}
					vm_idxr_value(vm, recv->val.v_tcompo, &key, 1,
						tbycode_idxr_compare(idxr), 0,
						gcache);
					/* rev 移入栈槽后按计数引用管理。 */
					vm->stk[vm->stklen] = vm->rev;
					vm->stklen++;
					tvm_set_rev_empty(vm);
					vm_own_result(stk_top(vm));
					i += 2;
					/* `if (obj['flag'])`、`x = obj['field']` 等
					 * 紧随的条件跳与赋值一并消费。 */
					vm_consume_cjpop(vm, cmdarr, &i, end);
					vm_consume_popcov(vm, env, cmdarr, &i, end);
					VM_NEXT();
				}
			}
			tobj v;
			tobj_set_nil(&v);
			/* Constant strings are interned at load time and immutable:
			 * pushing shares the pooled object without allocation. */
			tobj_set_compo(
				&v, (tcompo_v *)csobjs[tbycode_get_U(*iter)]);
			stk_push(vm, &v);
			tobj_set_nil(&v);
		}
		VM_NEXT();
		VM_CASE(OP_PUSHDICT): {
			uint_regs n = (uint_regs)tbycode_pushdict_count(*iter);
			tdict *d = tdict_new_sized(n);
			if (tbycode_pushdict_kv(*iter)) {
				/* Key-value form: each entry pushes value then key;
				 * no intermediate Pair objects. */
				tobj *ps = stk_topn(vm, (uint_regs)(2 * n));
				for (uint_regs i = 0; i < n; i++) {
					const tobj *key = &ps[i * 2 + 1];
					const tobj *value = &ps[i * 2];
					if (key->type == tint)
						vm_dict_set_int(d->items, key,
								value);
					else
						tdict_set(d, key, value);
				}
				stk_popcn(vm, (uint_regs)(2 * n));
			} else {
				tobj *ps = stk_topn(vm, n);
				for (uint_regs i = 0; i < n; i++) {
					if (ps[i].type != tcompo || tobj_compo_type(&ps[i]) != compo_tpair)
						twarn(ErrRuntime_ParamsType, "dictionary literal", "Pair required");
					tpair *pair = (tpair *)ps[i].val.v_tcompo;
					/* Integer-keyed literals dominate; insert through the
					 * inline integer write instead of the general set. */
					if (pair->first.type == tint)
						vm_dict_set_int(d->items, &pair->first,
								&pair->second);
					else
						tdict_set(d, &pair->first, &pair->second);
				}
				stk_popcn(vm, n);
			}
			tobj v;
			tobj_set_nil(&v);
			tobj_set_compo(&v, (tcompo_v *)d);
			stk_push(vm, &v);
			tobj_set_nil(&v);
		}
		VM_NEXT();
		VM_CASE(OP_PUSHLIST): {
			uint_regs n = (uint_regs)tbycode_get_U(*iter);
			tlist *list = tlist_new_sized(n);
			tobj *values = n ? stk_topn(vm, n) : nullptr;
			if (n) {
				/* Transfer the stack's owned references into the list. A
				 * retain-and-clear round trip would do two reference-count
				 * operations per composite literal element. */
				memcpy(list->items.data, values, n * sizeof(tobj));
				list->items.len = n;
				for (uint_regs j = 0; j < n; j++)
					if (values[j].type == tcompo)
						list->items.compo_count++;
				vm->stklen -= n;
			}
			tobj value;
			tobj_set_nil(&value);
			tobj_set_compo(&value, (tcompo_v *)list);
			stk_push(vm, &value);
			tobj_set_nil(&value);
		}
		VM_NEXT();
		VM_CASE(OP_PUSHINFO): {
			tobj v;
			tobj_set_nil(&v);
			vm_set_int_result(&v, (long)tbycode_get_U(*iter));
			stk_push(vm, &v);
		}
		VM_NEXT();
		VM_CASE(OP_IMPORT):
			vm_import(vm, (uint_csts)tbycode_get_U(*iter), cstrs, env);
			VM_NEXT();
		VM_CASE(OP_IDXR):
			if (tbycode_idxr_named(*iter)) {
				const tobj *object = vm_pushx_source(
					vm, env, tbycode_idxr_named_slot(*iter),
					tbycode_idxr_named_address(*iter));
				if (!vm_idxr_slot_hot(vm, object, *iter))
					vm_idxr_borrowed(
						vm, object, *iter,
						tvm_instruction_cache(
							&code_cache, i));
			} else if (!vm_idxr_stack_hot(vm, *iter))
				vm_idxr(vm, *iter,
					tvm_instruction_cache(&code_cache, i));
			VM_NEXT();
		VM_CASE(OP_EVALSF): {
			int direct = tbycode_get_i(*iter) != 0;
			uint_regs nparams = direct ? tbycode_get_b(*iter) :
				(uint_regs)tbycode_get_U(*iter);
			tobj *callable = direct ?
				vm_direct_slot(env, tbycode_get_L(*iter),
					tbycode_get_i(*iter) - 1) : stk_top(vm);
			if (VM_UNLIKELY(callable->type != tcompo ||
					callable->val.v_tcompo == nullptr ||
					callable->val.v_tcompo->vtable != &tcppsessf_vtable))
				twarn(ErrRuntime_RefType, "OP_EVALSF",
				      "C Session Function required");
			tobj *params = nparams ? stk_topn(vm, nparams) : callable;
			vm_eval_sessfunc(vm, (tcppsessf *)callable->val.v_tcompo,
					direct ? params : stk_topn(vm, nparams + 1),
					nparams, env);
			if (vm_try_discard_eval_result(
				    vm, cmdarr, i, end,
				    nparams + (direct ? 0 : 1))) {
				i++;
				VM_NEXT();
			}
			vm_finish_eval(vm, nparams + (direct ? 0 : 1));
		}
		VM_NEXT();
		VM_CASE(OP_EVALCF): {
			int direct = tbycode_get_i(*iter) != 0;
			uint_regs nparams = direct ? tbycode_get_b(*iter) :
				(uint_regs)tbycode_get_U(*iter);
			tobj *callable = direct ?
				vm_direct_slot(env, tbycode_get_L(*iter),
					tbycode_get_i(*iter) - 1) : stk_top(vm);
			if (VM_UNLIKELY(callable->type != tcompo ||
					callable->val.v_tcompo == nullptr ||
					callable->val.v_tcompo->vtable != &tcppgenf_vtable))
				twarn(ErrRuntime_RefType, "OP_EVALCF",
				      "C Function required");
			tobj *params = nparams ? stk_topn(vm, nparams) : callable;
			vm_eval_cppfunc(vm, (tcppgenf *)callable->val.v_tcompo,
					direct ? params : stk_topn(vm, nparams + 1),
					nparams);
			if (vm_try_discard_eval_result(
				    vm, cmdarr, i, end,
				    nparams + (direct ? 0 : 1))) {
				i++;
				VM_NEXT();
			}
			vm_finish_eval(vm, nparams + (direct ? 0 : 1));
		}
		VM_NEXT();
		VM_CASE(OP_EVALTF): {
			uint_regs nparams = (uint_regs)tbycode_get_U(*iter);
			if (!env->owner_func)
				twarn(ErrRuntime_RefType,
				      "exec_tin", "missing current function");
			call.function = env->owner_func;
			call.params = stk_topn(vm, nparams);
			call.nparams = nparams;
			call.return_stack_values = nparams;
			call.stack_has_callable = 0;
			call.tail_self_call = 1;
			call.retain_callable = nullptr;
			goto make_call;
		}
		VM_CASE(OP_EVALDF): {
			uint_regs nparams = tbycode_get_b(*iter);
			uint8_t encoded_address = tbycode_get_i(*iter);
			uint8_t address = encoded_address & 0x0f;
			int retain_callable = (encoded_address & 0x10) != 0;
			tobj *callable = address ?
				vm_direct_slot(env, tbycode_get_L(*iter), address - 1) :
				tmp_obj(vm, tbycode_get_L(*iter));
			if (VM_UNLIKELY(callable->type != tcompo ||
					callable->val.v_tcompo == nullptr))
				twarn(ErrRuntime_RefType, "OP_EVALDF", "Function required");
			tcompo_v *value = callable->val.v_tcompo;
			if (value->vtable == &tfunc_vtable) {
				call.function = (tfunc *)value;
				call.params = stk_topn(vm, nparams);
				call.nparams = nparams;
				call.return_stack_values = nparams;
				call.stack_has_callable = 0;
				call.tail_self_call = 0;
				call.retain_callable = retain_callable ? value : nullptr;
				goto make_call;
			}

			/* A named binding may be reassigned to another callable kind.
			 * Dispatch it without materializing an extra owning stack slot;
			 * the binding itself keeps the callable alive for the call. */
			tobj *params = nparams ? stk_topn(vm, nparams) : callable;
			tobj retained;
			tobj_set_nil(&retained);
			if (address || retain_callable)
				tobj_copy(&retained, callable);
			switch (value->vtable->compo_code) {
			case compo_trule:
				tobj_set_compo(&vm->rev, (tcompo_v *)rule_bind_call(
					(trule *)value, params, nparams));
				break;
			case compo_cppfunc:
				vm_eval_cppfunc(vm, (tcppgenf *)value, params, nparams);
				break;
			case compo_sessfunc:
				vm_eval_sessfunc(
					vm, (tcppsessf *)value, params, nparams, env);
				break;
			case compo_trule_builtin:
				if (((trule_builtin *)value)->kind !=
				    trule_builtin_assert || nparams != 1)
					twarn(ErrRuntime_ParamsCtr, "assert",
						"one argument required");
				rule_check(vm, &params[0], &vm->rev, 1, env);
				break;
			default:
				twarn(ErrRuntime_RefType, "OP_EVALDF",
					"Function required");
			}
			vm_finish_eval(vm, nparams);
			tobj_ddc_ref_clear(&retained);
			VM_NEXT();
		}
		VM_CASE(OP_EVAL):
			if (vm_eval_generic_instruction(
					vm, iter, tvm_instruction_cache(&code_cache, i),
					&call, env))
				goto make_call;
			VM_NEXT();
		VM_CASE(OP_IDXL): {
			uint_objs loc = (uint_objs)tbycode_get_L(*iter);
			uint_regs np = (uint_regs)tbycode_get_b(*iter);
			int isenv = tbycode_get_i(*iter);
			/* Hot prefix: single-int writes on List and Dictionary
			 * resolve inline, without the out-of-line handler. */
			tobj *objp = nullptr;
			if (isenv) {
				if (loc < env->base.objs.len)
					objp = &env->base.objs.data[loc];
			} else if (loc < vm->tmps.len)
				objp = &vm->tmps.data[loc];
			if (objp && np == 1 && objp->type == tcompo &&
			    objp->val.v_tcompo && stk_len(vm) >= 2) {
				tcompo_v *arr = objp->val.v_tcompo;
				tobj *key = stk_top(vm);
				if (key->type == tint &&
				    arr->vtable == &tdict_vtable) {
					vm_dict_set_int(((tdict *)arr)->items,
							key, stk_at(vm, 1));
					stk_popcn(vm, 2);
					i = vm_fused_next(cmdarr, end, i + 1);
					VM_NEXT();
				}
				if (key->type == tint &&
				    arr->vtable == &tlist_vtable) {
					tlist *list = (tlist *)arr;
					long index = key->val.v_tint;
					uint_count len = list->items.len;
					if (index < 0)
						index += (long)len;
					if (index < 0 || (uint_objs)index >= len)
						twarn(ErrRuntime_IdxOutRange,
						      "list assignment", "");
					const tobj *value = stk_at(vm, 1);
					tobj *slotp = &list->items.data[index];
					if (slotp->type != tcompo &&
					    value->type != tcompo)
						*slotp = *value;
					else
						tobj_vec_set(&list->items,
							     (uint_objs)index,
							     value);
					stk_popcn(vm, 2);
					i = vm_fused_next(cmdarr, end, i + 1);
					VM_NEXT();
				}
				/* 结构成员写 `obj['field'] = ...`：内弦键配合
				 * 每点槽位缓存，指针直中时内联写回。 */
				if (key->type == tcompo &&
				    arr->vtable == &tdict_vtable) {
					tins_cache *wcache =
						tvm_instruction_cache(&code_cache, i);
					thashtbl *items = ((tdict *)arr)->items;
					if (wcache &&
					    wcache->kind == tins_cache_idxr_dict_str &&
					    wcache->key == key->val.v_tcompo &&
					    wcache->slot < items->capacity) {
						thash_entry *entry =
							&items->entries[wcache->slot];
						if (entry->hash >= 2 &&
						    entry->key.val.v_tcompo ==
							    key->val.v_tcompo) {
							const tobj *rv = stk_at(vm, 1);
							if (!vm_same_stored_value(
								    &entry->value, rv)) {
								vm_retain(rv);
								tobj_ddc_ref_clear(
									&entry->value);
								entry->value = *rv;
							}
							stk_popcn(vm, 2);
							i = vm_fused_next(cmdarr, end,
									  i + 1);
							VM_NEXT();
						}
					}
				}
			}
			vm_idxl(vm,
				loc,
				np,
				isenv,
				env,
				tvm_instruction_cache(&code_cache, i));
			VM_NEXT();
		}
		VM_CASE(OP_PUSHF): {
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
				nobjs, env, fregmax, ntmps, nparams, i + 1, ncmds);
			i += ncmds;
			iter += ncmds;
			tobj v;
			tobj_set_nil(&v);
			tobj_set_compo(&v, (tcompo_v *)f);
			stk_push(vm, &v);
			tobj_set_nil(&v);
		}
		VM_NEXT();
		VM_CASE(OP_PUSHRULE): {
			tobj *captures = stk_top(vm);
			tobj *metadata = stk_at(vm, 1);
			tobj *names = stk_at(vm, 2);
			tobj *signature = stk_at(vm, 3);
			tobj *checker = stk_at(vm, 4);
			if (checker->type != tcompo ||
			    tobj_compo_type(checker) != compo_tfunc)
				twarn(ErrRuntime_ParamsType, "OP_PUSHRULE",
				      "checker Function required");
			if (signature->type != tcompo ||
			    tobj_compo_type(signature) != compo_tstr)
				twarn(ErrRuntime_ParamsType, "OP_PUSHRULE",
				      "signature String required");
			if (names->type != tcompo || tobj_compo_type(names) != compo_tstr)
				twarn(ErrRuntime_ParamsType, "OP_PUSHRULE",
				      "parameter names String required");
			if (metadata->type != tcompo ||
			    tobj_compo_type(metadata) != compo_tstr)
				twarn(ErrRuntime_ParamsType, "OP_PUSHRULE",
				      "item metadata String required");
			if (captures->type != tcompo ||
			    tobj_compo_type(captures) != compo_tstr)
				twarn(ErrRuntime_ParamsType, "OP_PUSHRULE",
				      "capture metadata String required");
			trule *rule = trule_new((tfunc *)checker->val.v_tcompo,
				tstring_cstr(cstrs[tbycode_get_U(*iter)]),
				tstring_cstr(((tstr *)signature->val.v_tcompo)->data),
				tstring_cstr(((tstr *)names->val.v_tcompo)->data),
				tstring_cstr(((tstr *)metadata->val.v_tcompo)->data),
				tstring_cstr(((tstr *)captures->val.v_tcompo)->data));
			stk_popcn(vm, 5);
			tobj value;
			tobj_set_nil(&value);
			tobj_set_compo(&value, (tcompo_v *)rule);
			stk_push(vm, &value);
			tobj_set_nil(&value);
		}
		VM_NEXT();
		VM_CASE(OP_RULEVALUE):
		VM_CASE(OP_RULENOT):
		VM_CASE(OP_RULETRUTH): {
			if (!vm->rule_output || !vm->rule_logic_values)
				twarn(ErrRuntime_Other, "Rule logic", "logical expression outside checker");
			/* Nested checks may grow the VM stack: retain the value, not its address. */
			tobj operand;
			tobj_set_nil(&operand);
			tobj_copy(&operand, stk_top(vm));
			int truth = tbycode_ins(*iter) == OP_RULEVALUE ? 0 : rule_antecedent_truth(vm, &operand, env);
			tobj value, pair, key, entry;
			tobj_set_nil(&value); tobj_set_nil(&key);
			if (tbycode_ins(*iter) == OP_RULEVALUE) tobj_copy(&value, &operand);
			else tobj_set_bool(&value, tbycode_ins(*iter) == OP_RULENOT ? !truth : truth);
			tobj_set_nil(&pair); tobj_set_nil(&entry);
			tobj_set_int(&key, tbycode_get_U(*iter));
			tobj_set_compo(&pair, (tcompo_v *)tpair_new(&operand, &value));
			tobj_set_compo(&entry, (tcompo_v *)tpair_new(&key, &pair));
			tobj_vec_push(&vm->rule_logic_values->items, &entry);
			tobj_try_clear(&entry); tobj_try_clear(&pair); tobj_ddc_ref_clear(&operand);
			stk_popc(vm);
			stk_push(vm, &value);
			tobj_ddc_ref_clear(&value);
		}
		VM_NEXT();
		VM_CASE(OP_RULEITEM):
		VM_CASE(OP_RULEIMPLY): {
			if (!vm->rule_output)
				twarn(ErrRuntime_Other, "Rule condition",
				      "condition outside checker");
			tobj *condition = stk_top(vm);
			if (tbycode_ins(*iter) == OP_RULEITEM && condition->type == tcompo &&
			    tobj_compo_type(condition) == compo_trule_instance) {
				tobj_vec_push(&vm->rule_output->items, condition);
				stk_popc(vm);
				VM_NEXT();
			}
			if (tbycode_get_U(*iter) == TRULE_ANTECEDENT_RECORD) {
				/* Keep the original Bool/RuleInstance available through source IR.
				 * Isolate its violations from the enclosing implication. */
				tobj empty, record;
				tobj_set_nil(&empty);
				tobj_set_nil(&record);
				tobj_set_compo(&empty, (tcompo_v *)tstr_new(""));
				tobj_set_compo(&record, (tcompo_v *)tpair_new(&empty, condition));
				tobj_vec_push(&vm->rule_output->items, &record);
				int truth = rule_antecedent_truth(vm, condition, env);
				tobj_try_clear(&record);
				tobj_try_clear(&empty);
				stk_popc(vm);
				tobj value;
				tobj_set_nil(&value);
				tobj_set_bool(&value, truth);
				stk_push(vm, &value);
				VM_NEXT();
			}
			if (condition->type != tbool)
				twarn(ErrRuntime_ParamsType, "Rule condition",
				      "Bool result required");
			tobj description;
			tobj_set_nil(&description);
			tobj_set_compo(&description, (tcompo_v *)tstr_new(
				tstring_cstr(cstrs[tbycode_get_U(*iter)])));
			tobj pair;
			tobj_set_nil(&pair);
			tobj_set_compo(&pair,
				(tcompo_v *)tpair_new(&description, condition));
			tobj_vec_push(&vm->rule_output->items, &pair);
			tobj_try_clear(&pair);
			tobj_try_clear(&description);
			stk_popc(vm);
		}
		VM_NEXT();
		VM_CASE(OP_ADD): {
			VM_PARSE_BINOP(vm, operator_add, *iter, env);
			vm_consume_popcov(vm, env, cmdarr, &i, end);
		}
		VM_NEXT();
		VM_CASE(OP_SUB): {
			VM_PARSE_BINOP(vm, operator_sub, *iter, env);
			vm_consume_popcov(vm, env, cmdarr, &i, end);
		}
		VM_NEXT();
		VM_CASE(OP_MUL): {
			VM_PARSE_BINOP(vm, operator_mul, *iter, env);
			vm_consume_popcov(vm, env, cmdarr, &i, end);
		}
		VM_NEXT();
		VM_CASE(OP_DIV): {
			VM_PARSE_BINOP(vm, operator_div, *iter, env);
			vm_consume_popcov(vm, env, cmdarr, &i, end);
		}
		VM_NEXT();
		VM_CASE(OP_MOD): {
			VM_PARSE_BINOP(vm, operator_mod, *iter, env);
			vm_consume_popcov(vm, env, cmdarr, &i, end);
		}
		VM_NEXT();
		VM_CASE(OP_POW): {
			VM_PARSE_BINOP(vm, operator_pow, *iter, env);
		}
		VM_NEXT();
		VM_CASE(OP_MMUL): {
			VM_PARSE_BINOP(vm, operator_mmul, *iter, env);
		}
		VM_NEXT();
		VM_CASE(OP_EQ): {
			VM_PARSE_BINOP(vm, operator_eq, *iter, env);
			vm_consume_cjpop(vm, cmdarr, &i, end);
			vm_consume_popcov(vm, env, cmdarr, &i, end);
		}
		VM_NEXT();
		VM_CASE(OP_NE): {
			VM_PARSE_BINOP(vm, operator_ne, *iter, env);
			vm_consume_cjpop(vm, cmdarr, &i, end);
			vm_consume_popcov(vm, env, cmdarr, &i, end);
		}
		VM_NEXT();
		VM_CASE(OP_GE): {
			VM_PARSE_BINOP(vm, operator_ge, *iter, env);
			vm_consume_cjpop(vm, cmdarr, &i, end);
			vm_consume_popcov(vm, env, cmdarr, &i, end);
		}
		VM_NEXT();
		VM_CASE(OP_SG): {
			VM_PARSE_BINOP(vm, operator_sg, *iter, env);
			vm_consume_cjpop(vm, cmdarr, &i, end);
			vm_consume_popcov(vm, env, cmdarr, &i, end);
		}
		VM_NEXT();
		VM_CASE(OP_LE): {
			VM_PARSE_BINOP(vm, operator_le, *iter, env);
			vm_consume_cjpop(vm, cmdarr, &i, end);
			vm_consume_popcov(vm, env, cmdarr, &i, end);
		}
		VM_NEXT();
		VM_CASE(OP_SL): {
			VM_PARSE_BINOP(vm, operator_sl, *iter, env);
			vm_consume_cjpop(vm, cmdarr, &i, end);
			vm_consume_popcov(vm, env, cmdarr, &i, end);
		}
		VM_NEXT();
		VM_CASE(OP_AND): {
			VM_PARSE_BINOP(vm, operator_and, *iter, env);
		}
		VM_NEXT();
		VM_CASE(OP_OR): {
			VM_PARSE_BINOP(vm, operator_or, *iter, env);
		}
		VM_NEXT();
		VM_CASE(OP_BAND): {
			VM_PARSE_BINOP(vm, operator_and, *iter, env);
		}
		VM_NEXT();
		VM_CASE(OP_BOR): {
			VM_PARSE_BINOP(vm, operator_or, *iter, env);
		}
		VM_NEXT();
		VM_CASE(OP_POS):
			operator_pos(stk_top(vm));
			VM_NEXT();
		VM_CASE(OP_NEG):
			operator_neg(stk_top(vm));
			VM_NEXT();
		VM_CASE(OP_NOT):
			if (stk_top(vm)->type != tbool)
				twarn(ErrRuntime_ParamsType, "OP_NOT", "");
			stk_top(vm)->val.v_tbool = !stk_top(vm)->val.v_tbool;
			VM_NEXT();
		VM_DEFAULT:
			twarn(ErrRuntime_Other, "exec_tins",
			      "invalid bytecode instruction");
		VM_SWITCH_END
#ifdef TAPAS_VM_THREADED
		vm_advance:
#endif
		i++;
		goto dispatch;

		do_return:
		if (vm->frame_len == base_frame_depth)
			goto loop_done;
		goto resume_caller;

		make_call:
		tfunc *function = call.function;
		/* this() 自调用的目标就是正在执行的函数，其 wrapper 必为当前
		 * wrapper；跳过库指针解引用与沿环境链的 wrapper 查找。 */
		twrapper *callee_wrapper = call.tail_self_call ? wrapper :
			(function->library ?
				function->library->wrapper :
				tfunc_get_wrapper_from_env(&function->env));
		int tail_recursion = call.tail_self_call &&
			i + 1 < end &&
			tbycode_ins(cmdarr[i + 1]) == OP_RET &&
			vm->frame_len > base_frame_depth;
		if (tail_recursion) {
			tvm_replace_current_call_frame(
				vm, function, call.params, call.nparams);
			env = &vm->frames[vm->frame_len - 1]->env;
		} else {
			tcall_frame *frame = tvm_push_call_frame_fast(
				vm, function, call.params, call.nparams);
			if (!frame)
				frame = tvm_push_call_frame(
					vm, function, call.params, call.nparams, 1);
			frame->return_pc = i;
			frame->return_end = end;
			frame->return_stack_values =
				call.return_stack_values;
			frame->return_has_callable = call.stack_has_callable;
			if (call.retain_callable) {
				tobj borrowed;
				tobj_set_nil(&borrowed);
				tobj_set_compo(&borrowed, call.retain_callable);
				tobj_copy(&frame->retained_callable, &borrowed);
				tobj_set_nil(&borrowed);
			}
			frame->return_env = env;
			frame->return_wrapper = wrapper;
			env = &frame->env;
		}

		/* Same-wrapper calls (all recursion and intra-module calls)
		 * keep the loaded code context; only crossings reload it. */
		if (callee_wrapper != wrapper) {
			code_cache = *tvm_get_code_cache(vm, callee_wrapper);
			wrapper = callee_wrapper;
			cmdarr = wrapper->cmdarr;
			cints = wrapper->consts.cints;
			cflts = wrapper->consts.cflts;
			cstrs = wrapper->consts.cstrs;
			csobjs = wrapper->consts.csobjs;
			vm->error_source_locs = wrapper->source_locs;
			vm->error_source_loc_count = wrapper->ncmds;
		}
		i = function->cmdloc;
		end = function->cmdloc + function->ncmds;
		goto dispatch;

resume_caller:
		tcall_frame *frame = vm->frames[vm->frame_len - 1];
		uint_cmds return_pc = frame->return_pc;
		uint_cmds return_end = frame->return_end;
		uint_regs return_stack_values = frame->return_stack_values;
		int return_has_callable = frame->return_has_callable;
		tcompo_env *return_env = frame->return_env;
		twrapper *return_wrapper = frame->return_wrapper;

		tvm_pop_call_frame(vm);
		stk_finish_tapas_call(
			vm, return_stack_values, return_has_callable);

		env = return_env;
		if (return_wrapper != wrapper) {
			code_cache = *tvm_get_code_cache(vm, return_wrapper);
			wrapper = return_wrapper;
			cmdarr = wrapper->cmdarr;
			cints = wrapper->consts.cints;
			cflts = wrapper->consts.cflts;
			cstrs = wrapper->consts.cstrs;
			csobjs = wrapper->consts.csobjs;
			vm->error_source_locs = wrapper->source_locs;
			vm->error_source_loc_count = wrapper->ncmds;
		}
		i = return_pc + 1;
		end = return_end;
		goto dispatch;
	}
	loop_done:
	vm->execution_depth--;
	vm->error_source_locs = previous_source_locs;
	vm->error_source_loc_count = previous_source_loc_count;
	vm->error_instruction = previous_instruction;
	if (outermost)
		terror_restore_runtime_context_resolver(previous_error_context);
}

#ifdef TAPAS_VM_THREADED
#pragma GCC diagnostic pop
#endif

void eval_bycodes(tvm *vm, uint_cmds from, tlib *lib)
{
	twrapper *wrapper = tlib_get_wrapper(lib);
	if (!wrapper)
		return;
	if (from > wrapper->ncmds)
		twarn(ErrRuntime_Other, "eval_bycodes", "invalid bytecode offset");
	const tvm_services *previous_services = lib->env.vm_services;
	void *previous_context = lib->env.vm_services_context;
	tcompo_env_set_vm_services(&lib->env, &core_vm_services, vm);
	exec_tins(vm, from, wrapper->ncmds - from, &lib->env);
	tcompo_env_set_vm_services(&lib->env, previous_services, previous_context);
}
