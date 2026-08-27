#ifndef TAPAS_COMPILE_INTERNAL_H
#define TAPAS_COMPILE_INTERNAL_H

#include "tapas/compile/compiler.h"
#include "tapas/compile/frontend.h"
#include "tapas/runtime/ttype.h"

/* Symbol and static-analysis metadata shared by compiler phases. */
int tcompile_find_binding(tobj_ctr *c, const tstring *name,
			  tobj_ctr **owner, uint_objs *slot);
void tcompile_set_metadata(tobj_ctr *c, uint_objs slot,
			   ttypeval *value_type, ttypeval *type_value,
			   int has_annotation);
void tcompile_set_field_order(tobj_ctr *c, uint_objs slot,
			      tstring *const *field_order, uint_objs count);
void tcompile_module_interface_free(tcompile_module_interface *interface);
const tcompile_export *tcompile_module_export(
	const tcompile_module_interface *interface, const char *name);
tcompile_module_interface *tcompile_extract_module_interface(
	tcp *cp, const tfrontend *frontend);
ttypeval *compile_resolve_annotation(tcp *cp, const tstring *annotation);
ttypeval *compile_infer_expr_type(tcp *cp, const tstring *expr,
				  ttypeval **static_value);
int compile_type_assignable(const ttypeval *actual, const ttypeval *target);

/* The statement entry point is a compatibility probe used only by parse_unit:
 * it returns zero without modifying compiler state when legacy syntax is not
 * represented by the AST. The module entry point is the authoritative
 * production path for source files and blocks. */
int tcompile_try_ast_statement(tcp *cp, const tstring *source,
			       tvmcmd_vect *tcmds, tconsts *consts,
			       tstring **paths, uint_lexs npaths,
			       int cleanstk, int inblk);
int tcompile_try_ast_module(tcp *cp, const tstring *source,
			    tvmcmd_vect *tcmds, tconsts *consts,
			    tstring **paths, uint_lexs npaths,
			    int inblk);

/* Expression bytecode generation. */
void compile_emit_reference(tcp *cp, const tstring *name,
			    tvmcmd_vect *tcmds, tconsts *consts);
void parse_v(tcp *cp, const ttoken *tok,
	     tvmcmd_vect *tcmds, tconsts *consts);
void parse_binop_split(tcp *cp, int ins, const tbin_expr *expr,
		       tvmcmd_vect *tcmds, tconsts *consts,
		       tstring **paths, uint_lexs npaths, int inblk);
void parse_binop_opt(tcp *cp, int ins, const ttoken *tok,
		     tvmcmd_vect *tcmds, tconsts *consts,
		     tstring **paths, uint_lexs npaths, int inblk);
void parse_binop_short_circuit(tcp *cp, int ins, const ttoken *tok,
			       tvmcmd_vect *tcmds, tconsts *consts,
			       tstring **paths, uint_lexs npaths, int inblk);
void parse_binop_simple(tcp *cp, int ins, const ttoken *tok,
			tvmcmd_vect *tcmds, tconsts *consts,
			tstring **paths, uint_lexs npaths, int inblk);
void parse_eval(tcp *cp, const ttoken *tok,
		tvmcmd_vect *tcmds, tconsts *consts,
		tstring **paths, uint_lexs npaths, int inblk);
void parse_idx(tcp *cp, const ttoken *tok,
	       tvmcmd_vect *tcmds, tconsts *consts,
	       tstring **paths, uint_lexs npaths, int cleanstk, int inblk);
void parse_idx2(tcp *cp, const ttoken *tok,
		tvmcmd_vect *tcmds, tconsts *consts,
		tstring **paths, uint_lexs npaths, int inblk);
void parse_dict(tcp *cp, const ttoken *tok,
		tvmcmd_vect *tcmds, tconsts *consts,
		tstring **paths, uint_lexs npaths, int inblk);

/* Statement bytecode generation. */
#define TAPAS_PARSE_STATEMENT_DECL(name)                                      \
	void name(tcp *cp, const ttoken *tok, tvmcmd_vect *tcmds,              \
		  tconsts *consts, tstring **paths, uint_lexs npaths, int inblk)

TAPAS_PARSE_STATEMENT_DECL(parse_return);
TAPAS_PARSE_STATEMENT_DECL(parse_var);
TAPAS_PARSE_STATEMENT_DECL(parse_let);
TAPAS_PARSE_STATEMENT_DECL(parse_asg);
TAPAS_PARSE_STATEMENT_DECL(parse_if);

#undef TAPAS_PARSE_STATEMENT_DECL

#define TAPAS_PARSE_CONTROL_DECL(name)                                        \
	void name(tcp *cp, const ttoken *tok, tvmcmd_vect *tcmds,              \
		  tconsts *consts, tstring **paths, uint_lexs npaths)

TAPAS_PARSE_CONTROL_DECL(parse_elif);
TAPAS_PARSE_CONTROL_DECL(parse_else);
TAPAS_PARSE_CONTROL_DECL(parse_while);
TAPAS_PARSE_CONTROL_DECL(parse_func);
TAPAS_PARSE_CONTROL_DECL(parse_kappa);

#undef TAPAS_PARSE_CONTROL_DECL

void parse_for(tcp *cp, const ttoken *tok,
	       tvmcmd_vect *tcmds, tconsts *consts,
	       tstring **paths, uint_lexs npaths, int cleanstk, int inblk);
void parse_import(tcp *cp, const ttoken *tok,
		  tvmcmd_vect *tcmds, tconsts *consts,
		  tstring **paths, uint_lexs npaths, int inblk);

/* Token dispatch and source orchestration. */
void parse_token(tcp *cp, const ttoken *tok,
		 tvmcmd_vect *tcmds, tconsts *consts,
		 tstring **paths, uint_lexs npaths, int cleanstk, int inblk);
void clean_stk(tcp *cp, tvmcmd_vect *tcmds, int isroot, uint_regs regs_ori);

#endif /* TAPAS_COMPILE_INTERNAL_H */
