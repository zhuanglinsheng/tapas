#ifndef TAPAS_TEXTENSION_H
#define TAPAS_TEXTENSION_H

#include "tapas/tbasis.h"

#include <stdint.h>

typedef struct tobj tobj;
typedef struct tcompo_env tcompo_env;

#ifdef __cplusplus
extern "C" {
#endif

#define TAPAS_EXTENSION_ABI 2u

typedef void (*tnative_function)(tobj *arguments, uint_regs argument_count,
				 tobj *result);
typedef void (*tnative_session_function)(tobj *arguments,
	uint_regs argument_count, tobj *result, tcompo_env *environment);
typedef void (*tnative_value_factory)(tobj *result);

typedef enum {
	textension_function,
	textension_value,
	textension_type
} textension_symbol_kind;

typedef enum {
	textension_root,
	textension_package
} textension_module_scope;

typedef enum {
	tnative_intrinsic_none,
	tnative_intrinsic_type_make,
	tnative_intrinsic_type_union,
	tnative_intrinsic_type_list,
	tnative_intrinsic_type_iterator,
	tnative_intrinsic_type_optional,
	tnative_intrinsic_type_pair,
	tnative_intrinsic_type_dictionary,
	tnative_intrinsic_type_rule,
	tnative_intrinsic_type_rule_instance,
	tnative_intrinsic_type_enum
} tnative_intrinsic;

typedef enum {
	tnative_result_declared,
	tnative_result_argument
} tnative_result_relation;

typedef struct {
	const char *name;
	/* Function signature, value Type, or (for textension_type) its definition.
	 * "Type" keeps the legacy builtin-Type-name convention. */
	const char *type;
	const char *detail;
	textension_symbol_kind kind;
	tnative_function function;
	tnative_session_function session_function;
	tnative_value_factory value_factory;
	uint_regs minimum_arguments;
	uint_regs maximum_arguments;
	tnative_intrinsic intrinsic;
	tnative_result_relation result_relation;
	uint_regs result_argument;
} textension_symbol;

typedef struct {
	textension_module_scope scope;
	const char *name;
	const char *detail;
	const textension_symbol *symbols;
	uint32_t symbol_count;
} textension_module;

typedef struct {
	uint32_t abi_version;
	uint32_t structure_size;
	const char *name;
	const char *version;
	const textension_module *const *modules;
	uint32_t module_count;
} textension_descriptor;

typedef struct {
	const textension_module *module;
	const textension_symbol *symbol;
} textension_symbol_ref;

#define TAPAS_NATIVE_FUNCTION_DETAIL( \
	name_, function_, minimum_, maximum_, type_, detail_) \
	{ \
		.name = (name_), .type = (type_), .detail = (detail_), \
		.kind = textension_function, .function = (function_), \
		.minimum_arguments = (minimum_), \
		.maximum_arguments = (maximum_) \
	}

#define TAPAS_NATIVE_FUNCTION(name_, function_, minimum_, maximum_, type_) \
	TAPAS_NATIVE_FUNCTION_DETAIL(name_, function_, minimum_, maximum_, \
		type_, type_)

#define TAPAS_NATIVE_SESSION_FUNCTION_DETAIL( \
	name_, function_, minimum_, maximum_, type_, detail_) \
	{ \
		.name = (name_), .type = (type_), .detail = (detail_), \
		.kind = textension_function, .session_function = (function_), \
		.minimum_arguments = (minimum_), \
		.maximum_arguments = (maximum_) \
	}

#define TAPAS_NATIVE_SESSION_FUNCTION( \
	name_, function_, minimum_, maximum_, type_) \
	TAPAS_NATIVE_SESSION_FUNCTION_DETAIL(name_, function_, minimum_, maximum_, \
		type_, type_)

#define TAPAS_NATIVE_VALUE(name_, factory_, type_) \
	{ \
		.name = (name_), .type = (type_), .detail = (type_), \
		.kind = textension_value, .value_factory = (factory_) \
	}

#define TAPAS_NATIVE_TYPE(name_, factory_) \
	{ \
		.name = (name_), .type = "Type", .detail = "Type", \
		.kind = textension_type, .value_factory = (factory_) \
	}

#define TAPAS_ROOT_MODULE(symbols_) \
	{ \
		.scope = textension_root, .symbols = (symbols_), \
		.symbol_count = sizeof(symbols_) / sizeof((symbols_)[0]) \
	}

#define TAPAS_PACKAGE_MODULE(name_, symbols_) \
	{ \
		.scope = textension_package, .name = (name_), \
		.detail = "default package", .symbols = (symbols_), \
		.symbol_count = sizeof(symbols_) / sizeof((symbols_)[0]) \
	}

#define TAPAS_EXTENSION(name_, version_, modules_) \
	{ \
		.abi_version = TAPAS_EXTENSION_ABI, \
		.structure_size = sizeof(textension_descriptor), \
		.name = (name_), .version = (version_), .modules = (modules_), \
		.module_count = sizeof(modules_) / sizeof((modules_)[0]) \
	}

int textension_validate(const textension_descriptor *extension);
uint32_t textension_symbol_count(const textension_descriptor *extension);
int textension_symbol_at(const textension_descriptor *extension, uint32_t index,
			 textension_symbol_ref *result);
int textension_find(const textension_descriptor *extension,
		     const char *package, const char *name,
		     textension_symbol_ref *result);

#ifdef __cplusplus
}
#endif

#endif
