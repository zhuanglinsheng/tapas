#ifndef TAPAS_TENV_H
#define TAPAS_TENV_H

#include "tapas/tval.h"
#include "tapas/ds/tobj_array.h"
#include "tapas/runtime/tcfn.h"
#include "tapas/tbycs.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================*
 * Forward Declarations
 *===========================================================================*/

typedef struct tcompo_env_abstract tcompo_env_abstract;

/*===========================================================================*
 * 1. Abstract Environment (Tree of object arrays)
 *===========================================================================*/

struct tcompo_env_abstract {
	tobj_array objs;
	tcompo_env_abstract *father_env;
	uint_objs loc_in_father_env;
};

int env_self_not_in_tree_of(const tcompo_env_abstract *self, const tcompo_env_abstract *env);
int env_self_tree_has_no(const tcompo_env_abstract *self, const tcompo_env_abstract *env);
void tcompo_env_abstract_init(tcompo_env_abstract *env, uint_objs objlst_cap, tcompo_env_abstract *father);
tcompo_env_abstract *tcompo_env_abstract_get_father(tcompo_env_abstract *env);
tcompo_env_abstract *tcompo_env_abstract_get_top(tcompo_env_abstract *env);
uint_objs tcompo_env_abstract_get_loc_in_father(const tcompo_env_abstract *env);
tobj *tcompo_env_abstract_get_obj(tcompo_env_abstract *env, uint_objs loc, tcompo_env_abstract *from);
void tcompo_env_abstract_set_obj(tcompo_env_abstract *env, uint_objs loc, tcompo_env_abstract *from, const tobj *v);

/*===========================================================================*
 * 2. Concrete Environment (with VM state)
 *===========================================================================*/

typedef struct tcompo_env {
	tcompo_env_abstract base;

	tobj *vmstack;
	tobj *params;
	uint_objs tmpmax;
	uint_regs regmax;
	uint_regs nparams;
	uint_regs dynamic_nparams;

	tcompo_type compo_type;
	struct tfunc *owner_func;
} tcompo_env;

void tcompo_env_set_regmax(tcompo_env *env, uint_regs n);
void tcompo_env_set_tmpmax(tcompo_env *env, uint_objs ntmps);
void tcompo_env_set_nparams(tcompo_env *env, uint_regs n);
void tcompo_env_init(tcompo_env *env, uint_objs objlst_cap, tcompo_env *father, uint_regs regmax, uint_objs tmpmax, uint_regs nparams, tcompo_type compo_type);
tcompo_type tcompo_env_get_compo_type(tcompo_env *env);
void tcompo_env_set_compo_type(tcompo_env *env, tcompo_type t);
tcompo_env *tcompo_env_get_father(tcompo_env *env);
uint_regs tcompo_env_get_nparams(const tcompo_env *env);
uint_regs tcompo_env_get_dynamic_nparams(const tcompo_env *env);
uint_regs tcompo_env_get_regmax(const tcompo_env *env);
tobj *tcompo_env_get_vmstack(tcompo_env *env);
uint_objs tcompo_env_get_tmpmax(const tcompo_env *env);
tobj *tcompo_env_get_params(const tcompo_env *env);
void tcompo_env_set_dynamic_nparams(tcompo_env *env, uint_regs n);
void tcompo_env_set_params(tcompo_env *env, tobj *params);
uint_objs tcompo_env_get_objlst_len(tcompo_env *env);
tobj *tcompo_env_get_obj(tcompo_env *env, uint_objs loc);
void tcompo_env_set_obj(tcompo_env *env, uint_objs loc, const tobj *v);
void tcompo_env_add_obj(tcompo_env *env, uint_csts nameloc);
void tcompo_env_set_objlst_len(tcompo_env *env, uint_objs n);
void tcompo_env_del_obj(tcompo_env *env, uint_objs n);
uint_objs tcompo_env_get_objlst_cap(tcompo_env *env);
void tcompo_env_try_expand_objlist(tcompo_env *env, uint_objs n);
uint_objs tcompo_env_get_ref_obj_loc(tcompo_env *env, tcompo_v *compo);
tcompo_v *tcompo_env_as_compo(tcompo_env *env);
void tcompo_env_copy_to_obj(tcompo_env *env, tobj *vre);

/*===========================================================================*
 * 3. Tap Function (tfunc)
 *===========================================================================*/

struct tfunc {
	tcompo_v compo_base;
	tcompo_env env;
	uint_cmds cmdloc;
	uint_cmds ncmds;
};

extern tcompo_vtable tfunc_vtable;

tfunc *tfunc_new(uint_objs nlocals, tcompo_env *father_env, uint_regs reg_max, uint_objs tmpmax, uint_regs nparams, uint_cmds cmdloc, uint_cmds ncmds);
void tfunc_assign_params(tfunc *f, tobj *params, uint_regs nparams);
void tfunc_close_over(tfunc *f, tcompo_env *env);
uint_cmds tfunc_get_cmdloc(tfunc *f);
uint_cmds tfunc_get_ncmds(tfunc *f);
tcompo_env *tfunc_get_env(tfunc *f);

/*===========================================================================*
 * 4. Library (tlib) — string fields now use tstring
 *===========================================================================*/

struct tlib {
	tcompo_v compo_base;
	tcompo_env env;

	tstring **default_v_names;
	uint_objs ndefault;
	uint_objs ndefault_cap;

	tstring **paths;
	uint_lexs npaths;
	uint_lexs npaths_cap;

	twrapper *wrapper;

	tdict *exposed;
};

extern tcompo_vtable tlib_vtable;

tlib *tlib_new(void);
void tlib_set_wrapper(tlib *lb, twrapper *wrapper);
twrapper *tlib_get_wrapper(tlib *lb);
tdict *tlib_get_exposed(tlib *lb);
void tlib_set_exposed(tlib *lb, tdict *d);
void tlib_lib_add_obj(tlib *lb, const char *name, const tobj *v);
tdict *tlib_add_pkg(tlib *lb, const char *pkgname);
void tlib_add_cppf(tlib *lb, const char *name, genf_t f, uint_regs nparams_sig);
void tlib_add_path(tlib *lb, const char *paths_str);
tstring **tlib_get_paths(tlib *lb);
uint_lexs tlib_get_npaths(tlib *lb);
tstring **tlib_get_default_v_names(tlib *lb);
uint_objs tlib_get_ndefault(tlib *lb);
tlib *tlib_recreate(tlib *lb);
void tlib_idx(tlib *lb, const tobj *params, uint_regs np, tobj *vre);
tlist *tlib_listing_objects(tlib *lb);
tlib *tlib_from_env(tcompo_env *env);
twrapper *tfunc_get_wrapper_from_env(tcompo_env *env);
tlist *tlib_listing_paths(tlib *lb);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_TENV_H */
