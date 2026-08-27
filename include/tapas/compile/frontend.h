#ifndef TAPAS_COMPILE_FRONTEND_H
#define TAPAS_COMPILE_FRONTEND_H

#include "tapas/compile/parser.h"
#include "tapas/compile/semantic.h"
#include "tapas/compile/type_info.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	tfrontend_expression,
	tfrontend_statement,
	tfrontend_module
} tfrontend_mode;

typedef struct {
	tsource_document document;
	tsyntax_tokens tokens;
	tast_arena arena;
	tdiagnostics diagnostics;
	tsemantic_model semantic;
	ttype_info_model types;
	tast_id root;
	tfrontend_mode mode;
} tfrontend;

void tfrontend_init(tfrontend *frontend, const char *name,
		    const char *source, tfrontend_mode mode);
void tfrontend_free(tfrontend *frontend);
int tfrontend_valid(const tfrontend *frontend);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_COMPILE_FRONTEND_H */
