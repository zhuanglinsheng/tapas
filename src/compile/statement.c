/** Internal compiler implementation. */
#include "internal.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

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
	ttypeval *annotation = compile_resolve_annotation(cp, tok->val2);
	ttypeval *static_value = NULL;
	ttypeval *inferred = compile_infer_expr_type(cp, tok->val3, &static_value);
	if (annotation && inferred &&
	    !compile_type_assignable(inferred, annotation))
		twarn(ErrCompile_Other, "parse_var", "initializer Type mismatch");
	/* A mutable binding can contain a Type value, but is never a static Type
	 * binding usable by annotations. */
	ttypeval_release(static_value);
	static_value = NULL;

	uint_objs loc_tmp = tobj_ctr_obj_loc(&cp->tmpctr, tok->val1);
	if (loc_tmp < tobj_ctr_obj_len_cur(&cp->tmpctr))
		twarn(ErrCompile_DblVDeclare, "parse_var", tstring_cstr(tok->val1));

	uint_csts nameloc = UNDEF_NAMELOC;
	uint_objs loc = tobj_ctr_obj_create(
		&cp->objctr, tok->val1, inblk, consts, &nameloc);
	tcompile_set_metadata(&cp->objctr, loc,
			      annotation ? annotation : inferred,
			      NULL, annotation != NULL);
	tvmcmd_vect_append(tcmds,
			   tbycode_make_lr(OP_VCRT, (uint16_t)nameloc, 1));

	if (tok->nvals == 3) {
		parse_unit(cp, tok->val3, tcmds, consts, paths, npaths, 0, inblk);
		tvmcmd_vect_append(
			tcmds, tbycode_make_lr(OP_POPCOV, (uint16_t)loc, 1));
		treg_ctr_ddt(&cp->regctr);
	}
	ttypeval_release(annotation);
	ttypeval_release(inferred);
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
	ttypeval *annotation = compile_resolve_annotation(cp, tok->val2);
	ttypeval *static_value = NULL;
	ttypeval *inferred = compile_infer_expr_type(cp, tok->val3, &static_value);
	if (annotation && inferred &&
	    !compile_type_assignable(inferred, annotation))
		twarn(ErrCompile_Other, "parse_let", "initializer Type mismatch");

	uint_objs loc_obj = tobj_ctr_obj_loc(&cp->objctr, tok->val1);
	if (loc_obj < tobj_ctr_obj_len_cur(&cp->objctr) &&
	    !tobj_ctr_is_preload(&cp->objctr, loc_obj))
		twarn(ErrCompile_DblVDeclare, "parse_let", tstring_cstr(tok->val1));

	uint_csts nameloc = UNDEF_NAMELOC;
	uint_objs loc_tmp = tobj_ctr_obj_create(
		&cp->tmpctr, tok->val1, 0, consts, &nameloc);
	tcompile_set_metadata(&cp->tmpctr, loc_tmp,
			      annotation ? annotation : inferred,
			      static_value, annotation != NULL);
	tvmcmd_vect_append(tcmds,
			   tbycode_make_lr(OP_VCRT, (uint16_t)nameloc, 0));

	if (tok->nvals == 3) {
		parse_unit(cp, tok->val3, tcmds, consts, paths, npaths, 0, inblk);
		tvmcmd_vect_append(
			tcmds,
			tbycode_make_lr(OP_POPCOV, (uint16_t)loc_tmp, 0));
		treg_ctr_ddt(&cp->regctr);
	}
	ttypeval_release(annotation);
	ttypeval_release(inferred);
	ttypeval_release(static_value);
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
	tobj_ctr *owner = NULL;
	uint_objs owner_slot = 0;

	if (loc_tmp != tobj_ctr_obj_len_all(&cp->tmpctr)) {
		loc = loc_tmp;
		tcompile_find_binding(&cp->tmpctr, tok->val1, &owner, &owner_slot);
	} else if (loc_env != tobj_ctr_obj_len_all(&cp->objctr)) {
		if (tobj_ctr_is_preload(&cp->objctr, loc_env))
			twarn(ErrCompile_AsgDefault, "parse_asg", tstring_cstr(tok->val1));
		loc = loc_env;
		isenv = 1;
		tcompile_find_binding(&cp->objctr, tok->val1, &owner, &owner_slot);
	} else
		twarn(ErrCompile_ObjUnfound, "parse_asg", tstring_cstr(tok->val1));

	ttypeval *actual = compile_infer_expr_type(cp, tok->val2, NULL);
	if (owner && owner->bindings[owner_slot].has_annotation && actual &&
	    !compile_type_assignable(actual, owner->bindings[owner_slot].value_type))
		twarn(ErrCompile_Other, "parse_asg", "assigned Type mismatch");
	if (owner && !owner->bindings[owner_slot].has_annotation)
		tcompile_set_metadata(owner, owner_slot, actual, NULL, 0);
	ttypeval_release(actual);

	parse_unit(cp, tok->val2, tcmds, consts, paths, npaths, 0, inblk);
	tvmcmd_vect_append(
		tcmds,
		tbycode_make_lr(OP_POPCOV, (uint16_t)loc, (uint16_t)isenv));
	treg_ctr_ddt(&cp->regctr);
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
	cp->in_loop++;
	parse_blk(cp, tok->val2, &tcmds_blk, consts, paths, npaths, 1, 1);
	cp->in_loop--;

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

	cp->in_loop++;
	parse_blk(cp, tok->val3, &tcmds_blk, consts, paths, npaths, 1, 1);
	cp->in_loop--;

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
	/* Keep accepting the non-normative #{ expression } compatibility form. */
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

