#include "tapas/runtime/tsolve.h"
#include "tapas/tvm.h"

#include "tapas/runtime/tarray.h"
#include "tapas/runtime/tdict.h"
#include "tapas/runtime/titer.h"
#include "tapas/runtime/tlist.h"
#include "tapas/runtime/tcfn.h"
#include "tapas/runtime/tpair.h"
#include "tapas/runtime/tstr.h"
#include "tapas/runtime/ttype.h"
#include "tapas/runtime/trule.h"
#include "tapas/runtime/tevaluator.h"

#include <stdlib.h>
#include <string.h>


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
    int left_term = v1->type == tcompo && tobj_compo_type(v1) == compo_trule_term;
    int right_term = v2->type == tcompo && tobj_compo_type(v2) == compo_trule_term;
    if (left_term || right_term) {
        trule_term *left = left_term ? (trule_term *)v1->val.v_tcompo : trule_term_constant_new(v1);
        trule_term *right = right_term ? (trule_term *)v2->val.v_tcompo : trule_term_constant_new(v2);
        tobj_set_compo(vre, (tcompo_v *)trule_term_in_new(left, right));
        return;
    }
	tobj_set_bool(vre, v2->type == tcompo && v2->val.v_tcompo &&
			    tcompo_contains(v2->val.v_tcompo, v1));
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
	if (value->type == tcompo && value->val.v_tcompo &&
	    value->val.v_tcompo->vtable->op_neg) {
		tcompo_v *self = value->val.v_tcompo;
		self->vtable->op_neg(self, value);
		return;
	}
	twarn(ErrRuntime_ParamsType, "operator_neg", "unsupported operand");
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

static void tvm_drop_call_frames(tvm *vm);

typedef enum {
	tins_cache_empty,
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
	tfunction_metadata *function_metadata;
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
		if (instruction == OP_LOOPAS)
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
	vm->solve_worker = nullptr;
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
	tsolve_worker_free(vm->solve_worker);
	vm->solve_worker = nullptr;
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
		for (uint_cmds j = 0; j < vm->code_caches[i].entry_count; j++)
			tfunction_metadata_release(vm->code_caches[i].entries[j].function_metadata);
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
		vm->loop_states[vm->loop_state_len].iterator_slot = nullptr;
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
		vm->loop_states[i].iterator_slot = nullptr;
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
	fr->env.params = nullptr;
	fr->env.dynamic_nparams = 0;
	fr->env.owner_func = nullptr;
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

static inline void tcall_frame_assign_params(
		tcall_frame *frame, tobj *params, uint_regs nparams)
{
	for (uint_regs i = 0; i < nparams; i++) {
		ttypeval *type = tfunction_metadata_type(frame->func->metadata, i);
		if (type && (type->contains_instance || type->contains_domain) && !ttypeval_matches(&params[i], type))
			twarn(ErrRuntime_ParamsType, "Type annotation", "argument does not match declared Type or Rule identity");
	}
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
	ttypeval *return_type = tfunction_metadata_return_type(fr->func->metadata);
	if (return_type && (return_type->contains_instance || return_type->contains_domain) && !ttypeval_matches(&vm->rev, return_type))
		twarn(ErrRuntime_ParamsType, "Type annotation", "return value does not match declared Type or Rule identity");
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
		cache->guard = nullptr;
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
	if (arr->vtable == &tstr_vtable && nparams == 1 &&
	    params[0].type == tint) {
		tins_cache_observe(cache, tins_cache_idxr_str_int, arr->vtable);
		vm_str_idx_int((tstr *)arr, params[0].val.v_tint, &vm->rev);
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

static void rule_result_field(tdict *result, const char *name,
			      const tobj *value)
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

static void vm_invoke(tvm *vm, const tobj *callable, tobj *arguments,
		      uint_regs argument_count, tcompo_env *environment,
		      tobj *result);

static int rule_parameter_index(const trule_ir *ir, const trule_term *term)
{
	for (uint_objs i = 0; i < ir->parameters.len; i++)
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
		instance->arguments.data, (uint_regs)instance->arguments.len);
	exec_tins(vm, checker->cmdloc, checker->ncmds, &frame->env);
	tvm_pop_call_frame(vm);
	tobj_try_clear(&vm->rev);
	vm->rule_output = saved;
	vm->rule_logic_values = saved_negations;
	const char *role = tstring_cstr(term->provider_kind);
	if (strcmp(role, "negation-value") == 0 || strcmp(role, "negation-operand") == 0 || strcmp(role, "expression-value") == 0) {
		const tpair *record = nullptr;
		for (uint_objs i = 0; i < tlist_size(negations); i++) {
			const tpair *entry = (tpair *)tlist_at(negations, i)->val.v_tcompo;
			if (entry->first.val.v_tint == term->provider_version)
				record = (tpair *)entry->second.val.v_tcompo;
		}
		if (!record) twarn(ErrRuntime_Other, "Rule", "logical Term was not evaluated (short-circuited)");
		tobj_copy(result, strcmp(role, "negation-value") == 0 ? &record->second : &record->first);
	} else {
		long item_index = term->provider_version;
		if (item_index < 0 || (uint_objs)item_index >= tlist_size(output))
			twarn(ErrRuntime_Other, "Rule", "invalid source Term index");
		const tobj *item = tlist_at(output, (uint_objs)item_index);
		if (item->type == tcompo && tobj_compo_type(item) == compo_tpair)
			tobj_copy(result, &((tpair *)item->val.v_tcompo)->second);
		else tobj_copy(result, item);
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
			   trule_term *term, tcompo_env *environment,
			   tobj *result)
{
	tobj_set_nil(result);
    if (!strcmp(tstring_cstr(term->provider), "tapas.source") &&
        !strcmp(tstring_cstr(term->provider_kind), "expression-value")) {
        rule_eval_source_item(vm, instance, term, result);
        return;
    }
    if (term->kind == trule_term_in) {
        if (!strcmp(tstring_cstr(term->provider), "tapas.source")) {
            rule_eval_source_item(vm, instance, term, result); return;
        }
        if (term->arguments.len != 2) twarn(ErrRuntime_Other, "Rule", "In requires two operands");
        tobj left, right;
        tobj_set_nil(&left); tobj_set_nil(&right);
        /* Match ordinary Tapas membership evaluation order. */
        rule_eval_term(vm, instance, (trule_term *)term->arguments.data[1].val.v_tcompo, environment, &right);
        rule_eval_term(vm, instance, (trule_term *)term->arguments.data[0].val.v_tcompo, environment, &left);
        operator_in(&left, &right, result);
        tobj_try_clear(&left); tobj_try_clear(&right); return;
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
		rule_eval_term(vm, instance, (trule_term *)term->arguments.data[0].val.v_tcompo,
			environment, &operand);
		int truth = rule_antecedent_truth(vm, &operand, environment);
		tobj_try_clear(&operand);
		if ((term->kind == trule_term_and && truth) || (term->kind == trule_term_or && !truth)) {
			rule_eval_term(vm, instance, (trule_term *)term->arguments.data[1].val.v_tcompo,
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
		trule_term *function = term->payload.type == tcompo &&
			tobj_compo_type(&term->payload) == compo_trule_term ?
			(trule_term *)term->payload.val.v_tcompo : nullptr;
		if (!function) twarn(ErrRuntime_Other, "Rule", "invalid Call Term");
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
		for (uint_regs i = 0; i < count; i++) tobj_try_clear(&arguments[i]);
		free(arguments);
		tobj_try_clear(&callable);
		return;
	}
	if (term->kind == trule_term_construct ||
	    term->kind == trule_term_convert) {
		if (term->kind == trule_term_construct &&
		    strcmp(tstring_cstr(term->provider), "tapas.source") == 0) {
			rule_eval_source_item(vm, instance,
				term, result);
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
	const char *operation = term->payload.type == tcompo &&
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
	tobj_set_compo(&value, (tcompo_v *)tstr_new(
		tstring_cstr(condition->description)));
	rule_result_field(violation, "description", &value);
	tobj_try_clear(&value);
	tlist *arguments = tlist_new();
	if (depth) {
		trule_instance *current = path[depth - 1];
		for (uint_objs i = 0; i < current->arguments.len; i++)
			tobj_vec_push(&arguments->items, &current->arguments.data[i]);
	}
	tobj_set_compo(&value, (tcompo_v *)arguments);
	rule_result_field(violation, "arguments", &value);
	tobj_try_clear(&value);
	tlist *requirements = tlist_new();
	for (uint32_t i = 0; i + 1 < depth; i++) {
		if (!requirement_path[i]) continue;
		tobj requirement;
		tobj_set_nil(&requirement);
		tobj_set_compo(&requirement,
			(tcompo_v *)requirement_path[i]);
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
		tobj_set_compo(&value, (tcompo_v *)tstr_new(
			tstring_cstr(owner->source)));
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
	if (value->type == tbool) return value->val.v_tbool;
	if (value->type != tcompo || tobj_compo_type(value) != compo_trule_instance)
		twarn(ErrRuntime_ParamsType, "Rule truth", "Bool or RuleInstance required");
	/* Bound source and dynamic recursion even though each nested check starts
	 * a new requirement path. */
	if (vm->antecedent_depth >= 128)
		twarn(ErrRuntime_Other, "Rule truth", "antecedent evaluation depth exceeded (cyclic or deeply nested Rule check)");
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
	if (!record || record->type != tcompo || tobj_compo_type(record) != compo_tpair ||
	    ((tpair *)record->val.v_tcompo)->second.type != tbool)
		twarn(ErrRuntime_Other, "Rule", "invalid implication checker record");
	return ((tpair *)record->val.v_tcompo)->second.val.v_tbool;
}

static void rule_check_implication(tvm *vm, trule_instance *instance,
	trule_item *item, tlist *violations, tlist *implications,
	trule_instance **path, trule_item **requirement_path, uint32_t depth,
	tcompo_env *environment, tlist *records, uint_objs *record_index)
{
	tobj guard;
	tobj_set_nil(&guard);
	if (records && (((trule *)instance->rule.val.v_tcompo)->ir->version >= 7 ||
        item->term->kind == trule_term_not ||
	    item->term->kind == trule_term_in || item->term->kind == trule_term_and || item->term->kind == trule_term_or ||
	    strcmp(tstring_cstr(item->term->provider_kind), "antecedent-value") == 0))
		(*record_index)++; /* use the already checked truth, not a second evaluation */
	if (records)
		tobj_set_bool(&guard, rule_bool_record(tlist_at(records, *record_index)));
	else rule_eval_term(vm, instance, item->term, environment, &guard);
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
		if (!triggered) continue;
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
			same = tobj_identical(&path[i]->arguments.data[j],
				&instance->arguments.data[j]);
		if (same)
			twarn(ErrRuntime_Other, "Rule", "cyclic requirement");
	}
	path[depth] = instance;
	trule *rule = (trule *)instance->rule.val.v_tcompo;
	if (rule->evaluate_ir) {
		for (uint_objs i = 0; i < rule->ir->items.len; i++) {
			trule_item *item =
				(trule_item *)rule->ir->items.data[i].val.v_tcompo;
			if (item->kind == trule_item_implication) {
				rule_check_implication(vm, instance, item, violations, implications,
					path, requirement_path, depth + 1, environment, nullptr, nullptr);
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
			if (rule_value.type != tcompo ||
			    tobj_compo_type(&rule_value) != compo_trule)
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
		(uint_regs)instance->arguments.len);
	exec_tins(vm, checker->cmdloc, checker->ncmds, &frame->env);
	tvm_pop_call_frame(vm);
	tobj_try_clear(&vm->rev);
	vm->rule_output = saved;
	vm->rule_logic_values = saved_negations;

	uint_objs metadata_index = 0;
	for (uint_objs i = 0; i < tlist_size(output); i++) {
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
			twarn(ErrRuntime_ParamsType, "Rule dependency",
			      "RuleInstance required");
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
		twarn(ErrRuntime_ParamsType, fatal ? "assert" : "rules::check",
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

static void rule_inspect(const tobj *value, tobj *result)
{
	trule *rule = nullptr;
	if (value && value->type == tcompo && value->val.v_tcompo) {
		if (tobj_compo_type(value) == compo_trule)
			rule = (trule *)value->val.v_tcompo;
		else if (tobj_compo_type(value) == compo_trule_instance)
			rule = (trule *)((trule_instance *)value->val.v_tcompo)
				->rule.val.v_tcompo;
	}
	if (!rule)
		twarn(ErrRuntime_ParamsType, "rules::inspect",
		      "Rule or RuleInstance required");
	tobj ir_value;
	tobj_set_nil(&ir_value);
	tobj_set_compo(&ir_value, (tcompo_v *)rule->ir);
	tobj_copy(result, &ir_value);
}

static trule_ir *rule_ir_argument(const tobj *value, const char *name)
{
	if (value && value->type == tcompo && value->val.v_tcompo) {
		if (tobj_compo_type(value) == compo_trule_ir)
			return (trule_ir *)value->val.v_tcompo;
		if (tobj_compo_type(value) == compo_trule)
			return ((trule *)value->val.v_tcompo)->ir;
		if (tobj_compo_type(value) == compo_trule_instance)
			return ((trule *)((trule_instance *)value->val.v_tcompo)
				->rule.val.v_tcompo)->ir;
	}
	twarn(ErrRuntime_ParamsType, name, "RuleIR, Rule or RuleInstance required");
	return nullptr;
}

static void rule_ir_list(const tobj *value, tobj *result,
			 trule_builtin_kind kind)
{
	trule_ir *ir = rule_ir_argument(value, "rules IR reader");
	const tobj_vec *source = kind == trule_builtin_parameters ?
		&ir->parameters : kind == trule_builtin_items ?
		&ir->items : &ir->terms;
	tlist *list = tlist_new();
	for (uint_objs i = 0; i < source->len; i++)
		tobj_vec_push(&list->items, &source->data[i]);
	tobj_set_compo(result, (tcompo_v *)list);
}

static void rule_origin(const tobj *value, tobj *result)
{
	long start = -1;
	long end = -1;
	if (value && value->type == tcompo && value->val.v_tcompo) {
		if (tobj_compo_type(value) == compo_trule_term) {
			start = ((trule_term *)value->val.v_tcompo)->origin_start;
			end = ((trule_term *)value->val.v_tcompo)->origin_end;
		} else if (tobj_compo_type(value) == compo_trule_item) {
			start = ((trule_item *)value->val.v_tcompo)->origin_start;
			end = ((trule_item *)value->val.v_tcompo)->origin_end;
		} else if (tobj_compo_type(value) != compo_trule_ir)
			twarn(ErrRuntime_ParamsType, "rules::origin", "IR value required");
	} else twarn(ErrRuntime_ParamsType, "rules::origin", "IR value required");
	tdict *origin = tdict_new();
	tobj field;
	tobj_set_nil(&field);
	tobj_set_int(&field, start);
	rule_result_field(origin, "start", &field);
	tobj_set_int(&field, end);
	rule_result_field(origin, "end", &field);
	tobj_set_nil(&field);
	rule_result_field(origin, "source", &field);
	tobj_set_compo(&field, (tcompo_v *)tlist_new());
	rule_result_field(origin, "transforms", &field);
	tobj_try_clear(&field);
	tobj_set_compo(result, (tcompo_v *)origin);
}

static void rule_hash(const tobj *value, tobj *result, int content)
{
	trule_ir *ir = rule_ir_argument(value, content ?
		"rules::content_hash" : "rules::semantic_hash");
	uint64_t hash = content ? trule_ir_content_hash(ir) :
		trule_ir_semantic_hash(ir);
	tobj_set_int(result, (long)(hash & 0x7fffffffffffffffULL));
}

typedef struct rule_serial_entry {
	tstring *key;
	tobj rule;
	struct rule_serial_entry *next;
} rule_serial_entry;

static rule_serial_entry *rule_serialized;

static void rule_serialize(const tobj *value, tobj *result)
{
	trule *rule = nullptr;
	trule_ir *standalone = nullptr;
	if (value && value->type == tcompo && value->val.v_tcompo &&
	    tobj_compo_type(value) == compo_trule)
		rule = (trule *)value->val.v_tcompo;
	else if (value && value->type == tcompo && value->val.v_tcompo &&
		 tobj_compo_type(value) == compo_trule_ir)
		standalone = (trule_ir *)value->val.v_tcompo;
	if (!rule && !standalone)
		twarn(ErrRuntime_ParamsType, "rules::serialize",
		      "Rule or RuleIR required");
	if (standalone || rule->checker.type == tnil) {
		trule_ir *ir = standalone ? standalone : rule->ir;
		for (uint_objs i = 0; i < ir->terms.len; i++) {
			trule_term *term = (trule_term *)ir->terms.data[i].val.v_tcompo;
			if (strcmp(tstring_cstr(term->provider), "tapas.source") == 0)
				twarn(ErrRuntime_Other, "rules::serialize",
				      "source RuleIR requires its Rule closure");
		}
		tstring *serialized = nullptr;
		if (!trule_ir_serialize(ir, &serialized))
			twarn(ErrRuntime_Other, "rules::serialize",
			      "Rule contains a value that cannot be serialized");
		tobj_set_compo(result,
			(tcompo_v *)tstr_new(tstring_cstr(serialized)));
		tstring_free(serialized);
		return;
	}
	tstring *key = tstring_new("TPRULE1:");
	tstring_append_fmt(key, "%llu:%llu:%p",
		(unsigned long long)trule_ir_semantic_hash(rule->ir),
		(unsigned long long)trule_ir_content_hash(rule->ir), (void *)rule);
	for (rule_serial_entry *entry = rule_serialized; entry; entry = entry->next)
		if (tstring_cmp(entry->key, key) == 0) {
			tstring_free(key);
			tobj_set_compo(result, (tcompo_v *)tstr_new(
				tstring_cstr(entry->key)));
			return;
		}
	rule_serial_entry *entry = calloc(1, sizeof(*entry));
	entry->key = key;
	tobj_set_nil(&entry->rule);
	tobj_copy(&entry->rule, value);
	entry->next = rule_serialized;
	rule_serialized = entry;
	tobj_set_compo(result, (tcompo_v *)tstr_new(tstring_cstr(key)));
}

static void rule_deserialize(const tobj *value, tobj *result)
{
	if (!value || value->type != tcompo ||
	    tobj_compo_type(value) != compo_tstr)
		twarn(ErrRuntime_ParamsType, "rules::deserialize", "String required");
	const tstring *key = ((tstr *)value->val.v_tcompo)->data;
	if (strncmp(tstring_cstr(key), "TPIR1;", 6) == 0 ||
	    strncmp(tstring_cstr(key), "TPIR2;", 6) == 0 ||
	    strncmp(tstring_cstr(key), "TPIR3;", 6) == 0 ||
	    strncmp(tstring_cstr(key), "TPIR4;", 6) == 0 ||
	    strncmp(tstring_cstr(key), "TPIR5;", 6) == 0 ||
	    strncmp(tstring_cstr(key), "TPIR6;", 6) == 0) {
		trule_ir *ir = trule_ir_deserialize(tstring_cstr(key));
		if (!ir)
			twarn(ErrRuntime_Other, "rules::deserialize",
			      "invalid or unsupported RuleIR serialization");
		tstring *signature = tstring_new_empty();
		for (uint_objs i = 0; i < ir->parameters.len; i++) {
			trule_term *parameter =
				(trule_term *)ir->parameters.data[i].val.v_tcompo;
			if (i) tstring_append_c(signature, '\x1f');
			tstring_append_ts(signature, parameter->type->canonical);
		}
		tobj_set_compo(result, (tcompo_v *)trule_new_dynamic(
			ir, tstring_cstr(signature)));
		tstring_free(signature);
		if (ir->base.refctr > 0) ir->base.refctr--;
		return;
	}
	for (rule_serial_entry *entry = rule_serialized; entry; entry = entry->next)
		if (tstring_cmp(entry->key, key) == 0) {
			tobj_copy(result, &entry->rule);
			return;
		}
	twarn(ErrRuntime_Other, "rules::deserialize",
	      "serialized Rule closure is unavailable in this process");
}

static void vm_invoke(tvm *vm, const tobj *callable, tobj *arguments,
		      uint_regs argument_count, tcompo_env *environment,
		      tobj *result)
{
	if (!callable || callable->type != tcompo || !callable->val.v_tcompo)
		twarn(ErrRuntime_RefType, "Evaluator", "Function required");
	switch (tobj_compo_type(callable)) {
	case compo_trule:
		tobj_set_compo(result, (tcompo_v *)trule_bind(
			(trule *)callable->val.v_tcompo, arguments, argument_count));
		break;
	case compo_tfunc: {
		tfunc *function = (tfunc *)callable->val.v_tcompo;
		tcall_frame *frame = tvm_push_call_frame(
			vm, function, arguments, argument_count);
		exec_tins(vm, function->cmdloc, function->ncmds, &frame->env);
		tvm_pop_call_frame(vm);
		tobj returned = vm->rev;
		tobj_set_nil(&vm->rev);
		*result = returned;
	} break;
	case compo_cppfunc: {
		tcppgenf *function = (tcppgenf *)callable->val.v_tcompo;
		if (!tcppgenf_accepts(function, argument_count))
			twarn(ErrRuntime_ParamsCtr, "Evaluator callback",
			      "incorrect parameter count");
		function->f(arguments, argument_count, result);
	} break;
	case compo_sessfunc: {
		tcppsessf *function = (tcppsessf *)callable->val.v_tcompo;
		if (!tcppsessf_accepts(function, argument_count))
			twarn(ErrRuntime_ParamsCtr, "Evaluator callback",
			      "incorrect parameter count");
		function->f(arguments, argument_count, result, environment);
	} break;
	default:
		twarn(ErrRuntime_RefType, "Evaluator", "Function required");
	}
}

static void evaluator_context(trule_instance *instance, tobj *result)
{
	tdict *context = tdict_new();
	rule_result_field(context, "instance",
		&(tobj){ .type = tcompo, .val.v_tcompo = (tcompo_v *)instance });
	rule_result_field(context, "rule", &instance->rule);
	tobj ir;
	tobj_set_nil(&ir);
	rule_inspect(&instance->rule, &ir);
	rule_result_field(context, "ir", &ir);
	tobj_try_clear(&ir);
	tlist *bindings = tlist_new();
	for (uint_objs i = 0; i < instance->arguments.len; i++)
		tobj_vec_push(&bindings->items, &instance->arguments.data[i]);
	tobj value;
	tobj_set_nil(&value);
	tobj_set_compo(&value, (tcompo_v *)bindings);
	rule_result_field(context, "binding", &value);
	tobj_try_clear(&value);
	tobj_set_nil(&value);
	rule_result_field(context, "capture", &value);
	rule_result_field(context, "value", &value);
	rule_result_field(context, "requirement", &value);
	tobj_set_compo(result, (tcompo_v *)context);
}

static void evaluator_result(tobj *result, const char *status,
			     const tobj *value, const char *diagnostic)
{
	tdict *record = tdict_new();
	tobj field;
	tobj_set_nil(&field);
	tobj_set_compo(&field, (tcompo_v *)tstr_new(status));
	rule_result_field(record, "status", &field);
	tobj_try_clear(&field);
	if (value) tobj_copy(&field, value);
	rule_result_field(record, "value", &field);
	tobj_try_clear(&field);
	tobj_set_compo(&field, (tcompo_v *)tlist_new());
	rule_result_field(record, "violations", &field);
	tobj_try_clear(&field);
	tlist *diagnostics = tlist_new();
	if (diagnostic && *diagnostic) {
		tobj message;
		tobj_set_nil(&message);
		tobj_set_compo(&message, (tcompo_v *)tstr_new(diagnostic));
		tobj_vec_push(&diagnostics->items, &message);
		tobj_try_clear(&message);
	}
	tobj_set_compo(&field, (tcompo_v *)diagnostics);
	rule_result_field(record, "diagnostics", &field);
	tobj_try_clear(&field);
	tobj_set_compo(result, (tcompo_v *)record);
}

static void context_field(const tobj *context, const char *name, tobj *result)
{
	if (!context || context->type != tcompo ||
	    tobj_compo_type(context) != compo_tdict)
		twarn(ErrRuntime_ParamsType, "evaluators::Context",
		      "Context required");
	tobj key;
	tobj_set_nil(&key);
	tobj_set_compo(&key, (tcompo_v *)tstr_new(name));
	tdict_get((tdict *)context->val.v_tcompo, &key, result);
	tobj_try_clear(&key);
}

static trule_instance *context_instance(const tobj *context)
{
	tobj instance;
	tobj_set_nil(&instance);
	context_field(context, "instance", &instance);
	if (instance.type != tcompo ||
	    tobj_compo_type(&instance) != compo_trule_instance)
		twarn(ErrRuntime_ParamsType, "evaluators::Context",
		      "invalid Context instance");
	return (trule_instance *)instance.val.v_tcompo;
}

static void evaluator_context_access(tvm *vm, trule_builtin_kind kind,
				     tobj *params, uint_regs count,
				     tcompo_env *environment, tobj *result)
{
	if (count != 2)
		twarn(ErrRuntime_ParamsCtr, "evaluators::Context",
		      "two arguments required");
	trule_instance *instance = context_instance(&params[0]);
	trule *rule = (trule *)instance->rule.val.v_tcompo;
	if (kind == trule_builtin_context_capture) {
		trule_term *capture = nullptr;
		if (params[1].type == tint && params[1].val.v_tint >= 0 &&
		    (uint_objs)params[1].val.v_tint < rule->ir->captures.len)
			capture = (trule_term *)rule->ir->captures
				.data[params[1].val.v_tint].val.v_tcompo;
		else if (params[1].type == tcompo &&
			 tobj_compo_type(&params[1]) == compo_trule_term &&
			 ((trule_term *)params[1].val.v_tcompo)->kind ==
			 trule_term_capture)
			capture = (trule_term *)params[1].val.v_tcompo;
        if (!trule_read_capture(rule, capture, result))
            twarn(ErrRuntime_Other, "evaluators::capture", "unknown or unavailable CaptureId");
		return;
	}
	if (kind == trule_builtin_context_binding) {
		int index = -1;
		if (params[1].type == tint) index = (int)params[1].val.v_tint;
		else if (params[1].type == tcompo &&
			 tobj_compo_type(&params[1]) == compo_trule_term)
			index = rule_parameter_index(rule->ir,
				(trule_term *)params[1].val.v_tcompo);
		if (index < 0 || (uint_objs)index >= instance->arguments.len)
			twarn(ErrRuntime_Other, "evaluators::binding",
			      "unknown ParameterId");
		tobj_copy(result, &instance->arguments.data[index]);
		return;
	}
	if (kind == trule_builtin_context_value) {
		if (params[1].type != tcompo ||
		    tobj_compo_type(&params[1]) != compo_trule_term)
			twarn(ErrRuntime_ParamsType, "evaluators::value",
			      "Term required");
		rule_eval_term(vm, instance,
			(trule_term *)params[1].val.v_tcompo, environment, result);
		return;
	}
	if (params[1].type != tcompo ||
	    tobj_compo_type(&params[1]) != compo_trule_item ||
	    ((trule_item *)params[1].val.v_tcompo)->kind !=
		trule_item_requirement)
		twarn(ErrRuntime_ParamsType, "evaluators::requirement",
		      "Requirement required");
	trule_item *item = (trule_item *)params[1].val.v_tcompo;
	tobj target;
	tobj_set_nil(&target);
	rule_eval_term(vm, instance, item->rule, environment, &target);
	if (target.type == tcompo &&
	    tobj_compo_type(&target) == compo_trule_instance &&
	    item->arguments.len == 0) {
		tobj_copy(result, &target);
		tobj_try_clear(&target);
		return;
	}
	if (target.type != tcompo || tobj_compo_type(&target) != compo_trule)
		twarn(ErrRuntime_ParamsType, "evaluators::requirement",
		      "Requirement Rule Term did not produce Rule");
	uint_regs argument_count = (uint_regs)item->arguments.len;
	tobj *arguments = argument_count ? calloc(argument_count,
		sizeof(*arguments)) : nullptr;
	for (uint_regs i = 0; i < argument_count; i++) {
		tobj_set_nil(&arguments[i]);
		rule_eval_term(vm, instance,
			(trule_term *)item->arguments.data[i].val.v_tcompo,
			environment, &arguments[i]);
	}
	tobj_set_compo(result, (tcompo_v *)trule_bind(
		(trule *)target.val.v_tcompo, arguments, argument_count));
	for (uint_regs i = 0; i < argument_count; i++)
		tobj_try_clear(&arguments[i]);
	free(arguments);
	tobj_try_clear(&target);
}

static void evaluator_eval(tvm *vm, tobj *params, uint_regs count,
			   tcompo_env *environment, tobj *result)
{
	if (count != 2 || params[0].type != tcompo ||
	    tobj_compo_type(&params[0]) != compo_trule_instance ||
	    params[1].type != tcompo ||
	    tobj_compo_type(&params[1]) != compo_tevaluator)
		twarn(ErrRuntime_ParamsType, "evaluators::eval",
		      "RuleInstance and Evaluator required");
	tevaluator *evaluator = (tevaluator *)params[1].val.v_tcompo;
	tobj context;
	tobj_set_nil(&context);
	evaluator_context((trule_instance *)params[0].val.v_tcompo, &context);
	context.val.v_tcompo->refctr++;
	vm_invoke(vm, &evaluator->evaluate, &context, 1, environment, result);
	tobj_try_clear(&context);
	if (result->type != tcompo || tobj_compo_type(result) != compo_tdict)
		twarn(ErrRuntime_ParamsType, "evaluators::eval",
		      "evaluate must return evaluators::Result");
}

typedef struct evaluator_cache_entry {
	uint64_t semantic_hash;
	tstring *name;
	long version;
	tobj result;
	struct evaluator_cache_entry *next;
} evaluator_cache_entry;

static evaluator_cache_entry *evaluator_cache;

static void evaluator_cached_copy(const tobj *cached, tobj *result)
{
	if (cached->type == tcompo && cached->val.v_tcompo)
		tobj_set_compo(result, cached->val.v_tcompo->vtable->copy(
			cached->val.v_tcompo));
	else
		*result = *cached;
}

static void evaluator_compile(tvm *vm, tobj *params, uint_regs count,
			      tcompo_env *environment, tobj *result)
{
	if (count != 2 || params[0].type != tcompo ||
	    tobj_compo_type(&params[0]) != compo_trule ||
	    params[1].type != tcompo ||
	    tobj_compo_type(&params[1]) != compo_tevaluator)
		twarn(ErrRuntime_ParamsType, "evaluators::compile",
		      "Rule and Evaluator required");
	tevaluator *evaluator = (tevaluator *)params[1].val.v_tcompo;
	if (evaluator->compile.type == tnil) {
		evaluator_result(result, "Unsupported", nullptr,
			"Evaluator does not provide compile");
		return;
	}
	trule *rule = (trule *)params[0].val.v_tcompo;
	uint64_t semantic_hash = trule_ir_semantic_hash(rule->ir);
	for (evaluator_cache_entry *entry = evaluator_cache; entry;
	     entry = entry->next)
		if (entry->semantic_hash == semantic_hash &&
		    entry->version == evaluator->version &&
		    tstring_cmp(entry->name, evaluator->name) == 0) {
			evaluator_cached_copy(&entry->result, result);
			return;
		}
	vm_invoke(vm, &evaluator->compile, &params[0], 1, environment, result);
	if (result->type != tcompo || tobj_compo_type(result) != compo_tdict)
		twarn(ErrRuntime_ParamsType, "evaluators::compile",
		      "compile must return evaluators::Result");
	tobj status;
	tobj_set_nil(&status);
	context_field(result, "status", &status);
	if (status.type != tcompo || tobj_compo_type(&status) != compo_tstr)
		twarn(ErrRuntime_ParamsType, "evaluators::compile",
		      "Result.status must be String");
	if (strcmp(tstring_cstr(((tstr *)status.val.v_tcompo)->data),
		   "Success") == 0) {
		tobj compiled;
		tobj_set_nil(&compiled);
		context_field(result, "value", &compiled);
		if (compiled.type != tcompo || !compiled.val.v_tcompo)
			twarn(ErrRuntime_ParamsType, "evaluators::compile",
			      "successful compile must return a Function value");
		tcompo_type code = tobj_compo_type(&compiled);
		if (code != compo_tfunc && code != compo_cppfunc &&
		    code != compo_sessfunc)
			twarn(ErrRuntime_ParamsType, "evaluators::compile",
			      "successful compile must return a Function value");
		if (code == compo_tfunc &&
		    ((tfunc *)compiled.val.v_tcompo)->env.nparams !=
		    rule->ir->parameters.len)
			twarn(ErrRuntime_ParamsCtr, "evaluators::compile",
			      "compiled Function signature does not match Rule");
	}
	evaluator_cache_entry *entry = calloc(1, sizeof(*entry));
	entry->semantic_hash = semantic_hash;
	entry->name = tstring_dup(evaluator->name);
	entry->version = evaluator->version;
	tobj_set_nil(&entry->result);
	evaluator_cached_copy(result, &entry->result);
	entry->next = evaluator_cache;
	evaluator_cache = entry;
}

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
	case compo_trule: {
		trule *rule = (trule *)v;
		tobj_set_compo(&vm->rev,
			(tcompo_v *)trule_bind(rule, params, nparams));
	} break;
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
		if (!tcppgenf_accepts(g, nparams))
			twarn(ErrRuntime_ParamsCtr, tstring_cstr(g->name),
			      "incorrect parameter count");
		g->f(params, nparams, &vm->rev);
	} break;
	case compo_sessfunc: {
		tcppsessf *s = (tcppsessf *)v;
		if (!tcppsessf_accepts(s, nparams))
			twarn(ErrRuntime_ParamsCtr, tstring_cstr(s->name),
			      "incorrect parameter count");
		s->f(params, nparams, &vm->rev, env);
	} break;
	case compo_trule_builtin:
		switch (((trule_builtin *)v)->kind) {
		case trule_builtin_assert:
			if (nparams != 1) twarn(ErrRuntime_ParamsCtr,
				"assert", "one argument required");
			rule_check(vm, &params[0], &vm->rev, 1, env);
			break;
        case trule_builtin_hold: {
            if (nparams != 1) twarn(ErrRuntime_ParamsCtr, "solve::hold", "one argument required");
            tobj held; tobj_set_nil(&held);
            tsolve_hold(&vm->solve_worker, &params[0], &held);
            tobj key; tobj_set_nil(&key); tobj_set_compo(&key, (tcompo_v *)tstr_new("witness"));
            tobj candidate; tobj_set_nil(&candidate);
            tdict_get((tdict *)held.val.v_tcompo, &key, &candidate);
            tobj_try_clear(&key);
            if (candidate.type == tcompo && tobj_compo_type(&candidate) == compo_trule_instance) {
                tobj checked; tobj_set_nil(&checked);
                rule_check(vm, &candidate, &checked, 0, env);
                tobj_set_compo(&key, (tcompo_v *)tstr_new("passed"));
                tobj passed; tobj_set_nil(&passed);
                tdict_get((tdict *)checked.val.v_tcompo, &key, &passed);
                if (passed.type != tbool || !passed.val.v_tbool)
                    tsolve_reject_witness(&held, "solver witness failed the Rule checker");
                tobj_try_clear(&key); tobj_try_clear(&passed); tobj_try_clear(&checked);
            }
            tobj_ddc_ref_clear(&candidate);
            tobj_copy(&vm->rev, &held);
            tobj_try_clear(&held);
        } break;
		case trule_builtin_check:
			if (nparams != 1) twarn(ErrRuntime_ParamsCtr,
				"rules::check", "one argument required");
			rule_check(vm, &params[0], &vm->rev, 0, env);
			break;
		case trule_builtin_inspect:
			if (nparams != 1) twarn(ErrRuntime_ParamsCtr,
				"rules::inspect", "one argument required");
			rule_inspect(&params[0], &vm->rev);
			break;
		case trule_builtin_parameters:
		case trule_builtin_items:
		case trule_builtin_terms:
			if (nparams != 1) twarn(ErrRuntime_ParamsCtr,
				"rules IR reader", "one argument required");
			rule_ir_list(&params[0], &vm->rev,
				((trule_builtin *)v)->kind);
			break;
		case trule_builtin_origin:
			if (nparams != 1) twarn(ErrRuntime_ParamsCtr,
				"rules::origin", "one argument required");
			rule_origin(&params[0], &vm->rev);
			break;
		case trule_builtin_semantic_hash:
		case trule_builtin_content_hash:
			if (nparams != 1) twarn(ErrRuntime_ParamsCtr,
				"rules hash", "one argument required");
			rule_hash(&params[0], &vm->rev,
				((trule_builtin *)v)->kind ==
				trule_builtin_content_hash);
			break;
		case trule_builtin_serialize:
			if (nparams != 1) twarn(ErrRuntime_ParamsCtr,
				"rules::serialize", "one argument required");
			rule_serialize(&params[0], &vm->rev);
			break;
		case trule_builtin_deserialize:
			if (nparams != 1) twarn(ErrRuntime_ParamsCtr,
				"rules::deserialize", "one argument required");
			rule_deserialize(&params[0], &vm->rev);
			break;
		case trule_builtin_evaluate:
			evaluator_eval(vm, params, nparams, env, &vm->rev);
			break;
		case trule_builtin_compile:
			evaluator_compile(vm, params, nparams, env, &vm->rev);
			break;
		case trule_builtin_context_binding:
		case trule_builtin_context_capture:
		case trule_builtin_context_value:
		case trule_builtin_context_requirement:
			evaluator_context_access(vm,
				((trule_builtin *)v)->kind, params, nparams,
				env, &vm->rev);
			break;
		}
		break;
	default:
		twarn(ErrRuntime_RefType, "vm_eval", "");
	}
	stk_popcn(vm, 1 + nparams);
	stk_push(vm, &vm->rev);
	tvm_set_rev_empty(vm);
}

void tvm_call(tvm *vm, const tobj *callable, tobj *arguments,
	      uint_regs argument_count, tcompo_env *environment,
	      tobj *result)
{
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
	tvm_set_vmstack(&vm2, nullptr, 0);
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
static const tobj *resolve_type_rule(void *context, const char *name)
{
	tdict *values = context;
	tobj key;
	tobj_set_nil(&key);
	tobj_set_compo(&key, (tcompo_v *)tstr_new(name));
	const tobj *value = thashtbl_get(values->items, &key);
	tobj_try_clear(&key);
	if (!value) twarn(ErrRuntime_ParamsType, "InstanceOf", "missing Rule binding");
	return value;
}

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
			else if (stk_top(vm)->type == tcompo &&
				 tobj_compo_type(stk_top(vm)) == compo_trule)
				trule_close_over(
					(trule *)stk_top(vm)->val.v_tcompo, env);
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
		for (i = 0; i < n; i++) {
			if (ps[i].type != tcompo ||
			    tobj_compo_type(&ps[i]) != compo_tpair)
				twarn(ErrRuntime_ParamsType, "dictionary literal",
				      "Pair required");
			tpair *pair = (tpair *)ps[i].val.v_tcompo;
			tdict_set(d, &pair->first, &pair->second);
		}
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
				cache->guard = nullptr;
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
	case OP_BINDTYPE: {
		tobj *encoded = stk_top(vm);
		tobj *bindings = stk_at(vm, 1);
		if (tobj_compo_type(encoded) != compo_tstr || tobj_compo_type(bindings) != compo_tdict)
			twarn(ErrRuntime_ParamsType, "InstanceOf", "invalid Type binding operands");
		ttypeval *template = ttypeval_retain(ttypeval_from_canonical(
			tstring_cstr(((tstr *)encoded->val.v_tcompo)->data)));
		if (!template) twarn(ErrRuntime_ParamsType, "InstanceOf", "invalid Type template");
		ttypeval *bound = ttypeval_resolve_instances(template, resolve_type_rule, bindings->val.v_tcompo);
		ttypeval_release(template);
		stk_popcn(vm, 2);
		tobj value;
		tobj_set_nil(&value);
		tobj_set_compo(&value, (tcompo_v *)bound);
		stk_push(vm, &value);
		tobj_set_nil(&value);
		ttypeval_release(bound);
	} break;
	case OP_RULETYPE: {
		tobj *signature = stk_top(vm);
		tobj *value = stk_at(vm, 1);
		if (tobj_compo_type(signature) != compo_ttypeval || tobj_compo_type(value) != compo_trule)
			twarn(ErrRuntime_ParamsType, "InstanceOf", "Rule signature required");
		ttypeval *type = (ttypeval *)signature->val.v_tcompo;
		trule *rule = (trule *)value->val.v_tcompo;
		if (type->kind != ttype_kind_rule || type->function_parameter_count != rule->ir->parameters.len)
			twarn(ErrRuntime_ParamsType, "InstanceOf", "Rule parameter count mismatch");
		tstring *encoded = tstring_new_empty();
		for (uint_objs i = 0; i < rule->ir->parameters.len; i++) {
			trule_term *parameter = (trule_term *)rule->ir->parameters.data[i].val.v_tcompo;
			ttypeval_release(parameter->type);
			parameter->type = ttypeval_retain(ttypeval_function_parameter_at(type, i));
			if (i) tstring_append_c(encoded, '\x1f');
			tstring_append_ts(encoded, parameter->type->canonical);
		}
		tstring_free(rule->signature);
		rule->signature = encoded;
		stk_popc(vm);
	} break;
	case OP_CHECKTYPE: {
		tobj *type = stk_top(vm);
		if (tobj_compo_type(type) != compo_ttypeval ||
		    !ttypeval_matches(stk_at(vm, 1), (ttypeval *)type->val.v_tcompo))
			twarn(ErrRuntime_ParamsType, "Type annotation", "value does not match declared Type or Rule identity");
		stk_popc(vm);
	} break;
	case OP_FUNCMETA: {
		tobj *signature = stk_top(vm);
		tobj *names = stk_at(vm, 1);
		tobj *function = stk_at(vm, 2);
		if (tobj_compo_type(function) != compo_tfunc ||
		    tobj_compo_type(names) != compo_tstr ||
		    (tobj_compo_type(signature) != compo_tstr && tobj_compo_type(signature) != compo_ttypeval))
			twarn(ErrRuntime_ParamsType, "Function metadata", "Function and Strings required");
		tfunc *f = (tfunc *)function->val.v_tcompo;
		if (tobj_compo_type(signature) == compo_ttypeval) {
			tfunction_metadata *metadata = tfunction_metadata_from_type(
				tstring_cstr(((tstr *)names->val.v_tcompo)->data),
				(ttypeval *)signature->val.v_tcompo);
			if (!metadata) twarn(ErrRuntime_ParamsType, "InstanceOf", "invalid function signature");
			tfunction_metadata_release(f->metadata);
			f->metadata = metadata;
			stk_popcn(vm, 2);
			break;
		}
		tins_cache *cache = tvm_instruction_cache(code_cache, *idx);
		if (!cache->function_metadata) {
			cache->function_metadata = tfunction_metadata_decode(
				tstring_cstr(((tstr *)names->val.v_tcompo)->data),
				tstring_cstr(((tstr *)signature->val.v_tcompo)->data));
			if (!cache->function_metadata)
				twarn(ErrRuntime_ParamsType, "Function metadata", "invalid parameter declaration");
		}
		tfunction_metadata_release(f->metadata);
		f->metadata = tfunction_metadata_retain(cache->function_metadata);
		stk_popc(vm);
		stk_popc(vm);
	} break;
	case OP_PUSHRULE: {
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
	} break;
	case OP_RULEVALUE:
	case OP_RULENOT:
	case OP_RULETRUTH: {
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
	} break;
	case OP_RULEITEM:
	case OP_RULECOND: {
		if (!vm->rule_output)
			twarn(ErrRuntime_Other, "Rule condition",
			      "condition outside checker");
		tobj *condition = stk_top(vm);
		if (tbycode_ins(*iter) == OP_RULEITEM && condition->type == tcompo &&
		    tobj_compo_type(condition) == compo_trule_instance) {
			tobj_vec_push(&vm->rule_output->items, condition);
			stk_popc(vm);
			break;
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
			break;
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
	} break;
	case OP_RULEREQ: {
		if (!vm->rule_output)
			twarn(ErrRuntime_Other, "Rule dependency",
			      "dependency outside checker");
		tobj *requirement = stk_top(vm);
		if (requirement->type != tcompo ||
		    tobj_compo_type(requirement) != compo_trule_instance)
			twarn(ErrRuntime_ParamsType, "Rule dependency",
			      "RuleInstance required");
		tobj_vec_push(&vm->rule_output->items, requirement);
		stk_popc(vm);
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
	*source = location->source ? tstring_cstr(location->source) : nullptr;
	*file = location->file ? tstring_cstr(location->file) : nullptr;
	*line = location->line;
	*column = location->column;
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
				       &code_cache, &call,
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
				code_cache = *tvm_get_code_cache(vm, callee_wrapper);
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
				code_cache = *tvm_get_code_cache(vm, return_wrapper);
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
