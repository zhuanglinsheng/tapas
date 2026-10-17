/**
 * @file module.h
 * @brief Declares source-module and standard-extension Type metadata.
 * @details Bridges declarative C extension symbols into package-neutral
 * compiler symbols without loading package object implementations.
 * @note Package templates are consumed as public schemas; their private C
 * structures must not be exposed through this interface.
 */
#ifndef TAPAS_COMPILE_MODULE_H
#define TAPAS_COMPILE_MODULE_H

#include "compile/frontend/source.h"
#include "compile/types/static_type.h"
#include "compile/types/type_constructor.h"
#include "tapas/textension.h"

typedef struct tfrontend tfrontend;

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	tmodule_symbol_value,
	tmodule_symbol_function,
	tmodule_symbol_package,
	tmodule_symbol_type
} tmodule_symbol_kind;

typedef struct {
	tstring *name;
	tstring *detail;
	tstring *definition_uri;
	tstring *namespace_uri;
	tsource_span name_span;
	tsource_span definition_span;
	uint32_t local_symbol;
	tmodule_symbol_kind kind;
} tmodule_export;

typedef struct {
	tstring *uri;
	tmodule_export *exports;
	uint32_t export_count;
	uint32_t export_capacity;
	uint64_t version;
	uint8_t builtin;
} tmodule_interface;

typedef struct {
	const char *package;
	const char *name;
	const char *type;
	const char *detail;
	tmodule_symbol_kind kind;
	uint32_t result_argument;
	uint8_t result_from_argument;
	uint8_t result_template_from_argument;
	uint32_t type_capabilities;
	const textension_nominal_template *nominal_template;
	const textension_nominal_template *result_template;
	uint8_t following_arguments_match_result_parameter;
	uint8_t compile_time_signature;
	tnative_intrinsic intrinsic;
	uint_regs minimum_arguments;
	uint_regs maximum_arguments;
} tstandard_symbol;

typedef struct tstandard_catalog {
	const textension_registry *extensions;
	tstandard_symbol *symbols;
	uint32_t symbol_count;
	uint64_t revision;
	ttype_constructor constructors[ttype_constructor_count];
} tstandard_catalog;

void tstandard_catalog_init(tstandard_catalog *catalog,
	const textension_registry *extensions);
void tstandard_catalog_free(tstandard_catalog *catalog);

void tmodule_interface_init(tmodule_interface *interface, const char *uri);

void tmodule_interface_free(tmodule_interface *interface);

void tmodule_interface_extract(const tfrontend *frontend, const char *uri,
			       uint64_t version,
			       tmodule_interface *interface);

const tmodule_export *tmodule_interface_find(
	const tmodule_interface *interface, const char *name);

tstatic_type_id tstandard_type_resolve(tstandard_catalog *catalog,
	tstatic_type_arena *arena, const char *name, int static_value);

uint32_t tstandard_symbol_count(tstandard_catalog *catalog);

const tstandard_symbol *tstandard_symbol_at(
	tstandard_catalog *catalog, uint32_t index);

const tstandard_symbol *tstandard_symbol_find(tstandard_catalog *catalog,
	const char *package, const char *name);

const tstandard_symbol *tstandard_package(
	tstandard_catalog *catalog, const char *name);

#ifdef __cplusplus
}
#endif

#endif
