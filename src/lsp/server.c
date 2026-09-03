#include "tapas/lsp/server.h"

#include "json.h"
#include "tapas/compile/workspace.h"
#include "tapas/tbuiltin.h"
#include "tapas/version.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef tworkspace_document tlsp_document;

typedef struct {
	const char *name;
	uint32_t length;
	uint8_t type;
} tlsp_semantic_name;

typedef struct {
	FILE *input;
	FILE *output;
	tworkspace workspace;
	tlsp_semantic_name *semantic_names;
	uint32_t semantic_name_count;
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

enum {
	semantic_namespace,
	semantic_type,
	semantic_function,
	semantic_parameter,
	semantic_variable,
	semantic_keyword
};

static int semantic_name_compare(const void *left, const void *right)
{
	const tlsp_semantic_name *a = left;
	const tlsp_semantic_name *b = right;
	return strcmp(a->name, b->name);
}

static void add_semantic_name(tlsp_server *server, const char *name, int type)
{
	tlsp_semantic_name *entry = &server->semantic_names[
		server->semantic_name_count++];
	entry->name = name;
	entry->length = (uint32_t)strlen(name);
	entry->type = (uint8_t)type;
}

static int semantic_name_priority(uint8_t type)
{
	return type == semantic_type ? 3 : type == semantic_namespace ? 2 : 1;
}

static void init_semantic_names(tlsp_server *server)
{
	uint32_t capacity = (uint32_t)tbuiltin_count + 1 +
		tstandard_symbol_count();
	server->semantic_names = capacity ? calloc(capacity,
		sizeof(*server->semantic_names)) : nullptr;
	if (capacity && !server->semantic_names) abort();
	for (int i = 0; i < tbuiltin_count; i++)
		add_semantic_name(server, tbuiltin_name((tbuiltin_id)i), semantic_type);
	add_semantic_name(server, "Union", semantic_type);
	for (uint32_t i = 0; i < tstandard_symbol_count(); i++) {
		const tstandard_symbol *symbol = tstandard_symbol_at(i);
		int type = symbol->kind == tmodule_symbol_package ? semantic_namespace :
			symbol->kind == tmodule_symbol_type ? semantic_type :
			symbol->kind == tmodule_symbol_function ? semantic_function : -1;
		if (type >= 0) add_semantic_name(server, symbol->name, type);
	}
	qsort(server->semantic_names, server->semantic_name_count,
		sizeof(*server->semantic_names), semantic_name_compare);
	uint32_t unique = 0;
	for (uint32_t i = 0; i < server->semantic_name_count; i++) {
		if (unique && !strcmp(server->semantic_names[unique - 1].name,
		    server->semantic_names[i].name)) {
			if (semantic_name_priority(server->semantic_names[i].type) >
			    semantic_name_priority(server->semantic_names[unique - 1].type))
				server->semantic_names[unique - 1].type =
					server->semantic_names[i].type;
			continue;
		}
		server->semantic_names[unique++] = server->semantic_names[i];
	}
	server->semantic_name_count = unique;
}

static int semantic_symbol_type(const tsemantic_symbol *symbol)
{
	if (!symbol || symbol->external) return -1;
	if (symbol->kind == tsemantic_symbol_function) return semantic_function;
	if (symbol->kind == tsemantic_symbol_parameter) return semantic_parameter;
	if (symbol->kind == tsemantic_symbol_import) return semantic_namespace;
	return semantic_variable;
}

static uint32_t token_at_span(const tsyntax_tokens *tokens, tsource_span span)
{
	uint32_t low = 0, high = tokens->count;
	while (low < high) {
		uint32_t middle = low + (high - low) / 2;
		if (tokens->items[middle].span.start < span.start)
			low = middle + 1;
		else
			high = middle;
	}
	return low < tokens->count &&
		tokens->items[low].span.start == span.start &&
		tokens->items[low].span.end == span.end ? low : UINT32_MAX;
}

static int catalog_token_type(const tlsp_server *server,
			      const tlsp_document *document,
			      const tsyntax_token *token)
{
	const char *source = tstring_cstr(document->frontend.document.text) +
		token->span.start;
	uint32_t length = token->span.end - token->span.start;
	uint32_t low = 0, high = server->semantic_name_count;
	while (low < high) {
		uint32_t middle = low + (high - low) / 2;
		const tlsp_semantic_name *entry = &server->semantic_names[middle];
		uint32_t shared = length < entry->length ? length : entry->length;
		int comparison = memcmp(source, entry->name, shared);
		if (!comparison)
			comparison = length < entry->length ? -1 :
				length > entry->length ? 1 : 0;
		if (comparison > 0)
			low = middle + 1;
		else if (comparison < 0)
			high = middle;
		else
			return entry->type;
	}
	return -1;
}

static void handle_semantic_tokens(tlsp_server *server,
				   const tjson_value *message,
				   const tjson_value *id)
{
	const char *uri = tjson_string_value(
		path3(message, "params", "textDocument", "uri"));
	tlsp_document *document = uri ? find_document(server, uri) : nullptr;
	if (!document) { send_result(server, id, "{\"data\":[]}"); return; }
	const tsyntax_tokens *tokens = &document->frontend.tokens;
	uint8_t *types = tokens->count ? calloc(tokens->count, sizeof(*types)) : nullptr;
	if (tokens->count && !types) abort();
	for (uint32_t i = 0; i < document->frontend.semantic.symbol_count; i++) {
		const tsemantic_symbol *symbol = &document->frontend.semantic.symbols[i];
		uint32_t token = token_at_span(tokens, symbol->span);
		int type = semantic_symbol_type(symbol);
		if (type >= 0 && token < tokens->count)
			types[token] = (uint8_t)(type + 1);
	}
	for (tast_id node_id = 0; node_id < document->frontend.arena.node_count;
	     node_id++) {
		const tast_node *node = tast_get(&document->frontend.arena, node_id);
		if (!node || node->kind != tast_name) continue;
		const tsemantic_symbol *symbol = tsemantic_resolved_symbol(
			&document->frontend.semantic, node_id);
		uint32_t token = token_at_span(tokens, node->span);
		int type = semantic_symbol_type(symbol);
		if (type >= 0 && token < tokens->count)
			types[token] = (uint8_t)(type + 1);
	}
	tstring *result = tstring_new("{\"data\":[");
	uint32_t previous_line = 0, previous_character = 0;
	int first = 1;
	for (uint32_t i = 0; i < tokens->count; i++) {
		const tsyntax_token *token = &tokens->items[i];
		int type = -1;
		if (token->kind >= tsyntax_kw_and && token->kind <= tsyntax_kw_while)
			type = semantic_keyword;
		else if (types[i])
			type = types[i] - 1;
		else if (token->kind == tsyntax_identifier)
			type = catalog_token_type(server, document, token);
		if (type < 0) continue;
		uint32_t line, character, end_line, end_character;
		tsource_document_lsp_position(&document->frontend.document,
			token->span.start, &line, &character);
		tsource_document_lsp_position(&document->frontend.document,
			token->span.end, &end_line, &end_character);
		if (line != end_line || end_character <= character) continue;
		uint32_t delta_line = first ? line : line - previous_line;
		uint32_t delta_start = !first && delta_line == 0 ?
			character - previous_character : character;
		if (!first) tstring_append_c(result, ',');
		tstring_append_fmt(result, "%u,%u,%u,%d,0", delta_line,
			delta_start, end_character - character, type);
		previous_line = line;
		previous_character = character;
		first = 0;
	}
	free(types);
	tstring_append(result, "]}");
	send_result(server, id, tstring_cstr(result));
	tstring_free(result);
}

static void append_catalog_names(tstring *result, int *first,
				 const char *name)
{
	if (!*first) tstring_append_c(result, ',');
	tjson_append_escaped(result, name);
	*first = 0;
}

static void handle_syntax_catalog(tlsp_server *server,
				  const tjson_value *id)
{
	tstring *result = tstring_new("{\"keywords\":[");
	int first = 1;
	for (uint32_t i = 0; i < tsyntax_keyword_count(); i++)
		append_catalog_names(result, &first, tsyntax_keyword_at(i));
	tstring_append(result, "],\"types\":[");
	first = 1;
	for (int i = 0; i < tbuiltin_count; i++)
		append_catalog_names(result, &first, tbuiltin_name((tbuiltin_id)i));
	append_catalog_names(result, &first, "Union");
	tstring_append(result, "],\"packages\":[");
	first = 1;
	for (uint32_t i = 0; i < tstandard_symbol_count(); i++) {
		const tstandard_symbol *symbol = tstandard_symbol_at(i);
		if (symbol->kind != tmodule_symbol_package) continue;
		append_catalog_names(result, &first, symbol->name);
	}
	tstring_append(result, "]}");
	send_result(server, id, tstring_cstr(result));
	tstring_free(result);
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

static void send_hover(tlsp_server *server, const tjson_value *id,
		       const tlsp_document *document, tsource_span span,
		       const char *description)
{
	tstring *result = tstring_new(
		"{\"contents\":{\"kind\":\"markdown\",\"value\":");
	tstring *markdown = tstring_new("```tapas\n");
	tstring_append(markdown, description);
	tstring_append(markdown, "\n```");
	tjson_append_escaped(result, tstring_cstr(markdown));
	tstring_append(result, "},\"range\":");
	append_range(result, &document->frontend.document, span);
	tstring_append_c(result, '}');
	send_result(server, id, tstring_cstr(result));
	tstring_free(markdown);
	tstring_free(result);
}

static void handle_hover(tlsp_server *server, const tjson_value *message,
			 const tjson_value *id)
{
	const char *uri = nullptr;
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
		tstring *description = tstring_new_empty();
		tstring_append_fmt(description, "%s: %s", name,
			detail ? detail : "AnyType");
		send_hover(server, id, document, member.reference_span,
			tstring_cstr(description));
		tstring_free(description);
		return;
	}
	const tsemantic_symbol *symbol = tsemantic_symbol_at(
		&document->frontend.semantic, &document->frontend.arena, offset, nullptr);
	if (!symbol) {
		const tstandard_symbol *standard = nullptr;
		tsource_span span;
		if (!tworkspace_resolve_standard_at(
		    document, offset, &standard, &span)) {
			send_result(server, id, "null");
			return;
		}
		send_hover(server, id, document, span,
			standard->detail ? standard->detail : standard->name);
		return;
	}
	tstring *description = tstring_new_empty();
	const char *type = ttype_info_for_symbol(&document->frontend.types,
		&document->frontend.semantic, symbol);
	tstring_append_fmt(description, "%s %s%s%s",
		tsemantic_symbol_kind_name(symbol->kind), tstring_cstr(symbol->name),
		type ? ": " : "", type ? type : "");
	send_hover(server, id, document, symbol->span, tstring_cstr(description));
	tstring_free(description);
}

static void handle_definition(tlsp_server *server, const tjson_value *message,
			      const tjson_value *id)
{
	const char *uri = nullptr;
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
		&document->frontend.semantic, &document->frontend.arena, offset, nullptr);
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
	const char *uri = nullptr;
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
		tworkspace_reference *references = nullptr;
		uint32_t reference_count = tworkspace_find_references(
			&server->workspace, member.document, member.exported,
			&references);
		for (uint32_t i = 0; i < reference_count; i++) {
			if (!first) tstring_append_c(result, ',');
			append_location(result,
				tstring_cstr(references[i].document->uri),
				&references[i].document->frontend.document,
				references[i].span);
			first = 0;
		}
		free(references);
		tstring_append_c(result, ']');
		send_result(server, id, tstring_cstr(result));
		tstring_free(result); return;
	}
	const tsemantic_symbol *symbol = tsemantic_symbol_at(
		&document->frontend.semantic, &document->frontend.arena, offset, nullptr);
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
	tlsp_document *document = uri ? find_document(server, uri) : nullptr;
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
		return nullptr;
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
	const tsemantic_symbol *found = nullptr;
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
			tast_get(arena, symbol->declaration) : nullptr;
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
	tast_id receiver = tcontrol_find_node_at(&document->frontend.flow,
		&document->frontend.arena, document->frontend.root,
		receiver_end - 1);
	tast_id dictionary = dictionary_expression(document, receiver, 0);
	if (dictionary == TAST_INVALID_ID) {
		const tsemantic_symbol *symbol = visible_symbol_named(
			document, receiver_start, receiver_end, receiver_end);
		const tast_node *declaration = symbol ? tast_get(
			&document->frontend.arena, symbol->declaration) : nullptr;
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
		tworkspace_import_for_symbol(document, symbol) : nullptr;
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
	const char *uri = nullptr;
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
	const char *uri = nullptr;
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
		&document->frontend.semantic, &document->frontend.arena, offset, nullptr);
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
	const char *uri = nullptr;
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
		tworkspace_reference *references = nullptr;
		uint32_t reference_count = tworkspace_find_references(
			&server->workspace, member.document, member.exported,
			&references);
		tstring *result = tstring_new("{\"changes\":{");
		tjson_append_escaped(result, tstring_cstr(member.document->uri));
		tstring_append(result, ":[");
		append_text_edit(result, &member.document->frontend.document,
			member.exported->name_span, new_name);
		for (uint32_t i = 0; i < reference_count; i++)
			if (references[i].document == member.document) {
				tstring_append_c(result, ',');
				append_text_edit(result,
					&member.document->frontend.document,
					references[i].span, new_name);
			}
		tstring_append_c(result, ']');
		for (uint32_t i = 0; i < reference_count;) {
			const tworkspace_document *source = references[i].document;
			if (source == member.document) { i++; continue; }
			tstring_append_c(result, ',');
			tjson_append_escaped(result, tstring_cstr(source->uri));
			tstring_append(result, ":[");
			int first = 1;
			while (i < reference_count && references[i].document == source) {
				if (!first) tstring_append_c(result, ',');
				append_text_edit(result, &source->frontend.document,
					references[i].span, new_name);
				first = 0;
				i++;
			}
			tstring_append_c(result, ']');
		}
		free(references);
		tstring_append(result, "}}");
		send_result(server, id, tstring_cstr(result));
		tstring_free(result); return;
	}
	const tsemantic_symbol *symbol = tsemantic_symbol_at(
		&document->frontend.semantic, &document->frontend.arena, offset, nullptr);
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
			"{\"capabilities\":{\"positionEncoding\":\"utf-16\",\"textDocumentSync\":1,\"hoverProvider\":true,\"definitionProvider\":true,\"referencesProvider\":true,\"documentSymbolProvider\":true,\"workspaceSymbolProvider\":true,\"completionProvider\":{\"triggerCharacters\":[\":\"]},\"semanticTokensProvider\":{\"legend\":{\"tokenTypes\":[\"namespace\",\"type\",\"function\",\"parameter\",\"variable\",\"keyword\"],\"tokenModifiers\":[]},\"full\":true},\"renameProvider\":{\"prepareProvider\":true},\"workspace\":{\"workspaceFolders\":{\"supported\":true,\"changeNotifications\":false}}},\"serverInfo\":{\"name\":\"Tapas Language Server\",\"version\":\""
			TAPAS_VERSION "\"}}");
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
			tjson_string_value(tjson_get(changes->array.items[changes->array.count - 1], "text")) : nullptr;
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
	} else if (!strcmp(method, "textDocument/semanticTokens/full")) {
		handle_semantic_tokens(server, message, id);
	} else if (!strcmp(method, "tapas/syntaxCatalog")) {
		handle_syntax_catalog(server, id);
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
		if (!strcmp(line, "content-length")) length = (size_t)strtoull(colon + 1, nullptr, 10);
	}
	if (!saw_header || !length || length > 64 * 1024 * 1024) return nullptr;
	char *body = (char *)malloc(length + 1);
	if (!body) abort();
	if (fread(body, 1, length, input) != length) { free(body); return nullptr; }
	body[length] = '\0';
	return body;
}

int tlsp_run(FILE *input, FILE *output)
{
	if (!input || !output) return 1;
	tlsp_server server = { .input = input, .output = output };
	tworkspace_init(&server.workspace);
	init_semantic_names(&server);
	while (!server.exit_requested) {
		char *body = read_message(input);
		if (!body) break;
		const char *error = nullptr;
		tjson_value *message = tjson_parse(body, &error);
		free(body);
		if (message) {
			handle_message(&server, message);
			tjson_free(message);
		} else
			send_error(&server, nullptr, -32700,
				error ? error : "invalid JSON");
	}
	free(server.semantic_names);
	tworkspace_free(&server.workspace);
	return server.shutdown || !server.exit_requested ? 0 : 1;
}
