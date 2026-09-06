#ifndef TAPAS_COMPILE_SEMANTIC_H
#define TAPAS_COMPILE_SEMANTIC_H

#include "compile/frontend/ast.h"
#include "compile/frontend/annotation_index.h"

typedef struct tdiagnostics tdiagnostics;

#ifdef __cplusplus
extern "C" {
#endif

#define TSEMANTIC_INVALID_ID UINT32_MAX

typedef enum {
	tsemantic_symbol_let,
	tsemantic_symbol_var,
	tsemantic_symbol_function,
	tsemantic_symbol_parameter,
	tsemantic_symbol_import,
	tsemantic_symbol_iteration
} tsemantic_symbol_kind;

typedef int (*tsemantic_external_resolver)(
	void *context, const char *name,
	tsemantic_symbol_kind *kind, int *initialized);

typedef struct tsemantic_symbol {
	tstring *name;
	tsource_span span;
	tast_id declaration;
	uint32_t scope;
	tsemantic_symbol_kind kind;
	uint8_t external;
	uint8_t initially_assigned;
	uint8_t captured;
} tsemantic_symbol;

typedef struct {
	uint32_t parent;
	tsource_span span;
	uint8_t function_boundary;
	uint8_t rule_boundary;
} tsemantic_scope;

typedef struct tsemantic_model {
	tannotation_index annotations;
	tsemantic_symbol *symbols;
	uint32_t symbol_count;
	uint32_t symbol_capacity;
	tsemantic_scope *scopes;
	uint32_t scope_count;
	uint32_t scope_capacity;
	uint32_t *resolutions;
	uint32_t resolution_count;
	uint32_t *declaration_symbols;
	uint32_t declaration_count;
} tsemantic_model;

void tsemantic_model_init(tsemantic_model *model);
void tsemantic_model_free(tsemantic_model *model);
void tsemantic_analyze(const tsource_document *document,
		       const tast_arena *arena, tast_id root,
		       tsemantic_model *model, tdiagnostics *diagnostics);
void tsemantic_analyze_with_resolver(
	const tsource_document *document, const tast_arena *arena, tast_id root,
	tsemantic_model *model, tdiagnostics *diagnostics,
	tsemantic_external_resolver resolver, void *resolver_context);

const tsemantic_symbol *tsemantic_resolved_symbol(
	const tsemantic_model *model, tast_id reference);

const tsemantic_symbol *tsemantic_symbol_for_declaration(
	const tsemantic_model *model, tast_id declaration);

const tsemantic_symbol *tsemantic_symbol_at(
	const tsemantic_model *model, const tast_arena *arena,
	uint32_t offset, tast_id *reference);

const char *tsemantic_symbol_kind_name(tsemantic_symbol_kind kind);

uint32_t tsemantic_scope_at(const tsemantic_model *model, uint32_t offset);

int tsemantic_scope_contains(const tsemantic_model *model,
			     uint32_t scope, uint32_t descendant);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_COMPILE_SEMANTIC_H */
