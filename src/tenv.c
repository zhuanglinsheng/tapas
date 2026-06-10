#include "tapas/tenv.h"

#include "tapas/tval.h"

#include <stddef.h>


/*===========================================================================*
 * 1. Abstract Environment (Tree of object arrays)
 *===========================================================================*/

/* Check if this node is already in the ancestor chain of env */
int env_self_not_in_tree_of(
		const tcompo_env_abstract *self,
		const tcompo_env_abstract *env)
{
	if (env == NULL)
		return 1;
	if (env == self)
		return 0;
	return env_self_not_in_tree_of(self, env->father_env);
}

/* Check if env is not in the descendant chain of self */
int env_self_tree_has_no(
		const tcompo_env_abstract *self,
		const tcompo_env_abstract *env)
{
	if (env == NULL)
		return 1;
	if (env == self)
		return 0;
	if (!self->father_env)
		return 1;
	else
		return env_self_tree_has_no(self->father_env, env);
}

void tcompo_env_abstract_init(
		tcompo_env_abstract *env,
		uint_objs objlst_cap,
		tcompo_env_abstract *father)
{
	tobj_array_init(&env->objs, objlst_cap);
	env->father_env = father;
	env->loc_in_father_env = 0;

	/* Check looping reference */
	if (!env_self_not_in_tree_of(env, father))
		twarn(ErrRuntime_LoopRef, "tcompo_env_abstract_init", "");

	/* Find location in father env */
	if (father == NULL)
		return;

	uint_objs i;
	for (i = 0; i < tobj_array_get_len(&father->objs); i++) {
		tobj *vi = tobj_array_get_obj(&father->objs, i);
		if (vi->type != tcompo)
			continue;
		if (env == (tcompo_env_abstract *)vi->val.v_tcompo)
			break;
	}
	env->loc_in_father_env = i;
}

tcompo_env_abstract *tcompo_env_abstract_get_father(tcompo_env_abstract *env)
{
	return env->father_env;
}

tcompo_env_abstract *tcompo_env_abstract_get_top(tcompo_env_abstract *env)
{
	tcompo_env_abstract *top = env;
	while (top->father_env)
		top = top->father_env;
	return top;
}

uint_objs tcompo_env_abstract_get_loc_in_father(const tcompo_env_abstract *env)
{
	return env->loc_in_father_env;
}

/**
 * Get object at loc in the environment chain.
 * Searches from current env upward through fathers.
 */
tobj *tcompo_env_abstract_get_obj(
		tcompo_env_abstract *env,
		uint_objs loc,
		tcompo_env_abstract *from)
{
	if (from == NULL) {
		if (loc < tobj_array_get_len(&env->objs))
			return tobj_array_get_obj(&env->objs, loc);
		else {
			if (!env->father_env)
				twarn(ErrRuntime_ObjUnfound,
				      "tcompo_env_get_obj",
				      "");
			return tcompo_env_abstract_get_obj(
				env->father_env,
				loc - tobj_array_get_len(&env->objs),
				env);
		}
	} else {
		uint_objs from_loc = from->loc_in_father_env;
		if (loc < from_loc)
			return tobj_array_get_obj(&env->objs, loc);
		else {
			if (!env->father_env)
				twarn(ErrRuntime_ObjUnfound,
				      "tcompo_env_get_obj",
				      "");
			return tcompo_env_abstract_get_obj(
				env->father_env, loc - from_loc, env);
		}
	}
}

void tcompo_env_abstract_set_obj(
		tcompo_env_abstract *env,
		uint_objs loc,
		tcompo_env_abstract *from,
		const tobj *v)
{
	if (from == NULL) {
		if (loc < tobj_array_get_len(&env->objs)) {
			tobj_array_set_obj(&env->objs, loc, v);
			return;
		}
		if (!env->father_env)
			twarn(ErrRuntime_ObjUnfound,
			      "tcompo_env_set_obj",
			      "");
		tcompo_env_abstract_set_obj(
			env->father_env,
			loc - tobj_array_get_len(&env->objs),
			env,
			v);
		return;
	}

	uint_objs from_loc = from->loc_in_father_env;
	if (loc < from_loc) {
		tobj_array_set_obj(&env->objs, loc, v);
		return;
	}
	if (!env->father_env)
		twarn(ErrRuntime_ObjUnfound, "tcompo_env_set_obj", "");
	tcompo_env_abstract_set_obj(env->father_env, loc - from_loc, env, v);
}

/*===========================================================================*
 * 2. Concrete Environment (with VM state)
 *===========================================================================*/

/* Forward declarations */
void tcompo_env_set_regmax(tcompo_env *env, uint_regs n);
void tcompo_env_set_tmpmax(tcompo_env *env, uint_objs ntmps);
void tcompo_env_set_nparams(tcompo_env *env, uint_regs n);

void tcompo_env_init(
		tcompo_env *env,
		uint_objs objlst_cap,
		tcompo_env *father,
		uint_regs regmax,
		uint_objs tmpmax,
		uint_regs nparams,
		tcompo_type compo_type)
{
	tcompo_env_abstract_init(
		&env->base, objlst_cap, (tcompo_env_abstract *)father);
	env->vmstack = NULL;
	env->params = NULL;
	env->tmpmax = 0;
	env->regmax = 0;
	env->dynamic_nparams = 0;
	env->compo_type = compo_type;
	env->owner_func = NULL;

	/* These set functions allocate vmstack */
	tcompo_env_set_tmpmax(env, tmpmax);
	tcompo_env_set_regmax(env, regmax);
	tcompo_env_set_nparams(env, nparams);
}

tcompo_type tcompo_env_get_compo_type(tcompo_env *env)
{
	return env->compo_type;
}

void tcompo_env_set_compo_type(tcompo_env *env, tcompo_type t)
{
	env->compo_type = t;
}

tcompo_env *tcompo_env_get_father(tcompo_env *env)
{
	return (tcompo_env *)env->base.father_env;
}

uint_regs tcompo_env_get_nparams(const tcompo_env *env)
{
	return env->nparams;
}

uint_regs tcompo_env_get_dynamic_nparams(const tcompo_env *env)
{
	return env->dynamic_nparams;
}

uint_regs tcompo_env_get_regmax(const tcompo_env *env)
{
	return env->regmax;
}

tobj *tcompo_env_get_vmstack(tcompo_env *env)
{
	return env->vmstack;
}

uint_objs tcompo_env_get_tmpmax(const tcompo_env *env)
{
	return env->tmpmax;
}

tobj *tcompo_env_get_params(const tcompo_env *env)
{
	return env->params;
}

void tcompo_env_set_dynamic_nparams(tcompo_env *env, uint_regs n)
{
	env->dynamic_nparams = n;
}

void tcompo_env_set_params(tcompo_env *env, tobj *params)
{
	env->params = params;
}

void tcompo_env_set_nparams(tcompo_env *env, uint_regs n)
{
	env->nparams = n;
}

void tcompo_env_set_regmax(tcompo_env *env, uint_regs n)
{
	env->regmax = n;
	free(env->vmstack);
	env->vmstack = NULL;
	if (n > 0) {
		env->vmstack = (tobj *)calloc(n, sizeof(tobj));
		uint_regs i;
		for (i = 0; i < n; i++)
			tobj_set_nil(&env->vmstack[i]);
	}
}

void tcompo_env_set_tmpmax(tcompo_env *env, uint_objs ntmps)
{
	env->tmpmax = ntmps;
}

/* Forward operations to tobj_array */
uint_objs tcompo_env_get_objlst_len(tcompo_env *env)
{
	return tobj_array_get_len(&env->base.objs);
}

tobj *tcompo_env_get_obj(tcompo_env *env, uint_objs loc)
{
	return tcompo_env_abstract_get_obj(&env->base, loc, NULL);
}

void tcompo_env_set_obj(tcompo_env *env, uint_objs loc, const tobj *v)
{
	tcompo_env_abstract_set_obj(&env->base, loc, NULL, v);
}

void tcompo_env_add_obj(tcompo_env *env, uint_csts nameloc)
{
	tobj_array_add_obj(&env->base.objs, nameloc);
}

void tcompo_env_set_objlst_len(tcompo_env *env, uint_objs n)
{
	tobj_array_set_len(&env->base.objs, n);
}

void tcompo_env_del_obj(tcompo_env *env, uint_objs n)
{
	tobj_array_del_obj(&env->base.objs, n);
}

uint_objs tcompo_env_get_objlst_cap(tcompo_env *env)
{
	return tobj_array_get_cap(&env->base.objs);
}

void tcompo_env_try_expand_objlist(tcompo_env *env, uint_objs n)
{
	tobj_array_try_expand(&env->base.objs, n);
}

uint_objs tcompo_env_get_ref_obj_loc(tcompo_env *env, tcompo_v *compo)
{
	return tobj_array_get_ref_obj_loc(&env->base.objs, compo);
}

tcompo_v *tcompo_env_as_compo(tcompo_env *env)
{
	switch (env->compo_type) {
	case compo_tlib:
		return &((tlib *)((char *)env - offsetof(tlib, env)))->compo_base;
	case compo_tfunc:
		if (env->owner_func)
			return &env->owner_func->compo_base;
		return &((tfunc *)((char *)env - offsetof(tfunc, env)))->compo_base;
	default:
		twarn(ErrRuntime_RefType, "tcompo_env_as_compo", "");
		return NULL;
	}
}

void tcompo_env_copy_to_obj(tcompo_env *env, tobj *vre)
{
	tobj_set_nil(vre);

	if (!env) {
		return;
	}

	if (env->compo_type == compo_tlib)
		twarn(ErrRuntime_RefType, "tcompo_env_copy_to_obj", "");

	tcompo_v *compo = tcompo_env_as_compo(env);
	tobj_set_compo(vre, (tcompo_v *)compo->vtable->copy(compo));
}


/*===========================================================================*
 * 3. Session-Level C Function (tcppsessf) — name uses tstring
 *===========================================================================*/

static const char *tcppsessf_get_type(void)
{
	return "C Session Function";
}

static tcompo_type tcppsessf_get_code(void)
{
	return compo_sessfunc;
}

static long tcppsessf_len(void *self)
{
	(void)self;
	return 0;
}

static void *tcppsessf_copy(void *self)
{
	tcppsessf *s = (tcppsessf *)self;
	tcppsessf *n = (tcppsessf *)calloc(1, sizeof(tcppsessf));
	n->base.vtable = s->base.vtable;
	n->f = s->f;
	n->name = tstring_dup(s->name);
	return n;
}

static void tcppsessf_free(void *self)
{
	tcppsessf *s = (tcppsessf *)self;
	tstring_free(s->name);
	free(s);
}

static int tcppsessf_identical(void *self, void *other)
{
	return self == other;
}

static tstring *tcppsessf_tostring_abbr(void *self)
{
	return tobj_tostring_pointer("C Session Function", self);
}

static tstring *tcppsessf_tostring_full(void *self)
{
	return tobj_tostring_pointer("C Session Function", self);
}

tcompo_vtable tcppsessf_vtable = {
	tcppsessf_get_type,	 tcppsessf_get_code,	 tcppsessf_len,
	tcppsessf_copy,		 tcppsessf_free,	 tcppsessf_identical,
	tcppsessf_tostring_abbr, tcppsessf_tostring_full
};

tcppsessf *tcppsessf_new(sessf_t f, const char *name)
{
	tcppsessf *s = (tcppsessf *)calloc(1, sizeof(tcppsessf));
	s->base.vtable = &tcppsessf_vtable;
	s->f = f;
	s->name = tstring_new(name ? name : "");
	return s;
}

sessf_t tcppsessf_get_f(tcppsessf *s)
{
	return s->f;
}

/*===========================================================================*
 * 4. Tap Function (tfunc)
 *===========================================================================*/

static const char *tfunc_get_type(void)
{
	return "Function";
}

static tcompo_type tfunc_get_code(void)
{
	return compo_tfunc;
}

static long tfunc_len(void *self)
{
	(void)self;
	return 0;
}

static void *tfunc_copy(void *self)
{
	tfunc *f = (tfunc *)self;
	tfunc *n = (tfunc *)calloc(1, sizeof(tfunc));
	n->compo_base.vtable = f->compo_base.vtable;
	tcompo_env_init(&n->env,
			tobj_array_get_cap(&f->env.base.objs),
			tcompo_env_get_father(&f->env),
			tcompo_env_get_regmax(&f->env),
			tcompo_env_get_tmpmax(&f->env),
			tcompo_env_get_nparams(&f->env),
			compo_tfunc);
	n->env.owner_func = n;
	n->cmdloc = f->cmdloc;
	n->ncmds = f->ncmds;
	return n;
}

static void tfunc_free(void *self)
{
	tfunc *f = (tfunc *)self;
	tcompo_env_set_regmax(&f->env, 0);
	tobj_array_free(&f->env.base.objs);
	free(f);
}

static int tfunc_identical(void *self, void *other)
{
	return self == other;
}

static tfunc *tfunc_from_env(tcompo_env *env)
{
	if (!env->owner_func)
		twarn(ErrRuntime_RefType, "tfunc_from_env", "");
	return env->owner_func;
}

static tfunc *tfunc_snapshot_from_env(tcompo_env *env)
{
	tfunc *src = tfunc_from_env(env);
	tfunc *n = (tfunc *)calloc(1, sizeof(tfunc));
	n->compo_base.vtable = src->compo_base.vtable;
	tcompo_env_init(&n->env,
			tobj_array_get_cap(&env->base.objs),
			tcompo_env_get_father(env),
			tcompo_env_get_regmax(env),
			tcompo_env_get_tmpmax(env),
			tcompo_env_get_nparams(env),
			compo_tfunc);
	n->env.owner_func = n;
	n->cmdloc = src->cmdloc;
	n->ncmds = src->ncmds;

	uint_objs i;
	for (i = 0; i < tobj_array_get_len(&env->base.objs); i++) {
		tobj_array_add_obj(&n->env.base.objs,
				   env->base.objs.data[i].name_loc);
		tobj_array_set_obj(&n->env.base.objs,
				   i,
				   &env->base.objs.data[i]);
	}
	return n;
}

void tfunc_close_over(tfunc *f, tcompo_env *env)
{
	if (env->compo_type != compo_tfunc)
		return;
	if (f->env.base.father_env != &env->base)
		return;
	tfunc *snapshot = tfunc_snapshot_from_env(env);
	f->env.base.father_env = &snapshot->env.base;
	f->env.base.loc_in_father_env =
		tobj_array_get_len(&snapshot->env.base.objs);
}

static tstring *tfunc_tostring_abbr(void *self)
{
	return tobj_tostring_pointer("Function", self);
}

static tstring *tfunc_tostring_full(void *self)
{
	return tobj_tostring_pointer("Function", self);
}

tcompo_vtable tfunc_vtable = {
	tfunc_get_type,	     tfunc_get_code,	 tfunc_len,
	tfunc_copy,	     tfunc_free,	 tfunc_identical,
	tfunc_tostring_abbr, tfunc_tostring_full
};

tfunc *tfunc_new(uint_objs nlocals,
			       tcompo_env *father_env,
			       uint_regs reg_max,
			       uint_objs tmpmax,
			       uint_regs nparams,
			       uint_cmds cmdloc,
			       uint_cmds ncmds)
{
	tfunc *f = (tfunc *)calloc(1, sizeof(tfunc));
	f->compo_base.vtable = &tfunc_vtable;
	tcompo_env_init(&f->env,
			nlocals,
			father_env,
			reg_max,
			tmpmax,
			nparams,
			compo_tfunc);
	f->env.owner_func = f;
	f->cmdloc = cmdloc;
	f->ncmds = ncmds;
	return f;
}

void tfunc_assign_params(tfunc *f, tobj *params, uint_regs nparams)
{
	tcompo_env_set_objlst_len(&f->env, 0);
	if (tcompo_env_get_nparams(&f->env) != UNDEF_NPARAMS) {
		uint_regs i;
		for (i = 0; i < nparams; i++) {
			tcompo_env_add_obj(&f->env, UNDEF_NAMELOC);
			tcompo_env_set_obj(&f->env, i, &params[i]);
		}
	}
	tcompo_env_set_dynamic_nparams(&f->env, nparams);
	tcompo_env_set_params(&f->env, params);
}

uint_cmds tfunc_get_cmdloc(tfunc *f)
{
	return f->cmdloc;
}

uint_cmds tfunc_get_ncmds(tfunc *f)
{
	return f->ncmds;
}

tcompo_env *tfunc_get_env(tfunc *f)
{
	return &f->env;
}

/*===========================================================================*
 * 5. Library (tlib) — uses tstring
 *===========================================================================*/

static const char *tlib_get_type(void)
{
	return "Library";
}

static tcompo_type tlib_get_code(void)
{
	return compo_tlib;
}

static long tlib_len(void *self)
{
	tlib *l = (tlib *)self;
	return (long)tobj_array_get_len(&l->env.base.objs);
}

static void *tlib_copy(void *self)
{
	(void)self;
	twarn(ErrRuntime_Other, "tlib_copy", "tlib can not be copied.");
	return NULL;
}

static void tlib_free(void *self)
{
	tlib *l = (tlib *)self;
	if (l->wrapper)
		tanalyser_clean_wrapper(l->wrapper);
	if (l->exposed &&
	    tobj_array_get_ref_obj_loc(&l->env.base.objs,
				       (tcompo_v *)l->exposed) == UNDEF_ENVLOC)
		l->exposed->base.vtable->free(l->exposed);
	tcompo_env_set_regmax(&l->env, 0);
	tobj_array_free(&l->env.base.objs);

	{
		uint_objs i;
		for (i = 0; i < l->ndefault; i++)
			tstring_free(l->default_v_names[i]);
		free(l->default_v_names);
	}
	{
		uint_lexs i;
		for (i = 0; i < l->npaths; i++)
			tstring_free(l->paths[i]);
		free(l->paths);
	}
	free(l);
}

static int tlib_identical(void *self, void *other)
{
	return self == other;
}

static tstring *tlib_tostring_abbr(void *self)
{
	return tobj_tostring_pointer("Library", self);
}

static tstring *tlib_tostring_full(void *self)
{
	tlib *lb = (tlib *)self;
	if (lb->exposed)
		return lb->exposed->base.vtable->tostring_full(lb->exposed);
	return tstring_new("{}");
}

tcompo_vtable tlib_vtable = {
	tlib_get_type,
	tlib_get_code,
	tlib_len,
	tlib_copy,
	tlib_free,
	tlib_identical,
	tlib_tostring_abbr,
	tlib_tostring_full
};

tlib *tlib_new(void)
{
	tlib *lb = (tlib *)calloc(1, sizeof(tlib));
	lb->compo_base.vtable = &tlib_vtable;
	tcompo_env_init(&lb->env, 0, NULL, 0, 0, 0, compo_tlib);
	lb->default_v_names = NULL;
	lb->ndefault = 0;
	lb->ndefault_cap = 0;
	lb->paths = NULL;
	lb->npaths = 0;
	lb->npaths_cap = 0;
	lb->wrapper = NULL;
	lb->exposed = NULL;
	return lb;
}

/* ---- Library methods ---- */

void tlib_set_wrapper(tlib *lb, twrapper *wrapper)
{
	if (lb->wrapper)
		tanalyser_clean_wrapper(lb->wrapper);
	lb->wrapper = wrapper;
	if (wrapper) {
		tcompo_env_try_expand_objlist(&lb->env, wrapper->info.obj_max);
		tcompo_env_set_tmpmax(&lb->env, wrapper->info.tmp_max);
		tcompo_env_set_regmax(&lb->env, wrapper->info.reg_max);
	}
}

twrapper *tlib_get_wrapper(tlib *lb)
{
	return lb->wrapper;
}

tdict *tlib_get_exposed(tlib *lb)
{
	return lb->exposed;
}

void tlib_set_exposed(tlib *lb, tdict *d)
{
	lb->exposed = d;
}

void tlib_lib_add_obj(tlib *lb, const char *name, const tobj *v)
{
	uint_objs current_len = lb->ndefault;
	tcompo_env_try_expand_objlist(&lb->env, current_len + 10);
	tcompo_env_add_obj(&lb->env, UNDEF_NAMELOC);
	tcompo_env_set_obj(&lb->env, current_len, v);

	if (lb->ndefault >= lb->ndefault_cap) {
		lb->ndefault_cap = lb->ndefault_cap ? lb->ndefault_cap * 2 : 16;
		lb->default_v_names = (tstring **)realloc(
			lb->default_v_names, lb->ndefault_cap * sizeof(tstring *));
	}
	lb->default_v_names[lb->ndefault] = tstring_new(name);
	lb->ndefault++;
}

tdict *tlib_add_pkg(tlib *lb, const char *pkgname)
{
	uint_objs i;
	for (i = 0; i < lb->ndefault; i++) {
		if (tstring_eq_cstr(lb->default_v_names[i], pkgname))
			twarn(ErrRuntime_Other, "tlib_add_pkg", pkgname);
	}
	tdict *pkg = tdict_new();
	tobj v;
	tobj_set_nil(&v);
	tobj_set_compo(&v, (tcompo_v *)pkg);
	tlib_lib_add_obj(lb, pkgname, &v);
	return pkg;
}

void tlib_add_cppf(tlib *lb, const char *name, genf_t f, uint_regs nparams_sig)
{
	tobj v;
	tobj_set_nil(&v);
	tobj_set_compo(&v, (tcompo_v *)tcppgenf_new(f, name, nparams_sig));
	tlib_lib_add_obj(lb, name, &v);
}

void tlib_add_path(tlib *lb, const char *paths_str)
{
	uint_lexs i;
	uint_lexs len = (uint_lexs)strlen(paths_str);
	uint_lexs loc = 0;

	for (i = 0; i < len; i++) {
		if (paths_str[i] != ';')
			continue;
		uint_lexs path_len = i - loc;
		if (path_len == 0) {
			loc = i + 1;
			continue;
		}
		tstring *path = tstring_new_len(paths_str + loc, path_len);

		/* Check if already exists */
		int exists = 0;
		uint_lexs j;
		for (j = 0; j < lb->npaths; j++) {
			if (tstring_cmp(lb->paths[j], path) == 0) {
				exists = 1;
				break;
			}
		}
		if (!exists) {
			if (lb->npaths >= lb->npaths_cap) {
				lb->npaths_cap =
					lb->npaths_cap ? lb->npaths_cap * 2 : 8;
				lb->paths = (tstring **)realloc(
					lb->paths,
					lb->npaths_cap * sizeof(tstring *));
			}
			lb->paths[lb->npaths++] = path;
		} else {
			tstring_free(path);
		}
		loc = i + 1;
	}
	if (loc < len) {
		uint_lexs path_len = len - loc;
		tstring *path = tstring_new_len(paths_str + loc, path_len);

		int exists = 0;
		uint_lexs j;
		for (j = 0; j < lb->npaths; j++) {
			if (tstring_cmp(lb->paths[j], path) == 0) {
				exists = 1;
				break;
			}
		}
		if (!exists) {
			if (lb->npaths >= lb->npaths_cap) {
				lb->npaths_cap =
					lb->npaths_cap ? lb->npaths_cap * 2 : 8;
				lb->paths = (tstring **)realloc(
					lb->paths,
					lb->npaths_cap * sizeof(tstring *));
			}
			lb->paths[lb->npaths++] = path;
		} else {
			tstring_free(path);
		}
	}
}

tstring **tlib_get_paths(tlib *lb)
{
	return lb->paths;
}

uint_lexs tlib_get_npaths(tlib *lb)
{
	return lb->npaths;
}

/** Get default variable names vector (as array of tstring*) */
tstring **tlib_get_default_v_names(tlib *lb)
{
	return lb->default_v_names;
}

uint_objs tlib_get_ndefault(tlib *lb)
{
	return lb->ndefault;
}

/** Recreate: copy only default objects (C functions) */
tlib *tlib_recreate(tlib *lb)
{
	tlib *lib_rct = tlib_new();
	uint_objs i;
	for (i = 0; i < lb->ndefault; i++) {
		tlib_lib_add_obj(lib_rct,
				 tstring_cstr(lb->default_v_names[i]),
				 tobj_array_get_obj(&lb->env.base.objs, i));
	}
	return lib_rct;
}

/** lib::object - Get the object from library (exposed dict) */
void tlib_idx(tlib *lb, const tobj *params, uint_regs np, tobj *vre)
{
	if (lb->exposed)
		tdict_idx(lb->exposed, params, np, vre);
	else
		tobj_set_nil(vre);
}

/** List object names */
tlist *tlib_listing_objects(tlib *lb)
{
	tlist *ls = tlist_new();
	uint_objs i;

	/* Exposed dict keys */
	if (lb->exposed) {
		tlist *eks = tdict_keys(lb->exposed);
		for (i = 0; i < tlist_size(eks); i++)
			tlist_push(ls, tlist_at(eks, i));
		eks->base.vtable->free(eks);
	}

	/* Preload names */
	for (i = 0; i < lb->ndefault; i++) {
		tobj v;
		tobj_set_nil(&v);
		tobj_set_compo(&v,
			       (tcompo_v *)tstr_new(tstring_cstr(lb->default_v_names[i])));
		tlist_push(ls, &v);
		tobj_try_clear(&v);
	}

	/* Non-preload object names */
	for (i = lb->ndefault; i < tobj_array_get_len(&lb->env.base.objs);
	     i++) {
		tobj *vi = tobj_array_get_obj(&lb->env.base.objs, i);
		if (lb->wrapper && vi->name_loc >= 0 &&
		    (uint_csts)vi->name_loc < lb->wrapper->consts.ncstrs) {
			tobj v;
			tobj_set_nil(&v);
			tobj_set_compo(&v,
				       (tcompo_v *)tstr_new(
					       tstring_cstr(lb->wrapper->consts
						       .cstrs[vi->name_loc])));
			tlist_push(ls, &v);
			tobj_try_clear(&v);
		}
	}
	return ls;
}

/**
 * Get the library from any environment in its tree.
 * Follows the env tree to the tlib root and uses offsetof to correctly cast
 * from the embedded tcompo_env back to the host tlib.
 */
tlib *tlib_from_env(tcompo_env *env)
{
	while (env->base.father_env)
		env = (tcompo_env *)env->base.father_env;
	return (tlib *)((char *)env - offsetof(tlib, env));
}

/**
 * Get the twrapper from the top-level environment.
 * Follows the env tree to the tlib root.
 */
twrapper *tfunc_get_wrapper_from_env(tcompo_env *env)
{
	return tlib_from_env(env)->wrapper;
}

/** List paths */
tlist *tlib_listing_paths(tlib *lb)
{
	tlist *paths = tlist_new();
	uint_lexs i;
	for (i = 0; i < lb->npaths; i++) {
		tobj v;
		tobj_set_nil(&v);
		tobj_set_compo(&v, (tcompo_v *)tstr_new(tstring_cstr(lb->paths[i])));
		tlist_push(paths, &v);
		tobj_try_clear(&v);
	}
	return paths;
}
