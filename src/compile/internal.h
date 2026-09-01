#ifndef TAPAS_COMPILE_INTERNAL_H
#define TAPAS_COMPILE_INTERNAL_H

#include "tapas/compile/compiler.h"
#include "tapas/compile/frontend.h"
#include "tapas/runtime/ttype.h"

int str_to_long_int(const tstring *literal, long *value);
int str_to_float(const tstring *literal, double *value);

/* These values exist only while a loop body is being emitted. The resolver
 * replaces every marker with a jump before a bytecode wrapper is created. */
enum {
	TCOMPILE_BREAK_MARK = 62,
	TCOMPILE_CONTINUE_MARK = 63
};

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
ttypeval *compile_type_from_static(const tstatic_type_arena *arena,
				   tstatic_type_id id);
int compile_type_assignable(const ttypeval *actual, const ttypeval *target);
int compile_runtime_types_assignable(const ttypeval *actual,
				     const ttypeval *target);
void tcompile_frontend_init(tcp *cp, tfrontend *frontend,
			    const char *name, const char *source,
			    tfrontend_mode mode);

void tcompile_ast_statement(tcp *cp, const tstring *source,
			       tvmcmd_vect *tcmds, tconsts *consts,
			       tstring **paths, uint_lexs npaths,
			       int cleanstk, int inblk);
void tcompile_ast_module(tcp *cp, const tstring *source,
			    tvmcmd_vect *tcmds, tconsts *consts,
			    tstring **paths, uint_lexs npaths,
			    int inblk);

/* Bytecode helpers shared by AST emitters. */
void compile_emit_reference(tcp *cp, const tstring *name,
			    tvmcmd_vect *tcmds, tconsts *consts);
void tcompile_emit_import(tcp *cp, const tstring *path, const tstring *alias,
			  tvmcmd_vect *tcmds, tconsts *consts,
			  tstring **paths, uint_lexs npaths, int inblk);
void clean_stk(tcp *cp, tvmcmd_vect *tcmds, int isroot, uint_regs regs_ori);

#endif /* TAPAS_COMPILE_INTERNAL_H */
