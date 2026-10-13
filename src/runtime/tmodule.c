#include "tmodule.h"

#include "compile/cache.h"
#include "compile/compiler.h"
#include "tapas/objects/tdict.h"
#include "tvm.h"

tlib *tmodule_load(tcompo_env *environment, const tstring *source)
{
	tlib *current = tlib_from_env(environment);
	tlib *module = tlib_recreate(current);
	if (!tlib_get_build_root(current))
		tcompile_cache_configure(current, source,
			tlib_get_build_interactive(current));
	if (!tlib_get_build_root(module))
		tcompile_cache_configure(module, source,
			tlib_get_build_interactive(current));

	twrapper *wrapper = tcompile_cache_load_current(current, source);
	if (!wrapper) {
		tcp *compiler = tcp_new_library(
			current, tlib_get_build_interactive(current));
		wrapper = compile_file(compiler, source,
			tlib_get_paths(current), tlib_get_npaths(current));
		tcp_delete(compiler);
		if (!wrapper)
			twarn(ErrSession_IO, "module import", tstring_cstr(source));
		(void)tcompile_cache_save(current, source, wrapper);
	}

	tlib_set_wrapper(module, wrapper);
	tvm nested;
	tvm_init(&nested, wrapper->info.tmp_max);
	tvm_set_vmstack(&nested, tcompo_env_get_vmstack(&module->env),
		wrapper->info.reg_max);
	exec_tins(&nested, 0, wrapper->ncmds, &module->env);
	tobj returned = tvm_take_result(&nested);
	tvm_destroy(&nested);

	if (returned.type == tcompo &&
	    tobj_compo_type(&returned) == compo_tdict) {
		tlib_set_exposed(module, (tdict *)returned.val.v_tcompo);
	} else {
		tobj_try_clear(&returned);
		tlib_set_exposed(module, tdict_new());
	}
	return module;
}
