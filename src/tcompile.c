/**
 * @file tcompile.c
 * @brief Compiler for Tapas — uses tstring throughout
 */
#include "tapas/tcompile.h"
#include "tapas/tlex.h"

#include <ctype.h>
#include <stdlib.h>


/*===========================================================================*
 * 1. Register Counter
 *===========================================================================*/

void treg_ctr_init(treg_ctr *c)
{
	c->reg_ctr = 0;
	c->reg_max = 0;
}

void treg_ctr_update_max(treg_ctr *c)
{
	if (c->reg_max < c->reg_ctr)
		c->reg_max = c->reg_ctr;
}

void treg_ctr_add(treg_ctr *c)
{
	if (c->reg_ctr + 1 > REG_LIMIT)
		twarn(ErrCompile_REGOutOfLimit, "treg_ctr_add", "");
	c->reg_ctr++;
	treg_ctr_update_max(c);
}

void treg_ctr_add_n(treg_ctr *c, uint_regs n)
{
	if (c->reg_ctr + n > REG_LIMIT)
		twarn(ErrCompile_REGOutOfLimit, "treg_ctr_add_n", "");
	c->reg_ctr += n;
	treg_ctr_update_max(c);
}

void treg_ctr_ddt(treg_ctr *c)
{
	if (c->reg_ctr == 0)
		twarn(ErrCompile_REGOutOfLimit, "treg_ctr_ddt", "");
	c->reg_ctr--;
}

void treg_ctr_ddt_n(treg_ctr *c, uint_regs n)
{
	if (c->reg_ctr < n)
		twarn(ErrCompile_REGOutOfLimit, "treg_ctr_ddt_n", "");
	c->reg_ctr -= n;
}

uint_regs treg_ctr_get(const treg_ctr *c)
{
	return c->reg_ctr;
}

uint_regs treg_ctr_get_max(const treg_ctr *c)
{
	return c->reg_max;
}


/*===========================================================================*
 * 2. Object Counter — uses tstring
 *===========================================================================*/

void tobj_ctr_init(tobj_ctr *c, tobj_ctr *father)
{
	c->objs = NULL;
	c->len = 0;
	c->capacity = 0;
	c->current_env_objmax = 0;
	c->father = father;
	c->npreload = 0;
}

uint_objs tobj_ctr_len_in_all(tobj_ctr *father)
{
	if (!father)
		return 0;
	return father->len + tobj_ctr_len_in_all(father->father);
}

void tobj_ctr_init_preload(
		tobj_ctr *c, tstring **precludes, uint_objs npre, tobj_ctr *father)
{
	c->objs = NULL;
	c->len = 0;
	c->capacity = 0;
	c->current_env_objmax = 0;
	c->father = father;
	c->npreload = npre;

	uint_objs i;
	for (i = 0; i < npre; i++) {
		if (c->len >= c->capacity) {
			c->capacity = c->capacity ? c->capacity * 2 : 16;
			c->objs = (tstring **)realloc(
				c->objs, c->capacity * sizeof(tstring *));
		}
		c->objs[c->len] = tstring_dup(precludes[i]);
		c->len++;
	}
	c->current_env_objmax = c->len;

	if (c->len + tobj_ctr_len_in_all(c->father) >= OBJ_LIMIT)
		twarn(ErrCompile_OBJOutOfLimit, "tobj_ctr_init_preload", "");
}

void tobj_ctr_free(tobj_ctr *c)
{
	uint_objs i;
	if (!c)
		return;
	for (i = 0; i < c->len; i++)
		tstring_free(c->objs[i]);
	free(c->objs);
	c->objs = NULL;
	c->len = 0;
	c->capacity = 0;
	c->current_env_objmax = 0;
}

void tobj_ctr_update_obj_max(tobj_ctr *c)
{
	if (c->current_env_objmax < c->len)
		c->current_env_objmax = c->len;
}

uint_objs tobj_ctr_obj_len_all(tobj_ctr *c)
{
	if (!c)
		return 0;
	return c->len + tobj_ctr_obj_len_all(c->father);
}

uint_objs tobj_ctr_obj_len_cur(tobj_ctr *c)
{
	if (!c)
		return 0;
	return c->len;
}

uint_objs tobj_ctr_obj_max_cur(const tobj_ctr *c)
{
	if (!c)
		return 0;
	return c->current_env_objmax;
}

/** Search for objname from local env upward through fathers. */
uint_objs tobj_ctr_obj_loc(tobj_ctr *c, const tstring *objname)
{
	uint_objs i;
	if (!c || !objname)
		return OBJ_LIMIT;
	for (i = 0; i < c->len; i++) {
		if (tstring_eq(c->objs[i], objname))
			return i;
	}
	if (c->father)
		return c->len + tobj_ctr_obj_loc(c->father, objname);
	return tobj_ctr_obj_len_all(NULL) + c->len;
}

int tobj_ctr_obj_addr(tobj_ctr *c, const tstring *objname, tobj_ctr_addr *addr)
{
	uint16_t depth = 0;
	for (tobj_ctr *cur = c; cur; cur = cur->father, depth++) {
		uint_objs i;
		for (i = 0; i < cur->len; i++) {
			if (tstring_eq(cur->objs[i], objname)) {
				addr->slot = i;
				addr->depth = depth;
				return 1;
			}
		}
	}
	return 0;
}

/** Create a new object and return its local index. */
uint_objs tobj_ctr_obj_create(
		tobj_ctr *c, const tstring *left, int inblk,
		tconsts *consts, uint_csts *nameloc)
{
	uint_objs loc = tobj_ctr_obj_loc(c, left);
	uint_objs len_all = tobj_ctr_obj_len_all(c);

	if (len_all + 1 >= OBJ_LIMIT)
		twarn(ErrCompile_OBJOutOfLimit, "tobj_ctr_obj_create", "");
	if (loc < c->len)
		twarn(ErrCompile_DblVDeclare, "tobj_ctr_obj_create", "");
	if (inblk)
		twarn(ErrCompile_InBlkVarDef, "tobj_ctr_obj_create", "");

	*nameloc = tconsts_add_str_const(consts, tstring_cstr(left));

	if (c->len >= c->capacity) {
		c->capacity = c->capacity ? c->capacity * 2 : 16;
		c->objs =
			(tstring **)realloc(c->objs, c->capacity * sizeof(tstring *));
	}
	c->objs[c->len] = tstring_dup(left);
	c->len++;
	tobj_ctr_update_obj_max(c);
	return c->len - 1;
}

void tobj_ctr_obj_del_last_n(tobj_ctr *c, uint_objs n)
{
	while (n > 0 && c->len > c->npreload) {
		c->len--;
		tstring_free(c->objs[c->len]);
		n--;
	}
}

int tobj_ctr_is_preload(const tobj_ctr *c, uint_objs loc)
{
	if (!c)
		return 0;
	if (loc < c->len)
		return loc < c->npreload;
	if (!c->father)
		return 0;
	return tobj_ctr_is_preload(c->father, loc - c->len);
}

/** Get first n objects */
tstring **tobj_ctr_first_n_objs(tobj_ctr *c, uint_objs n)
{
	tstring **result = NULL;
	uint_objs i;
	for (i = 0; i < n && i < c->len; i++) {
		result = (tstring **)realloc(result, (i + 1) * sizeof(tstring *));
		result[i] = tstring_dup(c->objs[i]);
	}
	return result;
}

void tobj_ctr_first_n_objs_free(tstring **objs, uint_objs n)
{
	uint_objs i;
	if (!objs)
		return;
	for (i = 0; i < n; i++)
		tstring_free(objs[i]);
	free(objs);
}


/*===========================================================================*
 * 4. Compiler
 *===========================================================================*/

void tcp_init(tcp *cp, tobj_ctr *father_objctr, int interactive)
{
	tobj_ctr_init(&cp->objctr, father_objctr);
	tobj_ctr_init(&cp->tmpctr, NULL);
	tunit_ctr_init(&cp->lexctr);
	treg_ctr_init(&cp->regctr);
	cp->n_default_objs = 0;
	cp->interactive = interactive;
}

void tcp_init_preload(
		tcp *cp,
		tstring **default_objs,
		uint_objs ndefault,
		tobj_ctr *father_objctr,
		int interactive)
{
	tobj_ctr_init_preload(
		&cp->objctr, default_objs, ndefault, father_objctr);
	tobj_ctr_init(&cp->tmpctr, NULL);
	tunit_ctr_init(&cp->lexctr);
	treg_ctr_init(&cp->regctr);
	cp->n_default_objs = ndefault;
	cp->interactive = interactive;
}

tcinfo tcp_get_compile_info(tcp *cp)
{
	tcinfo info;
	info.obj_max = tobj_ctr_obj_max_cur(&cp->objctr);
	info.tmp_max = tobj_ctr_obj_max_cur(&cp->tmpctr);
	info.reg_max = treg_ctr_get_max(&cp->regctr);
	info.padding_1 = 0;
	return info;
}

void tcp_free(tcp *cp)
{
	if (!cp)
		return;
	tobj_ctr_free(&cp->objctr);
	tobj_ctr_free(&cp->tmpctr);
}


/*===========================================================================*
 * 4a. Compiler Helpers
 *===========================================================================*/

/** Find imported file in paths */
int find_imported_file(tstring **file_ptr, tstring **paths, uint_lexs npaths)
{
	tstring *file;
	FILE *f;
	if (!file_ptr || !*file_ptr)
		return 0;
	file = *file_ptr;

	/* Absolute path: try directory */
	{
		size_t len = tstring_len(file) + 14;
		tstring *dir_init = tstring_new_cap(len);
		tstring_append(dir_init, tstring_cstr(file));
		tstring_append(dir_init, "/__init__.tap");
		f = fopen(tstring_cstr(dir_init), "r");
		if (f) {
			fclose(f);
			tstring_free(*file_ptr);
			*file_ptr = dir_init;
			return 1;
		}
		tstring_free(dir_init);
	}

	/* Absolute path: try file */
	f = fopen(tstring_cstr(file), "r");
	if (f) {
		fclose(f);
		return 1;
	}

	/* Relative path */
	uint_lexs i;
	for (i = 0; i < npaths; i++) {
		/* Try dir */
		{
			tstring *p = tstring_new_cap(tstring_len(paths[i]) + tstring_len(file) + 16);
			tstring_append_ts(p, paths[i]);
			tstring_append_c(p, '/');
			tstring_append_ts(p, file);
			tstring_append(p, "/__init__.tap");
			f = fopen(tstring_cstr(p), "r");
			if (f) {
				fclose(f);
				tstring_free(*file_ptr);
				*file_ptr = p;
				return 1;
			}
			tstring_free(p);
		}

		/* Try file */
		{
			tstring *p = tstring_new_cap(tstring_len(paths[i]) + tstring_len(file) + 3);
			tstring_append_ts(p, paths[i]);
			tstring_append_c(p, '/');
			tstring_append_ts(p, file);
			f = fopen(tstring_cstr(p), "r");
			if (f) {
				fclose(f);
				tstring_free(*file_ptr);
				*file_ptr = p;
				return 1;
			}
			tstring_free(p);
		}
	}
	return 0;
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

int str_to_long_int(const tstring *cmds, long *it)
{
	char end = '\0';
	return (tstring_find_c(cmds, '.', 0) == SIZE_MAX &&
		tstring_find_c(cmds, 'e', 0) == SIZE_MAX &&
		tstring_find_c(cmds, 'E', 0) == SIZE_MAX &&
		sscanf(tstring_cstr(cmds), "%li%c", it, &end) == 1);
}

int str_to_float(const tstring *cmds, double *dt)
{
	char end = '\0';
	return sscanf(tstring_cstr(cmds), "%lf%c", dt, &end) == 1;
}

static int split_pipeline_method(
		const tstring *src, tstring **receiver, tstring **method)
{
	const char *buf = tstring_cstr(src);
	size_t len = tstring_len(src);
	size_t pos = SIZE_MAX;
	size_t i;
	tunit_ctr ctr;
	tunit_ctr_init(&ctr);

	if (len < 3)
		return 0;

	for (i = 0; i < len; i++) {
		if (tunit_ctr_independent(&ctr) && buf[i] == '.' &&
		    !(i > 0 && i + 1 < len &&
		      isdigit((unsigned char)buf[i - 1]) &&
		      isdigit((unsigned char)buf[i + 1]))) {
			pos = i;
		}
		tunit_ctr_update(&ctr, buf[i]);
	}
	if (pos == SIZE_MAX || pos == 0 || pos + 1 >= len)
		return 0;

	*receiver = tstring_new_len(buf, pos);
	tstring_trim(*receiver);
	*method = tstring_new_len(buf + pos + 1, len - pos - 1);
	tstring_trim(*method);
	if (tstring_empty(*receiver) || tstring_empty(*method)) {
		tstring_free(*receiver);
		tstring_free(*method);
		*receiver = NULL;
		*method = NULL;
		return 0;
	}
	return 1;
}

static tstring *parse_for_let_name(const tstring *left)
{
	if (!tstring_starts_with(left, "let "))
		return NULL;

	tstring *name = tstring_substr(left, 4, tstring_len(left) - 4);
	size_t colon = tstring_find_c(name, ':', 0);
	if (colon != SIZE_MAX) {
		tstring *trimmed = tstring_substr(name, 0, colon);
		tstring_free(name);
		name = trimmed;
	}
	tstring_trim(name);
	if (tstring_empty(name))
		twarn(ErrCompile_InvalidVname, "parse_for", tstring_cstr(left));
	return name;
}


/*===========================================================================*
 * 6. Binary Operation Split (for short-circuit optimization)
 *===========================================================================*/

static int binop_is_simple_name(const tstring *name)
{
	if (tstring_empty(name) || isdigit((unsigned char)tstring_at(name, 0)))
		return 0;

	size_t i;
	for (i = 0; i < tstring_len(name); i++) {
		char ch = tstring_at(name, i);
		if (!isalpha((unsigned char)ch) &&
			!isdigit((unsigned char)ch) && ch != '_')
			return 0;
	}
	return 1;
}

static tbin_expr binop_split(tcp *cp, const ttoken *toc)
{
	tbin_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.left = toc->val1;
	expr.right = toc->val2;

	int left_simple = binop_is_simple_name(expr.left);
	int right_simple = binop_is_simple_name(expr.right);
	uint_objs obj_left_loc = left_simple ?
		tobj_ctr_obj_loc(&cp->objctr, expr.left) :
		tobj_ctr_obj_len_all(&cp->objctr);
	uint_objs tmp_left_loc = left_simple ?
		tobj_ctr_obj_loc(&cp->tmpctr, expr.left) :
		tobj_ctr_obj_len_all(&cp->tmpctr);
	uint_objs obj_right_loc = right_simple ?
		tobj_ctr_obj_loc(&cp->objctr, expr.right) :
		tobj_ctr_obj_len_all(&cp->objctr);
	uint_objs tmp_right_loc = right_simple ?
		tobj_ctr_obj_loc(&cp->tmpctr, expr.right) :
		tobj_ctr_obj_len_all(&cp->tmpctr);
	uint_objs obj_size_all = tobj_ctr_obj_len_all(&cp->objctr);
	uint_objs tmp_size_all = tobj_ctr_obj_len_all(&cp->tmpctr);

	expr.al_type = 0; /* value value */
	if (!left_simple || !right_simple)
		return expr;

	if (obj_left_loc < obj_size_all && tmp_right_loc == tmp_size_all &&
		obj_right_loc == obj_size_all) {
		expr.lloc = obj_left_loc;
		expr.al_type = 1; /* env value */
	} else if (tmp_left_loc == tmp_size_all &&
		   obj_left_loc == obj_size_all &&
		   obj_right_loc < obj_size_all) {
		expr.rloc = obj_right_loc;
		expr.al_type = 2; /* value env */
	} else if (obj_left_loc < obj_size_all &&
		   obj_right_loc < obj_size_all) {
		expr.lloc = obj_left_loc;
		expr.rloc = obj_right_loc;
		expr.al_type = 3; /* env env */
	} else if (tmp_left_loc < tmp_size_all &&
		   tmp_right_loc == tmp_size_all &&
		   obj_right_loc == obj_size_all) {
		expr.lloc = tmp_left_loc;
		expr.al_type = 4; /* tmp value */
	} else if (tmp_left_loc == tmp_size_all &&
		   obj_left_loc == obj_size_all &&
		   tmp_right_loc < tmp_size_all) {
		expr.rloc = tmp_right_loc;
		expr.al_type = 5; /* value tmp */
	} else if (tmp_left_loc < tmp_size_all &&
		   tmp_right_loc < tmp_size_all) {
		expr.lloc = tmp_left_loc;
		expr.rloc = tmp_right_loc;
		expr.al_type = 6; /* tmp tmp */
	} else if (obj_left_loc < obj_size_all &&
		   tmp_right_loc < tmp_size_all) {
		expr.lloc = obj_left_loc;
		expr.rloc = tmp_right_loc;
		expr.al_type = 7; /* env tmp */
	} else if (tmp_left_loc < tmp_size_all &&
		   obj_right_loc < obj_size_all) {
		expr.lloc = tmp_left_loc;
		expr.rloc = obj_right_loc;
		expr.al_type = 8; /* tmp env */
	}
	return expr;
}

static tstring *normalize_slice_param(const tstring *target, const tstring *param)
{
	uint_lexs off;
	if (!tlex_find_last_top_level_char_ts(param, ':', &off))
		return tstring_dup(param);

	uint_lexs len = (uint_lexs)tstring_len(param);
	if (off > 0 && off + 1 < len)
		return tstring_dup(param);

	tstring *out = tstring_new_empty();
	if (off == 0)
		tstring_append(out, "0");
	else
		tstring_append_len(out, tstring_cstr(param), off);

	tstring_append_c(out, ':');

	if (off + 1 < len)
		tstring_append_len(out,
				   tstring_cstr(param) + off + 1,
				   len - off - 1);
	else {
		tstring_append_ts(out, target);
		tstring_append(out, ".len()");
	}
	return out;
}


/*===========================================================================*
 * 8. parse_v (value reference)
 *===========================================================================*/

void parse_v(tcp *cp, const ttoken *tok, tvmcmd_vect *tcmds, tconsts *consts)
{
	if (tstring_empty(tok->val1))
		twarn(ErrCompile_InvalidLiter, "parse_v", "empty liter");

	long it;
	double dt;

	if (str_to_long_int(tok->val1, &it)) {
		uint_csts loc = tconsts_add_int_const(consts, it);
		tvmcmd_vect_append(tcmds, tbycode_make_u(OP_PUSHI, loc));
		treg_ctr_add(&cp->regctr);
	} else if (str_to_float(tok->val1, &dt)) {
		uint_csts loc = tconsts_add_float_const(consts, dt);
		tvmcmd_vect_append(tcmds, tbycode_make_u(OP_PUSHFLT, loc));
		treg_ctr_add(&cp->regctr);
	} else {
		uint_objs loc_tmp = tobj_ctr_obj_loc(&cp->tmpctr, tok->val1);
		tobj_ctr_addr env_addr;

		if (loc_tmp < tobj_ctr_obj_len_all(&cp->tmpctr)) {
			tvmcmd_vect_append(tcmds,
					   tbycode_make_lr(OP_PUSHX,
							   (uint16_t)loc_tmp,
							   tpushx_tmp_addr()));
			treg_ctr_add(&cp->regctr);
		} else if (tobj_ctr_obj_addr(&cp->objctr, tok->val1, &env_addr)) {
			uint16_t r = env_addr.depth == 0 ?
				tpushx_local_addr() :
				tpushx_upval_addr(env_addr.depth);
			tvmcmd_vect_append(tcmds,
					   tbycode_make_lr(OP_PUSHX,
							   (uint16_t)env_addr.slot,
							   r));
			treg_ctr_add(&cp->regctr);
		} else
			twarn(ErrCompile_InvalidLiter, "parse_v", tstring_cstr(tok->val1));
	}
}


/*===========================================================================*
 * 9. Binary OP Parser (with optimization for and/or)
 *===========================================================================*/

void parse_binop_split(
		tcp *cp,
		int ins,
		const tbin_expr *expr,
		tvmcmd_vect *tcmds,
		tconsts *consts,
		tstring **paths,
		uint_lexs npaths,
		int inblk)
{
	switch (expr->al_type) {
	case 0: /* value value */
		parse_unit(cp,
			   expr->right,
			   tcmds,
			   consts,
			   paths,
			   npaths,
			   0,
			   inblk);
		parse_unit(
			cp, expr->left, tcmds, consts, paths, npaths, 0, inblk);
		tvmcmd_vect_append(
			tcmds,
			tbycode_make_u(OP_PUSHINFO, (uint32_t)expr->al_type));
		treg_ctr_add(&cp->regctr);
		tvmcmd_vect_append(tcmds, tbycode_make_lr((uint8_t)ins, 0, 1));
		treg_ctr_ddt(&cp->regctr);
		treg_ctr_ddt(&cp->regctr);
		break;
	case 1: /* env value */
		parse_unit(cp,
			   expr->right,
			   tcmds,
			   consts,
			   paths,
			   npaths,
			   0,
			   inblk);
		tvmcmd_vect_append(
			tcmds,
			tbycode_make_u(OP_PUSHINFO, (uint32_t)expr->al_type));
		treg_ctr_add(&cp->regctr);
		tvmcmd_vect_append(
			tcmds,
			tbycode_make_lr((uint8_t)ins, (uint16_t)expr->lloc, 0));
		treg_ctr_ddt(&cp->regctr);
		break;
	case 2: /* value env */
		parse_unit(
			cp, expr->left, tcmds, consts, paths, npaths, 0, inblk);
		tvmcmd_vect_append(
			tcmds,
			tbycode_make_u(OP_PUSHINFO, (uint32_t)expr->al_type));
		treg_ctr_add(&cp->regctr);
		tvmcmd_vect_append(
			tcmds,
			tbycode_make_lr((uint8_t)ins, 0, (uint16_t)expr->rloc));
		treg_ctr_ddt(&cp->regctr);
		break;
	case 3:
	case 6:
	case 7:
	case 8: /* env env / tmp tmp / env tmp / tmp env */
		tvmcmd_vect_append(
			tcmds,
			tbycode_make_u(OP_PUSHINFO, (uint32_t)expr->al_type));
		treg_ctr_add(&cp->regctr);
		tvmcmd_vect_append(tcmds,
				   tbycode_make_lr((uint8_t)ins,
						   (uint16_t)expr->lloc,
						   (uint16_t)expr->rloc));
		treg_ctr_ddt(&cp->regctr);
		treg_ctr_add(&cp->regctr);
		break;
	case 4: /* tmp value */
		parse_unit(cp,
			   expr->right,
			   tcmds,
			   consts,
			   paths,
			   npaths,
			   0,
			   inblk);
		tvmcmd_vect_append(
			tcmds,
			tbycode_make_u(OP_PUSHINFO, (uint32_t)expr->al_type));
		treg_ctr_add(&cp->regctr);
		tvmcmd_vect_append(
			tcmds,
			tbycode_make_lr((uint8_t)ins, (uint16_t)expr->lloc, 0));
		treg_ctr_ddt(&cp->regctr);
		break;
	case 5: /* value tmp */
		parse_unit(
			cp, expr->left, tcmds, consts, paths, npaths, 0, inblk);
		tvmcmd_vect_append(
			tcmds,
			tbycode_make_u(OP_PUSHINFO, (uint32_t)expr->al_type));
		treg_ctr_add(&cp->regctr);
		tvmcmd_vect_append(
			tcmds,
			tbycode_make_lr((uint8_t)ins, 0, (uint16_t)expr->rloc));
		treg_ctr_ddt(&cp->regctr);
		break;
	}
}

void parse_binop_opt(
		tcp *cp,
		int ins,
		const ttoken *tok,
		tvmcmd_vect *tcmds,
		tconsts *consts,
		tstring **paths,
		uint_lexs npaths,
		int inblk)
{
	tbin_expr e = binop_split(cp, tok);
	parse_binop_split(cp, ins, &e, tcmds, consts, paths, npaths, inblk);
}

void parse_binop_short_circuit(
		tcp *cp,
		int ins,
		const ttoken *tok,
		tvmcmd_vect *tcmds,
		tconsts *consts,
		tstring **paths,
		uint_lexs npaths,
		int inblk)
{
	parse_unit(cp, tok->val1, tcmds, consts, paths, npaths, 0, inblk);
	tvmcmd_vect_append(
		tcmds,
		tbycode_make_u(ins == OP_AND ? OP_CJPFPOP : OP_CJPBPOP, 0));
	uint_cmds loc_cond = tvmcmd_vect_size32(tcmds) - 1;
	treg_ctr_ddt(&cp->regctr);

	tvmcmd_vect tcmds_right;
	tvmcmd_vect_init(&tcmds_right);
	parse_unit(cp, tok->val2, &tcmds_right, consts, paths, npaths, 0, inblk);

	uint_cmds right_len = tvmcmd_vect_size32(&tcmds_right);
	tcmds->data[loc_cond] =
		tbycode_make_u(ins == OP_AND ? OP_CJPFPOP : OP_CJPBPOP,
			       right_len + 4);

	tvmcmd_vect_insert_vect(tcmds, tvmcmd_vect_size32(tcmds), &tcmds_right);
	tvmcmd_vect_free(&tcmds_right);

	tvmcmd_vect_append(tcmds, tbycode_make_u(OP_PUSHB, ins == OP_AND ? 1 : 0));
	treg_ctr_add(&cp->regctr);
	tvmcmd_vect_append(tcmds, tbycode_make_u(OP_PUSHINFO, 0));
	treg_ctr_add(&cp->regctr);
	tvmcmd_vect_append(tcmds, tbycode_make_lr((uint8_t)ins, 0, 1));
	treg_ctr_ddt(&cp->regctr);
	treg_ctr_ddt(&cp->regctr);

	tvmcmd_vect_append(tcmds, tbycode_make_u(OP_JPF, 1));
	tvmcmd_vect_append(tcmds, tbycode_make_u(OP_PUSHB, ins == OP_AND ? 0 : 1));
}

/** Simple binary op (no short-circuit) */
void parse_binop_simple(
		tcp *cp,
		int ins,
		const ttoken *tok,
		tvmcmd_vect *tcmds,
		tconsts *consts,
		tstring **paths,
		uint_lexs npaths,
		int inblk)
{
	parse_unit(cp, tok->val2, tcmds, consts, paths, npaths, 0, inblk);
	parse_unit(cp, tok->val1, tcmds, consts, paths, npaths, 0, inblk);
	tvmcmd_vect_append(tcmds, tbycode_make((uint8_t)ins));
	treg_ctr_ddt_n(&cp->regctr, 2);
	treg_ctr_add(&cp->regctr);
}


/*===========================================================================*
 * 10. Statement Parsers
 *===========================================================================*/

void parse_return(
		tcp *cp,
		const ttoken *tok,
		tvmcmd_vect *tcmds,
		tconsts *consts,
		tstring **paths,
		uint_lexs npaths,
		int inblk)
{
	if (tok->nvals == 1) {
		if (tobj_ctr_obj_loc(&cp->tmpctr, tok->val1) !=
			tobj_ctr_obj_len_all(&cp->tmpctr))
			twarn(ErrCompile_ReturnTmpObj, "parse_return", "");
		parse_unit(cp, tok->val1, tcmds, consts, paths, npaths, 0, inblk);
	}
	tvmcmd_vect_append(tcmds, tbycode_make(OP_RET));
	treg_ctr_ddt_n(&cp->regctr, treg_ctr_get(&cp->regctr));
}

void parse_var(
		tcp *cp,
		const ttoken *tok,
		tvmcmd_vect *tcmds,
		tconsts *consts,
		tstring **paths,
		uint_lexs npaths,
		int inblk)
{
	uint_objs loc_tmp = tobj_ctr_obj_loc(&cp->tmpctr, tok->val1);
	if (loc_tmp < tobj_ctr_obj_len_cur(&cp->tmpctr))
		twarn(ErrCompile_DblVDeclare, "parse_var", tstring_cstr(tok->val1));

	uint_csts nameloc = UNDEF_NAMELOC;
	uint_objs loc = tobj_ctr_obj_create(
		&cp->objctr, tok->val1, inblk, consts, &nameloc);
	tvmcmd_vect_append(tcmds,
			   tbycode_make_lr(OP_VCRT, (uint16_t)nameloc, 1));

	if (tok->nvals == 3) {
		parse_unit(cp, tok->val3, tcmds, consts, paths, npaths, 0, inblk);
		tvmcmd_vect_append(
			tcmds, tbycode_make_lr(OP_POPCOV, (uint16_t)loc, 1));
		treg_ctr_ddt(&cp->regctr);
	}
}

void parse_let(
		tcp *cp,
		const ttoken *tok,
		tvmcmd_vect *tcmds,
		tconsts *consts,
		tstring **paths,
		uint_lexs npaths,
		int inblk)
{
	uint_objs loc_obj = tobj_ctr_obj_loc(&cp->objctr, tok->val1);
	if (loc_obj < tobj_ctr_obj_len_cur(&cp->objctr) &&
	    !tobj_ctr_is_preload(&cp->objctr, loc_obj))
		twarn(ErrCompile_DblVDeclare, "parse_let", tstring_cstr(tok->val1));

	uint_csts nameloc = UNDEF_NAMELOC;
	uint_objs loc_tmp = tobj_ctr_obj_create(
		&cp->tmpctr, tok->val1, 0, consts, &nameloc);
	tvmcmd_vect_append(tcmds,
			   tbycode_make_lr(OP_VCRT, (uint16_t)nameloc, 0));

	if (tok->nvals == 3) {
		parse_unit(cp, tok->val3, tcmds, consts, paths, npaths, 0, inblk);
		tvmcmd_vect_append(
			tcmds,
			tbycode_make_lr(OP_POPCOV, (uint16_t)loc_tmp, 0));
		treg_ctr_ddt(&cp->regctr);
	}
}

void parse_asg(
		tcp *cp,
		const ttoken *tok,
		tvmcmd_vect *tcmds,
		tconsts *consts,
		tstring **paths,
		uint_lexs npaths,
		int inblk)
{
	uint_objs loc = 0;
	uint_objs loc_env = tobj_ctr_obj_loc(&cp->objctr, tok->val1);
	uint_objs loc_tmp = tobj_ctr_obj_loc(&cp->tmpctr, tok->val1);
	int isenv = 0;

	if (loc_tmp != tobj_ctr_obj_len_all(&cp->tmpctr)) {
		loc = loc_tmp;
	} else if (loc_env != tobj_ctr_obj_len_all(&cp->objctr)) {
		if (tobj_ctr_is_preload(&cp->objctr, loc_env))
			twarn(ErrCompile_AsgDefault, "parse_asg", tstring_cstr(tok->val1));
		loc = loc_env;
		isenv = 1;
	} else
		twarn(ErrCompile_ObjUnfound, "parse_asg", tstring_cstr(tok->val1));

	parse_unit(cp, tok->val2, tcmds, consts, paths, npaths, 0, inblk);
	tvmcmd_vect_append(
		tcmds,
		tbycode_make_lr(OP_POPCOV, (uint16_t)loc, (uint16_t)isenv));
	treg_ctr_ddt(&cp->regctr);
}

void parse_eval(
		tcp *cp,
		const ttoken *tok,
		tvmcmd_vect *tcmds,
		tconsts *consts,
		tstring **paths,
		uint_lexs npaths,
		int inblk)
{
	tstring *receiver = NULL;
	tstring *method = NULL;
	if (split_pipeline_method(tok->val1, &receiver, &method)) {
		parse_unit(cp, receiver, tcmds, consts, paths, npaths, 0, inblk);
		uint_regs n = parse_params(
			cp, tok->val2, tcmds, consts, paths, npaths, inblk);
		parse_unit(cp, method, tcmds, consts, paths, npaths, 0, inblk);
		tvmcmd_vect_append(tcmds, tbycode_make_u(OP_EVAL, (uint32_t)(n + 1)));
		treg_ctr_ddt_n(&cp->regctr, 2 + n);
		treg_ctr_add(&cp->regctr);
		tstring_free(receiver);
		tstring_free(method);
		return;
	}

	uint_regs n = parse_params(
		cp, tok->val2, tcmds, consts, paths, npaths, inblk);
	parse_unit(cp, tok->val1, tcmds, consts, paths, npaths, 0, inblk);
	tvmcmd_vect_append(tcmds, tbycode_make_u(OP_EVAL, (uint32_t)n));
	treg_ctr_ddt_n(&cp->regctr, 1 + n);
	treg_ctr_add(&cp->regctr);
}

void parse_idx(
		tcp *cp,
		const ttoken *tok,
		tvmcmd_vect *tcmds,
		tconsts *consts,
		tstring **paths,
		uint_lexs npaths,
		int cleanstk,
		int inblk)
{
	if (tstring_empty(tok->val1)) {
		/* [obj_1, ...] -> list(obj_1, ...) */
		tstring *reform = tstring_new("list(");
		tstring_append_ts(reform, tok->val2);
		tstring_append_c(reform, ')');
		parse_unit(cp, reform, tcmds, consts, paths, npaths, cleanstk, inblk);
		tstring_free(reform);
	} else {
		tstring *idx_param = normalize_slice_param(tok->val1, tok->val2);
		uint_regs n = parse_params(
			cp, idx_param, tcmds, consts, paths, npaths, inblk);
		tstring_free(idx_param);
		parse_unit(cp, tok->val1, tcmds, consts, paths, npaths, 0, inblk);
		tvmcmd_vect_append(tcmds, tbycode_make_u(OP_IDXR, (uint32_t)n));
		treg_ctr_ddt_n(&cp->regctr, 1 + n);
		treg_ctr_add_n(&cp->regctr, 1);
	}
}

void parse_idx2(
		tcp *cp,
		const ttoken *tok,
		tvmcmd_vect *tcmds,
		tconsts *consts,
		tstring **paths,
		uint_lexs npaths,
		int inblk)
{
	tstring *str_lit = tstring_new("'");
	tstring_append_ts(str_lit, tok->val2);
	tstring_append_c(str_lit, '\'');
	parse_unit(cp, str_lit, tcmds, consts, paths, npaths, 0, inblk);
	tstring_free(str_lit);
	parse_unit(cp, tok->val1, tcmds, consts, paths, npaths, 0, inblk);
	tvmcmd_vect_append(tcmds, tbycode_make_u(OP_IDXR, 1));
	treg_ctr_ddt(&cp->regctr);
}

void parse_dict(
		tcp *cp,
		const ttoken *tok,
		tvmcmd_vect *tcmds,
		tconsts *consts,
		tstring **paths,
		uint_lexs npaths,
		int inblk)
{
	uint_regs n = parse_params(
		cp, tok->val1, tcmds, consts, paths, npaths, inblk);
	tvmcmd_vect_append(tcmds, tbycode_make_u(OP_PUSHDICT, (uint32_t)n));
	treg_ctr_ddt_n(&cp->regctr, n);
	treg_ctr_add(&cp->regctr);
}

void parse_if(
		tcp *cp,
		const ttoken *tok,
		tvmcmd_vect *tcmds,
		tconsts *consts,
		tstring **paths,
		uint_lexs npaths,
		int inblk)
{
	parse_unit(cp, tok->val1, tcmds, consts, paths, npaths, 0, inblk);
	tvmcmd_vect_append(tcmds, tbycode_make_u(OP_CJPFPOP, 0));
	uint_cmds loc_CJPFPOP = tvmcmd_vect_size32(tcmds) - 1;
	treg_ctr_ddt(&cp->regctr);

	tvmcmd_vect tcmds_blk;
	tvmcmd_vect_init(&tcmds_blk);
	parse_blk(cp, tok->val2, &tcmds_blk, consts, paths, npaths, 1, 1);

	/* Update CJPFPOP jump distance */
	uint_cmds blk_len = tvmcmd_vect_size32(&tcmds_blk);
	tcmds->data[loc_CJPFPOP] = tbycode_make_u(OP_CJPFPOP, blk_len + 1);

	/* Append block */
	tvmcmd_vect_insert_vect(tcmds, tvmcmd_vect_size32(tcmds), &tcmds_blk);
	tvmcmd_vect_append(tcmds, tbycode_make(OP_PASS));
	tvmcmd_vect_free(&tcmds_blk);
}

void parse_elif(
		tcp *cp,
		const ttoken *tok,
		tvmcmd_vect *tcmds,
		tconsts *consts,
		tstring **paths,
		uint_lexs npaths)
{
	if (tvmcmd_vect_size32(tcmds) > 0 &&
		tbycode_ins(tvmcmd_vect_back(tcmds)) == OP_PASS)
		tvmcmd_vect_pop_back(tcmds);

	tvmcmd_vect_append(tcmds, tbycode_make_u(OP_JPF, 0));
	uint_cmds loc_JPF = tvmcmd_vect_size32(tcmds) - 1;

	tvmcmd_vect tcmds_con;
	tvmcmd_vect_init(&tcmds_con);
	parse_unit(cp, tok->val1, &tcmds_con, consts, paths, npaths, 0, 1);
	tvmcmd_vect_insert_vect(tcmds, tvmcmd_vect_size32(tcmds), &tcmds_con);

	tvmcmd_vect_append(tcmds, tbycode_make_u(OP_CJPFPOP, 0));
	treg_ctr_ddt(&cp->regctr);
	uint_cmds loc_CJPFPOP = tvmcmd_vect_size32(tcmds) - 1;

	tvmcmd_vect tcmds_blk;
	tvmcmd_vect_init(&tcmds_blk);
	parse_blk(cp, tok->val2, &tcmds_blk, consts, paths, npaths, 1, 1);
	tvmcmd_vect_insert_vect(tcmds, tvmcmd_vect_size32(tcmds), &tcmds_blk);
	tvmcmd_vect_append(tcmds, tbycode_make(OP_PASS));

	tcmds->data[loc_JPF] =
		tbycode_make_u(OP_JPF,
				   tvmcmd_vect_size32(&tcmds_con) + 1 +
					   tvmcmd_vect_size32(&tcmds_blk));
	tcmds->data[loc_CJPFPOP] =
		tbycode_make_u(OP_CJPFPOP, tvmcmd_vect_size32(&tcmds_blk) + 1);

	tvmcmd_vect_free(&tcmds_con);
	tvmcmd_vect_free(&tcmds_blk);
}

void parse_else(
		tcp *cp,
		const ttoken *tok,
		tvmcmd_vect *tcmds,
		tconsts *consts,
		tstring **paths,
		uint_lexs npaths)
{
	if (tvmcmd_vect_size32(tcmds) > 0 &&
		tbycode_ins(tvmcmd_vect_back(tcmds)) == OP_PASS)
		tvmcmd_vect_pop_back(tcmds);

	tvmcmd_vect tcmds_blk;
	tvmcmd_vect_init(&tcmds_blk);
	parse_blk(cp, tok->val1, &tcmds_blk, consts, paths, npaths, 1, 1);
	tvmcmd_vect_append(
		tcmds, tbycode_make_u(OP_JPF, tvmcmd_vect_size32(&tcmds_blk)));
	tvmcmd_vect_insert_vect(tcmds, tvmcmd_vect_size32(tcmds), &tcmds_blk);
	tvmcmd_vect_free(&tcmds_blk);
}

void parse_while(
		tcp *cp,
		const ttoken *tok,
		tvmcmd_vect *tcmds,
		tconsts *consts,
		tstring **paths,
		uint_lexs npaths)
{
	uint_cmds ncmds_ori = tvmcmd_vect_size32(tcmds);
	tvmcmd_vect tcmds_blk;
	tvmcmd_vect_init(&tcmds_blk);

	parse_unit(cp, tok->val1, tcmds, consts, paths, npaths, 0, 1);
	treg_ctr_ddt(&cp->regctr);
	parse_blk(cp, tok->val2, &tcmds_blk, consts, paths, npaths, 1, 1);

	uint_cmds cjpfpop_n = tvmcmd_vect_size32(&tcmds_blk) + 1;
	tvmcmd_vect_append(tcmds, tbycode_make_u(OP_CJPFPOP, cjpfpop_n));
	tvmcmd_vect_insert_vect(tcmds, tvmcmd_vect_size32(tcmds), &tcmds_blk);

	uint_cmds jpb_n = 1 + tvmcmd_vect_size32(tcmds) - ncmds_ori;
	tvmcmd_vect_append(tcmds, tbycode_make_u(OP_JPB, jpb_n));
	tvmcmd_vect_free(&tcmds_blk);
}

void parse_for(
		tcp *cp,
		const ttoken *tok,
		tvmcmd_vect *tcmds,
		tconsts *consts,
		tstring **paths,
		uint_lexs npaths,
		int cleanstk,
		int inblk)
{
	tvmcmd_vect tcmds_blk;
	tvmcmd_vect_init(&tcmds_blk);

	parse_unit(cp, tok->val2, tcmds, consts, paths, npaths, 0, inblk);

	uint_objs loc = 0;
	int isenv = 0;
	uint_objs loc_left = tobj_ctr_obj_loc(&cp->objctr, tok->val1);
	uint_objs tmp_left = tobj_ctr_obj_loc(&cp->tmpctr, tok->val1);
	uint_objs nobjs = tobj_ctr_obj_len_cur(&cp->objctr);
	uint_objs ntmps = tobj_ctr_obj_len_cur(&cp->tmpctr);
	tstring *let_name = parse_for_let_name(tok->val1);

	if (let_name) {
		uint_objs loc_obj = tobj_ctr_obj_loc(&cp->objctr, let_name);
		uint_objs loc_tmp = tobj_ctr_obj_loc(&cp->tmpctr, let_name);
		if (loc_tmp < tobj_ctr_obj_len_cur(&cp->tmpctr))
			twarn(ErrCompile_DblVDeclare, "parse_for", tstring_cstr(let_name));
		if (loc_obj < tobj_ctr_obj_len_cur(&cp->objctr) &&
		    !tobj_ctr_is_preload(&cp->objctr, loc_obj))
			twarn(ErrCompile_DblVDeclare, "parse_for", tstring_cstr(let_name));
		uint_csts nameloc = UNDEF_NAMELOC;
		loc = tobj_ctr_obj_create(&cp->tmpctr, let_name, 0, consts, &nameloc);
		tvmcmd_vect_append(tcmds,
				   tbycode_make_lr(OP_VCRT, (uint16_t)nameloc, 0));
		tstring_free(let_name);
	} else if (tmp_left != tobj_ctr_obj_len_all(&cp->tmpctr))
		loc = tmp_left;
	else if (loc_left != tobj_ctr_obj_len_all(&cp->objctr)) {
		loc = loc_left;
		isenv = 1;
	} else {
		parse_unit(cp, tok->val1, tcmds, consts, paths, npaths, cleanstk, inblk);
		if (tobj_ctr_obj_len_cur(&cp->objctr) >= nobjs + 1)
			twarn(ErrCompile_InBlkVarDef,
				  "parse_for",
				  tstring_cstr(tok->val1));
		if (tobj_ctr_obj_len_cur(&cp->tmpctr) < ntmps + 1)
			twarn(ErrCompile_ObjUnfound, "parse_for", "");
		loc = tobj_ctr_obj_len_cur(&cp->tmpctr) - 1;
	}

	tvmcmd_vect_append(
		tcmds,
		tbycode_make_lr(OP_LOOPAS, (uint16_t)loc, (uint16_t)isenv));
	treg_ctr_add(&cp->regctr);
	treg_ctr_ddt(&cp->regctr);

	parse_blk(cp, tok->val3, &tcmds_blk, consts, paths, npaths, 1, 1);

	uint_cmds cjpfpop_n = 1 + tvmcmd_vect_size32(&tcmds_blk);
	tvmcmd_vect_append(tcmds, tbycode_make_u(OP_CJPFPOP, cjpfpop_n));
	tvmcmd_vect_insert_vect(tcmds, tvmcmd_vect_size32(tcmds), &tcmds_blk);

	uint_cmds jpb_n = 3 + tvmcmd_vect_size32(&tcmds_blk);
	tvmcmd_vect_append(tcmds, tbycode_make_u(OP_JPB, jpb_n));
	tvmcmd_vect_append(tcmds, tbycode_make_lr(OP_POPN, 1, 0));
	treg_ctr_ddt(&cp->regctr);

	uint_objs newtmps = tobj_ctr_obj_len_cur(&cp->tmpctr) - ntmps;
	if (newtmps > 0) {
		tobj_ctr_obj_del_last_n(&cp->tmpctr, newtmps);
		tvmcmd_vect_append(
			tcmds, tbycode_make_u(OP_TMPDEL, (uint32_t)newtmps));
	}
	tvmcmd_vect_free(&tcmds_blk);
}

void parse_func(
		tcp *cp,
		const ttoken *tok,
		tvmcmd_vect *tcmds,
		tconsts *consts,
		tstring **paths,
		uint_lexs npaths)
{
	uint_regs nparams = UNDEF_NPARAMS;
	tstring **params = NULL;
	uint_regs nparams_actual = 0;

	if (!tstring_eq_cstr(tok->val1, "...")) {
		split_params_ts(tok->val1, &params, &nparams_actual);
		nparams = nparams_actual;
	}

	/* Create child compiler */
	tcp new_syner;
	tcp_init_preload(&new_syner,
			 params,
			 (uint_objs)nparams_actual,
			 &cp->objctr,
			 cp->interactive);

	tvmcmd_vect tcmdblk;
	tvmcmd_vect_init(&tcmdblk);
	parse_blk(&new_syner, tok->val2, &tcmdblk, consts, paths, npaths, 1, 0);
	tcinfo info = tcp_get_compile_info(&new_syner);

	uint_cmds ncmds = tvmcmd_vect_size32(&tcmdblk);
	tvmcmd_vect_append(tcmds,
			   tbycode_make_u(OP_PUSHINFO, (uint32_t)info.obj_max));
	tvmcmd_vect_append(tcmds,
			   tbycode_make_u(OP_PUSHINFO, (uint32_t)info.tmp_max));
	tvmcmd_vect_append(tcmds,
			   tbycode_make_u(OP_PUSHINFO, (uint32_t)info.reg_max));
	tvmcmd_vect_append(tcmds,
			   tbycode_make_u(OP_PUSHINFO, (uint32_t)nparams));
	treg_ctr_add_n(&cp->regctr, 4);
	tvmcmd_vect_append(tcmds, tbycode_make_u(OP_PUSHF, ncmds));
	treg_ctr_ddt_n(&cp->regctr, 4);
	treg_ctr_add(&cp->regctr);

	tvmcmd_vect_insert_vect(tcmds, tvmcmd_vect_size32(tcmds), &tcmdblk);
	tvmcmd_vect_free(&tcmdblk);
	tcp_free(&new_syner);
	split_params_free(params, nparams_actual);
}

void parse_kappa(
		tcp *cp,
		const ttoken *tok,
		tvmcmd_vect *tcmds,
		tconsts *consts,
		tstring **paths,
		uint_lexs npaths)
{
	tcp new_syner;
	tcp_init(&new_syner, &cp->objctr, cp->interactive);

	tvmcmd_vect tcmdblk;
	tvmcmd_vect_init(&tcmdblk);
	parse_unit(&new_syner, tok->val1, &tcmdblk, consts, paths, npaths, 0, 0);
	tvmcmd_vect_append(&tcmdblk, tbycode_make(OP_RET));
	tcinfo info = tcp_get_compile_info(&new_syner);

	uint_cmds ncmds = tvmcmd_vect_size32(&tcmdblk);
	tvmcmd_vect_append(tcmds,
			   tbycode_make_u(OP_PUSHINFO, (uint32_t)info.obj_max));
	tvmcmd_vect_append(tcmds,
			   tbycode_make_u(OP_PUSHINFO, (uint32_t)info.tmp_max));
	tvmcmd_vect_append(tcmds,
			   tbycode_make_u(OP_PUSHINFO, (uint32_t)info.reg_max));
	tvmcmd_vect_append(
		tcmds, tbycode_make_u(OP_PUSHINFO, (uint32_t)UNDEF_NPARAMS));
	treg_ctr_add_n(&cp->regctr, 4);
	tvmcmd_vect_append(tcmds, tbycode_make_u(OP_PUSHF, ncmds));
	treg_ctr_ddt_n(&cp->regctr, 4);
	treg_ctr_add(&cp->regctr);
	tvmcmd_vect_insert_vect(tcmds, tvmcmd_vect_size32(tcmds), &tcmdblk);

	tvmcmd_vect_free(&tcmdblk);
	tcp_free(&new_syner);
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

static void set_unit_error_context(
		const tstring *file,
		const tstring *source,
		const tstring *unit,
		size_t *cursor,
		uint64_t *line_cursor)
{
	uint64_t line = source_line_for_unit(source, unit, cursor, line_cursor);
	terror_set_source_context(tstring_cstr(unit));
	if (file)
		terror_set_file_context(tstring_cstr(file), line, 0);
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
		tstring **units = NULL;
		uint32_t n_units = 0;
		size_t cursor = 0;
		uint64_t line_cursor = 1;
		lex_str_ts(source, &units, &n_units);
		for (uint32_t i = 0; i < n_units; i++) {
			set_unit_error_context(file, source, units[i], &cursor, &line_cursor);
			parse_unit(cp, units[i], tcmds, consts, paths, npaths, 1, 0);
		}
		lex_str_free(units, n_units);
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

	tstring **units = NULL;
	uint32_t n_units = 0;
	size_t cursor = 0;
	uint64_t line_cursor = 1;
	lex_str_ts(code, &units, &n_units);
	for (uint32_t i = 0; i < n_units; i++) {
		set_unit_error_context(file, code, units[i], &cursor, &line_cursor);
		parse_unit(cp, units[i], tcmds, consts, paths, npaths, 1, 0);
	}
	lex_str_free(units, n_units);
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

	/* Recursively compile the imported file (simplified) */
	FILE *f = fopen(tstring_cstr(file), "r");
	if (!f)
		twarn(ErrCompile_UnfoundFile, "parse_import", tstring_cstr(file));

	tcp sub_comp;
	tstring **default_names =
		tobj_ctr_first_n_objs(&cp->objctr, cp->n_default_objs);
	tcp_init_preload(
		&sub_comp, default_names, cp->n_default_objs, NULL, cp->interactive);

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

	tvmcmd_vect_free(&sub_cmds);
	tconsts_free(&sub_consts);
	tcp_free(&sub_comp);
	tobj_ctr_first_n_objs_free(default_names, cp->n_default_objs);
	tstring_free(file);
}


/*===========================================================================*
 * 11. Token Dispatcher
 *===========================================================================*/

void parse_token(
		tcp *cp,
		const ttoken *tok,
		tvmcmd_vect *tcmds,
		tconsts *consts,
		tstring **paths,
		uint_lexs npaths,
		int cleanstk,
		int inblk)
{
	switch (tok->type) {
	case token_continue:
		tvmcmd_vect_append(tcmds, tbycode_make(OP_CONTI));
		break;
	case token_break:
		tvmcmd_vect_append(tcmds, tbycode_make(OP_BREAK));
		break;
	case token_return:
		parse_return(cp, tok, tcmds, consts, paths, npaths, inblk);
		break;
	case token_var:
		parse_var(cp, tok, tcmds, consts, paths, npaths, inblk);
		break;
	case token_let:
		parse_let(cp, tok, tcmds, consts, paths, npaths, inblk);
		break;
	case token_import:
		parse_import(cp, tok, tcmds, consts, paths, npaths, inblk);
		break;
	case token_while:
		parse_while(cp, tok, tcmds, consts, paths, npaths);
		break;
	case token_for:
		parse_for(cp, tok, tcmds, consts, paths, npaths, cleanstk, inblk);
		break;
	case token_if:
		parse_if(cp, tok, tcmds, consts, paths, npaths, inblk);
		break;
	case token_elif:
		parse_elif(cp, tok, tcmds, consts, paths, npaths);
		break;
	case token_else:
		parse_else(cp, tok, tcmds, consts, paths, npaths);
		break;
	case token_asg:
		parse_asg(cp, tok, tcmds, consts, paths, npaths, inblk);
		break;
	case token_idxl: {
		/* Left-value index assignment */
		uint_objs loc_env = tobj_ctr_obj_loc(&cp->objctr, tok->val1);
		uint_objs loc_tmp = tobj_ctr_obj_loc(&cp->tmpctr, tok->val1);
		uint_objs loc = 0;
		int isenv = 0;
		if (loc_tmp != tobj_ctr_obj_len_all(&cp->tmpctr))
			loc = loc_tmp;
		else if (loc_env != tobj_ctr_obj_len_all(&cp->objctr)) {
			loc = loc_env;
			isenv = 1;
		} else
			twarn(ErrCompile_ObjUnfound,
				  "parse_idxl",
				  tstring_cstr(tok->val1));

		parse_unit(cp, tok->val3, tcmds, consts, paths, npaths, 0, inblk);
		uint_regs n = parse_params(
			cp, tok->val2, tcmds, consts, paths, npaths, inblk);
		tvmcmd_vect_append(tcmds,
			tbycode_make_lbi(
				OP_IDXL,
				(uint16_t)loc,
				(uint8_t)n,
				(uint8_t)isenv
			)
		);
		treg_ctr_ddt_n(&cp->regctr, n + 1);
	} break;

	/* Binary operators */
	case token_in:
		parse_binop_simple(cp, OP_IN, tok, tcmds, consts, paths, npaths, inblk);
		break;
	case token_pair:
		parse_binop_simple(cp, OP_PAIR, tok, tcmds, consts, paths, npaths, inblk);
		break;
	case token_to:
		parse_binop_simple(cp, OP_TO, tok, tcmds, consts, paths, npaths, inblk);
		break;
	case token_and:
		parse_binop_short_circuit(
			cp, OP_AND, tok, tcmds, consts, paths, npaths, inblk);
		break;
	case token_or:
		parse_binop_short_circuit(
			cp, OP_OR, tok, tcmds, consts, paths, npaths, inblk);
		break;
	case token_eq:
		parse_binop_opt(cp, OP_EQ, tok, tcmds, consts, paths, npaths, inblk);
		break;
	case token_ne:
		parse_binop_opt(cp, OP_NE, tok, tcmds, consts, paths, npaths, inblk);
		break;
	case token_ge:
		parse_binop_opt(cp, OP_GE, tok, tcmds, consts, paths, npaths, inblk);
		break;
	case token_le:
		parse_binop_opt(cp, OP_LE, tok, tcmds, consts, paths, npaths, inblk);
		break;
	case token_sg:
		parse_binop_opt(cp, OP_SG, tok, tcmds, consts, paths, npaths, inblk);
		break;
	case token_sl:
		parse_binop_opt(cp, OP_SL, tok, tcmds, consts, paths, npaths, inblk);
		break;
	case token_add:
		parse_binop_opt(cp, OP_ADD, tok, tcmds, consts, paths, npaths, inblk);
		break;
	case token_sub:
		parse_binop_opt(cp, OP_SUB, tok, tcmds, consts, paths, npaths, inblk);
		break;
	case token_mul:
		parse_binop_opt(cp, OP_MUL, tok, tcmds, consts, paths, npaths, inblk);
		break;
	case token_div:
		parse_binop_opt(cp, OP_DIV, tok, tcmds, consts, paths, npaths, inblk);
		break;
	case token_mod:
		parse_binop_opt(cp, OP_MOD, tok, tcmds, consts, paths, npaths, inblk);
		break;
	case token_mmul:
		parse_binop_opt(cp, OP_MMUL, tok, tcmds, consts, paths, npaths, inblk);
		break;
	case token_pow:
		parse_binop_opt(cp, OP_POW, tok, tcmds, consts, paths, npaths, inblk);
		break;

	/* Indexing / evaluation */
	case token_eval:
		parse_eval(cp, tok, tcmds, consts, paths, npaths, inblk);
		break;
	case token_idx:
		parse_idx(
			cp, tok, tcmds, consts, paths, npaths, cleanstk, inblk);
		break;
	case token_idx2:
		parse_idx2(cp, tok, tcmds, consts, paths, npaths, inblk);
		break;

	/* Literal values */
	case token_true:
		tvmcmd_vect_append(tcmds, tbycode_make_u(OP_PUSHB, 1));
		treg_ctr_add(&cp->regctr);
		break;
	case token_false:
		tvmcmd_vect_append(tcmds, tbycode_make_u(OP_PUSHB, 0));
		treg_ctr_add(&cp->regctr);
		break;
	case token_this:
		tvmcmd_vect_append(tcmds, tbycode_make(OP_THIS));
		treg_ctr_add(&cp->regctr);
		break;
	case token_base:
		tvmcmd_vect_append(tcmds, tbycode_make(OP_BASE));
		treg_ctr_add(&cp->regctr);
		break;
	case token_sstr:
	case token_dstr:
		tvmcmd_vect_append(
			tcmds,
			tbycode_make_u(OP_PUSHS,
					   (uint32_t)tconsts_add_str_const(
						   consts, tstring_cstr(tok->val1))));
		treg_ctr_add(&cp->regctr);
		break;
	case token_dict:
		parse_dict(cp, tok, tcmds, consts, paths, npaths, inblk);
		break;
	case token_func:
		parse_func(cp, tok, tcmds, consts, paths, npaths);
		break;
	case token_kappa:
		parse_kappa(cp, tok, tcmds, consts, paths, npaths);
		break;
	case token_v:
		parse_v(cp, tok, tcmds, consts);
		break;
	}
}


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
	tstring **units = NULL;
	uint32_t n_units = 0;
	lex_str_ts(src, &units, &n_units);

	uint_objs ntmps = tobj_ctr_obj_len_all(&cp->tmpctr);

	uint32_t i;
	size_t cursor = 0;
	uint64_t line_cursor = 1;
	uint64_t base_line = terror_current_line_context();
	const char *file_ctx = terror_current_file_context();
	tstring *file_ctx_copy = file_ctx ? tstring_new(file_ctx) : NULL;
	for (i = 0; i < n_units; i++) {
		uint64_t rel_line =
			source_line_for_unit(src, units[i], &cursor, &line_cursor);
		terror_set_source_context(tstring_cstr(units[i]));
		if (file_ctx_copy) {
			uint64_t abs_line = base_line ?
				base_line + rel_line - 1 : rel_line;
			terror_set_file_context(tstring_cstr(file_ctx_copy), abs_line, 0);
		}
		parse_unit(cp, units[i], tcmds, consts, paths, npaths, cleanstk, inblk);
	}
	tstring_free(file_ctx_copy);

	uint_objs newtmps = tobj_ctr_obj_len_all(&cp->tmpctr) - ntmps;
	if (newtmps > 0) {
		tobj_ctr_obj_del_last_n(&cp->tmpctr, newtmps);
		tvmcmd_vect_append(
			tcmds, tbycode_make_u(OP_TMPDEL, (uint32_t)newtmps));
	}

	lex_str_free(units, n_units);
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
	tstring **units = NULL;
	uint32_t n_units = 0;
	tstring *source = read_source_to_tstring(f);
	size_t cursor = 0;
	uint64_t line_cursor = 1;
	lex_str_ts(source, &units, &n_units);

	uint32_t i;
	for (i = 0; i < n_units; i++) {
		set_unit_error_context(NULL, source, units[i], &cursor, &line_cursor);
		parse_unit(cp, units[i], tcmds, consts, paths, npaths, 1, 0);
	}

	lex_str_free(units, n_units);
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
	cmd_ts = stripped_ts;
	if (tstring_empty(cmd_ts)) {
		tstring_free(cmd_ts);
		return tcp_get_compile_info(cp);
	}

	/* Lexing */
	ttoken *tokens = NULL;
	uint32_t ntokens = 0;
	get_tokens_ts(cmd_ts, &tokens, &ntokens);

	uint_regs regs_ori = treg_ctr_get(&cp->regctr);

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
		tstring **units = NULL;
		uint32_t n_units = 0;
		size_t cursor = 0;
		uint64_t line_cursor = 1;
		lex_str_ts(code, &units, &n_units);
		uint32_t i;
		for (i = 0; i < n_units; i++) {
			set_unit_error_context(file, code, units[i], &cursor, &line_cursor);
			parse_unit(cp, units[i], &tcmds, &consts, local_paths, nlocal_paths, 1, 0);
		}
		lex_str_free(units, n_units);
		tstring_free(line);
		tstring_free(code);
	} else {
		tstring *source = read_source_to_tstring(f);
		tstring **units = NULL;
		uint32_t n_units = 0;
		size_t cursor = 0;
		uint64_t line_cursor = 1;
		lex_str_ts(source, &units, &n_units);
		for (uint32_t i = 0; i < n_units; i++) {
			set_unit_error_context(file, source, units[i], &cursor, &line_cursor);
			parse_unit(cp, units[i], &tcmds, &consts, local_paths, nlocal_paths, 1, 0);
		}
		lex_str_free(units, n_units);
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
