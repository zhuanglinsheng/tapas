#ifndef TAPAS_COMPILE_SOURCE_H
#define TAPAS_COMPILE_SOURCE_H

#include "tapas/ds/tstring.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	uint32_t start;
	uint32_t end;
} tsource_span;

typedef struct {
	tstring *name;
	tstring *text;
	uint32_t *line_starts;
	uint32_t line_count;
	uint32_t line_capacity;
} tsource_document;

tsource_span tsource_span_make(uint32_t start, uint32_t end);
tsource_span tsource_span_cover(tsource_span first, tsource_span last);
int tsource_span_valid(tsource_span span, const tsource_document *document);

void tsource_document_init(tsource_document *document,
			   const char *name, const char *text);
void tsource_document_free(tsource_document *document);
uint32_t tsource_document_length(const tsource_document *document);
void tsource_document_position(const tsource_document *document,
			       uint32_t offset,
			       uint32_t *line, uint32_t *column);
/* LSP positions use zero-based lines and UTF-16 code units. Invalid UTF-8 is
 * treated as one code unit so editor queries remain total on incomplete text. */
void tsource_document_lsp_position(const tsource_document *document,
				   uint32_t offset,
				   uint32_t *line, uint32_t *character);
uint32_t tsource_document_lsp_offset(const tsource_document *document,
				    uint32_t line, uint32_t character);
tstring *tsource_document_slice(const tsource_document *document,
				tsource_span span);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_COMPILE_SOURCE_H */
