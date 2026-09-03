#ifndef TAPAS_RUNTIME_TRULE_H
#define TAPAS_RUNTIME_TRULE_H

#include "tapas/ds/tobj_vec.h"
#include "tapas/tenv.h"
#include "tapas/runtime/trule_ir.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	trule_builtin_assert,
	trule_builtin_check,
	trule_builtin_inspect,
	trule_builtin_parameters,
	trule_builtin_items,
	trule_builtin_terms,
	trule_builtin_origin,
	trule_builtin_semantic_hash,
	trule_builtin_content_hash,
	trule_builtin_serialize,
	trule_builtin_deserialize,
	trule_builtin_evaluate,
	trule_builtin_compile,
	trule_builtin_context_binding,
	trule_builtin_context_capture,
	trule_builtin_context_value,
	trule_builtin_context_requirement
} trule_builtin_kind;

struct trule {
	tcompo_v base;
	tobj checker;
	tstring *source;
	tstring *signature;
	trule_ir *ir;
};

struct trule_instance {
	tcompo_v base;
	tobj rule;
	tobj_vec arguments;
};

struct trule_builtin {
	tcompo_v base;
	trule_builtin_kind kind;
};

extern tcompo_vtable trule_vtable;
extern tcompo_vtable trule_instance_vtable;
extern tcompo_vtable trule_builtin_vtable;

trule *trule_new(tfunc *checker, const char *source, const char *signature,
		 const char *parameter_names, const char *item_metadata,
		 const char *capture_metadata);
trule *trule_new_dynamic(trule_ir *ir, const char *signature);
void trule_close_over(trule *rule, tcompo_env *environment);
trule_instance *trule_bind(trule *rule, const tobj *arguments,
			   uint_regs argument_count);
trule_builtin *trule_builtin_new(trule_builtin_kind kind);

#ifdef __cplusplus
}
#endif

#endif
