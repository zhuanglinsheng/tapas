#ifndef T_VAL_H
#define T_VAL_H

#include "ds/tobj_vec.h"
#include "ds/thashtbl.h"
#include "tbycs.h"
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================*
 * Forward Declarations
 *===========================================================================*/

typedef struct tobj tobj;
typedef struct tcompo_v tcompo_v;
typedef struct tstr tstr;
typedef struct tlist tlist;
typedef struct tpair tpair;
typedef struct tdict tdict;
typedef struct titer titer;
typedef struct tdarr tdarr;
typedef struct tbarr tbarr;
typedef struct ttime ttime;
typedef struct tfunc tfunc;
typedef struct tlib tlib;
typedef struct tcompo_env tcompo_env;
typedef struct tcppgenf tcppgenf;
typedef struct tcppsessf tcppsessf;

/*===========================================================================*
 * 1. Function Pointer Types for Composite Dispatch
 *===========================================================================*/

typedef const char *(*compo_get_type_fn)(void);
typedef tcompo_type (*compo_get_code_fn)(void);
typedef long (*compo_len_fn)(void *self);
typedef void *(*compo_copy_fn)(void *self);
typedef void (*compo_free_fn)(void *self);
typedef int (*compo_identical_fn)(void *self, void *other);
typedef tstring *(*compo_tostring_abbr_fn)(void *self);
typedef tstring *(*compo_tostring_full_fn)(void *self);

/* Binary operator dispatch: self is the composite object, other is the
 * other operand. is_rhs is 0 if self is the left operand (self op other),
 * or 1 if self is the right operand (other op self). vre receives result. */
typedef void (*compo_op_bin_fn)(void *self,
				const tobj *other,
				int is_rhs,
				tobj *vre);

typedef struct {
	compo_get_type_fn get_type;
	compo_get_code_fn get_compo_type_code;
	compo_len_fn len;
	compo_copy_fn copy;
	compo_free_fn free;
	compo_identical_fn identical;
	compo_tostring_abbr_fn tostring_abbr;
	compo_tostring_full_fn tostring_full;

	/* Operator dispatch — NULL means "not supported for this type".
	 * When both operands are primitive types the existing numeric
	 * handler is used; these fields are tried only when at least one
	 * operand is tcompo. */
	compo_op_bin_fn op_add;
	compo_op_bin_fn op_sub;
	compo_op_bin_fn op_mul;
	compo_op_bin_fn op_div;
	compo_op_bin_fn op_mod;
	compo_op_bin_fn op_pow;
	compo_op_bin_fn op_mmul;
	compo_op_bin_fn op_eq;
	compo_op_bin_fn op_ne;
	compo_op_bin_fn op_sg;   /* > */
	compo_op_bin_fn op_sl;   /* < */
	compo_op_bin_fn op_ge;   /* >= */
	compo_op_bin_fn op_le;   /* <= */
	compo_op_bin_fn op_and;
	compo_op_bin_fn op_or;
} tcompo_vtable;

struct tcompo_v {
	tcompo_vtable *vtable;
	int refctr;
};

/*===========================================================================*
 * 2. Tagged Union Value (tobj)
 *===========================================================================*/

struct tobj {
	ttypes type;
	int name_loc;
	union {
		long v_tint;
		double v_tfloat;
		int v_tbool;
		tcompo_v *v_tcompo;
	} val;
};

/* ---- tobj declarations ---- */
void tobj_set_nil(tobj *v);
void tobj_set_bool(tobj *v, int b);
void tobj_set_int(tobj *v, long i);
void tobj_set_float(tobj *v, double d);
void tobj_set_compo(tobj *v, tcompo_v *compo);
void tobj_ddc_ref_clear(tobj *v);
void tobj_try_clear(tobj *v);
void tobj_copy(tobj *dst, const tobj *src);
int tobj_identical(const tobj *a, const tobj *b);

tstring *tobj_tostring_pointer(const char *type, const void *ptr);
tstring *tobj_tostring_abbr(const tobj *v);
tstring *tobj_tostring_full(const tobj *v);

ttypes tobj_get_type(const tobj *v);
long tobj_get_v_tint(const tobj *v);
double tobj_get_v_tfloat(const tobj *v);
int tobj_get_v_tbool(const tobj *v);
tcompo_v *tobj_get_v_tcompo(const tobj *v);
int tobj_get_name_loc(const tobj *v);
int tobj_is_nil(const tobj *v);

tcompo_type tobj_compo_type(const tobj *v);
const char *tobj_compo_type_name(const tobj *v);

/*===========================================================================*
 * 3. Object Array (tobj_array)
 *===========================================================================*/

typedef struct {
	tobj *data;
	uint_objs len;
	uint_objs capacity;
} tobj_array;

void tobj_array_init(tobj_array *arr, uint_objs cap);
void tobj_array_free(tobj_array *arr);
void tobj_array_try_expand(tobj_array *arr, uint_objs newcap);
uint_objs tobj_array_get_len(const tobj_array *arr);
uint_objs tobj_array_get_cap(const tobj_array *arr);
tobj *tobj_array_get_obj(tobj_array *arr, uint_objs loc);
void tobj_array_set_obj(tobj_array *arr, uint_objs loc, const tobj *v);
void tobj_array_add_obj(tobj_array *arr, uint_csts nameloc);
void tobj_array_set_len(tobj_array *arr, uint_objs n);
void tobj_array_del_obj(tobj_array *arr, uint_objs n);
uint_objs tobj_array_get_ref_obj_loc(tobj_array *arr, tcompo_v *compo);

/*===========================================================================*
 * 4. String (tstr) — uses tstring internally
 *===========================================================================*/

struct tstr {
	tcompo_v base;
	tstring *data;
};

extern tcompo_vtable tstr_vtable;

tstr *tstr_new(const char *s);
void tstr_idx(tstr *s, const tobj *params, uint_regs np, tobj *vre);
void tstr_iset(tstr *s, const tobj *params, uint_regs np, const tobj *vright);

/*===========================================================================*
 * 5. List (tlist)
 *===========================================================================*/

struct tlist {
	tcompo_v base;
	tobj_vec items;
};

extern tcompo_vtable tlist_vtable;

tlist *tlist_new(void);
uint_objs tlist_size(const tlist *l);
const tobj *tlist_at(const tlist *l, uint_objs idx);
void tlist_push(tlist *l, const tobj *v);
void tlist_insert(tlist *l, uint_objs idx, const tobj *v);
void tlist_set_at(tlist *l, uint_objs idx, const tobj *v);
void tlist_pop(tlist *l, uint_objs idx);
void tlist_sort(tlist *l, int (*compar)(const void *, const void *));
int tlist_in(tlist *l, const tobj *v);
void tlist_idx(tlist *l, const tobj *params, uint_regs np, tobj *vre);
void tlist_iset(tlist *l, const tobj *params, uint_regs np, const tobj *vright);
int tlist_next_at(tlist *l, long *iter_pos, tobj *vre);

/*===========================================================================*
 * 6. Pair (tpair)
 *===========================================================================*/

struct tpair {
	tcompo_v base;
	tobj first;
	tobj second;
};

extern tcompo_vtable tpair_vtable;

tpair *tpair_new(const tobj *f, const tobj *s);
void tpair_idx(tpair *p, const tobj *params, uint_regs np, tobj *vre);
void tpair_iset(tpair *p, const tobj *params, uint_regs np, const tobj *vright);

/*===========================================================================*
 * 7. Dictionary (tdict)
 *===========================================================================*/

struct tdict {
	tcompo_v base;
	thashtbl *items;
};

extern tcompo_vtable tdict_vtable;

tdict *tdict_new(void);
void tdict_set(tdict *d, const tobj *key, const tobj *val);
void tdict_get(tdict *d, const tobj *key, tobj *vre);
int tdict_contains(tdict *d, const tobj *key);
int tdict_delete(tdict *d, const tobj *key);
void tdict_idx(tdict *d, const tobj *params, uint_regs np, tobj *vre);
void tdict_iset(tdict *d, const tobj *params, uint_regs np, const tobj *vright);
void tdict_set_append(tdict *d, const tobj *pair_val);
tlist *tdict_keys(tdict *d);
tlist *tdict_values(tdict *d);

/*===========================================================================*
 * 8. Iterator (titer)
 *===========================================================================*/

struct titer {
	tcompo_v base;
	long start;
	long end;
	long step;
	long current;
};

extern tcompo_vtable titer_vtable;

titer *titer_new_step(long start, long step, long end);
titer *titer_new(long start, long end);
int titer_next(titer *it);
int titer_next_at(titer *it, long *iter_pos, tobj *vre);
void titer_get_v_at_loc(titer *it, tobj *vre);
void titer_iter_restore(titer *it);
int titer_in(titer *it, const tobj *v);

/*===========================================================================*
 * 9. Dense two-dimensional arrays
 *===========================================================================*/

struct tdarr {
	tcompo_v base;
	size_t rows;
	size_t cols;
	double *data; /* row-major */
};

struct tbarr {
	tcompo_v base;
	size_t rows;
	size_t cols;
	unsigned char *data; /* row-major, values are normalised to 0 or 1 */
};

extern tcompo_vtable tdarr_vtable;
extern tcompo_vtable tbarr_vtable;

tdarr *tdarr_new(size_t rows, size_t cols, double value);
tdarr *tdarr_from_list(size_t rows, size_t cols, const tlist *values);
tbarr *tbarr_new(size_t rows, size_t cols, int value);
tbarr *tbarr_from_list(size_t rows, size_t cols, const tlist *values);
size_t tarr_rows(const tcompo_v *arr);
size_t tarr_cols(const tcompo_v *arr);
double tdarr_at(const tdarr *arr, size_t row, size_t col);
int tbarr_at(const tbarr *arr, size_t row, size_t col);
void tdarr_set(tdarr *arr, size_t row, size_t col, double value);
void tbarr_set(tbarr *arr, size_t row, size_t col, int value);
void tarr_idx(tcompo_v *arr, const tobj *params, uint_regs np, tobj *vre);
void tarr_iset(tcompo_v *arr, const tobj *params, uint_regs np,
	       const tobj *value);
tdarr *tdarr_slice(const tdarr *arr, size_t row, size_t col,
		   size_t rows, size_t cols);
tbarr *tbarr_slice(const tbarr *arr, size_t row, size_t col,
		   size_t rows, size_t cols);
tdarr *tdarr_transpose(const tdarr *arr);
tbarr *tbarr_transpose(const tbarr *arr);
tdarr *tdarr_matmul(const tdarr *left, const tdarr *right);

/*===========================================================================*
 * 10. Time values
 *===========================================================================*/

struct ttime {
	tcompo_v base;
	time_t value;
};

extern tcompo_vtable ttime_vtable;
ttime *ttime_new(void);
ttime *ttime_from_time(time_t value);
time_t ttime_get(const ttime *value);

/*===========================================================================*
 * 11. C General Function Wrapper (tcppgenf)
 *===========================================================================*/

typedef void (*genf_t)(tobj *params, uint_regs len, tobj *vre);

struct tcppgenf {
	tcompo_v base;
	genf_t f;
	tstring *name;
	uint_regs nparams_sig;
};

extern tcompo_vtable tcppgenf_vtable;

tcppgenf *tcppgenf_new(genf_t f, const char *name, uint_regs nparams_sig);
genf_t tcppgenf_get_f(tcppgenf *g);
uint_regs tcppgenf_get_nparams_sig(tcppgenf *g);

#ifdef __cplusplus
}
#endif

#endif /* T_VAL_H */
