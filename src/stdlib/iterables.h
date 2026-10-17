#ifndef TAPAS_STDLIB_ITERABLES_H
#define TAPAS_STDLIB_ITERABLES_H

#include "tapas/tval.h"

static inline tcompo_v *tstdlib_require_iterable(
		const tobj *value, const char *where)
{
	if (value->type != tcompo || !value->val.v_tcompo ||
	    !value->val.v_tcompo->vtable ||
	    !value->val.v_tcompo->vtable->capabilities ||
	    !value->val.v_tcompo->vtable->capabilities->iterable)
		twarn(ErrRuntime_ParamsType, where, "Iterable required");
	return value->val.v_tcompo;
}

#endif
