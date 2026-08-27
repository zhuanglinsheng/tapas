/** Internal compiler implementation. */
#include "internal.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

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
		if (cp->in_loop == 0)
			twarn(ErrCompile_Other, "parse_token", "continue outside loop");
		tvmcmd_vect_append(tcmds, tbycode_make(OP_CONTI));
		break;
	case token_break:
		if (cp->in_loop == 0)
			twarn(ErrCompile_Other, "parse_token", "break outside loop");
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
	case token_band:
		parse_binop_opt(cp, OP_BAND, tok, tcmds, consts, paths, npaths, inblk);
		break;
	case token_bor:
		parse_binop_opt(cp, OP_BOR, tok, tcmds, consts, paths, npaths, inblk);
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
	case token_pos:
	case token_neg:
		parse_unit(cp, tok->val1, tcmds, consts, paths, npaths, 0, inblk);
		tvmcmd_vect_append(
			tcmds, tbycode_make(tok->type == token_pos ? OP_POS : OP_NEG));
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


