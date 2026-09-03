#ifndef TAPAS_RUNTIME_TEVALUATOR_H
#define TAPAS_RUNTIME_TEVALUATOR_H

#include "tapas/tval.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	tcompo_v base;
	tstring *name;
	long version;
	tobj evaluate;
	tobj compile;
} tevaluator;

extern tcompo_vtable tevaluator_vtable;

tevaluator *tevaluator_new(const char *name, long version,
			   const tobj *evaluate, const tobj *compile);

#ifdef __cplusplus
}
#endif

#endif
