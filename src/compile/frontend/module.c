#include "tapas/compile/module.h"
#include "tapas/compile/frontend.h"
#include "tapas/tstdlib.h"

#include <stdlib.h>
#include <string.h>

static tstandard_symbol *standard_symbols;
static uint32_t standard_symbol_length;

static tmodule_symbol_kind standard_kind(textension_symbol_kind kind)
{
	if (kind == textension_function) return tmodule_symbol_function;
	if (kind == textension_type) return tmodule_symbol_type;
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
					.kind = standard_kind(symbol->kind),
					.result_argument = symbol->result_argument,
					.result_from_argument = symbol->result_relation ==
						tnative_result_argument
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
		tstring *name = tsource_document_slice(&frontend->document, contents);
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
