#ifndef TAPAS_RUNTIME_TMODULE_H
#define TAPAS_RUNTIME_TMODULE_H

#include "tenv.h"

/* Load, compile if necessary, and execute one imported module.  This boundary
 * keeps compiler/cache policy out of the VM dispatch implementation. */
tlib *tmodule_load(tcompo_env *environment, const tstring *source);

#endif /* TAPAS_RUNTIME_TMODULE_H */
