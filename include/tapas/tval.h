#ifndef TAPAS_TVAL_H
#define TAPAS_TVAL_H

#include "tapas/tbasis.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Core value types. Runtime object definitions have dedicated headers. */
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
typedef struct ttypeval ttypeval;

typedef const char *(*compo_get_type_fn)(void);
typedef tcompo_type (*compo_get_code_fn)(void);
typedef long (*compo_len_fn)(void *self);
typedef void *(*compo_copy_fn)(void *self);
typedef void (*compo_free_fn)(void *self);
typedef int (*compo_identical_fn)(void *self, void *other);
typedef tstring *(*compo_tostring_abbr_fn)(void *self);
typedef tstring *(*compo_tostring_full_fn)(void *self);
typedef void (*compo_op_bin_fn)(void *self, const tobj *other,
				int is_rhs, tobj *vre);

typedef struct {
	compo_get_type_fn get_type;
	compo_get_code_fn get_compo_type_code;
	compo_len_fn len;
	compo_copy_fn copy;
	compo_free_fn free;
	compo_identical_fn identical;
	compo_tostring_abbr_fn tostring_abbr;
	compo_tostring_full_fn tostring_full;
	compo_op_bin_fn op_add;
	compo_op_bin_fn op_sub;
	compo_op_bin_fn op_mul;
	compo_op_bin_fn op_div;
	compo_op_bin_fn op_mod;
	compo_op_bin_fn op_pow;
	compo_op_bin_fn op_mmul;
	compo_op_bin_fn op_eq;
	compo_op_bin_fn op_ne;
	compo_op_bin_fn op_sg;
	compo_op_bin_fn op_sl;
	compo_op_bin_fn op_ge;
	compo_op_bin_fn op_le;
	compo_op_bin_fn op_and;
	compo_op_bin_fn op_or;
} tcompo_vtable;

struct tcompo_v {
	tcompo_vtable *vtable;
	int refctr;
};

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

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_TVAL_H */
