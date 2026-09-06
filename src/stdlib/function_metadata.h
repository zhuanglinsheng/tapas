#ifndef TAPAS_STDLIB_FUNCTION_METADATA_H
#define TAPAS_STDLIB_FUNCTION_METADATA_H

#include "runtime/objects/tfunction_metadata.h"
#include "tapas/tval.h"

/* Returns a borrowed reference cached on the callable. */
tfunction_metadata *tstdlib_function_metadata(const tobj *value);

int tstdlib_validate_function_metadata(void);

#endif
