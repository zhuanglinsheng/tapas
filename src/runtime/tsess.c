#include "tapas/tsession.h"
#include "tapas/dsa/tstring.h"
#include "tapas/tstdlib.h"
#include "tapas/objects/tdict.h"
#include "tvm.h"
#include "compile/compiler.h"
#include "compile/frontend/workspace.h"
#include "tapas/objects/tlist.h"
#include "tapas/objects/tstr.h"

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct tsession {
	tlib *lib;
};

/*===========================================================================*
 * 2. Session Management
 *===========================================================================*/

/** Create a new session with default functions registered */
tsession *tsession_new(void)
{
	tsession *sess = (tsession *)malloc(sizeof(tsession));
	sess->lib = tlib_new();
	if (!tlib_install_extension(sess->lib, tstdlib_descriptor()))
		twarn(ErrRuntime_Other, "tsession_new",
		      "cannot install the standard library");
#ifdef TAPAS_SOURCE_STDLIB_DIR
	tlib_add_path(sess->lib, TAPAS_SOURCE_STDLIB_DIR);
#endif
#ifdef TAPAS_INSTALL_STDLIB_DIR
	tlib_add_path(sess->lib, TAPAS_INSTALL_STDLIB_DIR);
#endif
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
	tcp *cp = tcp_new_library(sess->lib, interactive);
	tstring **paths = tlib_get_paths(sess->lib);
	uint_lexs npaths = tlib_get_npaths(sess->lib);
	tstring *file_ts = tstring_new(file);
	compile_file_save(cp, file_ts, paths, npaths);
	tstring_free(file_ts);
	tcp_delete(cp);
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
	tvm_set_vmstack(&vm, nullptr, 0);
}

/** Compile & execute a .tap file without saving .tapc */
void tsession_execute_file(tsession *sess, const char *file, int interactive)
{
	terror_set_file_context(file, 0, 0);
	tcp *cp = tcp_new_library(sess->lib, interactive);
	(void)interactive;

	tstring **paths = tlib_get_paths(sess->lib);
	uint_lexs npaths = tlib_get_npaths(sess->lib);

	tstring *file_ts = tstring_new(file);
	twrapper *wrapper = compile_file(cp, file_ts, paths, npaths);
	tstring_free(file_ts);
	if (!wrapper) {
		tcp_delete(cp);
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
	tvm_set_vmstack(&vm, nullptr, 0);
	tcp_delete(cp);
}

static tstring *resolve_module(tlib *library, const char *module)
{
	tstring **paths = tlib_get_paths(library);
	uint_lexs count = tlib_get_npaths(library);
	const char **roots = count ?
		(const char **)calloc(count, sizeof(*roots)) : nullptr;
	if (count && !roots)
		twarn(ErrRuntime_Other, "module", "out of memory");
	for (uint_lexs i = 0; i < count; i++)
		roots[i] = tstring_cstr(paths[i]);
	tstring *resolved = tworkspace_resolve_module_file(module, roots, count);
	free(roots);
	return resolved;
}

static tlist *module_arguments(int count, const char *const *arguments)
{
	tlist *values = tlist_new();
	for (int i = 0; i < count; i++) {
		tobj value;
		tobj_set_nil(&value);
		tobj_set_compo(&value, (tcompo_v *)tstr_new(arguments[i]));
		tobj_vec_push(&values->items, &value);
		tobj_try_clear(&value);
	}
	return values;
}

int tsession_execute_module(tsession *session, const char *name,
			    int argument_count, const char *const *arguments)
{
	tstring *file = resolve_module(session->lib, name);
	if (!file)
		twarn(ErrCompile_UnfoundFile, "module", name);
	tlib *module = tlib_recreate(session->lib);
	tcp *compiler = tcp_new_library(module, 0);
	twrapper *wrapper = compile_file(
		compiler, file, tlib_get_paths(module), tlib_get_npaths(module));
	tcp_delete(compiler);
	tstring_free(file);
	tlib_set_wrapper(module, wrapper);

	tvm vm;
	tvm_init(&vm, wrapper->info.tmp_max);
	tvm_set_vmstack(&vm, tcompo_env_get_vmstack(&module->env),
			wrapper->info.reg_max);
	exec_tins(&vm, 0, wrapper->ncmds, &module->env);
	tobj returned = *tvm_get_vre(&vm);
	tvm_get_vre(&vm)->type = tnil;
	if (returned.type != tcompo ||
	    tobj_compo_type(&returned) != compo_tdict)
		twarn(ErrRuntime_RefType, "module",
		      "module must export a Dictionary");
	tlib_set_exposed(module, (tdict *)returned.val.v_tcompo);

	tobj key;
	tobj main_function;
	tobj_set_nil(&key);
	tobj_set_nil(&main_function);
	tobj_set_compo(&key, (tcompo_v *)tstr_new("main"));
	if (!tdict_contains(tlib_get_exposed(module), &key))
		twarn(ErrRuntime_ObjUnfound, "module", "main export required");
	tdict_get(tlib_get_exposed(module), &key, &main_function);
	tobj_try_clear(&key);

	tobj argument_list;
	tobj call_result;
	tobj_set_nil(&argument_list);
	tobj_set_nil(&call_result);
	tobj_set_compo(&argument_list,
		      (tcompo_v *)module_arguments(argument_count, arguments));
	tvm_call(&vm, &main_function, &argument_list, 1,
		 &module->env, &call_result);
	int exit_code = 0;
	if (call_result.type == tint)
		exit_code = (int)call_result.val.v_tint;
	else if (call_result.type != tnil)
		twarn(ErrRuntime_ParamsType, "module",
		      "main must return Int or Nil");

	tobj_try_clear(&call_result);
	tobj_try_clear(&argument_list);
	tobj_try_clear(&main_function);
	tvm_clean(&vm);
	tvm_set_vmstack(&vm, nullptr, 0);
	module->compo_base.vtable->free(module);
	return exit_code;
}

/** Compile & evaluate a string */
void tsession_execute_str(tsession *sess, const char *str, int interactive)
{
	tcp *cp = tcp_new_library(sess->lib, interactive);

	tconsts consts;
	tconsts_init(&consts);
	tvmcmd_vect tcmds;
	tvmcmd_vect_init(&tcmds);

	tstring **paths = tlib_get_paths(sess->lib);
	uint_lexs npaths = tlib_get_npaths(sess->lib);

	tstring *src = tstring_new(str);
	tcinfo info = parse_blk(cp, src, &tcmds, &consts, paths, npaths, 1, 0);
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
	tvm_set_vmstack(&vm, nullptr, 0);

	tvmcmd_vect_free(&tcmds);
	tconsts_free(&consts);
	tcp_delete(cp);
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
		tvm_set_vmstack(vm, nullptr, 0);
	} else {
		tapas_error_recover_enabled = 1;
		tstring **paths = tlib_get_paths(sess->lib);
		uint_lexs npaths = tlib_get_npaths(sess->lib);
		uint_cmds ncmd_old = tvmcmd_vect_size32(tcmds);
		terror_set_source_context(tstring_cstr(code));
		terror_set_file_context(file, start_line, 0);
		/* A Markdown fence contains a sequence of statements. */
		*info = parse_sequence(syner, code, tcmds, consts, paths, npaths);
		twrapper *wrapper = tanalyser_wrap(tcmds, consts, info);
		if (wrapper) {
			tlib_set_wrapper(sess->lib, wrapper);
			tvm_set_tmpmax(vm, info->tmp_max);
			tvm_set_vmstack(vm,
					tcompo_env_get_vmstack(&sess->lib->env),
					info->reg_max);
			eval_bycodes(vm, ncmd_old, sess->lib);
			tvm_clean(vm);
			tvm_set_vmstack(vm, nullptr, 0);
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

	tstring **lines = nullptr;
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
	tcp *syner = tcp_new_library(sess->lib, 1);
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
		const char *lang = nullptr;
		if (!md_line_is_fence(lines[i], &lang) || !md_lang_is_tapas(lang)) {
			tstring_append_ts(updated, lines[i++]);
			continue;
		}

		uint32_t fence_line = i + 1;
		uint64_t code_start_line = (uint64_t)fence_line + 1;
		tstring *code = tstring_new_empty();
		tstring_append_ts(updated, lines[i++]);
		while (i < nlines) {
			const char *end_lang = nullptr;
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

		tstring *captured_out = nullptr;
		tstring *captured_err = nullptr;
		if (!execute_block_capture(sess,
					   syner,
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
	tcp_delete(syner);
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
