#include "Tapas/tapas.h"
#include "tapas/runtime/tdict.h"
#include "tapas/runtime/tlist.h"
#include "Tapas/tvm.h"

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*===========================================================================*
 * 1. Session-Level Functions
 *===========================================================================*/

/** ls(lib): list library variables */
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
		if (env->base.father_env != NULL)
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
		if (env->base.father_env != NULL)
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
	if (env->base.father_env == NULL)
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
	if (env->base.father_env == NULL)
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
		tbycode_tostring(wrapper->cmdarr[i], buf);
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

/** Register session-level functions to a library */
void register_os_sessf(tlib *lib)
{
	tobj v;
	tobj_set_nil(&v);
#define TAPAS_ROOT(name, implementation, arity, signature)
#define TAPAS_SESSION(name, implementation, signature) do { \
		tobj_set_compo(&v, (tcompo_v *)tcppsessf_new(implementation, #name)); \
		tlib_lib_add_obj(lib, #name, &v); \
	} while (0);
#define TAPAS_PACKAGE(name)
#define TAPAS_MEMBER(package, name, implementation, arity, signature)
#define TAPAS_TYPE(name, builtin, signature)
#include "stdlib.def"
#undef TAPAS_ROOT
#undef TAPAS_SESSION
#undef TAPAS_PACKAGE
#undef TAPAS_MEMBER
#undef TAPAS_TYPE
	tobj_try_clear(&v);
}


/*===========================================================================*
 * 2. Session Management
 *===========================================================================*/

/** Create a new session with default functions registered */
tsession *tsession_new(void)
{
	tsession *sess = (tsession *)malloc(sizeof(tsession));
	sess->lib = tlib_new();
	register_cppfuncs(sess->lib);
	register_os_sessf(sess->lib);
	return sess;
}

/** Free a session */
void tsession_free(tsession *sess)
{
	if (sess) {
		sess->lib->compo_base.vtable->free(sess->lib);
		free(sess);
	}
}

tlib *tsession_get_lib(tsession *sess)
{
	return sess->lib;
}

/** Compile a .tap file to .tapc */
void tsession_compile_file(tsession *sess, const char *file, int interactive)
{
	terror_set_file_context(file, 0, 0);
	tcp cp;
	tstring **default_names = tlib_get_default_v_names(sess->lib);
	uint_objs ndef = tlib_get_ndefault(sess->lib);
	tcp_init_preload(&cp, default_names, ndef, NULL, interactive);
	tstring **paths = tlib_get_paths(sess->lib);
	uint_lexs npaths = tlib_get_npaths(sess->lib);
	tstring *file_ts = tstring_new(file);
	compile_file_save(&cp, file_ts, paths, npaths);
	tstring_free(file_ts);
	tcp_free(&cp);
	(void)interactive;
}

/** Evaluate compiled .tapc binary */
void tsession_eval_bycodes(tsession *sess, const char *file)
{
	terror_set_file_context(file, 0, 0);
	char *dot = strrchr(file, '.');
	size_t baselen = dot ? (size_t)(dot - file) : strlen(file);
	char *binf = (char *)malloc(baselen + 6);
	memcpy(binf, file, baselen);
	strcpy(binf + baselen, ".tapc");

	twrapper *w = tanalyser_load_bin_file(binf);
	free(binf);
	if (!w)
		return;

	tlib_set_wrapper(sess->lib, w);
	tvm vm;
	tvm_init(&vm, w->info.tmp_max);
	tvm_set_vmstack(&vm,
				tcompo_env_get_vmstack(&sess->lib->env),
				w->info.reg_max);
	eval_bycodes(&vm, 0, sess->lib);
	tvm_clean(&vm);
	tvm_set_vmstack(&vm, NULL, 0);
}

/** Compile & execute a .tap file without saving .tapc */
void tsession_execute_file(tsession *sess, const char *file, int interactive)
{
	terror_set_file_context(file, 0, 0);
	tcp cp;
	tstring **default_names = tlib_get_default_v_names(sess->lib);
	uint_objs ndef = tlib_get_ndefault(sess->lib);
	tcp_init_preload(&cp, default_names, ndef, NULL, interactive);
	(void)interactive;

	tstring **paths = tlib_get_paths(sess->lib);
	uint_lexs npaths = tlib_get_npaths(sess->lib);

	tstring *file_ts = tstring_new(file);
	twrapper *wrapper = compile_file(&cp, file_ts, paths, npaths);
	tstring_free(file_ts);
	if (!wrapper) {
		tcp_free(&cp);
		return;
	}

	tlib_set_wrapper(sess->lib, wrapper);
	tvm vm;
	tvm_init(&vm, wrapper->info.tmp_max);
	tvm_set_vmstack(&vm,
				tcompo_env_get_vmstack(&sess->lib->env),
				wrapper->info.reg_max);
	eval_bycodes(&vm, 0, sess->lib);
	tvm_clean(&vm);
	tvm_set_vmstack(&vm, NULL, 0);
	tcp_free(&cp);
}

/** Compile & evaluate a string */
void tsession_execute_str(tsession *sess, const char *str, int interactive)
{
	tcp cp;
	tstring **default_names = tlib_get_default_v_names(sess->lib);
	uint_objs ndef = tlib_get_ndefault(sess->lib);

	tcp_init_preload(&cp, default_names, ndef, NULL, interactive);

	tconsts consts;
	tconsts_init(&consts);
	tvmcmd_vect tcmds;
	tvmcmd_vect_init(&tcmds);

	tstring **paths = tlib_get_paths(sess->lib);
	uint_lexs npaths = tlib_get_npaths(sess->lib);

	tstring *src = tstring_new(str);
	tcinfo info = parse_blk(&cp, src, &tcmds, &consts, paths, npaths, 1, 0);
	tstring_free(src);
	twrapper *wrapper = tanalyser_wrap(&tcmds, &consts, &info);

	tlib_set_wrapper(sess->lib, wrapper);
	tvm vm;
	tvm_init(&vm, info.tmp_max);
	tvm_set_vmstack(&vm,
				tcompo_env_get_vmstack(&sess->lib->env),
				info.reg_max);
	eval_bycodes(&vm, 0, sess->lib);
	tvm_clean(&vm);
	tvm_set_vmstack(&vm, NULL, 0);

	tvmcmd_vect_free(&tcmds);
	tconsts_free(&consts);
	tcp_free(&cp);
}

static int md_line_is_fence(const tstring *line, const char **lang)
{
	const char *s = tstring_cstr(line);
	while (*s == ' ' || *s == '\t')
		s++;
	if (strncmp(s, "```", 3) != 0)
		return 0;
	s += 3;
	while (*s == ' ' || *s == '\t')
		s++;
	*lang = s;
	return 1;
}

static int md_lang_is_tapas(const char *lang)
{
	return strncmp(lang, "tapas", 5) == 0 || strncmp(lang, "tap", 3) == 0;
}

static int md_line_is_tapas_return_open(const tstring *line)
{
	tstring *trimmed = tstring_dup(line);
	tstring_trim(trimmed);
	int ok = tstring_eq_cstr(trimmed, "<pre class='Tapas-Return'>");
	tstring_free(trimmed);
	return ok;
}

static int md_line_is_pre_close(const tstring *line)
{
	tstring *trimmed = tstring_dup(line);
	tstring_trim(trimmed);
	int ok = tstring_eq_cstr(trimmed, "</pre>");
	tstring_free(trimmed);
	return ok;
}

static int md_line_is_blank(const tstring *line)
{
	tstring *trimmed = tstring_dup(line);
	tstring_trim(trimmed);
	int ok = tstring_empty(trimmed);
	tstring_free(trimmed);
	return ok;
}

static tstring *read_stream_all(FILE *f)
{
	tstring *out = tstring_new_empty();
	char buf[4096];
	size_t n;
	rewind(f);
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
		tstring_append_len(out, buf, n);
	return out;
}

static int execute_block_capture(
		tsession *sess,
		tcp *syner,
		tvm *vm,
		tvmcmd_vect *tcmds,
		tconsts *consts,
		tcinfo *info,
		const char *file,
		uint64_t start_line,
		const tstring *code,
		tstring **stdout_out,
		tstring **stderr_out)
{
	FILE *outf = tmpfile();
	FILE *errf = tmpfile();
	if (!outf || !errf)
		twarn(ErrSession_IO, "execute_str_capture", "tmpfile");

	fflush(stdout);
	fflush(stderr);
	int stdout_fd = dup(STDOUT_FILENO);
	int stderr_fd = dup(STDERR_FILENO);
	if (stdout_fd < 0 || stderr_fd < 0)
		twarn(ErrSession_IO, "execute_str_capture", "dup");

	dup2(fileno(outf), STDOUT_FILENO);
	dup2(fileno(errf), STDERR_FILENO);

	int failed = 0;
	if (setjmp(tapas_error_jmpbuf) != 0) {
		failed = 1;
		tvm_clean(vm);
		tvm_set_vmstack(vm, NULL, 0);
	} else {
		tapas_error_recover_enabled = 1;
		tstring **paths = tlib_get_paths(sess->lib);
		uint_lexs npaths = tlib_get_npaths(sess->lib);
		uint_cmds ncmd_old = tvmcmd_vect_size32(tcmds);
		terror_set_source_context(tstring_cstr(code));
		terror_set_file_context(file, start_line, 0);
		*info = parse_unit(syner, code, tcmds, consts, paths, npaths, 1, 0);
		twrapper *wrapper = tanalyser_wrap(tcmds, consts, info);
		if (wrapper) {
			tlib_set_wrapper(sess->lib, wrapper);
			tvm_set_tmpmax(vm, info->tmp_max);
			tvm_set_vmstack(vm,
					tcompo_env_get_vmstack(&sess->lib->env),
					info->reg_max);
			eval_bycodes(vm, ncmd_old, sess->lib);
			tvm_clean(vm);
			tvm_set_vmstack(vm, NULL, 0);
		}
	}
	tapas_error_recover_enabled = 0;

	fflush(stdout);
	fflush(stderr);
	dup2(stdout_fd, STDOUT_FILENO);
	dup2(stderr_fd, STDERR_FILENO);
	close(stdout_fd);
	close(stderr_fd);

	*stdout_out = read_stream_all(outf);
	*stderr_out = read_stream_all(errf);
	fclose(outf);
	fclose(errf);
	return failed ? 0 : 1;
}

static void append_tapas_return_block(tstring *out, const tstring *captured)
{
	if (tstring_empty(captured))
		return;
	tstring_append(out, "<pre class='Tapas-Return'>\n");
	tstring_append_ts(out, captured);
	if (!tstring_empty(captured) &&
	    tstring_at(captured, tstring_len(captured) - 1) != '\n')
		tstring_append_c(out, '\n');
	tstring_append(out, "</pre>\n");
}

static tstring *session_path_dirname(const char *file)
{
	const char *slash = strrchr(file, '/');
	if (!slash)
		return tstring_new(".");
	if (slash == file)
		return tstring_new("/");
	return tstring_new_len(file, (size_t)(slash - file));
}

/** Execute markdown and update Tapas return blocks in place */
void tsession_execute_markdown_update(tsession *sess, const char *file, int interactive)
{
	(void)interactive;
	FILE *f = fopen(file, "r");
	if (!f)
		twarn(ErrCompile_UnfoundFile, "tsession_execute_markdown_update", file);

	tstring **lines = NULL;
	uint32_t nlines = 0;
	uint32_t cap = 0;
	tstring *line = tstring_new_empty();
	while (tstring_getline(line, f) != SIZE_MAX) {
		if (nlines >= cap) {
			cap = cap ? cap * 2 : 128;
			lines = (tstring **)realloc(lines, cap * sizeof(tstring *));
			if (!lines)
				twarn(ErrRuntime_Other, "tsession_execute_markdown_update", "out of memory");
		}
		lines[nlines++] = tstring_dup(line);
		if (tstring_empty(lines[nlines - 1]) ||
		    tstring_at(lines[nlines - 1], tstring_len(lines[nlines - 1]) - 1) != '\n')
			tstring_append_c(lines[nlines - 1], '\n');
	}
	tstring_free(line);
	fclose(f);

	tstring *file_dir = session_path_dirname(file);
	tlib_add_path(sess->lib, tstring_cstr(file_dir));
	tstring_free(file_dir);

	tstring *updated = tstring_new_empty();
	int failed = 0;
	tcp syner;
	tstring **default_names = tlib_get_default_v_names(sess->lib);
	uint_objs ndef = tlib_get_ndefault(sess->lib);
	tcp_init_preload(&syner, default_names, ndef, NULL, 1);
	tvmcmd_vect tcmds;
	tvmcmd_vect_init(&tcmds);
	tconsts consts;
	tconsts_init(&consts);
	tcinfo info;
	memset(&info, 0, sizeof(info));
	tvm vm_storage;
	tvm_init(&vm_storage, 0);
	tvm *vm = &vm_storage;

	for (uint32_t i = 0; i < nlines;) {
		const char *lang = NULL;
		if (!md_line_is_fence(lines[i], &lang) || !md_lang_is_tapas(lang)) {
			tstring_append_ts(updated, lines[i++]);
			continue;
		}

		uint32_t fence_line = i + 1;
		uint64_t code_start_line = (uint64_t)fence_line + 1;
		tstring *code = tstring_new_empty();
		tstring_append_ts(updated, lines[i++]);
		while (i < nlines) {
			const char *end_lang = NULL;
			tstring_append_ts(updated, lines[i]);
			if (md_line_is_fence(lines[i], &end_lang)) {
				i++;
				break;
			}
			tstring_append_ts(code, lines[i]);
			i++;
		}

		uint32_t after_block = i;
		while (after_block < nlines && md_line_is_blank(lines[after_block]))
			after_block++;
		if (after_block < nlines &&
		    md_line_is_tapas_return_open(lines[after_block])) {
			i = after_block + 1;
			while (i < nlines && !md_line_is_pre_close(lines[i]))
				i++;
			if (i < nlines)
				i++;
		}

		tstring *captured_out = NULL;
		tstring *captured_err = NULL;
		if (!execute_block_capture(sess,
					   &syner,
					   vm,
					   &tcmds,
					   &consts,
					   &info,
					   file,
					   code_start_line,
					   code,
					   &captured_out,
					   &captured_err)) {
			tstring *err = tstring_empty(captured_err) ? captured_out : captured_err;
			printf("%s", tstring_cstr(err));
			failed = 1;
			tstring_free(captured_out);
			tstring_free(captured_err);
			tstring_free(code);
			break;
		}
		append_tapas_return_block(updated, captured_out);
		tstring_free(captured_out);
		tstring_free(captured_err);
		tstring_free(code);
	}

	if (!failed) {
		FILE *wf = fopen(file, "w");
		if (!wf)
			twarn(ErrSession_IO, "tsession_execute_markdown_update", file);
		tstring_print(updated, wf);
		fclose(wf);
	}

	for (uint32_t i = 0; i < nlines; i++)
		tstring_free(lines[i]);
	free(lines);
	tstring_free(updated);
	tvmcmd_vect_free(&tcmds);
	tconsts_free(&consts);
	tcp_free(&syner);
	tvm_clean(&vm_storage);
}

/** Show bycodes of compiled .tapc file */
void tsession_show_bycodes(tsession *sess, const char *file)
{
	(void)sess;
	char *dot = strrchr(file, '.');
	size_t baselen = dot ? (size_t)(dot - file) : strlen(file);
	char *binf = (char *)malloc(baselen + 6);
	memcpy(binf, file, baselen);
	strcpy(binf + baselen, ".tapc");

	twrapper *w = tanalyser_load_bin_file(binf);
	free(binf);
	if (w) {
		tanalyser_display_wrapper(w);
		tanalyser_clean_wrapper(w);
	}
}

/** Add a file-searching path */
void tsession_add_path(tsession *sess, const char *path)
{
	tlib_add_path(sess->lib, path);
}

/** Add a package (dict) to the session */
tdict *tsession_add_pkg(tsession *sess, const char *pkgname)
{
	return tlib_add_pkg(sess->lib, pkgname);
}

/** Get a Tap object from the session's root library */
tobj *tsession_get_obj(tsession *sess, uint_objs loc)
{
	uint_objs len = tobj_array_get_len(&sess->lib->env.base.objs);
	if (loc >= len)
		twarn(ErrRuntime_IdxOutRange, "tsession_get_obj", "");
	uint_objs rloc = len - loc - 1;
	return tobj_array_get_obj(&sess->lib->env.base.objs, rloc);
}
