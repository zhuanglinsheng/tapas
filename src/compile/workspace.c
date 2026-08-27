#include "tapas/compile/workspace.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int is_file(const char *path)
{
	struct stat status;
	return path && stat(path, &status) == 0 && S_ISREG(status.st_mode);
}

static int is_directory(const char *path)
{
	struct stat status;
	return path && stat(path, &status) == 0 && S_ISDIR(status.st_mode);
}

static int hex_value(char value)
{
	if (value >= '0' && value <= '9') return value - '0';
	if (value >= 'a' && value <= 'f') return value - 'a' + 10;
	if (value >= 'A' && value <= 'F') return value - 'A' + 10;
	return -1;
}

tstring *tworkspace_path_from_uri(const char *uri)
{
	if (!uri) return tstring_new_empty();
	const char *source = strncmp(uri, "file://", 7) == 0 ? uri + 7 : uri;
	tstring *path = tstring_new_empty();
	for (size_t i = 0; source[i]; i++) {
		if (source[i] == '%' && source[i + 1] && source[i + 2]) {
			int high = hex_value(source[i + 1]);
			int low = hex_value(source[i + 2]);
			if (high >= 0 && low >= 0) {
				tstring_append_c(path, (char)((high << 4) | low));
				i += 2;
				continue;
			}
		}
		tstring_append_c(path, source[i]);
	}
	return path;
}

tstring *tworkspace_uri_from_path(const char *path)
{
	if (!path) return tstring_new("file://");
	if (strncmp(path, "file://", 7) == 0) return tstring_new(path);
	tstring *uri = tstring_new("file://");
	static const char hex[] = "0123456789ABCDEF";
	for (const unsigned char *at = (const unsigned char *)path; *at; at++) {
		if (isalnum(*at) || *at == '/' || *at == '-' || *at == '_' ||
		    *at == '.' || *at == '~')
			tstring_append_c(uri, (char)*at);
		else {
			tstring_append_c(uri, '%');
			tstring_append_c(uri, hex[*at >> 4]);
			tstring_append_c(uri, hex[*at & 15]);
		}
	}
	return uri;
}

static tstring *path_directory(const char *path)
{
	const char *slash = path ? strrchr(path, '/') : NULL;
	if (!slash) return tstring_new(".");
	if (slash == path) return tstring_new("/");
	return tstring_new_len(path, (size_t)(slash - path));
}

static tstring *join_path(const char *directory, const char *path)
{
	if (!path || !*path) return tstring_new(directory ? directory : "");
	if (path[0] == '/') return tstring_new(path);
	tstring *joined = tstring_new(directory && *directory ? directory : ".");
	if (tstring_len(joined) && tstring_cstr(joined)[tstring_len(joined) - 1] != '/')
		tstring_append_c(joined, '/');
	tstring_append(joined, path);
	return joined;
}

static tstring *module_file(const char *candidate)
{
	char canonical[PATH_MAX];
	if (is_directory(candidate)) {
		tstring *init = join_path(candidate, "__init__.tap");
		if (is_file(tstring_cstr(init)) && realpath(tstring_cstr(init), canonical)) {
			tstring_free(init);
			return tstring_new(canonical);
		}
		tstring_free(init);
	}
	if (is_file(candidate) && realpath(candidate, canonical))
		return tstring_new(canonical);
	return NULL;
}

tstring *tworkspace_resolve_module_file(const char *path,
					 const char *const *search_roots,
					 uint32_t root_count)
{
	tstring *found = path && path[0] == '/' ? module_file(path) : NULL;
	if (found) return found;
	for (uint32_t i = 0; i < root_count; i++) {
		tstring *candidate = join_path(search_roots[i], path);
		found = module_file(tstring_cstr(candidate));
		tstring_free(candidate);
		if (found) return found;
	}
	return module_file(path);
}

static char *read_file(const char *path)
{
	FILE *file = fopen(path, "rb");
	if (!file) return NULL;
	if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return NULL; }
	long length = ftell(file);
	if (length < 0 || fseek(file, 0, SEEK_SET) != 0) {
		fclose(file); return NULL;
	}
	char *text = (char *)malloc((size_t)length + 1);
	if (!text) abort();
	size_t read = fread(text, 1, (size_t)length, file);
	fclose(file);
	text[read] = '\0';
	return text;
}

void tworkspace_init(tworkspace *workspace)
{
	*workspace = (tworkspace){ 0 };
}

static int has_tap_extension(const char *name)
{
	size_t length = strlen(name);
	return length >= 4 && strcmp(name + length - 4, ".tap") == 0;
}

static int ignored_directory(const char *name)
{
	return strcmp(name, ".git") == 0 || strcmp(name, "build") == 0 ||
		strcmp(name, "node_modules") == 0 || strcmp(name, ".vscode") == 0;
}

static void index_directory(tworkspace *workspace, const char *path,
			    uint32_t depth)
{
	if (depth > 64) return;
	DIR *directory = opendir(path);
	if (!directory) return;
	struct dirent *entry;
	while ((entry = readdir(directory))) {
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
			continue;
		tstring *child = join_path(path, entry->d_name);
		struct stat status;
		if (lstat(tstring_cstr(child), &status) == 0) {
			if (S_ISDIR(status.st_mode) && !ignored_directory(entry->d_name))
				index_directory(workspace, tstring_cstr(child), depth + 1);
			else if (S_ISREG(status.st_mode) && has_tap_extension(entry->d_name)) {
				tstring *uri = tworkspace_uri_from_path(tstring_cstr(child));
				tworkspace_load(workspace, tstring_cstr(uri));
				tstring_free(uri);
			}
		}
		tstring_free(child);
	}
	closedir(directory);
}

static void free_imports(tworkspace_document *document)
{
	for (uint32_t i = 0; i < document->import_count; i++) {
		tstring_free(document->imports[i].path);
		tstring_free(document->imports[i].alias);
		tstring_free(document->imports[i].target_uri);
	}
	free(document->imports);
	document->imports = NULL;
	document->import_count = 0;
}

static void free_document(tworkspace_document *document)
{
	if (!document) return;
	free_imports(document);
	tmodule_interface_free(&document->interface);
	tfrontend_free(&document->frontend);
	tstring_free(document->uri);
	tstring_free(document->path);
	free(document);
}

void tworkspace_free(tworkspace *workspace)
{
	if (!workspace) return;
	while (workspace->documents) {
		tworkspace_document *next = workspace->documents->next;
		free_document(workspace->documents);
		workspace->documents = next;
	}
	for (uint32_t i = 0; i < workspace->root_count; i++)
		tstring_free(workspace->roots[i]);
	free(workspace->roots);
	*workspace = (tworkspace){ 0 };
}

void tworkspace_add_root(tworkspace *workspace, const char *uri_or_path)
{
	if (!workspace || !uri_or_path || !*uri_or_path) return;
	tstring *path = tworkspace_path_from_uri(uri_or_path);
	char canonical[PATH_MAX];
	if (realpath(tstring_cstr(path), canonical)) {
		tstring_free(path);
		path = tstring_new(canonical);
	}
	for (uint32_t i = 0; i < workspace->root_count; i++)
		if (tstring_eq(workspace->roots[i], path)) { tstring_free(path); return; }
	if (workspace->root_count == workspace->root_capacity) {
		uint32_t capacity = workspace->root_capacity ? workspace->root_capacity * 2 : 4;
		tstring **roots = (tstring **)realloc(workspace->roots,
			capacity * sizeof(*roots));
		if (!roots) abort();
		workspace->roots = roots;
		workspace->root_capacity = capacity;
	}
	workspace->roots[workspace->root_count++] = path;
	index_directory(workspace, tstring_cstr(path), 0);
}

tworkspace_document *tworkspace_find(tworkspace *workspace, const char *uri)
{
	for (tworkspace_document *document = workspace ? workspace->documents : NULL;
	     document; document = document->next)
		if (tstring_eq_cstr(document->uri, uri)) return document;
	return NULL;
}

const tworkspace_document *tworkspace_find_const(
	const tworkspace *workspace, const char *uri)
{
	return tworkspace_find((tworkspace *)workspace, uri);
}

static tstring *resolve_import_path(const tworkspace *workspace,
				    const tworkspace_document *document,
				    const char *import_path)
{
	tstring *directory = path_directory(tstring_cstr(document->path));
	const char **roots = (const char **)calloc(workspace->root_count + 1,
		sizeof(const char *));
	if (!roots) abort();
	roots[0] = tstring_cstr(directory);
	for (uint32_t i = 0; i < workspace->root_count; i++)
		roots[i + 1] = tstring_cstr(workspace->roots[i]);
	tstring *found = tworkspace_resolve_module_file(import_path, roots,
		workspace->root_count + 1);
	free(roots);
	tstring_free(directory);
	return found;
}

static void collect_imports(tworkspace *workspace, tworkspace_document *document)
{
	free_imports(document);
	const tast_node *root = tast_get(&document->frontend.arena,
		document->frontend.root);
	if (!root || root->kind != tast_module) return;
	const tast_id *statements = tast_get_children(&document->frontend.arena,
		root->aggregate.children, root->aggregate.count);
	for (uint32_t i = 0; i < root->aggregate.count; i++) {
		const tast_node *node = tast_get(&document->frontend.arena, statements[i]);
		if (!node || node->kind != tast_import_statement) continue;
		tworkspace_import *imports = (tworkspace_import *)realloc(
			document->imports, (document->import_count + 1) * sizeof(*imports));
		if (!imports) abort();
		document->imports = imports;
		tworkspace_import *imported = &document->imports[document->import_count++];
		*imported = (tworkspace_import){
			.path_span = node->import_statement.path,
			.alias_span = node->import_statement.alias,
			.statement = statements[i],
			.has_alias = node->import_statement.has_alias
		};
		imported->path = tsource_document_slice(&document->frontend.document,
			node->import_statement.path);
		imported->alias = node->import_statement.has_alias ?
			tsource_document_slice(&document->frontend.document,
				node->import_statement.alias) : tstring_new_empty();
		tstring *path = resolve_import_path(workspace, document,
			tstring_cstr(imported->path));
		if (path) {
			imported->target_uri = tworkspace_uri_from_path(tstring_cstr(path));
			imported->resolved = 1;
			tstring_free(path);
		} else
			tdiagnostics_add(&document->frontend.diagnostics,
				tdiagnostic_error, imported->path_span,
				"imported module could not be resolved");
	}
}

static void link_exported_namespaces(tworkspace_document *document)
{
	for (uint32_t i = 0; i < document->interface.export_count; i++) {
		tmodule_export *exported = &document->interface.exports[i];
		if (exported->local_symbol >= document->frontend.semantic.symbol_count)
			continue;
		const tsemantic_symbol *symbol =
			&document->frontend.semantic.symbols[exported->local_symbol];
		const tworkspace_import *imported =
			tworkspace_import_for_symbol(document, symbol);
		if (!imported || !imported->resolved) continue;
		exported->kind = tmodule_symbol_package;
		exported->namespace_uri = tstring_dup(imported->target_uri);
	}
}

static int resolve_namespace(tworkspace *workspace,
			     const tworkspace_document *document, tast_id expression,
			     tworkspace_namespace *result, uint32_t depth);

static void validate_module_members(tworkspace *workspace,
				    tworkspace_document *document)
{
	for (tast_id id = 0; id < document->frontend.arena.node_count; id++) {
		const tast_node *node = tast_get(&document->frontend.arena, id);
		if (!node || node->kind != tast_member ||
		    node->member.op != tsyntax_scope) continue;
		tworkspace_namespace namespace = { 0 };
		if (!resolve_namespace(workspace, document, node->member.receiver,
			&namespace, 0)) continue;
		tstring *name = tsource_document_slice(&document->frontend.document,
			node->member.name);
		int found = namespace.document ?
			tmodule_interface_find(&namespace.document->interface,
				tstring_cstr(name)) != NULL : 0;
		if (namespace.standard_package)
			for (uint32_t j = 0; j < tstandard_symbol_count(); j++) {
				const tstandard_symbol *standard = tstandard_symbol_at(j);
				if (standard->package &&
				    strcmp(standard->package, namespace.standard_package) == 0 &&
				    strcmp(standard->name, tstring_cstr(name)) == 0) {
					found = 1; break;
				}
			}
		if (!found)
			tdiagnostics_add(&document->frontend.diagnostics,
				tdiagnostic_error, node->member.name,
				"module does not export this member");
		tstring_free(name);
	}
}

static tworkspace_document *analyze_document(tworkspace *workspace,
					     tworkspace_document *document,
					     const char *source, int64_t version,
					     int open)
{
	if (document->generation) {
		free_imports(document);
		tmodule_interface_free(&document->interface);
		tfrontend_free(&document->frontend);
	}
	document->version = version;
	document->open = (uint8_t)open;
	document->generation = ++workspace->generation;
	tfrontend_init(&document->frontend, tstring_cstr(document->uri), source,
		tfrontend_module);
	tmodule_interface_init(&document->interface, tstring_cstr(document->uri));
	tmodule_interface_extract(&document->frontend, tstring_cstr(document->uri),
		(uint64_t)(version < 0 ? 0 : version), &document->interface);
	collect_imports(workspace, document);
	link_exported_namespaces(document);
	for (uint32_t i = 0; i < document->import_count; i++) {
		if (!document->imports[i].resolved) continue;
		tworkspace_document *target = tworkspace_load(workspace,
			tstring_cstr(document->imports[i].target_uri));
		if (target && target->loading)
			tdiagnostics_add(&document->frontend.diagnostics,
				tdiagnostic_error, document->imports[i].path_span,
				"circular module import");
	}
	validate_module_members(workspace, document);
	return document;
}

static tworkspace_document *new_document(tworkspace *workspace, const char *uri)
{
	tworkspace_document *document = (tworkspace_document *)calloc(1, sizeof(*document));
	if (!document) abort();
	document->uri = tstring_new(uri);
	document->path = tworkspace_path_from_uri(uri);
	document->next = workspace->documents;
	workspace->documents = document;
	return document;
}

tworkspace_document *tworkspace_open(tworkspace *workspace, const char *uri,
				      const char *source, int64_t version)
{
	return tworkspace_update(workspace, uri, source, version);
}

tworkspace_document *tworkspace_update(tworkspace *workspace, const char *uri,
					const char *source, int64_t version)
{
	if (!workspace || !uri || !source) return NULL;
	tworkspace_document *document = tworkspace_find(workspace, uri);
	if (document && document->open && version < document->version) return document;
	if (!document) document = new_document(workspace, uri);
	document->loading = 1;
	analyze_document(workspace, document, source, version, 1);
	document->loading = 0;
	return document;
}

tworkspace_document *tworkspace_load(tworkspace *workspace, const char *uri)
{
	if (!workspace || !uri) return NULL;
	tworkspace_document *document = tworkspace_find(workspace, uri);
	if (document && (document->open || document->loading || document->generation))
		return document;
	if (!document) document = new_document(workspace, uri);
	document->loading = 1;
	char *source = read_file(tstring_cstr(document->path));
	if (!source) { document->loading = 0; return document; }
	analyze_document(workspace, document, source, -1, 0);
	free(source);
	document->loading = 0;
	return document;
}

tworkspace_document *tworkspace_reload(tworkspace *workspace, const char *uri)
{
	if (!workspace || !uri) return NULL;
	tworkspace_document *document = tworkspace_find(workspace, uri);
	if (document && document->open) return document;
	if (!document) return tworkspace_load(workspace, uri);
	char *source = read_file(tstring_cstr(document->path));
	if (!source) {
		document->loading = 1;
		analyze_document(workspace, document, "", -1, 0);
		document->loading = 0;
		return document;
	}
	document->loading = 1;
	analyze_document(workspace, document, source, -1, 0);
	document->loading = 0;
	free(source);
	return document;
}

void tworkspace_refresh(tworkspace *workspace)
{
	if (!workspace) return;
	for (tworkspace_document *document = workspace->documents; document;) {
		tworkspace_document *next = document->next;
		if (document->open) {
			char *source = strdup(tstring_cstr(document->frontend.document.text));
			if (!source) abort();
			analyze_document(workspace, document, source,
				document->version, 1);
			free(source);
		} else
			tworkspace_reload(workspace, tstring_cstr(document->uri));
		document = next;
	}
}

void tworkspace_close(tworkspace *workspace, const char *uri)
{
	tworkspace_document *document = tworkspace_find(workspace, uri);
	if (!document) return;
	document->open = 0;
	tworkspace_reload(workspace, uri);
}

const tworkspace_import *tworkspace_import_for_symbol(
	const tworkspace_document *document, const tsemantic_symbol *symbol)
{
	if (!document || !symbol || symbol->kind != tsemantic_symbol_import) return NULL;
	for (uint32_t i = 0; i < document->import_count; i++)
		if (document->imports[i].statement == symbol->declaration)
			return &document->imports[i];
	return NULL;
}

static int resolve_namespace(tworkspace *workspace,
			     const tworkspace_document *document, tast_id expression,
			     tworkspace_namespace *result, uint32_t depth)
{
	if (depth > 16) return 0;
	const tast_node *node = tast_get(&document->frontend.arena, expression);
	if (!node) return 0;
	if (node->kind == tast_group)
		return resolve_namespace(workspace, document, node->group.value,
			result, depth + 1);
	if (node->kind == tast_member && node->member.op == tsyntax_scope) {
		tworkspace_member_resolution member;
		if (!tworkspace_resolve_member_node(workspace, document, expression,
			&member) || !member.exported || !member.exported->namespace_uri)
			return 0;
		result->document = tworkspace_load(workspace,
			tstring_cstr(member.exported->namespace_uri));
		return result->document && result->document->generation;
	}
	if (node->kind != tast_name) return 0;
	const tsemantic_symbol *symbol = tsemantic_resolved_symbol(
		&document->frontend.semantic, expression);
	if (!symbol) {
		tstring *name = tsource_document_slice(&document->frontend.document,
			node->span);
		const tstandard_symbol *package = tstandard_package(tstring_cstr(name));
		if (package) result->standard_package = package->name;
		tstring_free(name);
		return package != NULL;
	}
	if (symbol->kind == tsemantic_symbol_import) {
		tworkspace_import *imported = (tworkspace_import *)
			tworkspace_import_for_symbol(document, symbol);
		if (!imported) return 0;
		if (!imported->resolved) {
			tstring *path = resolve_import_path(workspace, document,
				tstring_cstr(imported->path));
			if (!path) return 0;
			imported->target_uri = tworkspace_uri_from_path(tstring_cstr(path));
			imported->resolved = 1;
			tstring_free(path);
		}
		result->document = tworkspace_load(workspace,
			tstring_cstr(imported->target_uri));
		return result->document && result->document->generation;
	}
	const tast_node *declaration = tast_get(&document->frontend.arena,
		symbol->declaration);
	if (declaration && declaration->kind == tast_declaration_statement)
		return resolve_namespace(workspace, document,
			declaration->declaration_statement.initializer, result, depth + 1);
	return 0;
}

int tworkspace_resolve_member_node(tworkspace *workspace,
				   const tworkspace_document *document,
				   tast_id member,
				   tworkspace_member_resolution *result)
{
	if (result) *result = (tworkspace_member_resolution){ 0 };
	const tast_node *node = document ? tast_get(&document->frontend.arena, member) : NULL;
	if (!workspace || !node || node->kind != tast_member ||
	    node->member.op != tsyntax_scope) return 0;
	tworkspace_namespace namespace = { 0 };
	if (!resolve_namespace(workspace, document, node->member.receiver,
		&namespace, 0)) return 0;
	tstring *name = tsource_document_slice(&document->frontend.document,
		node->member.name);
	tworkspace_member_resolution found = { .reference_span = node->member.name };
	if (namespace.document) {
		found.document = namespace.document;
		found.exported = tmodule_interface_find(&namespace.document->interface,
			tstring_cstr(name));
	} else {
		for (uint32_t i = 0; i < tstandard_symbol_count(); i++) {
			const tstandard_symbol *standard = tstandard_symbol_at(i);
			if (standard->package &&
			    strcmp(standard->package, namespace.standard_package) == 0 &&
			    strcmp(standard->name, tstring_cstr(name)) == 0) {
				found.standard = standard;
				break;
			}
		}
	}
	tstring_free(name);
	if (!found.exported && !found.standard) return 0;
	if (result) *result = found;
	return 1;
}

int tworkspace_resolve_namespace_at(tworkspace *workspace,
				    const tworkspace_document *document,
				    uint32_t offset,
				    tworkspace_namespace *result)
{
	if (result) *result = (tworkspace_namespace){ 0 };
	if (!workspace || !document) return 0;
	tast_id expression = tast_find_node_at(&document->frontend.arena,
		document->frontend.root, offset);
	tworkspace_namespace resolved = { 0 };
	if (!resolve_namespace(workspace, document, expression, &resolved, 0))
		return 0;
	if (result) *result = resolved;
	return 1;
}

int tworkspace_resolve_member(tworkspace *workspace,
			      const tworkspace_document *document,
			      uint32_t offset,
			      tworkspace_member_resolution *result)
{
	if (!document) return 0;
	for (tast_id id = 0; id < document->frontend.arena.node_count; id++) {
		const tast_node *node = tast_get(&document->frontend.arena, id);
		if (node && node->kind == tast_member &&
		    node->member.name.start <= offset && offset <= node->member.name.end &&
		    tworkspace_resolve_member_node(workspace, document, id, result))
			return 1;
	}
	return 0;
}
