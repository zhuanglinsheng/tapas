/**
 * @file object.h
 * @brief Declares the evaluators package's native Evaluator object.
 * @details The package owns the object representation and its qualified Type.
 * @note VM integration must use a generic invocation service rather than add
 * evaluator-specific identities to the core Type tables.
 */
#ifndef TAPAS_STDLIB_EVALUATORS_OBJECT_H
#define TAPAS_STDLIB_EVALUATORS_OBJECT_H

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

ttypeval *tstdlib_evaluator_type(void);

extern tcompo_vtable tevaluator_vtable;

tevaluator *tevaluator_new(const char *name, long version,
			   const tobj *evaluate, const tobj *compile);

#ifdef __cplusplus
}
#endif /* TAPAS_STDLIB_EVALUATORS_OBJECT_H */

#endif
