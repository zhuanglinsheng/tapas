/**
 * @file object.c
 * @brief Implements the random package's native objects and Types.
 * @details Source and Generator use qualified package Type identities and the
 * core runtime's generic extension-object code.
 * @note Keep random algorithms and their public Type identities in this
 * package; they are not language builtins.
 */
#include "object.h"
#include "tapas/dsa/tstring.h"

#include "tapas/objects/ttype.h"

#include "pcg_variants.h"

#include <stdint.h>
#include <stdlib.h>

typedef enum {
	trandom_pcg32_xsh_rr
} trandom_source_kind;

struct trandom_source {
	tcompo_v base;
	trandom_source_kind kind;
};

struct trandom_generator {
	tcompo_v base;
	trandom_source_kind source;
	pcg32_random_t state;
};

static trandom_source *trandom_source_new(trandom_source_kind kind)
{
	trandom_source *source = calloc(1, sizeof(*source));
	if (!source)
		twarn(ErrRuntime_Other, "random source", "out of memory");
	source->base.vtable = &trandom_source_vtable;
	source->kind = kind;
	return source;
}

ttypeval *tstdlib_random_source_type(void)
{
	return ttypeval_new_named("random::Source");
}

ttypeval *tstdlib_random_generator_type(void)
{
	return ttypeval_new_named("random::Generator");
}

trandom_source *trandom_source_pcg32_xsh_rr(void)
{
	return trandom_source_new(trandom_pcg32_xsh_rr);
}

uint32_t trandom_generator_u32(trandom_generator *generator)
{
	return pcg32_random_r(&generator->state);
}

trandom_generator *trandom_generator_new(const trandom_source *source,
					 int64_t seed)
{
	if (!source || source->kind != trandom_pcg32_xsh_rr)
		twarn(ErrRuntime_ParamsType, "random::generator",
		      "unsupported random source");
	trandom_generator *generator = calloc(1, sizeof(*generator));
	if (!generator)
		twarn(ErrRuntime_Other, "random::generator", "out of memory");
	generator->base.vtable = &trandom_generator_vtable;
	generator->source = source->kind;
	pcg32_srandom_r(&generator->state, (uint64_t)seed, UINT64_C(54));
	return generator;
}

static uint64_t trandom_generator_u64(trandom_generator *generator)
{
	uint64_t high = trandom_generator_u32(generator);
	uint64_t low = trandom_generator_u32(generator);
	return (high << 32u) | low;
}

uint64_t trandom_generator_bounded(trandom_generator *generator,
					   uint64_t bound)
{
	if (bound == 0)
		twarn(ErrRuntime_Other, "random bounded integer",
		      "bound must be positive");
	if (bound <= UINT32_MAX)
		return pcg32_boundedrand_r(&generator->state,
					   (uint32_t)bound);
	uint64_t threshold = (UINT64_C(0) - bound) % bound;
	for (;;) {
		uint64_t value = trandom_generator_u64(generator);
		if (value >= threshold)
			return value % bound;
	}
}

double trandom_generator_unit(trandom_generator *generator)
{
	uint32_t high = trandom_generator_u32(generator) >> 5u;
	uint32_t low = trandom_generator_u32(generator) >> 6u;
	return ((double)high * 67108864.0 + (double)low) /
		9007199254740992.0;
}

void trandom_generator_advance(trandom_generator *generator, int64_t steps)
{
	pcg32_advance_r(&generator->state, (uint64_t)steps);
}

/*----------------------- Required Vtable Operations -----------------------*/

static const char *trandom_source_type(void)
{
	return "random::Source";
}

static tcompo_type trandom_source_code(void)
{
	return compo_extension;
}

static long trandom_source_len(void *self)
{
	(void)self;
	return 0;
}

static void *trandom_source_copy(void *self)
{
	return trandom_source_new(((trandom_source *)self)->kind);
}

static void trandom_free(void *self)
{
	free(self);
}

static int trandom_source_identical(void *self, void *other)
{
	return ((trandom_source *)self)->kind ==
		((trandom_source *)other)->kind;
}

static tstring *trandom_source_string(void *self)
{
	(void)self;
	return tstring_new("pcg32_xsh_rr");
}

static const char *trandom_generator_type(void)
{
	return "random::Generator";
}

static tcompo_type trandom_generator_code(void)
{
	return compo_extension;
}

static long trandom_generator_len(void *self)
{
	(void)self;
	return 0;
}

static void *trandom_generator_copy(void *self)
{
	trandom_generator *original = self;
	trandom_generator *copy = calloc(1, sizeof(*copy));
	if (!copy)
		twarn(ErrRuntime_Other, "random generator copy", "out of memory");
	*copy = *original;
	copy->base.refctr = 0;
	return copy;
}

static int trandom_generator_identical(void *self, void *other)
{
	trandom_generator *left = self;
	trandom_generator *right = other;
	return left->source == right->source &&
		left->state.state == right->state.state &&
		left->state.inc == right->state.inc;
}

static tstring *trandom_generator_string(void *self)
{
	(void)self;
	return tstring_new("Generator[pcg32_xsh_rr]");
}

tcompo_vtable trandom_source_vtable = {
	.get_type = trandom_source_type,
	.get_compo_type_code = trandom_source_code,
	.len = trandom_source_len,
	.copy = trandom_source_copy,
	.free = trandom_free,
	.identical = trandom_source_identical,
	.tostring_abbr = trandom_source_string,
	.tostring_full = trandom_source_string
};

tcompo_vtable trandom_generator_vtable = {
	.get_type = trandom_generator_type,
	.get_compo_type_code = trandom_generator_code,
	.len = trandom_generator_len,
	.copy = trandom_generator_copy,
	.free = trandom_free,
	.identical = trandom_generator_identical,
	.tostring_abbr = trandom_generator_string,
	.tostring_full = trandom_generator_string
};
