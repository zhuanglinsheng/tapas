#include "tapas/lsp/server.h"

#include "json.h"
#include "tapas/compile/workspace.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef tworkspace_document tlsp_document;

typedef struct {
	FILE *input;
	FILE *output;
	tworkspace workspace;
	int shutdown;
	int exit_requested;
} tlsp_server;

static const tjson_value *path2(const tjson_value *root,
				const char *first, const char *second)
{
	return tjson_get(tjson_get(root, first), second);
}

static const tjson_value *path3(const tjson_value *root,
				const char *first, const char *second,
				const char *third)
{
	return tjson_get(path2(root, first, second), third);
}

static tlsp_document *find_document(tlsp_server *server, const char *uri)
{
	return tworkspace_find(&server->workspace, uri);
}

static void write_message(tlsp_server *server, const tstring *body)
{
	fprintf(server->output, "Content-Length: %zu\r\n\r\n", tstring_len(body));
	fwrite(tstring_cstr(body), 1, tstring_len(body), server->output);
	fflush(server->output);
}

static void append_position(tstring *out, const tsource_document *document,
			    uint32_t offset)
{
	uint32_t line = 0, character = 0;
	tsource_document_lsp_position(document, offset, &line, &character);
	tstring_append_fmt(out, "{\"line\":%u,\"character\":%u}", line, character);
}

static void append_range(tstring *out, const tsource_document *document,
			 tsource_span span)
{
	tstring_append(out, "{\"start\":");
	append_position(out, document, span.start);
	tstring_append(out, ",\"end\":");
	append_position(out, document, span.end);
	tstring_append_c(out, '}');
}

static void publish_diagnostics(tlsp_server *server, tlsp_document *document)
{
	tstring *body = tstring_new("{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\",\"params\":{\"uri\":");
	tjson_append_escaped(body, tstring_cstr(document->uri));
	tstring_append_fmt(body, ",\"version\":%lld,\"diagnostics\":[",
		(long long)document->version);
	for (uint32_t i = 0; i < document->frontend.diagnostics.count; i++) {
		const tdiagnostic *diagnostic = &document->frontend.diagnostics.items[i];
		if (i) tstring_append_c(body, ',');
		tstring_append(body, "{\"range\":");
		append_range(body, &document->frontend.document, diagnostic->span);
		int severity = diagnostic->severity == tdiagnostic_error ? 1 :
			diagnostic->severity == tdiagnostic_warning ? 2 : 3;
		tstring_append_fmt(body, ",\"severity\":%d,\"source\":\"tapas\",\"message\":", severity);
		tjson_append_escaped(body, tstring_cstr(diagnostic->message));
		tstring_append_c(body, '}');
	}
	tstring_append(body, "]}}");
	write_message(server, body);
	tstring_free(body);
}

static void clear_diagnostics(tlsp_server *server, const char *uri)
{
	tstring *body = tstring_new("{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\",\"params\":{\"uri\":");
	tjson_append_escaped(body, uri);
	tstring_append(body, ",\"diagnostics\":[]}}");
	write_message(server, body);
	tstring_free(body);
}

static void set_document(tlsp_server *server, const char *uri,
			 const char *text, int version)
{
	tlsp_document *document = tworkspace_update(
		&server->workspace, uri, text, version);
	if (!document) return;
	publish_diagnostics(server, document);
}

static void close_document(tlsp_server *server, const char *uri)
{
	tworkspace_close(&server->workspace, uri);
	clear_diagnostics(server, uri);
}

static void send_result(tlsp_server *server, const tjson_value *id,
			const char *result)
{
	tstring *body = tstring_new("{\"jsonrpc\":\"2.0\",\"id\":");
	tjson_append_value(body, id);
	tstring_append(body, ",\"result\":");
	tstring_append(body, result ? result : "null");
	tstring_append_c(body, '}');
	write_message(server, body);
	tstring_free(body);
}

static void send_error(tlsp_server *server, const tjson_value *id,
		       int code, const char *message)
{
	tstring *body = tstring_new("{\"jsonrpc\":\"2.0\",\"id\":");
	tjson_append_value(body, id);
	tstring_append_fmt(body, ",\"error\":{\"code\":%d,\"message\":", code);
	tjson_append_escaped(body, message);
	tstring_append(body, "}}");
	write_message(server, body);
	tstring_free(body);
}

static int request_position(const tjson_value *message, const char **uri,
			    uint32_t *line, uint32_t *character)
{
	*uri = tjson_string_value(path3(message, "params", "textDocument", "uri"));
	const tjson_value *position = path2(message, "params", "position");
	int row = tjson_integer_value(tjson_get(position, "line"), -1);
	int column = tjson_integer_value(tjson_get(position, "character"), -1);
	if (!*uri || row < 0 || column < 0) return 0;
	*line = (uint32_t)row;
	*character = (uint32_t)column;
	return 1;
}

static void handle_hover(tlsp_server *server, const tjson_value *message,
			 const tjson_value *id)
{
	const char *uri = NULL;
	uint32_t line = 0, character = 0;
	if (!request_position(message, &uri, &line, &character)) {
		send_result(server, id, "null"); return;
	}
	tlsp_document *document = find_document(server, uri);
	if (!document) { send_result(server, id, "null"); return; }
	uint32_t offset = tsource_document_lsp_offset(
		&document->frontend.document, line, character);
	tworkspace_member_resolution member;
	if (tworkspace_resolve_member(&server->workspace, document, offset, &member)) {
		const char *name = member.exported ? tstring_cstr(member.exported->name) :
			member.standard->name;
		const char *detail = member.exported ? tstring_cstr(member.exported->detail) :
			member.standard->detail;
		tstring *result = tstring_new("{\"contents\":{\"kind\":\"markdown\",\"value\":");
		tstring *description = tstring_new_empty();
		tstring_append_fmt(description, "```tapas\n%s: %s\n```", name,
			detail ? detail : "AnyType");
		tjson_append_escaped(result, tstring_cstr(description));
		tstring_append(result, "},\"range\":");
		append_range(result, &document->frontend.document, member.reference_span);
		tstring_append_c(result, '}');
		send_result(server, id, tstring_cstr(result));
		tstring_free(description); tstring_free(result); return;
	}
	const tsemantic_symbol *symbol = tsemantic_symbol_at(
		&document->frontend.semantic, &document->frontend.arena, offset, NULL);
	if (!symbol) { send_result(server, id, "null"); return; }
	tstring *result = tstring_new("{\"contents\":{\"kind\":\"markdown\",\"value\":");
	tstring *description = tstring_new_empty();
	const char *type = ttype_info_for_symbol(&document->frontend.types,
		&document->frontend.semantic, symbol);
	tstring_append_fmt(description, "```tapas\n%s %s%s%s\n```",
		tsemantic_symbol_kind_name(symbol->kind), tstring_cstr(symbol->name),
		type ? ": " : "", type ? type : "");
	tjson_append_escaped(result, tstring_cstr(description));
	tstring_append(result, "},\"range\":");
	append_range(result, &document->frontend.document, symbol->span);
	tstring_append_c(result, '}');
	send_result(server, id, tstring_cstr(result));
	tstring_free(description);
	tstring_free(result);
}

static void handle_definition(tlsp_server *server, const tjson_value *message,
			      const tjson_value *id)
{
	const char *uri = NULL;
	uint32_t line = 0, character = 0;
	if (!request_position(message, &uri, &line, &character)) {
		send_result(server, id, "null"); return;
	}
	tlsp_document *document = find_document(server, uri);
	if (!document) { send_result(server, id, "null"); return; }
	uint32_t offset = tsource_document_lsp_offset(
		&document->frontend.document, line, character);
	tworkspace_member_resolution member;
	if (tworkspace_resolve_member(&server->workspace, document, offset, &member)) {
		if (!member.exported || !member.document) {
			send_result(server, id, "null"); return;
		}
		tstring *result = tstring_new("{\"uri\":");
		tjson_append_escaped(result, tstring_cstr(member.document->uri));
		tstring_append(result, ",\"range\":");
		append_range(result, &member.document->frontend.document,
			member.exported->definition_span);
		tstring_append_c(result, '}');
		send_result(server, id, tstring_cstr(result));
		tstring_free(result); return;
	}
	const tsemantic_symbol *symbol = tsemantic_symbol_at(
		&document->frontend.semantic, &document->frontend.arena, offset, NULL);
	if (!symbol) { send_result(server, id, "null"); return; }
	if (symbol->kind == tsemantic_symbol_import) {
		const tworkspace_import *imported = tworkspace_import_for_symbol(document, symbol);
		if (imported && imported->resolved) {
			tlsp_document *target = tworkspace_load(&server->workspace,
				tstring_cstr(imported->target_uri));
			if (target && target->generation) {
				tstring *result = tstring_new("{\"uri\":");
				tjson_append_escaped(result, tstring_cstr(target->uri));
				tstring_append(result, ",\"range\":");
				append_range(result, &target->frontend.document,
					(tsource_span){ 0, 0 });
				tstring_append_c(result, '}');
				send_result(server, id, tstring_cstr(result));
				tstring_free(result); return;
			}
		}
	}
	tstring *result = tstring_new("{\"uri\":");
	tjson_append_escaped(result, uri);
	tstring_append(result, ",\"range\":");
	append_range(result, &document->frontend.document, symbol->span);
	tstring_append_c(result, '}');
	send_result(server, id, tstring_cstr(result));
	tstring_free(result);
}

static void append_location(tstring *result, const char *uri,
			    const tsource_document *document, tsource_span span)
{
	tstring_append(result, "{\"uri\":");
	tjson_append_escaped(result, uri);
	tstring_append(result, ",\"range\":");
	append_range(result, document, span);
	tstring_append_c(result, '}');
}

static void handle_references(tlsp_server *server, const tjson_value *message,
			      const tjson_value *id)
{
	const char *uri = NULL;
	uint32_t line = 0, character = 0;
	if (!request_position(message, &uri, &line, &character)) {
		send_result(server, id, "[]"); return;
	}
	tlsp_document *document = find_document(server, uri);
	if (!document) { send_result(server, id, "[]"); return; }
	uint32_t offset = tsource_document_lsp_offset(
		&document->frontend.document, line, character);
	tworkspace_member_resolution member;
	if (tworkspace_resolve_member(&server->workspace, document, offset, &member)) {
		if (!member.exported || !member.document) {
			send_result(server, id, "[]"); return;
		}
		const tjson_value *context = path2(message, "params", "context");
		const tjson_value *include = tjson_get(context, "includeDeclaration");
		int include_declaration = include && include->kind == tjson_boolean && include->boolean;
		tstring *result = tstring_new("[");
		int first = 1;
		if (include_declaration) {
			append_location(result, tstring_cstr(member.document->uri),
				&member.document->frontend.document, member.exported->name_span);
			first = 0;
		}
		for (tlsp_document *candidate = server->workspace.documents; candidate;
		     candidate = candidate->next) {
			for (tast_id node_id = 0; node_id < candidate->frontend.arena.node_count;
			     node_id++) {
				tworkspace_member_resolution reference;
				if (!tworkspace_resolve_member_node(&server->workspace, candidate,
					node_id, &reference) || reference.document != member.document ||
					reference.exported != member.exported) continue;
				if (!first) tstring_append_c(result, ',');
				append_location(result, tstring_cstr(candidate->uri),
					&candidate->frontend.document, reference.reference_span);
				first = 0;
			}
		}
		tstring_append_c(result, ']');
		send_result(server, id, tstring_cstr(result));
		tstring_free(result); return;
	}
	const tsemantic_symbol *symbol = tsemantic_symbol_at(
		&document->frontend.semantic, &document->frontend.arena, offset, NULL);
	if (!symbol) { send_result(server, id, "[]"); return; }
	const tjson_value *context = path2(message, "params", "context");
	const tjson_value *include = tjson_get(context, "includeDeclaration");
	int include_declaration = include && include->kind == tjson_boolean && include->boolean;
	uint32_t symbol_id = (uint32_t)(symbol - document->frontend.semantic.symbols);
	tstring *result = tstring_new("[");
	int first = 1;
	if (include_declaration) {
		append_location(result, uri, &document->frontend.document, symbol->span);
		first = 0;
	}
	for (tast_id reference = 0;
	     reference < document->frontend.semantic.resolution_count; reference++) {
		if (document->frontend.semantic.resolutions[reference] != symbol_id)
			continue;
		const tast_node *node = tast_get(&document->frontend.arena, reference);
		if (!node) continue;
		if (!first) tstring_append_c(result, ',');
		append_location(result, uri, &document->frontend.document, node->span);
		first = 0;
	}
	tstring_append_c(result, ']');
	send_result(server, id, tstring_cstr(result));
	tstring_free(result);
}

static int document_symbol_kind(const tlsp_document *document,
				const tsemantic_symbol *symbol)
{
	if (symbol->kind == tsemantic_symbol_import) return 3; /* Namespace */
	const tast_node *declaration = tast_get(
		&document->frontend.arena, symbol->declaration);
	if (declaration && declaration->kind == tast_declaration_statement) {
		const tast_node *initializer = tast_get(&document->frontend.arena,
			declaration->declaration_statement.initializer);
		if (initializer && initializer->kind == tast_function) return 12;
	}
	return 13; /* Variable */
}

static void handle_document_symbols(tlsp_server *server,
				    const tjson_value *message,
				    const tjson_value *id)
{
	const char *uri = tjson_string_value(path3(
		message, "params", "textDocument", "uri"));
	tlsp_document *document = uri ? find_document(server, uri) : NULL;
	if (!document) { send_result(server, id, "[]"); return; }
	tstring *result = tstring_new("[");
	for (uint32_t i = 0; i < document->frontend.semantic.symbol_count; i++) {
		const tsemantic_symbol *symbol = &document->frontend.semantic.symbols[i];
		if (i) tstring_append_c(result, ',');
		tstring_append(result, "{\"name\":");
		tjson_append_escaped(result, tstring_cstr(symbol->name));
		tstring_append_fmt(result, ",\"kind\":%d,\"range\":",
			document_symbol_kind(document, symbol));
		const tast_node *declaration = tast_get(
			&document->frontend.arena, symbol->declaration);
		append_range(result, &document->frontend.document,
			declaration ? declaration->span : symbol->span);
		tstring_append(result, ",\"selectionRange\":");
		append_range(result, &document->frontend.document, symbol->span);
		tstring_append_c(result, '}');
	}
	tstring_append_c(result, ']');
	send_result(server, id, tstring_cstr(result));
	tstring_free(result);
}

static void handle_workspace_symbols(tlsp_server *server,
				     const tjson_value *message,
				     const tjson_value *id)
{
	const char *query = tjson_string_value(path2(message, "params", "query"));
	if (!query) query = "";
	tstring *result = tstring_new("[");
	int first = 1;
	for (tlsp_document *document = server->workspace.documents; document;
	     document = document->next) {
		for (uint32_t i = 0; i < document->frontend.semantic.symbol_count; i++) {
			const tsemantic_symbol *symbol = &document->frontend.semantic.symbols[i];
			if (symbol->scope != 0 ||
			    !strstr(tstring_cstr(symbol->name), query)) continue;
			if (!first) tstring_append_c(result, ',');
			tstring_append(result, "{\"name\":");
			tjson_append_escaped(result, tstring_cstr(symbol->name));
			tstring_append_fmt(result, ",\"kind\":%d,\"location\":{\"uri\":",
				document_symbol_kind(document, symbol));
			tjson_append_escaped(result, tstring_cstr(document->uri));
			tstring_append(result, ",\"range\":");
			append_range(result, &document->frontend.document, symbol->span);
			tstring_append(result, "}}");
			first = 0;
		}
	}
	tstring_append_c(result, ']');
	send_result(server, id, tstring_cstr(result));
	tstring_free(result);
}

static int completion_kind(const tlsp_document *document,
			   const tsemantic_symbol *symbol)
{
	int kind = document_symbol_kind(document, symbol);
	return kind == 12 ? 3 : kind == 3 ? 9 : 6;
}

static int name_already_emitted(const tsemantic_model *semantic,
				uint32_t from, const tsemantic_symbol *symbol,
				uint32_t scope, uint32_t offset)
{
	for (uint32_t i = from + 1; i < semantic->symbol_count; i++) {
		const tsemantic_symbol *later = &semantic->symbols[i];
		if (later->span.start <= offset &&
		    tsemantic_scope_contains(semantic, later->scope, scope) &&
		    tstring_eq(later->name, symbol->name)) return 1;
	}
	return 0;
}

static int identifier_byte(unsigned char value)
{
	return value >= 0x80 || isalnum(value) || value == '_';
}

static int starts_with(const char *value, const char *prefix)
{
	return strncmp(value, prefix, strlen(prefix)) == 0;
}

static int module_completion_kind(tmodule_symbol_kind kind)
{
	return kind == tmodule_symbol_function ? 3 :
		kind == tmodule_symbol_package ? 9 :
		kind == tmodule_symbol_type ? 7 : 6;
}

static void append_completion(tstring *result, int *first, const char *name,
			      int kind, const char *detail)
{
	if (!*first) tstring_append_c(result, ',');
	tstring_append(result, "{\"label\":");
	tjson_append_escaped(result, name);
	tstring_append_fmt(result, ",\"kind\":%d", kind);
	if (detail) {
		tstring_append(result, ",\"detail\":");
		tjson_append_escaped(result, detail);
	}
	tstring_append_c(result, '}');
	*first = 0;
}

static int valid_identifier(const char *name);

static tstring *dictionary_key(const tlsp_document *document,
			       const tast_node *node)
{
	if (!node || node->kind != tast_string || node->span.end < node->span.start + 2)
		return NULL;
	tsource_span contents = { node->span.start + 1, node->span.end - 1 };
	return tsource_document_slice(&document->frontend.document, contents);
}

static const tsemantic_symbol *visible_symbol_named(
	const tlsp_document *document, uint32_t name_start, uint32_t name_end,
	uint32_t offset)
{
	tsource_span span = { name_start, name_end };
	tstring *name = tsource_document_slice(&document->frontend.document, span);
	uint32_t scope = tsemantic_scope_at(&document->frontend.semantic, offset);
	const tsemantic_symbol *found = NULL;
	for (uint32_t i = 0; i < document->frontend.semantic.symbol_count; i++) {
		const tsemantic_symbol *symbol = &document->frontend.semantic.symbols[i];
		if (symbol->span.start > offset || !tstring_eq(symbol->name, name) ||
		    !tsemantic_scope_contains(&document->frontend.semantic,
			symbol->scope, scope)) continue;
		if (!found || symbol->span.start > found->span.start) found = symbol;
	}
	tstring_free(name);
	return found;
}

static tast_id dictionary_expression(const tlsp_document *document,
				     tast_id expression, uint32_t depth)
{
	if (depth > 16) return TAST_INVALID_ID;
	const tast_arena *arena = &document->frontend.arena;
	const tast_node *node = tast_get(arena, expression);
	if (!node) return TAST_INVALID_ID;
	if (node->kind == tast_dictionary) return expression;
	if (node->kind == tast_group)
		return dictionary_expression(document, node->group.value, depth + 1);
	if (node->kind == tast_name) {
		const tsemantic_symbol *symbol = tsemantic_resolved_symbol(
			&document->frontend.semantic, expression);
		const tast_node *declaration = symbol ?
			tast_get(arena, symbol->declaration) : NULL;
		if (declaration && declaration->kind == tast_declaration_statement)
			return dictionary_expression(document,
				declaration->declaration_statement.initializer, depth + 1);
		return TAST_INVALID_ID;
	}
	if (node->kind == tast_member) {
		tast_id dictionary = dictionary_expression(
			document, node->member.receiver, depth + 1);
		const tast_node *value = tast_get(arena, dictionary);
		if (!value) return TAST_INVALID_ID;
		const tast_id *items = tast_get_children(
			arena, value->aggregate.children, value->aggregate.count);
		tstring *member = tsource_document_slice(
			&document->frontend.document, node->member.name);
		for (uint32_t i = 0; i + 1 < value->aggregate.count; i += 2) {
			tstring *key = dictionary_key(document, tast_get(arena, items[i]));
			int matches = key && tstring_eq(key, member);
			tstring_free(key);
			if (matches) {
				tstring_free(member);
				return dictionary_expression(document, items[i + 1], depth + 1);
			}
		}
		tstring_free(member);
	}
	return TAST_INVALID_ID;
}

static int append_dictionary_completions(const tlsp_document *document,
					 uint32_t receiver_start,
					 uint32_t receiver_end,
					 const tstring *prefix,
					 tstring *result, int *first)
{
	tast_id receiver = tast_find_node_at(&document->frontend.arena,
		document->frontend.root, receiver_end - 1);
	tast_id dictionary = dictionary_expression(document, receiver, 0);
	if (dictionary == TAST_INVALID_ID) {
		const tsemantic_symbol *symbol = visible_symbol_named(
			document, receiver_start, receiver_end, receiver_end);
		const tast_node *declaration = symbol ? tast_get(
			&document->frontend.arena, symbol->declaration) : NULL;
		if (declaration && declaration->kind == tast_declaration_statement)
			dictionary = dictionary_expression(document,
				declaration->declaration_statement.initializer, 0);
	}
	const tast_node *node = tast_get(&document->frontend.arena, dictionary);
	if (!node) return 0;
	const tast_id *items = tast_get_children(&document->frontend.arena,
		node->aggregate.children, node->aggregate.count);
	int appended = 0;
	for (uint32_t i = 0; i + 1 < node->aggregate.count; i += 2) {
		tstring *key = dictionary_key(document,
			tast_get(&document->frontend.arena, items[i]));
		if (!key || !valid_identifier(tstring_cstr(key)) ||
		    !starts_with(tstring_cstr(key), tstring_cstr(prefix))) {
			tstring_free(key);
			continue;
		}
		int shadowed = 0;
		for (uint32_t later = i + 2; later + 1 < node->aggregate.count; later += 2) {
			tstring *later_key = dictionary_key(document,
				tast_get(&document->frontend.arena, items[later]));
			shadowed = later_key && tstring_eq(key, later_key);
			tstring_free(later_key);
			if (shadowed) break;
		}
		if (!shadowed) {
			const char *detail = ttype_info_for_node(
				&document->frontend.types, items[i + 1]);
			int kind = detail && strcmp(detail, "Function") == 0 ? 3 :
				detail && strcmp(detail, "Type") == 0 ? 7 : 6;
			append_completion(result, first, tstring_cstr(key), kind, detail);
			appended = 1;
		}
		tstring_free(key);
	}
	return appended;
}

static int resolve_completion_namespace(tlsp_server *server,
					const tlsp_document *document,
					uint32_t receiver_start,
					uint32_t receiver_end,
					tworkspace_namespace *namespace)
{
	if (tworkspace_resolve_namespace_at(&server->workspace, document,
		receiver_end - 1, namespace)) return 1;
	tsource_span span = { receiver_start, receiver_end };
	tstring *name = tsource_document_slice(&document->frontend.document, span);
	const tstandard_symbol *package = tstandard_package(tstring_cstr(name));
	if (package) {
		namespace->standard_package = package->name;
		tstring_free(name);
		return 1;
	}
	const tsemantic_symbol *symbol = visible_symbol_named(
		document, receiver_start, receiver_end, receiver_end);
	const tworkspace_import *imported = symbol ?
		tworkspace_import_for_symbol(document, symbol) : NULL;
	if (imported && imported->resolved && imported->target_uri)
		namespace->document = tworkspace_load(&server->workspace,
			tstring_cstr(imported->target_uri));
	tstring_free(name);
	return namespace->document && namespace->document->generation;
}

static int completion_suppressed(const tlsp_document *document, uint32_t offset)
{
	for (uint32_t i = 0; i < document->frontend.tokens.count; i++) {
		const tsyntax_token *token = &document->frontend.tokens.items[i];
		if (offset < token->span.start || offset > token->span.end) continue;
		if (token->kind == tsyntax_comment || token->kind == tsyntax_string)
			return 1;
	}
	return 0;
}

static void handle_completion(tlsp_server *server, const tjson_value *message,
			      const tjson_value *id)
{
	const char *uri = NULL;
	uint32_t line = 0, character = 0;
	if (!request_position(message, &uri, &line, &character)) {
		send_result(server, id, "[]"); return;
	}
	tlsp_document *document = find_document(server, uri);
	if (!document) { send_result(server, id, "[]"); return; }
	uint32_t offset = tsource_document_lsp_offset(
		&document->frontend.document, line, character);
	const char *source = tstring_cstr(document->frontend.document.text);
	uint32_t length = tsource_document_length(&document->frontend.document);
	if (offset > length) offset = length;
	if (completion_suppressed(document, offset)) {
		send_result(server, id, "[]"); return;
	}
	/* VS Code invokes completion for each trigger character.  A lone ':' is
	 * not a member-access operator; wait for the second ':' before offering
	 * namespace or dictionary members. */
	if (offset > 0 && source[offset - 1] == ':' &&
	    (offset < 2 || source[offset - 2] != ':')) {
		send_result(server, id, "[]"); return;
	}
	uint32_t prefix_start = offset;
	while (prefix_start && identifier_byte((unsigned char)source[prefix_start - 1]))
		prefix_start--;
	tstring *prefix = tstring_new_len(source + prefix_start, offset - prefix_start);
	uint32_t scope = tsemantic_scope_at(&document->frontend.semantic, offset);
	tstring *result = tstring_new("[");
	int first = 1;
	uint32_t colon = prefix_start;
	while (colon && isspace((unsigned char)source[colon - 1])) colon--;
	if (colon >= 2 && source[colon - 1] == ':' && source[colon - 2] == ':') {
		uint32_t receiver_end = colon - 2;
		while (receiver_end && isspace((unsigned char)source[receiver_end - 1]))
			receiver_end--;
		uint32_t receiver_start = receiver_end;
		while (receiver_start && identifier_byte((unsigned char)source[receiver_start - 1]))
			receiver_start--;
		tworkspace_namespace namespace;
		if (receiver_start < receiver_end &&
		    resolve_completion_namespace(server, document, receiver_start,
			receiver_end, &namespace)) {
			if (namespace.document) {
				for (uint32_t i = 0; i < namespace.document->interface.export_count; i++) {
					const tmodule_export *exported = &namespace.document->interface.exports[i];
					if (starts_with(tstring_cstr(exported->name), tstring_cstr(prefix)))
						append_completion(result, &first, tstring_cstr(exported->name),
							module_completion_kind(exported->kind),
							tstring_cstr(exported->detail));
				}
			} else if (namespace.standard_package) {
				for (uint32_t i = 0; i < tstandard_symbol_count(); i++) {
					const tstandard_symbol *standard = tstandard_symbol_at(i);
					if (standard->package &&
					    strcmp(standard->package, namespace.standard_package) == 0 &&
					    starts_with(standard->name, tstring_cstr(prefix)))
						append_completion(result, &first, standard->name,
							module_completion_kind(standard->kind), standard->detail);
				}
			}
		} else if (receiver_start < receiver_end) {
			append_dictionary_completions(document, receiver_start,
				receiver_end, prefix, result, &first);
		}
		tstring_append_c(result, ']');
		send_result(server, id, tstring_cstr(result));
		tstring_free(prefix); tstring_free(result); return;
	}
	for (uint32_t i = 0; i < document->frontend.semantic.symbol_count; i++) {
		const tsemantic_symbol *symbol = &document->frontend.semantic.symbols[i];
		if (symbol->span.start > offset ||
		    !tsemantic_scope_contains(&document->frontend.semantic,
			symbol->scope, scope) ||
		    name_already_emitted(&document->frontend.semantic, i, symbol,
			scope, offset)) continue;
		if (!starts_with(tstring_cstr(symbol->name), tstring_cstr(prefix))) continue;
		const char *type = ttype_info_for_symbol(&document->frontend.types,
			&document->frontend.semantic, symbol);
		append_completion(result, &first, tstring_cstr(symbol->name),
			completion_kind(document, symbol), type);
	}
	for (uint32_t i = 0; i < tstandard_symbol_count(); i++) {
		const tstandard_symbol *standard = tstandard_symbol_at(i);
		if (standard->package || !starts_with(standard->name, tstring_cstr(prefix)))
			continue;
		int shadowed = 0;
		for (uint32_t j = 0; j < document->frontend.semantic.symbol_count; j++)
			if (tstring_eq_cstr(document->frontend.semantic.symbols[j].name,
				standard->name) && document->frontend.semantic.symbols[j].span.start <= offset)
				shadowed = 1;
		if (!shadowed) append_completion(result, &first, standard->name,
			module_completion_kind(standard->kind), standard->detail);
	}
	tstring_append_c(result, ']');
	send_result(server, id, tstring_cstr(result));
	tstring_free(prefix);
	tstring_free(result);
}

static int valid_identifier(const char *name)
{
	tsource_document document;
	tsource_document_init(&document, "<rename>", name);
	tsyntax_tokens tokens;
	tsyntax_tokens_init(&tokens);
	tsyntax_lex(&document, &tokens);
	int valid = tokens.count == 2 &&
		tokens.items[0].kind == tsyntax_identifier &&
		tokens.items[0].span.end == tsource_document_length(&document) &&
		tokens.items[1].kind == tsyntax_eof;
	tsyntax_tokens_free(&tokens);
	tsource_document_free(&document);
	return valid;
}

static void handle_prepare_rename(tlsp_server *server,
				  const tjson_value *message,
				  const tjson_value *id)
{
	const char *uri = NULL;
	uint32_t line = 0, character = 0;
	if (!request_position(message, &uri, &line, &character)) {
		send_result(server, id, "null"); return;
	}
	tlsp_document *document = find_document(server, uri);
	if (!document) { send_result(server, id, "null"); return; }
	uint32_t offset = tsource_document_lsp_offset(
		&document->frontend.document, line, character);
	tworkspace_member_resolution member;
	if (tworkspace_resolve_member(&server->workspace, document, offset, &member)) {
		if (!member.exported) { send_result(server, id, "null"); return; }
		tstring *result = tstring_new("{\"range\":");
		append_range(result, &document->frontend.document, member.reference_span);
		tstring_append(result, ",\"placeholder\":");
		tjson_append_escaped(result, tstring_cstr(member.exported->name));
		tstring_append_c(result, '}');
		send_result(server, id, tstring_cstr(result));
		tstring_free(result); return;
	}
	const tsemantic_symbol *symbol = tsemantic_symbol_at(
		&document->frontend.semantic, &document->frontend.arena, offset, NULL);
	if (!symbol) { send_result(server, id, "null"); return; }
	tstring *result = tstring_new("{\"range\":");
	append_range(result, &document->frontend.document, symbol->span);
	tstring_append(result, ",\"placeholder\":");
	tjson_append_escaped(result, tstring_cstr(symbol->name));
	tstring_append_c(result, '}');
	send_result(server, id, tstring_cstr(result));
	tstring_free(result);
}

static void append_text_edit(tstring *result, const tsource_document *document,
			     tsource_span span, const char *new_name)
{
	tstring_append(result, "{\"range\":");
	append_range(result, document, span);
	tstring_append(result, ",\"newText\":");
	tjson_append_escaped(result, new_name);
	tstring_append_c(result, '}');
}

static void handle_rename(tlsp_server *server, const tjson_value *message,
			  const tjson_value *id)
{
	const char *new_name = tjson_string_value(path2(message, "params", "newName"));
	if (!new_name || !valid_identifier(new_name)) {
		send_error(server, id, -32602, "newName must be a Tapas identifier");
		return;
	}
	const char *uri = NULL;
	uint32_t line = 0, character = 0;
	if (!request_position(message, &uri, &line, &character)) {
		send_result(server, id, "null"); return;
	}
	tlsp_document *document = find_document(server, uri);
	if (!document) { send_result(server, id, "null"); return; }
	uint32_t offset = tsource_document_lsp_offset(
		&document->frontend.document, line, character);
	tworkspace_member_resolution member;
	if (tworkspace_resolve_member(&server->workspace, document, offset, &member)) {
		if (!member.exported || !member.document) {
			send_result(server, id, "null"); return;
		}
		tstring *result = tstring_new("{\"changes\":{");
		int first_document = 1;
		for (tlsp_document *candidate = server->workspace.documents; candidate;
		     candidate = candidate->next) {
			tstring *edits = tstring_new("[");
			int first_edit = 1;
			if (candidate == member.document) {
				append_text_edit(edits, &candidate->frontend.document,
					member.exported->name_span, new_name);
				first_edit = 0;
			}
			for (tast_id node_id = 0; node_id < candidate->frontend.arena.node_count;
			     node_id++) {
				tworkspace_member_resolution reference;
				if (!tworkspace_resolve_member_node(&server->workspace, candidate,
					node_id, &reference) || reference.document != member.document ||
					reference.exported != member.exported) continue;
				if (!first_edit) tstring_append_c(edits, ',');
				append_text_edit(edits, &candidate->frontend.document,
					reference.reference_span, new_name);
				first_edit = 0;
			}
			tstring_append_c(edits, ']');
			if (!first_edit) {
				if (!first_document) tstring_append_c(result, ',');
				tjson_append_escaped(result, tstring_cstr(candidate->uri));
				tstring_append_c(result, ':');
				tstring_append_ts(result, edits);
				first_document = 0;
			}
			tstring_free(edits);
		}
		tstring_append(result, "}}");
		send_result(server, id, tstring_cstr(result));
		tstring_free(result); return;
	}
	const tsemantic_symbol *symbol = tsemantic_symbol_at(
		&document->frontend.semantic, &document->frontend.arena, offset, NULL);
	if (!symbol) { send_result(server, id, "null"); return; }
	uint32_t symbol_id = (uint32_t)(symbol - document->frontend.semantic.symbols);
	tstring *result = tstring_new("{\"changes\":{");
	tjson_append_escaped(result, uri);
	tstring_append(result, ":[");
	append_text_edit(result, &document->frontend.document, symbol->span, new_name);
	for (tast_id reference = 0;
	     reference < document->frontend.semantic.resolution_count; reference++) {
		if (document->frontend.semantic.resolutions[reference] != symbol_id) continue;
		const tast_node *node = tast_get(&document->frontend.arena, reference);
		if (!node) continue;
		tstring_append_c(result, ',');
		append_text_edit(result, &document->frontend.document, node->span, new_name);
	}
	tstring_append(result, "]}}");
	send_result(server, id, tstring_cstr(result));
	tstring_free(result);
}

static void handle_message(tlsp_server *server, const tjson_value *message)
{
	const char *method = tjson_string_value(tjson_get(message, "method"));
	const tjson_value *id = tjson_get(message, "id");
	if (!method) return;
	if (!strcmp(method, "initialize")) {
		const char *root_uri = tjson_string_value(path2(message, "params", "rootUri"));
		if (root_uri) tworkspace_add_root(&server->workspace, root_uri);
		const tjson_value *folders = path2(message, "params", "workspaceFolders");
		if (folders && folders->kind == tjson_array)
			for (size_t i = 0; i < folders->array.count; i++) {
				const char *folder_uri = tjson_string_value(
					tjson_get(folders->array.items[i], "uri"));
				if (folder_uri) tworkspace_add_root(&server->workspace, folder_uri);
			}
		tworkspace_refresh(&server->workspace);
		send_result(server, id,
			"{\"capabilities\":{\"positionEncoding\":\"utf-16\",\"textDocumentSync\":1,\"hoverProvider\":true,\"definitionProvider\":true,\"referencesProvider\":true,\"documentSymbolProvider\":true,\"workspaceSymbolProvider\":true,\"completionProvider\":{\"triggerCharacters\":[\":\"]},\"renameProvider\":{\"prepareProvider\":true},\"workspace\":{\"workspaceFolders\":{\"supported\":true,\"changeNotifications\":false}}},\"serverInfo\":{\"name\":\"Tapas Language Server\",\"version\":\"0.2.0\"}}");
	} else if (!strcmp(method, "shutdown")) {
		server->shutdown = 1;
		send_result(server, id, "null");
	} else if (!strcmp(method, "exit")) {
		server->exit_requested = 1;
	} else if (!strcmp(method, "textDocument/didOpen")) {
		const char *uri = tjson_string_value(path3(message, "params", "textDocument", "uri"));
		const char *text = tjson_string_value(path3(message, "params", "textDocument", "text"));
		int version = tjson_integer_value(path3(message, "params", "textDocument", "version"), 0);
		if (uri && text) set_document(server, uri, text, version);
	} else if (!strcmp(method, "textDocument/didChange")) {
		const char *uri = tjson_string_value(path3(message, "params", "textDocument", "uri"));
		int version = tjson_integer_value(path3(message, "params", "textDocument", "version"), 0);
		const tjson_value *changes = path2(message, "params", "contentChanges");
		const char *text = changes && changes->kind == tjson_array && changes->array.count ?
			tjson_string_value(tjson_get(changes->array.items[changes->array.count - 1], "text")) : NULL;
		if (uri && text) set_document(server, uri, text, version);
	} else if (!strcmp(method, "textDocument/didClose")) {
		const char *uri = tjson_string_value(path3(message, "params", "textDocument", "uri"));
		if (uri) close_document(server, uri);
	} else if (!strcmp(method, "workspace/didChangeWatchedFiles")) {
		const tjson_value *changes = path2(message, "params", "changes");
		if (changes && changes->kind == tjson_array)
			for (size_t i = 0; i < changes->array.count; i++) {
				const char *uri = tjson_string_value(
					tjson_get(changes->array.items[i], "uri"));
				if (uri) tworkspace_reload(&server->workspace, uri);
			}
		tworkspace_refresh(&server->workspace);
	} else if (!strcmp(method, "textDocument/hover")) {
		handle_hover(server, message, id);
	} else if (!strcmp(method, "textDocument/definition")) {
		handle_definition(server, message, id);
	} else if (!strcmp(method, "textDocument/references")) {
		handle_references(server, message, id);
	} else if (!strcmp(method, "textDocument/documentSymbol")) {
		handle_document_symbols(server, message, id);
	} else if (!strcmp(method, "workspace/symbol")) {
		handle_workspace_symbols(server, message, id);
	} else if (!strcmp(method, "textDocument/completion")) {
		handle_completion(server, message, id);
	} else if (!strcmp(method, "textDocument/prepareRename")) {
		handle_prepare_rename(server, message, id);
	} else if (!strcmp(method, "textDocument/rename")) {
		handle_rename(server, message, id);
	} else if (id) {
		send_error(server, id, -32601, "method not found");
	}
}

static char *read_message(FILE *input)
{
	char line[1024];
	size_t length = 0;
	int saw_header = 0;
	while (fgets(line, sizeof(line), input)) {
		saw_header = 1;
		if (!strcmp(line, "\n") || !strcmp(line, "\r\n")) break;
		char *colon = strchr(line, ':');
		if (!colon) continue;
		*colon = '\0';
		for (char *at = line; *at; at++) *at = (char)tolower((unsigned char)*at);
		if (!strcmp(line, "content-length")) length = (size_t)strtoull(colon + 1, NULL, 10);
	}
	if (!saw_header || !length || length > 64 * 1024 * 1024) return NULL;
	char *body = (char *)malloc(length + 1);
	if (!body) abort();
	if (fread(body, 1, length, input) != length) { free(body); return NULL; }
	body[length] = '\0';
	return body;
}

int tlsp_run(FILE *input, FILE *output)
{
	if (!input || !output) return 1;
	tlsp_server server = { .input = input, .output = output };
	tworkspace_init(&server.workspace);
	while (!server.exit_requested) {
		char *body = read_message(input);
		if (!body) break;
		const char *error = NULL;
		tjson_value *message = tjson_parse(body, &error);
		free(body);
		if (message) {
			handle_message(&server, message);
			tjson_free(message);
		} else
			send_error(&server, NULL, -32700,
				error ? error : "invalid JSON");
	}
	tworkspace_free(&server.workspace);
	return server.shutdown || !server.exit_requested ? 0 : 1;
}
