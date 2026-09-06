#ifndef TAPAS_STDLIB_ARGUMENTS_H
#define TAPAS_STDLIB_ARGUMENTS_H

#include "tapas/basic_defs/tbasis.h"

static inline void tstdlib_require_arguments(
		const char *name, uint_regs actual, uint_regs expected)
{
	if (actual != expected)
		twarn(ErrRuntime_ParamsCtr, name, "");
}

#endif
