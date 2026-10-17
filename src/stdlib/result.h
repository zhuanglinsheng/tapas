/* Shared construction only: each operation supplies its own typed fallback. */
#ifndef TAPAS_STDLIB_RESULT_H
#define TAPAS_STDLIB_RESULT_H

#include "tapas/tval.h"

typedef struct {
	const char *code; /* Empty on success; stable machine-readable identifier. */
	char message[384];
	int system_errno;
} tstdlib_error;

void tstdlib_make_result(tobj *result, const tobj *value,
	const tstdlib_error *error);

#endif
