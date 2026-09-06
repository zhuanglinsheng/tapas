#ifndef TAPAS_RUNTIME_TFUNCTION_METADATA_H
#define TAPAS_RUNTIME_TFUNCTION_METADATA_H

#include "tapas/runtime/ttype.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Immutable after construction. Owners share a reference, never the storage. */
typedef struct tfunction_metadata tfunction_metadata;
typedef struct {
	const char *name;
	/* Factory returns one owned reference; metadata consumes it. */
	ttypeval *(*type)(void);
	uint8_t optional;
	uint8_t variadic;
} tparameter_spec;

tfunction_metadata *tfunction_metadata_decode(const char *names, const char *signature);
tfunction_metadata *tfunction_metadata_from_type(const char *names, ttypeval *signature);
tfunction_metadata *tfunction_metadata_new(const tparameter_spec *parameters,
	uint32_t count, ttypeval *return_type, int variadic);
tfunction_metadata *tfunction_metadata_retain(tfunction_metadata *metadata);
void tfunction_metadata_release(tfunction_metadata *metadata);
/* Encoded names may start with declaration-name + RS, followed by US-separated parameters. */
const char *tfunction_metadata_display_name(const tfunction_metadata *metadata);
uint32_t tfunction_metadata_count(const tfunction_metadata *metadata);
const char *tfunction_metadata_name(const tfunction_metadata *metadata, uint32_t index);
ttypeval *tfunction_metadata_type(const tfunction_metadata *metadata, uint32_t index);
ttypeval *tfunction_metadata_return_type(const tfunction_metadata *metadata);
int tfunction_metadata_variadic(const tfunction_metadata *metadata);
int tfunction_metadata_optional(const tfunction_metadata *metadata, uint32_t index);

#ifdef __cplusplus
}
#endif

#endif
