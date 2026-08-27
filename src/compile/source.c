/** Internal compiler implementation. */
#include "internal.h"
#include "tapas/compile/workspace.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

int find_imported_file(tstring **file_ptr, tstring **paths, uint_lexs npaths)
{
	if (!file_ptr || !*file_ptr)
		return 0;
	const char **roots = npaths ?
		(const char **)calloc(npaths, sizeof(const char *)) : NULL;
	if (npaths && !roots) abort();
	for (uint_lexs i = 0; i < npaths; i++)
		roots[i] = tstring_cstr(paths[i]);
	tstring *resolved = tworkspace_resolve_module_file(
		tstring_cstr(*file_ptr), roots, npaths);
	free(roots);
	if (!resolved) return 0;
	tstring_free(*file_ptr);
	*file_ptr = resolved;
	return 1;
}

static tstring *path_dirname(const tstring *file)
{
	size_t slash = tstring_rfind_c(file, '/');
	if (slash == SIZE_MAX)
		return tstring_new(".");
	if (slash == 0)
		return tstring_new("/");
	return tstring_substr(file, 0, slash);
}

static tstring **prepend_path(
		tstring **paths,
		uint_lexs npaths,
		tstring *path,
		uint_lexs *npaths_out)
{
	tstring **local_paths =
		(tstring **)malloc((npaths + 1) * sizeof(tstring *));
	if (!local_paths)
		twarn(ErrRuntime_Other, "prepend_path", "out of memory");
	local_paths[0] = path;
	for (uint_lexs i = 0; i < npaths; i++)
		local_paths[i + 1] = paths[i];
	*npaths_out = npaths + 1;
	return local_paths;
}

static tstring *read_source_to_tstring(FILE *f)
{
	tstring *src = tstring_new_empty();
	int c;
	while ((c = fgetc(f)) != EOF)
		tstring_append_c(src, (char)c);
	return src;
}

static uint64_t source_line_for_unit(
		const tstring *source,
		const tstring *unit,
		size_t *cursor,
		uint64_t *line_cursor)
{
	const char *src = tstring_cstr(source);
	const char *needle = tstring_cstr(unit);
	size_t src_len = tstring_len(source);
	size_t pos = *cursor;
	char *hit;
	if (pos > src_len)
		pos = 0;
	hit = strstr(src + pos, needle);
	if (!hit) {
		pos = 0;
		hit = strstr(src, needle);
		*line_cursor = 1;
	}
	if (!hit)
		return *line_cursor ? *line_cursor : 1;
	size_t hit_pos = (size_t)(hit - src);
	while (*cursor < hit_pos) {
		if (src[*cursor] == '\n')
			(*line_cursor)++;
		(*cursor)++;
	}
	uint64_t unit_line = *line_cursor ? *line_cursor : 1;
	size_t end_pos = hit_pos + strlen(needle);
	while (*cursor < end_pos && *cursor < src_len) {
		if (src[*cursor] == '\n')
			(*line_cursor)++;
		(*cursor)++;
	}
	return unit_line;
}

static tcinfo parse_source_stream(
		tcp *cp,
		const tstring *file,
		FILE *f,
		tvmcmd_vect *tcmds,
		tconsts *consts,
		tstring **paths,
		uint_lexs npaths)
{
	terror_set_file_context(tstring_cstr(file), 0, 0);
	int ismd = 0;
	if (tstring_ends_with(file, ".tap") || tstring_ends_with(file, ".Tap") ||
		tstring_ends_with(file, ".TAP"))
		ismd = 0;
	else if (tstring_ends_with(file, ".md") || tstring_ends_with(file, ".Md") ||
		 tstring_ends_with(file, ".MD"))
		ismd = 1;
	else
		twarn(ErrCompile_InvalidFile, "parse_source_stream", tstring_cstr(file));

	if (!ismd) {
		tstring *source = read_source_to_tstring(f);
		parse_blk(cp, source, tcmds, consts, paths, npaths, 1, 0);
		tstring_free(source);
		return tcp_get_compile_info(cp);
	}

	tstring *code = tstring_new_empty();
	int in_tapas_block = 0;
	tstring *line = tstring_new_empty();
	while (tstring_getline(line, f) != SIZE_MAX) {
		tstring *trimmed = tstring_dup(line);
		tstring_trim(trimmed);
		if (strncmp(tstring_cstr(trimmed), "```", 3) == 0) {
			if (!in_tapas_block &&
				(strncmp(tstring_cstr(trimmed), "```tapas", 8) == 0 ||
				 strncmp(tstring_cstr(trimmed), "```tap", 6) == 0)) {
				in_tapas_block = 1;
			} else if (in_tapas_block) {
				in_tapas_block = 0;
			}
			tstring_free(trimmed);
			tstring_append_c(code, '\n');
			continue;
		}
		tstring_free(trimmed);
		if (!in_tapas_block) {
			tstring_append_c(code, '\n');
			continue;
		}
		tstring_append_ts(code, line);
		if (tstring_empty(line) || tstring_at(line, tstring_len(line) - 1) != '\n')
			tstring_append_c(code, '\n');
	}

	parse_blk(cp, code, tcmds, consts, paths, npaths, 1, 0);
	tstring_free(line);
	tstring_free(code);
	return tcp_get_compile_info(cp);
}

void parse_import(
		tcp *cp,
		const ttoken *tok,
		tvmcmd_vect *tcmds,
		tconsts *consts,
		tstring **paths,
		uint_lexs npaths,
		int inblk)
{
	const char *source_ctx = terror_current_source_context();
	const char *file_ctx = terror_current_file_context();
	uint64_t line_ctx = terror_current_line_context();
	uint64_t column_ctx = terror_current_column_context();
	tstring *saved_source_ctx = source_ctx ? tstring_new(source_ctx) : NULL;
	tstring *saved_file_ctx = file_ctx ? tstring_new(file_ctx) : NULL;
	tstring *file = tstring_dup(tok->val1);
	if (tstring_empty(file))
		twarn(ErrCompile_InvalidLiter, "parse_import", "empty liter");
	if (!find_imported_file(&file, paths, npaths))
		twarn(ErrCompile_UnfoundFile, "parse_import", tstring_cstr(tok->val1));
	for (uint32_t i = 0; cp->imports && i < cp->imports->count; i++)
		if (tstring_eq(cp->imports->files[i], file))
			twarn(ErrCompile_Other, "parse_import",
			      "circular module import");
	if (cp->imports) {
		if (cp->imports->count == cp->imports->capacity) {
			uint32_t capacity = cp->imports->capacity ?
				cp->imports->capacity * 2 : 8;
			cp->imports->files = (tstring **)realloc(
				cp->imports->files, capacity * sizeof(tstring *));
			if (!cp->imports->files)
				abort();
			cp->imports->capacity = capacity;
		}
		cp->imports->files[cp->imports->count++] = tstring_dup(file);
	}

	/* Recursively compile the imported file (simplified) */
	FILE *f = fopen(tstring_cstr(file), "r");
	if (!f)
		twarn(ErrCompile_UnfoundFile, "parse_import", tstring_cstr(file));

	tcp sub_comp;
	tstring **default_names =
		tobj_ctr_first_n_objs(&cp->objctr, cp->n_default_objs);
	tcp_init_preload(
		&sub_comp, default_names, cp->n_default_objs, NULL, cp->interactive);
	tcp_share_import_stack(&sub_comp, cp);

	tvmcmd_vect sub_cmds;
	tvmcmd_vect_init(&sub_cmds);
	tconsts sub_consts;
	tconsts_init(&sub_consts);
	tstring *import_dir = path_dirname(file);
	uint_lexs nlocal_paths;
	tstring **local_paths =
		prepend_path(paths, npaths, import_dir, &nlocal_paths);

	/* Parse the imported file and save its bytecode beside the source file. */
	tcinfo sub_info =
		parse_source_stream(&sub_comp, file, f, &sub_cmds, &sub_consts, local_paths, nlocal_paths);
	tcompile_module_interface *module_interface = sub_comp.module_interface;
	sub_comp.module_interface = NULL;
	free(local_paths);
	tstring_free(import_dir);
	fclose(f);

	if (saved_source_ctx)
		terror_set_source_context(tstring_cstr(saved_source_ctx));
	else
		terror_clear_source_context();
	terror_set_file_context(
		saved_file_ctx ? tstring_cstr(saved_file_ctx) : NULL,
		line_ctx,
		column_ctx);
	tstring_free(saved_source_ctx);
	tstring_free(saved_file_ctx);

	/* Save compiled result */
	{
		twrapper *w = tanalyser_wrap(&sub_cmds, &sub_consts, &sub_info);
		if (w) {
			size_t dot = tstring_rfind_c(file, '.');
			tstring *binfile = dot != SIZE_MAX ?
				tstring_substr(file, 0, dot) :
				tstring_dup(file);
			tstring_append(binfile, ".tapc");
			tanalyser_save_bin_file(w, tstring_cstr(binfile));
			tanalyser_clean_wrapper(w);
			tstring_free(binfile);
		}
	}

	if (tok->nvals == 2) {
		uint_csts nameloc = UNDEF_NAMELOC;
		uint_objs loc = tobj_ctr_obj_create(
			&cp->objctr, tok->val2, inblk, consts, &nameloc);
		cp->objctr.bindings[loc].module_interface = module_interface;
		module_interface = NULL;
		tcompile_set_metadata(&cp->objctr, loc,
			ttypeval_builtin(ttype_builtin_library), NULL, 0);
		tvmcmd_vect_append(
			tcmds, tbycode_make_lr(OP_VCRT, (uint16_t)nameloc, 1));
		uint_csts sloc = tconsts_add_str_const(consts, tstring_cstr(file));
		tvmcmd_vect_append(tcmds,
				   tbycode_make_u(OP_IMPORT, (uint32_t)sloc));
		treg_ctr_add(&cp->regctr);
		tvmcmd_vect_append(
			tcmds, tbycode_make_lr(OP_POPCOV, (uint16_t)loc, 1));
		treg_ctr_ddt(&cp->regctr);
	} else {
		uint_csts sloc = tconsts_add_str_const(consts, tstring_cstr(file));
		tvmcmd_vect_append(tcmds,
				   tbycode_make_u(OP_IMPORT, (uint32_t)sloc));
		treg_ctr_add(&cp->regctr);
		tvmcmd_vect_append(tcmds, tbycode_make_lr(OP_POPN, 1, 0));
		treg_ctr_ddt(&cp->regctr);
	}
	tcompile_module_interface_free(module_interface);
	if (cp->imports && cp->imports->count) {
		cp->imports->count--;
		tstring_free(cp->imports->files[cp->imports->count]);
		cp->imports->files[cp->imports->count] = NULL;
	}

	tvmcmd_vect_free(&sub_cmds);
	tconsts_free(&sub_consts);
	tcp_free(&sub_comp);
	tobj_ctr_first_n_objs_free(default_names, cp->n_default_objs);
	tstring_free(file);
}


/*===========================================================================*
 * 11. Token Dispatcher
 *===========================================================================*/

uint_regs parse_params(
		tcp *cp,
		const tstring *src,
		tvmcmd_vect *tcmds,
		tconsts *consts,
		tstring **paths,
		uint_lexs npaths,
		int inblk)
{
	tstring **params = NULL;
	uint_regs np = 0;
	split_params_ts(src, &params, &np);

	uint_regs i;
	for (i = 0; i < np; i++) {
		parse_unit(
			cp, params[i], tcmds, consts, paths, npaths, 0, inblk);
	}
	split_params_free(params, np);
	return np;
}

/** Parse a block (multiple units, clean temporary variables). */
tcinfo parse_blk(
		tcp *cp,
		const tstring *src,
		tvmcmd_vect *tcmds,
		tconsts *consts,
		tstring **paths,
		uint_lexs npaths,
		int cleanstk,
		int inblk)
{
	(void)cleanstk;
	tcompile_try_ast_module(cp, src, tcmds, consts, paths, npaths, inblk);
	return tcp_get_compile_info(cp);
}

/** Parse a file into bycodes */
tcinfo parse_file(
		tcp *cp,
		FILE *f,
		tvmcmd_vect *tcmds,
		tconsts *consts,
		tstring **paths,
		uint_lexs npaths)
{
	tstring *source = read_source_to_tstring(f);
	parse_blk(cp, source, tcmds, consts, paths, npaths, 1, 0);
	tstring_free(source);
	return tcp_get_compile_info(cp);
}


/*===========================================================================*
 * 12. Clean Stack
 *===========================================================================*/

void clean_stk(tcp *cp, tvmcmd_vect *tcmds, int isroot, uint_regs regs_ori)
{
	uint_regs regs_now = treg_ctr_get(&cp->regctr);
	if (regs_now < regs_ori)
		twarn(ErrCompile_REGOutOfLimit, "clean_stk", "");
	if (!isroot || regs_now == regs_ori)
		return;

	uint_regs regs_ddt = regs_now - regs_ori;
	tvmcmd_vect_append(
		tcmds,
		tbycode_make_lr(
			OP_POPN,
			(uint16_t)regs_ddt,
			(uint16_t)cp->interactive
		)
	);
	treg_ctr_ddt_n(&cp->regctr, regs_ddt);
}


/*===========================================================================*
 * 13. Main Entry Point — parse_unit
 *===========================================================================*/

tcinfo parse_unit(
		tcp *cp,
		const tstring *src,
		tvmcmd_vect *tcmds,
		tconsts *consts,
		tstring **paths,
		uint_lexs npaths,
		int cleanstk,
		int inblk)
{
	/* Split by top-level separators before parsing a single unit. */
	tstring **cmdvec = NULL;
	uint32_t ncmdvec = 0;
	lex_str_ts(src, &cmdvec, &ncmdvec);

	if (ncmdvec > 1) {
		uint32_t i;
		size_t cursor = 0;
		uint64_t line_cursor = 1;
		uint64_t base_line = terror_current_line_context();
		const char *file_ctx = terror_current_file_context();
		tstring *file_ctx_copy = file_ctx ? tstring_new(file_ctx) : NULL;
		for (i = 0; i < ncmdvec; i++) {
			uint64_t rel_line =
				source_line_for_unit(src, cmdvec[i], &cursor, &line_cursor);
			terror_set_source_context(tstring_cstr(cmdvec[i]));
			if (file_ctx_copy) {
				uint64_t abs_line = base_line ?
					base_line + rel_line - 1 : rel_line;
				terror_set_file_context(tstring_cstr(file_ctx_copy), abs_line, 0);
			}
			parse_unit(
				cp, cmdvec[i], tcmds, consts, paths, npaths, cleanstk, inblk);
		}
		tstring_free(file_ctx_copy);
		lex_str_free(cmdvec, ncmdvec);
		return tcp_get_compile_info(cp);
	}
	lex_str_free(cmdvec, ncmdvec);

	tstring *cmd_ts = preprocessing_1_ts(src);
	if (tstring_empty(cmd_ts)) {
		tstring_free(cmd_ts);
		return tcp_get_compile_info(cp);
	}
	tstring *stripped_ts = tlex_strip_outer_parentheses_ts(cmd_ts);
	tstring_free(cmd_ts);
	cmd_ts = preprocessing_1_ts(stripped_ts);
	tstring_free(stripped_ts);
	if (tstring_empty(cmd_ts)) {
		tstring_free(cmd_ts);
		return tcp_get_compile_info(cp);
	}

	uint_regs regs_ori = treg_ctr_get(&cp->regctr);

	/* Prefer the reusable AST path for expression categories that have already
	 * migrated. Unsupported syntax falls through without changing state. */
	if (tcompile_try_ast_statement(cp, cmd_ts, tcmds, consts,
				       paths, npaths, cleanstk, inblk)) {
		clean_stk(cp, tcmds, cleanstk, regs_ori);
		tunit_ctr_restore(&cp->lexctr);
		tstring_free(cmd_ts);
		return tcp_get_compile_info(cp);
	}

	/* Compatibility lexing and immediate bytecode generation. */
	ttoken *tokens = NULL;
	uint32_t ntokens = 0;
	get_tokens_ts(cmd_ts, &tokens, &ntokens);

	/* Compilation */
	uint32_t i;
	for (i = 0; i < ntokens; i++) {
		parse_token(
			cp, &tokens[i], tcmds, consts, paths, npaths, cleanstk, inblk);
	}
	clean_stk(cp, tcmds, cleanstk, regs_ori);
	tunit_ctr_restore(&cp->lexctr);

	get_tokens_free(tokens, ntokens);
	tstring_free(cmd_ts);
	return tcp_get_compile_info(cp);
}


/*===========================================================================*
 * 14. High-level Compile Functions
 *===========================================================================*/

/** Compile a string */
twrapper *compile_str(tcp *cp, const tstring *src, tstring **paths, uint_lexs npaths)
{
	tconsts consts;
	tconsts_init(&consts);
	tvmcmd_vect tcmds;
	tvmcmd_vect_init(&tcmds);
	twrapper *wrapper = NULL;

	tcinfo info = parse_blk(cp, src, &tcmds, &consts, paths, npaths, 1, 0);
	wrapper = tanalyser_wrap(&tcmds, &consts, &info);

	tvmcmd_vect_free(&tcmds);
	tconsts_free(&consts);
	return wrapper;
}

/** Compile a .tap file and return wrapper */
twrapper *compile_file(tcp *cp, const tstring *file, tstring **paths, uint_lexs npaths)
{
	terror_set_file_context(tstring_cstr(file), 0, 0);
	/* Check suffix */
	int ismd = 0;
	if (tstring_ends_with(file, ".tap") || tstring_ends_with(file, ".Tap") ||
		tstring_ends_with(file, ".TAP"))
		ismd = 0;
	else if (tstring_ends_with(file, ".md") || tstring_ends_with(file, ".Md") ||
		 tstring_ends_with(file, ".MD"))
		ismd = 1;
	else
		twarn(ErrCompile_InvalidFile, "compile_file", tstring_cstr(file));

	FILE *f = fopen(tstring_cstr(file), "r");
	if (!f)
		twarn(ErrCompile_UnfoundFile, "compile_file", tstring_cstr(file));

	tconsts consts;
	tconsts_init(&consts);
	tvmcmd_vect tcmds;
	tvmcmd_vect_init(&tcmds);
	tstring *file_dir = path_dirname(file);
	uint_lexs nlocal_paths;
	tstring **local_paths =
		prepend_path(paths, npaths, file_dir, &nlocal_paths);

	/* Parse */
	tcinfo info;
	(void)info;
	if (ismd) {
		tstring *code = tstring_new_empty();
		int in_tapas_block = 0;
		tstring *line = tstring_new_empty();
		while (tstring_getline(line, f) != SIZE_MAX) {
			tstring *trimmed = tstring_dup(line);
			tstring_trim(trimmed);
			if (strncmp(tstring_cstr(trimmed), "```", 3) == 0) {
				if (!in_tapas_block &&
					(strncmp(tstring_cstr(trimmed), "```tapas", 8) == 0 ||
					 strncmp(tstring_cstr(trimmed), "```tap", 6) == 0)) {
					in_tapas_block = 1;
				} else if (in_tapas_block) {
					in_tapas_block = 0;
				}
				tstring_free(trimmed);
				tstring_append_c(code, '\n');
				continue;
			}
			tstring_free(trimmed);
			if (!in_tapas_block) {
				tstring_append_c(code, '\n');
				continue;
			}
			tstring_append_ts(code, line);
			if (tstring_empty(line) || tstring_at(line, tstring_len(line) - 1) != '\n')
				tstring_append_c(code, '\n');
		}
		parse_blk(cp, code, &tcmds, &consts,
			  local_paths, nlocal_paths, 1, 0);
		tstring_free(line);
		tstring_free(code);
	} else {
		tstring *source = read_source_to_tstring(f);
		parse_blk(cp, source, &tcmds, &consts,
			  local_paths, nlocal_paths, 1, 0);
		tstring_free(source);
		info = tcp_get_compile_info(cp);
	}
	info = tcp_get_compile_info(cp);

	twrapper *wrapper = tanalyser_wrap(&tcmds, &consts, &info);

	fclose(f);
	free(local_paths);
	tstring_free(file_dir);
	tvmcmd_vect_free(&tcmds);
	tconsts_free(&consts);
	return wrapper;
}

/** Compile file and save .tapc */
void compile_file_save(tcp *cp, const tstring *file, tstring **paths, uint_lexs npaths)
{
	twrapper *wrapper = compile_file(cp, file, paths, npaths);
	size_t dot = tstring_rfind_c(file, '.');
	tstring *binfile = dot != SIZE_MAX ?
		tstring_substr(file, 0, dot) :
		tstring_dup(file);
	tstring_append(binfile, ".tapc");
	tanalyser_save_bin_file(wrapper, tstring_cstr(binfile));
	tanalyser_clean_wrapper(wrapper);
	tstring_free(binfile);
}
