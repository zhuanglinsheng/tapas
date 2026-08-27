#include "tapas/compile/diagnostic.h"

#include <stdlib.h>

void tdiagnostics_init(tdiagnostics *diagnostics)
{
	*diagnostics = (tdiagnostics){ 0 };
}

void tdiagnostics_free(tdiagnostics *diagnostics)
{
	if (!diagnostics)
		return;
	for (uint32_t i = 0; i < diagnostics->count; i++)
		tstring_free(diagnostics->items[i].message);
	free(diagnostics->items);
	*diagnostics = (tdiagnostics){ 0 };
}

void tdiagnostics_add(tdiagnostics *diagnostics,
			      tdiagnostic_severity severity,
			      tsource_span span, const char *message)
{
	if (diagnostics->count >= diagnostics->capacity) {
		uint32_t capacity = diagnostics->capacity ?
			diagnostics->capacity * 2 : 8;
		tdiagnostic *items = (tdiagnostic *)realloc(
			diagnostics->items, capacity * sizeof(tdiagnostic));
		if (!items)
			abort();
		diagnostics->items = items;
		diagnostics->capacity = capacity;
	}
	diagnostics->items[diagnostics->count++] = (tdiagnostic){
		.severity = severity,
		.span = span,
		.message = tstring_new(message ? message : "")
	};
}
