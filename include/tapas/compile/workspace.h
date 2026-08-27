#ifndef TAPAS_COMPILE_WORKSPACE_H
#define TAPAS_COMPILE_WORKSPACE_H

#include "tapas/compile/module.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	tstring *path;
	tstring *alias;
	tstring *target_uri;
	tsource_span path_span;
	tsource_span alias_span;
	tast_id statement;
	uint8_t has_alias;
	uint8_t resolved;
} tworkspace_import;

typedef struct tworkspace_document {
	tstring *uri;
	tstring *path;
	int64_t version;
	uint64_t generation;
	uint8_t open;
	uint8_t loading;
	tfrontend frontend;
	tmodule_interface interface;
	tworkspace_import *imports;
	uint32_t import_count;
	struct tworkspace_document *next;
} tworkspace_document;

typedef struct {
	tworkspace_document *documents;
	tstring **roots;
	uint32_t root_count;
	uint32_t root_capacity;
	uint64_t generation;
} tworkspace;

typedef struct {
	const tworkspace_document *document;
	const tmodule_export *exported;
	const tstandard_symbol *standard;
	tsource_span reference_span;
} tworkspace_member_resolution;

typedef struct {
	const tworkspace_document *document;
	const char *standard_package;
} tworkspace_namespace;

void tworkspace_init(tworkspace *workspace);
void tworkspace_free(tworkspace *workspace);
void tworkspace_add_root(tworkspace *workspace, const char *uri_or_path);
tworkspace_document *tworkspace_find(tworkspace *workspace, const char *uri);
const tworkspace_document *tworkspace_find_const(
	const tworkspace *workspace, const char *uri);
tworkspace_document *tworkspace_open(tworkspace *workspace, const char *uri,
				      const char *source, int64_t version);
tworkspace_document *tworkspace_update(tworkspace *workspace, const char *uri,
					const char *source, int64_t version);
void tworkspace_close(tworkspace *workspace, const char *uri);
tworkspace_document *tworkspace_load(tworkspace *workspace, const char *uri);
tworkspace_document *tworkspace_reload(tworkspace *workspace, const char *uri);
void tworkspace_refresh(tworkspace *workspace);

const tworkspace_import *tworkspace_import_for_symbol(
	const tworkspace_document *document, const tsemantic_symbol *symbol);
int tworkspace_resolve_member(tworkspace *workspace,
			      const tworkspace_document *document,
			      uint32_t offset,
			      tworkspace_member_resolution *result);
int tworkspace_resolve_member_node(tworkspace *workspace,
				   const tworkspace_document *document,
				   tast_id member,
				   tworkspace_member_resolution *result);
int tworkspace_resolve_namespace_at(tworkspace *workspace,
				    const tworkspace_document *document,
				    uint32_t offset,
				    tworkspace_namespace *result);

/* URI/path helpers are public so protocol adapters do not duplicate escaping. */
tstring *tworkspace_uri_from_path(const char *path);
tstring *tworkspace_path_from_uri(const char *uri);
tstring *tworkspace_resolve_module_file(const char *path,
					 const char *const *search_roots,
					 uint32_t root_count);

#ifdef __cplusplus
}
#endif

#endif
