#ifndef TAPAS_COMPILE_AST_EMIT_INTERNAL_H
#define TAPAS_COMPILE_AST_EMIT_INTERNAL_H

#include "internal.h"
#include "tapas/compile/frontend.h"

typedef struct {
	tcp *cp;
	const tsource_document *document;
	const tast_arena *arena;
	const tfrontend *frontend;
	tvmcmd_vect *instructions;
	tconsts *constants;
	tstring **paths;
	uint_lexs npaths;
} tast_emitter;

int tast_expression_supported(const tast_arena *arena, tast_id id);
void tast_emit_expression(tast_emitter *emitter, tast_id id);
int tast_statement_supported(const tast_arena *arena, tast_id id);
void tast_emit_block(tast_emitter *emitter, const tast_node *block,
		     tstring **paths, uint_lexs npaths, int inblk);
ttypeval *tast_resolve_annotation(tast_emitter *emitter,
				  tsource_span annotation);
ttypeval *tast_infer_expression_type(tast_emitter *emitter, tast_id id,
				     ttypeval **static_value);
int tast_expression_assignable_to(tast_emitter *emitter, tast_id id,
				  const ttypeval *target);
void tast_validate_node(tast_emitter *emitter, tast_id id);
int tast_is_types_package_expression(tast_emitter *emitter, tast_id id);
void tast_static_type_field_order(tast_emitter *emitter, tast_id id,
				  tstring ***order, uint_objs *count);
void tast_annotation_field_order(tast_emitter *emitter,
				 tsource_span annotation,
				 tstring ***order, uint_objs *count);
void tast_free_field_order(tstring **order, uint_objs count);

#endif /* TAPAS_COMPILE_AST_EMIT_INTERNAL_H */
