#ifndef TAPAS_RUNTIME_TSESS_INTERNAL_H
#define TAPAS_RUNTIME_TSESS_INTERNAL_H

#include "tapas/tsession.h"

/* The CLI records its executable timestamp so development rebuilds invalidate
 * automatic bytecode even while the public Tapas version remains unchanged. */
void tsession_set_build_file(tsession *session, const char *path);

#endif
