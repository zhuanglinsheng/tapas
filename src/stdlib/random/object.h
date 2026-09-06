/**
 * @file object.h
 * @brief Declares the random package's native Source and Generator objects.
 * @details These objects are owned by the random standard-library package and
 * use the runtime's generic composite-object and named-Type interfaces.
 * @note Do not add random-specific identities to core tbuiltintype_id or
 * tcompo_type when extending this package.
 */
#ifndef TAPAS_STDLIB_RANDOM_OBJECT_H
#define TAPAS_STDLIB_RANDOM_OBJECT_H

#include "tapas/tval.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct trandom_source trandom_source;
typedef struct trandom_generator trandom_generator;

ttypeval *tstdlib_random_source_type(void);
ttypeval *tstdlib_random_generator_type(void);

/** Creates the PCG-XSH-RR 64/32 random source. */
trandom_source *trandom_source_pcg32_xsh_rr(void);

/** Creates a generator from a supported source and a signed 64-bit seed. */
trandom_generator *trandom_generator_new(const trandom_source *source,
					 int64_t seed);

/** Advances a generator and returns 32 random bits. */
uint32_t trandom_generator_u32(trandom_generator *generator);

/** Returns an unbiased random integer in [0, bound). Bound must be positive. */
uint64_t trandom_generator_bounded(trandom_generator *generator,
					   uint64_t bound);

/** Returns a random Float in [0, 1) using 53 random bits. */
double trandom_generator_unit(trandom_generator *generator);

/** Moves a generator by a signed number of outputs. */
void trandom_generator_advance(trandom_generator *generator, int64_t steps);

extern tcompo_vtable trandom_source_vtable;
extern tcompo_vtable trandom_generator_vtable;

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_STDLIB_RANDOM_OBJECT_H */
