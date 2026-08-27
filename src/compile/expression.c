/** Internal compiler implementation. */
#include "internal.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

int str_to_long_int(const tstring *cmds, long *it)
{
	const char *str = tstring_cstr(cmds);
	size_t len = tstring_len(cmds);
	if (len == 0 || (len > 1 && str[0] == '0') ||
	    (str[0] < '0' || str[0] > '9'))
		return 0;
	for (size_t i = 1; i < len; i++)
		if (!isdigit((unsigned char)str[i]))
			return 0;
	errno = 0;
	char *end = NULL;
	long value = strtol(str, &end, 10);
	if (errno == ERANGE || end != str + len)
		twarn(ErrCompile_InvalidLiter, "str_to_long_int", str);
	*it = value;
	return 1;
}

int str_to_float(const tstring *cmds, double *dt)
{
	const char *str = tstring_cstr(cmds);
	size_t len = tstring_len(cmds);
	size_t i = 0;
	size_t digits_before = 0;
	size_t digits_after = 0;
	int has_dot = 0;
	int has_exponent = 0;
	while (i < len && isdigit((unsigned char)str[i])) {
		digits_before++;
		i++;
	}
	if (i < len && str[i] == '.') {
		has_dot = 1;
		i++;
		while (i < len && isdigit((unsigned char)str[i])) {
			digits_after++;
			i++;
		}
	}
	if (digits_before + digits_after == 0)
		return 0;
	if (i < len && (str[i] == 'e' || str[i] == 'E')) {
		has_exponent = 1;
		i++;
		if (i < len && (str[i] == '+' || str[i] == '-'))
			i++;
		size_t exponent_start = i;
		while (i < len && isdigit((unsigned char)str[i]))
			i++;
		if (i == exponent_start)
			return 0;
	}
	if (i != len || (!has_dot && !has_exponent))
		return 0;
	errno = 0;
	char *end = NULL;
	double value = strtod(str, &end);
	if (errno == ERANGE || end != str + len)
		twarn(ErrCompile_InvalidLiter, "str_to_float", str);
	*dt = value;
	return 1;
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

void compile_emit_reference(tcp *cp, const tstring *name,
			    tvmcmd_vect *tcmds, tconsts *consts)
{
	if (tstring_empty(name))
		twarn(ErrCompile_InvalidLiter, "compile_emit_reference", "empty name");

	long it;
	double dt;

	if (str_to_long_int(name, &it)) {
		uint_csts loc = tconsts_add_int_const(consts, it);
		tvmcmd_vect_append(tcmds, tbycode_make_u(OP_PUSHI, loc));
		treg_ctr_add(&cp->regctr);
	} else if (str_to_float(name, &dt)) {
		uint_csts loc = tconsts_add_float_const(consts, dt);
		tvmcmd_vect_append(tcmds, tbycode_make_u(OP_PUSHFLT, loc));
		treg_ctr_add(&cp->regctr);
	} else {
		uint_objs loc_tmp = tobj_ctr_obj_loc(&cp->tmpctr, name);
		tobj_ctr_addr env_addr;

		if (loc_tmp < tobj_ctr_obj_len_all(&cp->tmpctr)) {
			tvmcmd_vect_append(tcmds,
					   tbycode_make_lr(OP_PUSHX,
							   (uint16_t)loc_tmp,
							   tpushx_tmp_addr()));
			treg_ctr_add(&cp->regctr);
		} else if (tobj_ctr_obj_addr(&cp->objctr, name, &env_addr)) {
			uint16_t r = env_addr.depth == 0 ?
				tpushx_local_addr() :
				tpushx_upval_addr(env_addr.depth);
			tvmcmd_vect_append(tcmds,
					   tbycode_make_lr(OP_PUSHX,
							   (uint16_t)env_addr.slot,
							   r));
			treg_ctr_add(&cp->regctr);
		} else
			twarn(ErrCompile_InvalidLiter, "compile_emit_reference",
			      tstring_cstr(name));
	}
}

void parse_v(tcp *cp, const ttoken *tok, tvmcmd_vect *tcmds, tconsts *consts)
{
	compile_emit_reference(cp, tok->val1, tcmds, consts);
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
