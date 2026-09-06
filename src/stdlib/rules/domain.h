/**
 * @file domain.h
 * @brief Declares the private Domain objects owned by the `rules` package.
 * @details Shares PointsOf and RangeOf representation only between translation
 * units of the same package; other packages must use ordinary object
 * capabilities instead of this C structure.
 * @note This header is private to `src/stdlib/rules` and is not a core runtime
 * or cross-package C API.
 */
#ifndef TAPAS_STDLIB_RULES_DOMAIN_H
#define TAPAS_STDLIB_RULES_DOMAIN_H

#include "tapas/dsa/tobj_vec.h"
#include "tapas/objects/ttype.h"

typedef struct {
	tcompo_v base;
	ttypeval *item_type;
	tobj_vec values;
	long start;
	long end;
	int range;
} trules_domain;

extern const textension_nominal_template trules_points_template;
extern const textension_nominal_template trules_range_template;

trules_domain *trules_points_new(ttypeval *type, const tobj *values,
	uint_regs count);
trules_domain *trules_range_new(long start, long end);
int trules_domain_is_points(const tobj *value);
int trules_domain_is_range(const tobj *value);

#endif
