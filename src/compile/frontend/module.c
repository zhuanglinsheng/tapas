/**
 * @file module.c
 * @brief Resolves source-module exports and standard extension symbols.
 * @details Converts declarative extension signatures and nominal template
 * schemas into the compiler's package-neutral static Type representation.
 * @note Resolution may know public package names and schemas, but never private
 * package object structures or functions.
 */
#include "compile/frontend/module.h"
#include "tapas/dsa/tstring.h"
#include "compile/frontend/frontend.h"
#include "tapas/tstdlib.h"

#include <stdlib.h>
#include <string.h>

static tstandard_symbol *standard_symbols;
static uint32_t standard_symbol_length;

static tmodule_symbol_kind standard_kind(const textension_symbol *symbol)
{
	if (symbol->kind == textension_function ||
        (symbol->kind == textension_value && !strncmp(symbol->type, "Function[", 9))) return tmodule_symbol_function;
	if (symbol->kind == textension_type) return tmodule_symbol_type;
	return tmodule_symbol_value;
}

static int package_added(const char *name)
{
	for (uint32_t i = 0; i < standard_symbol_length; i++)
		if (!standard_symbols[i].package &&
		    standard_symbols[i].kind == tmodule_symbol_package &&
		    strcmp(standard_symbols[i].name, name) == 0)
			return 1;
	return 0;
}

static void initialize_standard_symbols(void)
{
	if (standard_symbols) return;
	const textension_descriptor *extension = tstdlib_descriptor();
	uint32_t capacity = textension_symbol_count(extension) +
		extension->module_count;
	standard_symbols = (tstandard_symbol *)calloc(
		capacity, sizeof(*standard_symbols));
	if (!standard_symbols) abort();
	for (uint32_t i = 0; i < extension->module_count; i++) {
		const textension_module *module = extension->modules[i];
		const char *package = module->scope == textension_package ?
			module->name : nullptr;
		if (package && !package_added(package))
			standard_symbols[standard_symbol_length++] =
				(tstandard_symbol){
					.name = package,
					.detail = module->detail,
					.kind = tmodule_symbol_package
				};
		for (uint32_t j = 0; j < module->symbol_count; j++) {
			const textension_symbol *symbol = &module->symbols[j];
			standard_symbols[standard_symbol_length++] =
				(tstandard_symbol){
					.package = package,
					.name = symbol->name,
					.type = symbol->type,
					.detail = symbol->detail,
					.kind = standard_kind(symbol),
					.result_argument = symbol->result_argument,
					.result_from_argument = symbol->result_relation ==
						tnative_result_argument,
					.result_template_from_argument =
						symbol->result_relation ==
						tnative_result_template_argument,
					.nominal_template = symbol->nominal_template,
					.result_template = symbol->result_template,
					.following_arguments_match_result_parameter =
						symbol->following_arguments_match_result_parameter,
					.compile_time_signature =
						symbol->compile_time_signature
				};
		}
	}
}

void tmodule_interface_init(tmodule_interface *interface, const char *uri)
{
	*interface = (tmodule_interface){ .uri = tstring_new(uri ? uri : "") };
}

void tmodule_interface_free(tmodule_interface *interface)
{
	if (!interface) return;
	for (uint32_t i = 0; i < interface->export_count; i++) {
		tmodule_export *exported = &interface->exports[i];
		tstring_free(exported->name);
		tstring_free(exported->detail);
		tstring_free(exported->definition_uri);
		tstring_free(exported->namespace_uri);
	}
	free(interface->exports);
	tstring_free(interface->uri);
	*interface = (tmodule_interface){ 0 };
}

static tmodule_export *add_export(tmodule_interface *interface)
{
	if (interface->export_count == interface->export_capacity) {
		uint32_t capacity = interface->export_capacity ?
			interface->export_capacity * 2 : 8;
		tmodule_export *exports = (tmodule_export *)realloc(
			interface->exports, capacity * sizeof(*exports));
		if (!exports) abort();
		interface->exports = exports;
		interface->export_capacity = capacity;
	}
	tmodule_export *result = &interface->exports[interface->export_count++];
	*result = (tmodule_export){ .local_symbol = TSEMANTIC_INVALID_ID };
	return result;
}

static tmodule_export *put_export(tmodule_interface *interface, tstring *name)
{
	for (uint32_t i = 0; i < interface->export_count; i++) {
		tmodule_export *exported = &interface->exports[i];
		if (!tstring_eq(exported->name, name)) continue;
		tstring_free(exported->name);
		tstring_free(exported->detail);
		tstring_free(exported->definition_uri);
		tstring_free(exported->namespace_uri);
		*exported = (tmodule_export){ .name = name,
			.local_symbol = TSEMANTIC_INVALID_ID };
		return exported;
	}
	tmodule_export *exported = add_export(interface);
	exported->name = name;
	return exported;
}

static tmodule_symbol_kind symbol_kind(const tfrontend *frontend,
				       const tsemantic_symbol *symbol)
{
	if (symbol && symbol->kind == tsemantic_symbol_import)
		return tmodule_symbol_package;
	const tast_node *declaration = symbol ?
		tast_get(&frontend->arena, symbol->declaration) : nullptr;
	if (declaration && declaration->kind == tast_declaration_statement) {
		const tast_node *initializer = tast_get(&frontend->arena,
			declaration->declaration_statement.initializer);
		if (initializer && initializer->kind == tast_function)
			return tmodule_symbol_function;
		const char *type = ttype_info_for_symbol(&frontend->types,
			&frontend->semantic, symbol);
		if (type && strcmp(type, "Type") == 0)
			return tmodule_symbol_type;
	}
	return tmodule_symbol_value;
}

static tmodule_symbol_kind expression_kind(const tfrontend *frontend,
					    tast_id id, const tast_node *node)
{
	if (node && node->kind == tast_function) return tmodule_symbol_function;
	const char *type = ttype_info_for_node(&frontend->types, id);
	return type && strcmp(type, "Type") == 0 ?
		tmodule_symbol_type : tmodule_symbol_value;
}

void tmodule_interface_extract(const tfrontend *frontend, const char *uri,
			       uint64_t version,
			       tmodule_interface *interface)
{
	tmodule_interface_free(interface);
	tmodule_interface_init(interface, uri);
	interface->version = version;
	const tast_node *root = tast_get(&frontend->arena, frontend->root);
	if (!root || root->kind != tast_module || !root->aggregate.count) return;
	const tast_id *statements = tast_get_children(&frontend->arena,
		root->aggregate.children, root->aggregate.count);
	const tast_node *last = tast_get(&frontend->arena,
		statements[root->aggregate.count - 1]);
	const tast_node *dictionary = last && last->kind == tast_return_statement ?
		tast_get(&frontend->arena, last->return_statement.value) : nullptr;
	if (!dictionary || dictionary->kind != tast_dictionary) return;
	const tast_id *entries = tast_get_children(&frontend->arena,
		dictionary->aggregate.children, dictionary->aggregate.count);
	for (uint32_t i = 0; i + 1 < dictionary->aggregate.count; i += 2) {
		const tast_node *key = tast_get(&frontend->arena, entries[i]);
		const tast_node *value = tast_get(&frontend->arena, entries[i + 1]);
		if (!key || key->kind != tast_string || !value) continue;
		tsource_span contents = key->span;
		if (contents.end < contents.start + 2) continue;
		contents.start++;
		contents.end--;
		tstring *name = tast_string_value(&frontend->document, key);
		if (!name) continue;
		tmodule_export *exported = put_export(interface, name);
		exported->name_span = contents;
		exported->definition_uri = tstring_new(uri ? uri : "");
		exported->definition_span = value->span;
		exported->kind = tmodule_symbol_value;
		const tsemantic_symbol *symbol = value->kind == tast_name ?
			tsemantic_resolved_symbol(&frontend->semantic, entries[i + 1]) : nullptr;
		if (symbol) {
			exported->local_symbol = (uint32_t)(symbol - frontend->semantic.symbols);
			exported->definition_span = symbol->span;
			exported->kind = symbol_kind(frontend, symbol);
			const char *type = ttype_info_for_symbol(&frontend->types,
				&frontend->semantic, symbol);
			exported->detail = tstring_new(type ? type : "AnyType");
		} else {
			const char *type = ttype_info_for_node(&frontend->types, entries[i + 1]);
			exported->kind = expression_kind(frontend, entries[i + 1], value);
			exported->detail = tstring_new(type ? type : "AnyType");
		}
	}
}

const tmodule_export *tmodule_interface_find(
	const tmodule_interface *interface, const char *name)
{
	if (!interface || !name) return nullptr;
	for (uint32_t i = 0; i < interface->export_count; i++)
		if (tstring_eq_cstr(interface->exports[i].name, name))
			return &interface->exports[i];
	return nullptr;
}

uint32_t tstandard_symbol_count(void)
{
	initialize_standard_symbols();
	return standard_symbol_length;
}

const tstandard_symbol *tstandard_symbol_at(uint32_t index)
{
	initialize_standard_symbols();
	return index < tstandard_symbol_count() ? &standard_symbols[index] : nullptr;
}

const tstandard_symbol *tstandard_symbol_find(const char *package,
					       const char *name)
{
	if (!name) return nullptr;
	for (uint32_t i = 0; i < tstandard_symbol_count(); i++) {
		const tstandard_symbol *symbol = &standard_symbols[i];
		if (((!package && !symbol->package) ||
		    (package && symbol->package && strcmp(package, symbol->package) == 0)) &&
		    strcmp(name, symbol->name) == 0)
			return symbol;
	}
	return nullptr;
}

const tstandard_symbol *tstandard_package(const char *name)
{
	initialize_standard_symbols();
	for (uint32_t i = 0; i < tstandard_symbol_count(); i++) {
		const tstandard_symbol *symbol = &standard_symbols[i];
		if (!symbol->package && symbol->kind == tmodule_symbol_package
		                     && strcmp(symbol->name, name) == 0)
			return symbol;
	}
	return nullptr;
}

/* Resolve package signatures and named Type definitions without loading or executing values. */
typedef struct {
	tstatic_type_arena *arena;
	unsigned depth;
	const char *package;
} standard_type_context;
static tstatic_type_id resolve_standard_type(standard_type_context *, const char *, int);
static tstatic_type_id standard_type_name(void *opaque, const char *name)
{
    standard_type_context *context = opaque;
    return resolve_standard_type(context, name, 1);
}

static tstatic_type_id standard_nominal_template(
	standard_type_context *context,
	const textension_nominal_template *schema)
{
	uint32_t type_count = schema->type_parameter_count;
	uint32_t value_count = schema->value_parameter_count;
	uint32_t count = type_count + value_count;
	if (!count || !schema->identity) return TSTATIC_TYPE_UNKNOWN;
	tstatic_field *parameters = calloc(count, sizeof(*parameters));
	tstatic_field *body_parameters = calloc(count, sizeof(*body_parameters));
	if (!parameters || !body_parameters) abort();
	for (uint32_t i = 0; i < type_count; i++) {
		const textension_template_type_parameter *parameter =
			&schema->type_parameters[i];
		parameters[i] = (tstatic_field){
			.name = tstring_new(parameter->name), .type = tbuiltintype_any };
		body_parameters[i] = (tstatic_field){
			.name = tstring_new(parameter->slot),
			.type = tstatic_type_make_parameter(context->arena,
				parameter->name, 0) };
	}
	for (uint32_t i = 0; i < value_count; i++) {
		const textension_template_value_parameter *parameter =
			&schema->value_parameters[i];
		tstatic_type_id constraint = tstatic_type_parse(context->arena,
			parameter->constraint, standard_type_name, context);
		parameters[type_count + i] = (tstatic_field){
			.name = tstring_new(parameter->name), .type = constraint };
		body_parameters[type_count + i] = (tstatic_field){
			.name = tstring_new(parameter->slot),
			.type = tstatic_type_make_parameter(context->arena,
				parameter->name, 1) };
	}
	tstatic_type_id body = tstatic_type_make_named_application(context->arena,
		schema->identity, body_parameters, count, schema->capabilities);
	tstatic_type_id result = tstatic_type_make_template(context->arena,
		parameters, type_count, value_count, body);
	for (uint32_t i = 0; i < count; i++) {
		tstring_free(parameters[i].name);
		tstring_free(body_parameters[i].name);
	}
	free(parameters);
	free(body_parameters);
	return result;
}

static tstatic_type_id resolve_standard_type(standard_type_context *context, const char *name, int static_value)
{
    if (context->depth >= 32) return TSTATIC_TYPE_UNKNOWN;
    const char *separator = strstr(name, "::");
    tstring *package = separator ? tstring_new_len(name, separator-name) : nullptr;
	const char *package_name = package ? tstring_cstr(package) : context->package;
    const tstandard_symbol *symbol = tstandard_symbol_find(package_name,
        separator ? separator+2 : name);
	if (!symbol && !separator)
		symbol = tstandard_symbol_find(nullptr, name);
    tstring_free(package);
	if (!symbol) return TSTATIC_TYPE_UNKNOWN;
	if (symbol->kind == tmodule_symbol_type && !static_value)
		return tstatic_type_builtin_id(context->arena, tbuiltintype_type);
	if (symbol->kind == tmodule_symbol_type && symbol->nominal_template)
		return standard_nominal_template(context, symbol->nominal_template);
	if (symbol->kind == tmodule_symbol_type && !strcmp(symbol->type, "Type")) {
		tstatic_type_id builtin = tstatic_type_builtin_named(
			context->arena, symbol->name);
		if (builtin != TSTATIC_TYPE_UNKNOWN) return builtin;
		if (!symbol->package) return TSTATIC_TYPE_UNKNOWN;
		tstring *qualified = tstring_new(symbol->package);
		tstring_append(qualified, "::");
		tstring_append(qualified, symbol->name);
		tstatic_type_id result = tstatic_type_make_named(
			context->arena, tstring_cstr(qualified));
		tstring_free(qualified);
		return result;
	}
	const tstandard_symbol *value_type =
		!static_value && symbol->kind == tmodule_symbol_value &&
		symbol->package ?
		tstandard_symbol_find(symbol->package, symbol->type) : nullptr;
	int named_value = value_type &&
		value_type->kind == tmodule_symbol_type;
	if ((static_value && symbol->kind != tmodule_symbol_type) ||
	    (!static_value && symbol->kind != tmodule_symbol_function &&
	     !named_value))
		return TSTATIC_TYPE_UNKNOWN;
	const char *previous_package = context->package;
	context->package = symbol->package;
	context->depth++;
    tstatic_type_id result = tstatic_type_parse(context->arena,symbol->type,standard_type_name,context);
    context->depth--;
	context->package = previous_package;
    return static_value ? tstatic_type_with_name(context->arena,result,name) : result;
}
tstatic_type_id tstandard_type_resolve(tstatic_type_arena *arena, const char *name, int static_value)
{
    standard_type_context context = {.arena=arena};
    return resolve_standard_type(&context,name,static_value);
}
