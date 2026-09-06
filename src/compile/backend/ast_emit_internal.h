#ifndef TAPAS_COMPILE_AST_EMIT_INTERNAL_H
#define TAPAS_COMPILE_AST_EMIT_INTERNAL_H

#include "internal.h"

typedef struct {
	tcp *cp;
	const tsource_document *document;
	const tast_arena *arena;
	const tfrontend *frontend;
	tvmcmd_vect *instructions;
	tconsts *constants;
	tstring **paths;
	uint_lexs npaths;
	const ttypeval *pending_function_type;
	const char *pending_display_name;
	uint8_t *rule_ir_emitted; /* per-Rule expression DAG metadata */
} tast_emitter;

static inline tstring *tast_emitter_text(const tast_emitter *emitter,
					 tsource_span span)
{
	return tsource_document_slice(emitter->document, span);
}

void tast_emit_expression(tast_emitter *emitter, tast_id id);
void tast_emit_bound_type(tast_emitter *emitter, ttypeval *type);
void tast_emit_block(tast_emitter *emitter, const tast_node *block, int inblk);
ttypeval *tast_resolve_annotation(tast_emitter *emitter,
				  tsource_span annotation);
ttypeval *tast_infer_expression_type(tast_emitter *emitter, tast_id id,
				     ttypeval **static_value);
ttypeval *tast_function_signature(tast_emitter *emitter,
				  const tast_node *function);
int tast_is_types_package_expression(tast_emitter *emitter, tast_id id);
void tast_static_type_field_order(tast_emitter *emitter, tast_id id,
				  tstring ***order, uint_objs *count);
void tast_annotation_field_order(tast_emitter *emitter,
				 tsource_span annotation,
				 tstring ***order, uint_objs *count);
void tast_free_field_order(tstring **order, uint_objs count);

#endif /* TAPAS_COMPILE_AST_EMIT_INTERNAL_H */
