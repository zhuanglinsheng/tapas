/**
 * @file tfunction_metadata.h
 * @brief Declares immutable metadata for callable objects.
 * @details Provides parameter and result Type metadata, ownership, decoding,
 * and signature-formatting operations shared by core callables.
 * @note Metadata describes an interface and never owns package dispatch.
 */
#ifndef TAPAS_OBJECTS_TFUNCTION_METADATA_H
#define TAPAS_OBJECTS_TFUNCTION_METADATA_H

#include "tapas/tval.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Immutable after construction. Owners share a reference, never the storage. */
struct tformat_context;
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
void tformat_function_signature(struct tformat_context *context,
	const char *name, const tfunction_metadata *metadata);

#ifdef __cplusplus
}
#endif

#endif
