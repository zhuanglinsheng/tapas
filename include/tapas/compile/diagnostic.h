#ifndef TAPAS_COMPILE_DIAGNOSTIC_H
#define TAPAS_COMPILE_DIAGNOSTIC_H

#include "tapas/compile/source.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	tdiagnostic_error,
	tdiagnostic_warning,
	tdiagnostic_information
} tdiagnostic_severity;

typedef struct {
	tdiagnostic_severity severity;
	tsource_span span;
	tstring *message;
} tdiagnostic;

typedef struct tdiagnostics {
	tdiagnostic *items;
	uint32_t count;
	uint32_t capacity;
} tdiagnostics;


void tdiagnostics_init(tdiagnostics *diagnostics);

void tdiagnostics_free(tdiagnostics *diagnostics);

void tdiagnostics_add(tdiagnostics *diagnostics,
		      tdiagnostic_severity severity,
		      tsource_span span, const char *message);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_COMPILE_DIAGNOSTIC_H */
