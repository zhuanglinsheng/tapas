#ifndef TAPAS_COMPILE_ANNOTATION_INDEX_H
#define TAPAS_COMPILE_ANNOTATION_INDEX_H

#include "compile/frontend/source.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A source-reference index, not an expression AST. Indices and symbol IDs
 * belong to the current document analysis and are rebuilt on every edit. */
typedef struct {
	tsource_span span;
	uint32_t scope;
	uint32_t symbol;
	uint32_t receiver; /* UINT32_MAX for a lexical name. */
} tannotation_reference;

typedef struct {
	tannotation_reference *items;
	uint32_t count;
	uint32_t capacity;
} tannotation_index;

const tannotation_reference *tannotation_reference_at(
	const tannotation_index *index, uint32_t offset);

#ifdef __cplusplus
}
#endif

#endif
