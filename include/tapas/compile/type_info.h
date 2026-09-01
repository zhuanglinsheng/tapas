#ifndef TAPAS_COMPILE_TYPE_INFO_H
#define TAPAS_COMPILE_TYPE_INFO_H

#include "tapas/compile/semantic.h"
#include "tapas/compile/static_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Shared static type facts. Unknown is an analysis state, not a Tapas Type. */
typedef struct {
	tstatic_type_arena arena;
	tstatic_type_id *node_type_ids;
	tstatic_type_id *node_static_values;
	tstatic_type_id *symbol_type_ids;
	tstatic_type_id *symbol_static_values;
	tstring **node_types;
	uint32_t node_count;
	tstring **symbol_types;
	uint32_t symbol_count;
} ttype_info_model;

/* Supplies Types for names owned by an embedding compiler environment rather
 * than by the current source document. The returned TypeId must belong to the
 * provided arena. static_value distinguishes a Type value from its value Type. */
typedef tstatic_type_id (*ttype_info_external_resolver)(
	void *context, tstatic_type_arena *arena,
	const char *qualified_name, int static_value);

void ttype_info_model_init(ttype_info_model *model);
void ttype_info_model_free(ttype_info_model *model);
void ttype_info_analyze(const tsource_document *document,
			const tast_arena *arena,
			const tsemantic_model *semantic,
			ttype_info_model *model);
void ttype_info_analyze_with_resolver(
	const tsource_document *document, const tast_arena *arena,
	const tsemantic_model *semantic, ttype_info_model *model,
	ttype_info_external_resolver resolver, void *resolver_context);
tstatic_type_id ttype_info_node_id(const ttype_info_model *model, tast_id node);
tstatic_type_id ttype_info_node_static_value(
	const ttype_info_model *model, tast_id node);
tstatic_type_id ttype_info_symbol_id(const ttype_info_model *model,
				     const tsemantic_model *semantic,
				     const tsemantic_symbol *symbol);
const char *ttype_info_for_node(const ttype_info_model *model, tast_id node);
const char *ttype_info_for_symbol(const ttype_info_model *model,
				 const tsemantic_model *semantic,
				 const tsemantic_symbol *symbol);

#ifdef __cplusplus
}
#endif

#endif
