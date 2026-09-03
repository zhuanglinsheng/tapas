#ifndef TAPAS_COMPILE_FRONTEND_H
#define TAPAS_COMPILE_FRONTEND_H

#include "tapas/compile/diagnostic.h"
#include "tapas/compile/control_flow.h"
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

typedef struct tfrontend {
	tsource_document document;
	tsyntax_tokens tokens;
	tast_arena arena;
	tdiagnostics diagnostics;
	tsemantic_model semantic;
	tcontrol_flow flow;
	ttype_info_model types;
	tast_id root;
	tfrontend_mode mode;
} tfrontend;

void tfrontend_init(tfrontend *frontend, const char *name,
		    const char *source, tfrontend_mode mode);

void tfrontend_init_with_resolver(
	tfrontend *frontend, const char *name, const char *source,
	tfrontend_mode mode, ttype_info_external_resolver resolver,
	void *resolver_context);

void tfrontend_init_with_environment(
	tfrontend *frontend, const char *name, const char *source,
	tfrontend_mode mode, ttype_info_external_resolver type_resolver,
	void *type_context, tsemantic_external_resolver semantic_resolver,
	void *semantic_context);

void tfrontend_free(tfrontend *frontend);

int tfrontend_valid(const tfrontend *frontend);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_COMPILE_FRONTEND_H */
