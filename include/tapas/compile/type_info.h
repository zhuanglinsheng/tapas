#ifndef TAPAS_COMPILE_TYPE_INFO_H
#define TAPAS_COMPILE_TYPE_INFO_H

#include "tapas/compile/semantic.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Editor-facing type facts, independent from runtime ttypeval ownership and
 * VM compiler bindings. */
typedef struct {
	tstring **node_types;
	uint32_t node_count;
	tstring **symbol_types;
	uint32_t symbol_count;
} ttype_info_model;

void ttype_info_model_init(ttype_info_model *model);
void ttype_info_model_free(ttype_info_model *model);
void ttype_info_analyze(const tsource_document *document,
			const tast_arena *arena,
			const tsemantic_model *semantic,
			ttype_info_model *model);
const char *ttype_info_for_node(const ttype_info_model *model, tast_id node);
const char *ttype_info_for_symbol(const ttype_info_model *model,
				 const tsemantic_model *semantic,
				 const tsemantic_symbol *symbol);

#ifdef __cplusplus
}
#endif

#endif
