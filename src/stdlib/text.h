#ifndef TAPAS_STDLIB_TEXT_H
#define TAPAS_STDLIB_TEXT_H

#include "tapas/objects/tstr.h"

static inline tstr *tstdlib_string_arg(const tobj *value, const char *where)
{
	if (value->type != tcompo || tobj_compo_type(value) != compo_tstr)
		twarn(ErrRuntime_ParamsType, where, "String required");
	return (tstr *)value->val.v_tcompo;
}

static inline void tstdlib_string_result(tobj *result, const char *data,
	size_t length, const char *where)
{
	tstr *string = tstr_new_len(data, length);
	if (!string)
		twarn(ErrRuntime_Other, where, "out of memory");
	tobj_set_compo(result, (tcompo_v *)string);
}

#endif
