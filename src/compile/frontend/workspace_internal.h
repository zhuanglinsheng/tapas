#ifndef TAPAS_COMPILE_WORKSPACE_INTERNAL_H
#define TAPAS_COMPILE_WORKSPACE_INTERNAL_H

#include "compile/frontend/workspace.h"

void tworkspace_reference_index_free(tworkspace *workspace);
void tworkspace_reference_remove(tworkspace *workspace,
	const tworkspace_document *source);
void tworkspace_reference_add(tworkspace *workspace,
	const tworkspace_document *source, const tworkspace_document *target,
	const tmodule_export *exported, tsource_span span);
void tworkspace_reference_sort(tworkspace *workspace);

#endif
