#ifndef TAPAS_COMPILE_CACHE_H
#define TAPAS_COMPILE_CACHE_H

#include "runtime/tenv.h"
#include "tapas/dsa/tstring.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TAPAS_BUILD_DIRECTORY "__tapas_build__"

/* Configure one project-level automatic artifact tree from an entry source. */
void tcompile_cache_configure(tlib *library, const tstring *entry_source,
			      int interactive);

/* Map a source file into the configured artifact tree. */
tstring *tcompile_cache_path(const tlib *library, const tstring *source);

/* Create the artifact's parent directories and save it. */
int tcompile_cache_save(const tlib *library, const tstring *source,
			const twrapper *wrapper);

/* Load only when this source and its complete import tree are older than the
 * corresponding artifacts. Invalid, missing and incompatible files are cache
 * misses and are deliberately not reported as user-facing load errors. */
twrapper *tcompile_cache_load_current(const tlib *library,
				       const tstring *source);

#ifdef __cplusplus
}
#endif

#endif
