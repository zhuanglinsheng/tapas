#include "tapas/textension.h"

#include "tapas/tbycs.h"
#include "tapas/tenv.h"
#include "tapas/runtime/tlist.h"

#include <stdio.h>


void lib_ls(tobj *params, uint_regs len, tobj *vre, tcompo_env *env)
{
	if (len != 1 && len != 0)
		twarn(ErrRuntime_ParamsCtr, "lib_ls", "");

	if (len == 1) {
		if (params->type != tcompo)
			twarn(ErrRuntime_ParamsType, "lib_ls", "");
		if (params->val.v_tcompo->vtable->get_compo_type_code() !=
		    compo_tlib)
			twarn(ErrRuntime_ParamsType, "lib_ls", "");
		tlib *lib = (tlib *)params->val.v_tcompo;
		tlist *ls = tlib_listing_objects(lib);
		tobj_set_compo(vre, (tcompo_v *)ls);
	}
	if (len == 0) {
		if (env->base.father_env != nullptr)
			twarn(ErrRuntime_RefType,
			      "lib_ls",
			      "tlib supported only");
		tlib *lib = tlib_from_env(env);
		tlist *ls = tlib_listing_objects(lib);
		tobj_set_compo(vre, (tcompo_v *)ls);
	}
}

/** path([lib]): list library paths */
void lib_path(tobj *params, uint_regs len, tobj *vre, tcompo_env *env)
{
	if (len != 0 && len != 1)
		twarn(ErrRuntime_ParamsCtr, "lib_ls", "");
	if (len == 1) {
		if (params->type != tcompo)
			twarn(ErrRuntime_ParamsType, "lib_ls", "");
		if (params->val.v_tcompo->vtable->get_compo_type_code() != compo_tlib)
			twarn(ErrRuntime_ParamsType, "lib_ls", "");
		tlib *lib = (tlib *)params->val.v_tcompo;
		tlist *paths = tlib_listing_paths(lib);
		tobj_set_compo(vre, (tcompo_v *)paths);
	}
	if (len == 0) {
		if (env->base.father_env != nullptr)
			twarn(ErrRuntime_EnvInconsis, "lib_path", "");
		tlib *lib = tlib_from_env(env);
		tlist *paths = tlib_listing_paths(lib);
		tobj_set_compo(vre, (tcompo_v *)paths);
	}
}

/** __param__(idx): used in tap functions, return the value of the idx-th
 * param */
void tf_param(tobj *params, uint_regs len, tobj *vre, tcompo_env *env)
{
	if (env->base.father_env == nullptr)
		twarn(ErrRuntime_EnvInconsis, "tf_param", "");
	if (len != 1)
		twarn(ErrRuntime_ParamsCtr, "tf_param", "1 parameter");
	if (params->type != tint)
		twarn(ErrRuntime_ParamsType,
		      "tf_param",
		      "integer as parameter");
	long idx = params->val.v_tint;
	if (idx < 0 || idx >= (long)tcompo_env_get_dynamic_nparams(env))
		twarn(ErrRuntime_IdxOutRange, "tf_param", "");
	tobj_copy(vre, &tcompo_env_get_params(env)[idx]);
}

/** __nparam__(): used in tap functions, return the number of params */
void tf_nparam(tobj *params, uint_regs len, tobj *vre, tcompo_env *env)
{
	(void)params;
	if (env->base.father_env == nullptr)
		twarn(ErrRuntime_EnvInconsis, "tf_nparam", "");
	if (len != 0 || params->type != tcompo)
		twarn(ErrRuntime_ParamsCtr, "tf_nparam", "0 parameter");
	tobj_set_int(vre, (long)tcompo_env_get_dynamic_nparams(env));
}

static void display_wrapper_range(const twrapper *wrapper, uint_cmds from, uint_cmds ncmds)
{
	uint_cmds i;
	char buf[128];
	if (!wrapper)
		twarn(ErrRuntime_Other, "display_wrapper_range", "bytecode wrapper unavailable");
	if (from > wrapper->ncmds || ncmds > wrapper->ncmds - from)
		twarn(ErrRuntime_Other, "display_wrapper_range", "invalid bytecode range");
	for (i = from; i < from + ncmds; i++) {
		tbycode_tostring(wrapper->cmdarr[i], buf, sizeof(buf));
		printf("[%u]%s\n", (unsigned)i, buf);
	}
	printf("Max Obj. Number: %u\n", (unsigned)wrapper->info.obj_max);
	printf("Max Tmp. Number: %u\n", (unsigned)wrapper->info.tmp_max);
	printf("Max Reg. Number: %u\n", (unsigned)wrapper->info.reg_max);
	printf("Const Value List (Integers): ");
	for (i = 0; i < wrapper->consts.ncints; i++) {
		printf("%li", wrapper->consts.cints[i]);
		if (i < wrapper->consts.ncints - 1)
			printf(", ");
	}
	printf("\n");
	printf("Const Value List (Double Floats): ");
	for (i = 0; i < wrapper->consts.ncflts; i++) {
		printf("%f", wrapper->consts.cflts[i]);
		if (i < wrapper->consts.ncflts - 1)
			printf(", ");
	}
	printf("\n");
	printf("Const Value List (Character Strings): ");
	for (i = 0; i < wrapper->consts.ncstrs; i++) {
		printf("%s", tstring_cstr(wrapper->consts.cstrs[i]));
		if (i < wrapper->consts.ncstrs - 1)
			printf(", ");
	}
	printf("\n");
}

static void display_wrapper_full(const twrapper *wrapper)
{
	if (!wrapper)
		twarn(ErrRuntime_Other, "display_wrapper_full", "bytecode wrapper unavailable");
	display_wrapper_range(wrapper, 0, wrapper->ncmds);
}

/** __binary__([lib_or_function]): display bytecode for an environment value */
void tf_binary(tobj *params, uint_regs len, tobj *vre, tcompo_env *env)
{
	twrapper *wrapper;
	if (len > 1)
		twarn(ErrRuntime_ParamsCtr, "tf_binary", "0 or 1 parameter");
	if (len == 0) {
		wrapper = tfunc_get_wrapper_from_env(env);
		display_wrapper_full(wrapper);
		tobj_set_nil(vre);
		return;
	}
	if (params[0].type != tcompo)
		twarn(ErrRuntime_ParamsType, "tf_binary", "Library or Function");
	tcompo_v *compo = params[0].val.v_tcompo;
	switch (compo->vtable->get_compo_type_code()) {
	case compo_tlib: {
		tlib *lib = (tlib *)compo;
		wrapper = tlib_get_wrapper(lib);
		display_wrapper_full(wrapper);
	} break;
	case compo_tfunc: {
		tfunc *func = (tfunc *)compo;
		wrapper = tfunc_get_wrapper_from_env(tfunc_get_env(func));
		display_wrapper_range(wrapper, tfunc_get_cmdloc(func), tfunc_get_ncmds(func));
	} break;
	default:
		twarn(ErrRuntime_ParamsType, "tf_binary", "Library or Function");
	}
	tobj_set_nil(vre);
}

static const textension_symbol symbols[] = {
	{
		.name = "__ls__",
		.type = "Function[...] -> List[String]",
		.detail = "__ls__([library: Library]) -> List[String]",
		.kind = textension_function,
		.session_function = lib_ls,
		.minimum_arguments = 0,
		.maximum_arguments = 1
	},
	{
		.name = "__path__",
		.type = "Function[...] -> List[String]",
		.detail = "__path__([library: Library]) -> List[String]",
		.kind = textension_function,
		.session_function = lib_path,
		.minimum_arguments = 0,
		.maximum_arguments = 1
	},
	{
		.name = "__param__",
		.type = "Function[Int] -> AnyType",
		.detail = "__param__(index: Int) -> AnyType",
		.kind = textension_function,
		.session_function = tf_param,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "__nparam__",
		.type = "Function[] -> Int",
		.detail = "__nparam__() -> Int",
		.kind = textension_function,
		.session_function = tf_nparam,
		.minimum_arguments = 0,
		.maximum_arguments = 0
	},
	{
		.name = "__binary__",
		.type = "Function[...] -> Nil",
		.detail = "__binary__([environment: AnyType]) -> Nil",
		.kind = textension_function,
		.session_function = tf_binary,
		.minimum_arguments = 0,
		.maximum_arguments = 1
	}
};

const textension_module tstdlib_session_functions = {
	.scope = textension_root,
	.symbols = symbols,
	.symbol_count = sizeof(symbols) / sizeof(symbols[0])
};
