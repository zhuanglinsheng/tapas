#include "tapas/compile/type_info.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
	const tsource_document *document;
	const tast_arena *ast;
	const tsemantic_model *semantic;
	ttype_info_model *model;
	ttype_info_external_resolver external_resolver;
	void *external_context;
	uint32_t *declaration_symbols;
	uint32_t annotation_offset;
} type_analyzer;

static tstatic_type_id infer(type_analyzer *analyzer, tast_id id);
static tstatic_type_id static_value(type_analyzer *analyzer, tast_id id);

void ttype_info_model_init(ttype_info_model *model)
{
	*model = (ttype_info_model){ 0 };
	tstatic_type_arena_init(&model->arena);
}

void ttype_info_model_free(ttype_info_model *model)
{
	if (!model) return;
	for (uint32_t i = 0; i < model->node_count; i++)
		tstring_free(model->node_types[i]);
	for (uint32_t i = 0; i < model->symbol_count; i++)
		tstring_free(model->symbol_types[i]);
	free(model->node_type_ids);
	free(model->node_static_values);
	free(model->symbol_type_ids);
	free(model->symbol_static_values);
	free(model->node_types);
	free(model->symbol_types);
	tstatic_type_arena_free(&model->arena);
	*model = (ttype_info_model){ 0 };
}

static uint32_t symbol_for_declaration(const type_analyzer *analyzer, tast_id id)
{
	return id < analyzer->ast->node_count ?
		analyzer->declaration_symbols[id] : TSEMANTIC_INVALID_ID;
}

static tstatic_type_id resolve_annotation_name(void *context, const char *name)
{
	type_analyzer *analyzer = (type_analyzer *)context;
	if (strstr(name, "::"))
		return analyzer->external_resolver ? analyzer->external_resolver(
			analyzer->external_context, &analyzer->model->arena,
			name, 1) : TSTATIC_TYPE_UNKNOWN;
	uint32_t scope = tsemantic_scope_at(
		analyzer->semantic, analyzer->annotation_offset);
	for (uint32_t i = analyzer->semantic->symbol_count; i-- > 0;) {
		const tsemantic_symbol *symbol = &analyzer->semantic->symbols[i];
		if (!tstring_eq_cstr(symbol->name, name) ||
		    symbol->span.start >= analyzer->annotation_offset ||
		    !tsemantic_scope_contains(analyzer->semantic, symbol->scope, scope))
			continue;
		tstatic_type_id value = analyzer->model->symbol_static_values[i];
		return value == TSTATIC_TYPE_UNKNOWN ? TSTATIC_TYPE_INVALID_NAME : value;
	}
	return analyzer->external_resolver ? analyzer->external_resolver(
		analyzer->external_context, &analyzer->model->arena, name, 1) :
		TSTATIC_TYPE_UNKNOWN;
}

static tstatic_type_id parse_annotation(type_analyzer *analyzer,
					tsource_span span)
{
	tstring *text = tsource_document_slice(analyzer->document, span);
	analyzer->annotation_offset = span.start;
	tstatic_type_id result = tstatic_type_parse(&analyzer->model->arena,
		tstring_cstr(text), resolve_annotation_name, analyzer);
	tstring_free(text);
	return result == TSTATIC_TYPE_INVALID_NAME ? TSTATIC_TYPE_UNKNOWN : result;
}

static tstatic_type_id resolved_name(type_analyzer *analyzer, tast_id id)
{
	uint32_t symbol = id < analyzer->semantic->resolution_count ?
		analyzer->semantic->resolutions[id] : TSEMANTIC_INVALID_ID;
	if (symbol < analyzer->model->symbol_count)
		return analyzer->model->symbol_type_ids[symbol];
	if (!analyzer->external_resolver) return TSTATIC_TYPE_UNKNOWN;
	const tast_node *node = tast_get(analyzer->ast, id);
	if (!node) return TSTATIC_TYPE_UNKNOWN;
	tstring *name = tsource_document_slice(analyzer->document, node->span);
	tstatic_type_id result = analyzer->external_resolver(
		analyzer->external_context, &analyzer->model->arena,
		tstring_cstr(name), 0);
	tstring_free(name);
	return result;
}

static tstring *literal_string(type_analyzer *analyzer, const tast_node *node)
{
	if (!node || node->kind != tast_string ||
	    node->span.end < node->span.start + 2) return NULL;
	return tsource_document_slice(analyzer->document,
		(tsource_span){ node->span.start + 1, node->span.end - 1 });
}

static tstring *types_member_name(type_analyzer *analyzer,
				  const tast_node *node)
{
	if (!node || node->kind != tast_member ||
	    node->member.op != tsyntax_scope) return NULL;
	const tast_node *receiver = tast_get(analyzer->ast, node->member.receiver);
	if (!receiver || receiver->kind != tast_name) return NULL;
	tstring *scope = tsource_document_slice(analyzer->document, receiver->span);
	int matches = tstring_eq_cstr(scope, "types");
	tstring_free(scope);
	return matches ? tsource_document_slice(
		analyzer->document, node->member.name) : NULL;
}

static tstatic_type_id builtin_named(ttype_info_model *model, const char *name)
{
	static const char *const names[tstatic_builtin_count] = {
		"AnyType", "Nil", "Bool", "Int", "Float", "String", "List",
		"Pair", "Dictionary", "Iterator", "Function", "Library",
		"RealArray", "BoolArray", "Time", "Type"
	};
	for (uint32_t i = 0; i < tstatic_builtin_count; i++)
		if (strcmp(name, names[i]) == 0)
			return tstatic_type_builtin_id(&model->arena, (tstatic_builtin)i);
	return TSTATIC_TYPE_UNKNOWN;
}

static tstatic_type_id static_type_call(type_analyzer *analyzer,
					const tast_node *node)
{
	const tast_node *callee = tast_get(analyzer->ast, node->aggregate.receiver);
	tstring *member = types_member_name(analyzer, callee);
	if (!member) return TSTATIC_TYPE_UNKNOWN;
	const char *name = tstring_cstr(member);
	const tast_id *arguments = tast_get_children(analyzer->ast,
		node->aggregate.children, node->aggregate.count);
	uint32_t count = node->aggregate.count;
	tstatic_type_id result = TSTATIC_TYPE_UNKNOWN;
	if (strcmp(name, "list") == 0 && count == 1) {
		tstatic_type_id item = static_value(analyzer, arguments[0]);
		if (item != TSTATIC_TYPE_UNKNOWN)
			result = tstatic_type_make(&analyzer->model->arena,
				tstatic_type_list, &item, 1, 0);
	} else if ((strcmp(name, "pair") == 0 ||
		    strcmp(name, "dictionary") == 0) && count == 2) {
		tstatic_type_id values[2] = {
			static_value(analyzer, arguments[0]),
			static_value(analyzer, arguments[1])
		};
		if (values[0] != TSTATIC_TYPE_UNKNOWN &&
		    values[1] != TSTATIC_TYPE_UNKNOWN)
			result = tstatic_type_make(&analyzer->model->arena,
				strcmp(name, "pair") == 0 ? tstatic_type_pair :
				tstatic_type_dictionary, values, 2, 0);
	} else if (strcmp(name, "union") == 0 && count >= 2) {
		tstatic_type_id *values = (tstatic_type_id *)calloc(count, sizeof(*values));
		if (!values) abort();
		int valid = 1;
		for (uint32_t i = 0; i < count; i++) {
			values[i] = static_value(analyzer, arguments[i]);
			valid = valid && values[i] != TSTATIC_TYPE_UNKNOWN;
		}
		if (valid) result = tstatic_type_make(&analyzer->model->arena,
			tstatic_type_union, values, count, 0);
		free(values);
	} else if (strcmp(name, "make_type") == 0 && count) {
		tstatic_field *fields = (tstatic_field *)calloc(count, sizeof(*fields));
		if (!fields) abort();
		int valid = 1;
		for (uint32_t i = 0; i < count; i++) {
			const tast_node *pair = tast_get(analyzer->ast, arguments[i]);
			if (!pair || pair->kind != tast_binary ||
			    pair->binary.op != tsyntax_colon) { valid = 0; break; }
			fields[i].name = literal_string(analyzer,
				tast_get(analyzer->ast, pair->binary.left));
			fields[i].type = static_value(analyzer, pair->binary.right);
			valid = valid && fields[i].name &&
				fields[i].type != TSTATIC_TYPE_UNKNOWN;
		}
		if (valid) result = tstatic_type_make_fields(
			&analyzer->model->arena, fields, count);
		for (uint32_t i = 0; i < count; i++) tstring_free(fields[i].name);
		free(fields);
	}
	tstring_free(member);
	return result;
}

static tstatic_type_id static_value(type_analyzer *analyzer, tast_id id)
{
	const tast_node *node = tast_get(analyzer->ast, id);
	if (!node) return TSTATIC_TYPE_UNKNOWN;
	if (node->kind == tast_group) return static_value(analyzer, node->group.value);
	if (node->kind == tast_name) {
		const tsemantic_symbol *symbol = tsemantic_resolved_symbol(
			analyzer->semantic, id);
		if (!symbol) {
			if (!analyzer->external_resolver) return TSTATIC_TYPE_UNKNOWN;
			tstring *name = tsource_document_slice(
				analyzer->document, node->span);
			tstatic_type_id result = analyzer->external_resolver(
				analyzer->external_context, &analyzer->model->arena,
				tstring_cstr(name), 1);
			tstring_free(name);
			return result;
		}
		uint32_t sid = (uint32_t)(symbol - analyzer->semantic->symbols);
		return sid < analyzer->model->symbol_count ?
			analyzer->model->symbol_static_values[sid] : TSTATIC_TYPE_UNKNOWN;
	}
	if (node->kind == tast_member) {
		tstring *member = types_member_name(analyzer, node);
		tstatic_type_id result = member ? builtin_named(
			analyzer->model, tstring_cstr(member)) : TSTATIC_TYPE_UNKNOWN;
		tstring_free(member);
		if (result == TSTATIC_TYPE_UNKNOWN && analyzer->external_resolver) {
			tstring *name = tsource_document_slice(
				analyzer->document, node->span);
			result = analyzer->external_resolver(
				analyzer->external_context, &analyzer->model->arena,
				tstring_cstr(name), 1);
			tstring_free(name);
		}
		return result;
	}
	return node->kind == tast_call ? static_type_call(analyzer, node) :
		TSTATIC_TYPE_UNKNOWN;
}

static tstatic_type_id union_of(type_analyzer *analyzer,
				tstatic_type_id *types, uint32_t count)
{
	if (!count) return TSTATIC_TYPE_UNKNOWN;
	for (uint32_t i = 0; i < count; i++)
		if (types[i] == TSTATIC_TYPE_UNKNOWN) return TSTATIC_TYPE_UNKNOWN;
	int same = 1;
	for (uint32_t i = 1; i < count; i++)
		same = same && tstatic_type_equal(
			&analyzer->model->arena, types[0], types[i]);
	return same ? types[0] : tstatic_type_make(&analyzer->model->arena,
		tstatic_type_union, types, count, 0);
}

static tstatic_type_id infer_function(type_analyzer *analyzer, tast_id id,
				      const tast_node *node)
{
	uint32_t count = node->function.parameter_count;
	tstatic_type_id *signature = (tstatic_type_id *)calloc(
		count + 1, sizeof(*signature));
	if (!signature) abort();
	const tast_id *parameters = tast_get_children(analyzer->ast,
		node->function.parameters, count);
	for (uint32_t i = 0; i < count; i++) {
		const tast_node *parameter = tast_get(analyzer->ast, parameters[i]);
		signature[i] = parameter && parameter->parameter.has_annotation ?
			parse_annotation(analyzer, parameter->parameter.annotation) :
			TSTATIC_TYPE_UNKNOWN;
		uint32_t sid = symbol_for_declaration(analyzer, parameters[i]);
		if (sid < analyzer->model->symbol_count)
			analyzer->model->symbol_type_ids[sid] = signature[i];
	}
	signature[count] = node->function.has_return_annotation ?
		parse_annotation(analyzer, node->function.return_annotation) :
		TSTATIC_TYPE_UNKNOWN;
	tstatic_type_id result = tstatic_type_make(&analyzer->model->arena,
		tstatic_type_function, signature, count + 1, node->function.variadic);
	free(signature);
	analyzer->model->node_type_ids[id] = result;
	return result;
}

static tstatic_type_id enclosing_function(type_analyzer *analyzer,
					  const tast_node *child)
{
	tast_id closest = TAST_INVALID_ID;
	uint32_t closest_size = UINT32_MAX;
	for (tast_id id = 0; id < analyzer->ast->node_count; id++) {
		const tast_node *node = tast_get(analyzer->ast, id);
		if (!node || node->kind != tast_function ||
		    node->span.start > child->span.start ||
		    node->span.end < child->span.end)
			continue;
		uint32_t size = node->span.end - node->span.start;
		if (size < closest_size) {
			closest = id;
			closest_size = size;
		}
	}
	return closest == TAST_INVALID_ID ? TSTATIC_TYPE_UNKNOWN :
		infer(analyzer, closest);
}

static tstatic_type_id infer_dictionary(type_analyzer *analyzer,
					const tast_node *node)
{
	uint32_t count = node->aggregate.count / 2;
	if (!count) return tstatic_builtin_dictionary;
	const tast_id *entries = tast_get_children(analyzer->ast,
		node->aggregate.children, node->aggregate.count);
	tstatic_field *fields = (tstatic_field *)calloc(count, sizeof(*fields));
	if (!fields) abort();
	int valid = 1;
	for (uint32_t i = 0; i < count; i++) {
		fields[i].name = literal_string(analyzer,
			tast_get(analyzer->ast, entries[i * 2]));
		fields[i].type = infer(analyzer, entries[i * 2 + 1]);
		valid = valid && fields[i].name &&
			fields[i].type != TSTATIC_TYPE_UNKNOWN;
	}
	tstatic_type_id result = valid ? tstatic_type_make_fields(
		&analyzer->model->arena, fields, count) : tstatic_builtin_dictionary;
	for (uint32_t i = 0; i < count; i++) tstring_free(fields[i].name);
	free(fields);
	return result;
}

static tstatic_type_id infer_index(type_analyzer *analyzer,
				   const tast_node *node)
{
	tstatic_type_id receiver_id = infer(analyzer, node->aggregate.receiver);
	const tstatic_type *receiver = tstatic_type_get(
		&analyzer->model->arena, receiver_id);
	if (!receiver || node->aggregate.count != 1) return TSTATIC_TYPE_UNKNOWN;
	const tast_id *indices = tast_get_children(analyzer->ast,
		node->aggregate.children, node->aggregate.count);
	const tast_node *index = tast_get(analyzer->ast, indices[0]);
	if (index && index->kind == tast_slice &&
	    receiver->kind == tstatic_type_list) return receiver_id;
	const tstatic_type_id *children = tstatic_type_children(
		&analyzer->model->arena, receiver);
	if (receiver->kind == tstatic_type_list && receiver->child_count == 1)
		return children[0];
	if (receiver->kind == tstatic_type_dictionary && receiver->child_count == 2)
		return children[1];
	if (receiver->kind == tstatic_type_fields) {
		tstring *name = literal_string(analyzer, index);
		const tstatic_field *fields = tstatic_type_field_items(
			&analyzer->model->arena, receiver);
		for (uint32_t i = 0; name && i < receiver->field_count; i++)
			if (tstring_eq(fields[i].name, name)) {
				tstatic_type_id result = fields[i].type;
				tstring_free(name);
				return result;
			}
		tstring_free(name);
	}
	return TSTATIC_TYPE_UNKNOWN;
}

static tstatic_type_id infer(type_analyzer *analyzer, tast_id id)
{
	if (id >= analyzer->model->node_count) return TSTATIC_TYPE_UNKNOWN;
	if (analyzer->model->node_type_ids[id] != TSTATIC_TYPE_UNKNOWN)
		return analyzer->model->node_type_ids[id];
	const tast_node *node = tast_get(analyzer->ast, id);
	if (!node) return TSTATIC_TYPE_UNKNOWN;
	tstatic_type_id result = TSTATIC_TYPE_UNKNOWN;
	switch (node->kind) {
	case tast_nil: result = tstatic_builtin_nil; break;
	case tast_bool: result = tstatic_builtin_bool; break;
	case tast_integer: result = tstatic_builtin_int; break;
	case tast_float: result = tstatic_builtin_float; break;
	case tast_string: result = tstatic_builtin_string; break;
	case tast_name: {
		tstring *name = tsource_document_slice(analyzer->document, node->span);
		result = tstring_eq_cstr(name, "this") ? enclosing_function(analyzer, node) :
			resolved_name(analyzer, id);
		tstring_free(name);
	} break;
	case tast_function: return infer_function(analyzer, id, node);
	case tast_group: result = infer(analyzer, node->group.value); break;
	case tast_unary: result = infer(analyzer, node->unary.operand); break;
	case tast_binary:
		if (node->binary.op == tsyntax_colon) {
			tstatic_type_id values[2] = {
				infer(analyzer, node->binary.left),
				infer(analyzer, node->binary.right)
			};
			if (values[0] != TSTATIC_TYPE_UNKNOWN &&
			    values[1] != TSTATIC_TYPE_UNKNOWN)
				result = tstatic_type_make(&analyzer->model->arena,
					tstatic_type_pair, values, 2, 0);
		} else if (node->binary.op == tsyntax_eq || node->binary.op == tsyntax_ne ||
			node->binary.op == tsyntax_gt || node->binary.op == tsyntax_ge ||
			node->binary.op == tsyntax_lt || node->binary.op == tsyntax_le ||
			node->binary.op == tsyntax_kw_in || node->binary.op == tsyntax_kw_and ||
			node->binary.op == tsyntax_kw_or) result = tstatic_builtin_bool;
		else if (node->binary.op == tsyntax_kw_to) result = tstatic_builtin_iterator;
		else {
			tstatic_type_id left = infer(analyzer, node->binary.left);
			tstatic_type_id right = infer(analyzer, node->binary.right);
			if (left == tstatic_builtin_float || right == tstatic_builtin_float)
				result = tstatic_builtin_float;
			else if (tstatic_type_equal(&analyzer->model->arena, left, right))
				result = left;
		}
		break;
	case tast_list: {
		const tast_id *items = tast_get_children(analyzer->ast,
			node->aggregate.children, node->aggregate.count);
		if (!node->aggregate.count) { result = tstatic_builtin_list; break; }
		tstatic_type_id *types = (tstatic_type_id *)calloc(
			node->aggregate.count, sizeof(*types));
		if (!types) abort();
		for (uint32_t i = 0; i < node->aggregate.count; i++)
			types[i] = infer(analyzer, items[i]);
		tstatic_type_id item = union_of(analyzer, types, node->aggregate.count);
		free(types);
		result = item == TSTATIC_TYPE_UNKNOWN ? tstatic_builtin_list :
			tstatic_type_make(&analyzer->model->arena,
				tstatic_type_list, &item, 1, 0);
	} break;
	case tast_dictionary: result = infer_dictionary(analyzer, node); break;
	case tast_index: result = infer_index(analyzer, node); break;
	case tast_call: {
		const tast_id *arguments = tast_get_children(analyzer->ast,
			node->aggregate.children, node->aggregate.count);
		for (uint32_t i = 0; i < node->aggregate.count; i++)
			infer(analyzer, arguments[i]);
		tstatic_type_id callee = infer(analyzer, node->aggregate.receiver);
		const tstatic_type *function = tstatic_type_get(
			&analyzer->model->arena, callee);
		if (function && function->kind == tstatic_type_function) {
			const tstatic_type_id *signature = tstatic_type_children(
				&analyzer->model->arena, function);
			result = function->child_count ?
				signature[function->child_count - 1] : TSTATIC_TYPE_UNKNOWN;
		}
	} break;
	case tast_member: {
		tstring *member = types_member_name(analyzer, node);
		if (member && builtin_named(analyzer->model,
		    tstring_cstr(member)) != TSTATIC_TYPE_UNKNOWN)
			result = tstatic_builtin_type;
		tstring_free(member);
		if (result == TSTATIC_TYPE_UNKNOWN) {
			tstatic_type_id receiver_id = infer(
				analyzer, node->member.receiver);
			const tstatic_type *receiver = tstatic_type_get(
				&analyzer->model->arena, receiver_id);
			if (receiver && receiver->kind == tstatic_type_fields) {
				tstring *name = tsource_document_slice(
					analyzer->document, node->member.name);
				const tstatic_field *fields = tstatic_type_field_items(
					&analyzer->model->arena, receiver);
				for (uint32_t i = 0; i < receiver->field_count; i++)
					if (tstring_eq(fields[i].name, name)) {
						result = fields[i].type;
						break;
					}
				tstring_free(name);
			}
		}
		if (result == TSTATIC_TYPE_UNKNOWN && analyzer->external_resolver) {
			tstring *name = tsource_document_slice(
				analyzer->document, node->span);
			result = analyzer->external_resolver(
				analyzer->external_context, &analyzer->model->arena,
				tstring_cstr(name), 0);
			tstring_free(name);
		}
	} break;
	default: break;
	}
	analyzer->model->node_type_ids[id] = result;
	return result;
}

static void analyze_declarations(type_analyzer *analyzer)
{
	tast_id *declarations = (tast_id *)calloc(
		analyzer->ast->node_count, sizeof(*declarations));
	if (!declarations && analyzer->ast->node_count) abort();
	uint32_t count = 0;
	for (tast_id id = 0; id < analyzer->ast->node_count; id++) {
		const tast_node *node = tast_get(analyzer->ast, id);
		if (node && node->kind == tast_declaration_statement)
			declarations[count++] = id;
	}
	for (uint32_t i = 1; i < count; i++) {
		tast_id value = declarations[i];
		const tast_node *value_node = tast_get(analyzer->ast, value);
		uint32_t at = i;
		while (at > 0) {
			const tast_node *previous = tast_get(
				analyzer->ast, declarations[at - 1]);
			if (previous->span.start <= value_node->span.start) break;
			declarations[at] = declarations[at - 1];
			at--;
		}
		declarations[at] = value;
	}
	/* A declaration without an initializer and explicitly annotated Type is a
	 * forward Type definition. Register every placeholder before resolving any
	 * initializer so mutually recursive definitions share the same graph. */
	for (uint32_t i = 0; i < count; i++) {
		const tast_node *node = tast_get(analyzer->ast, declarations[i]);
		if (!node || node->declaration_statement.has_initializer ||
		    node->declaration_statement.is_mutable ||
		    !node->declaration_statement.has_annotation)
			continue;
		tstatic_type_id annotation = parse_annotation(
			analyzer, node->declaration_statement.annotation);
		if (annotation != tstatic_builtin_type) continue;
		uint32_t sid = symbol_for_declaration(analyzer, declarations[i]);
		if (sid < analyzer->model->symbol_count) {
			analyzer->model->symbol_type_ids[sid] = tstatic_builtin_type;
			analyzer->model->symbol_static_values[sid] =
				tstatic_type_make_recursive(&analyzer->model->arena);
		}
	}
	for (uint32_t i = 0; i < count; i++) {
		tast_id id = declarations[i];
		const tast_node *node = tast_get(analyzer->ast, id);
		tstatic_type_id inferred = node->declaration_statement.has_initializer ?
			infer(analyzer, node->declaration_statement.initializer) :
			TSTATIC_TYPE_UNKNOWN;
		tstatic_type_id target = node->declaration_statement.has_annotation ?
			parse_annotation(analyzer, node->declaration_statement.annotation) :
			inferred;
		uint32_t sid = symbol_for_declaration(analyzer, id);
		if (sid < analyzer->model->symbol_count) {
			analyzer->model->symbol_type_ids[sid] = target;
			if (!node->declaration_statement.is_mutable &&
			    node->declaration_statement.has_initializer)
				analyzer->model->symbol_static_values[sid] = static_value(
					analyzer, node->declaration_statement.initializer);
		}
	}
	free(declarations);
}

static void analyze_recursive_assignments(type_analyzer *analyzer)
{
	for (tast_id id = 0; id < analyzer->ast->node_count; id++) {
		const tast_node *node = tast_get(analyzer->ast, id);
		if (!node || node->kind != tast_assignment_statement) continue;
		const tast_node *target = tast_get(
			analyzer->ast, node->assignment_statement.target);
		if (!target || target->kind != tast_name) continue;
		const tsemantic_symbol *symbol = tsemantic_resolved_symbol(
			analyzer->semantic, node->assignment_statement.target);
		if (!symbol) continue;
		uint32_t sid = (uint32_t)(symbol - analyzer->semantic->symbols);
		if (sid >= analyzer->model->symbol_count) continue;
		tstatic_type_id recursive =
			analyzer->model->symbol_static_values[sid];
		tstatic_type_id body = static_value(
			analyzer, node->assignment_statement.value);
		const tstatic_type *type = tstatic_type_get(
			&analyzer->model->arena, recursive);
		if (type && type->kind == tstatic_type_recursive &&
		    !type->child_count && body != TSTATIC_TYPE_UNKNOWN)
			tstatic_type_define_recursive(
				&analyzer->model->arena, recursive, body);
		else if (analyzer->model->symbol_type_ids[sid] ==
			 tstatic_builtin_type)
			analyzer->model->symbol_static_values[sid] = body;
	}
}

void ttype_info_analyze_with_resolver(
	const tsource_document *document, const tast_arena *arena,
	const tsemantic_model *semantic, ttype_info_model *model,
	ttype_info_external_resolver resolver, void *resolver_context)
{
	ttype_info_model_free(model);
	ttype_info_model_init(model);
	model->node_count = arena->node_count;
	model->symbol_count = semantic->symbol_count;
	model->node_type_ids = (tstatic_type_id *)malloc(
		model->node_count * sizeof(*model->node_type_ids));
	model->node_static_values = (tstatic_type_id *)malloc(
		model->node_count * sizeof(*model->node_static_values));
	model->symbol_type_ids = (tstatic_type_id *)malloc(
		model->symbol_count * sizeof(*model->symbol_type_ids));
	model->symbol_static_values = (tstatic_type_id *)malloc(
		model->symbol_count * sizeof(*model->symbol_static_values));
	model->node_types = (tstring **)calloc(model->node_count, sizeof(tstring *));
	model->symbol_types = (tstring **)calloc(model->symbol_count, sizeof(tstring *));
	if ((model->node_count && (!model->node_type_ids ||
	     !model->node_static_values || !model->node_types)) ||
	    (model->symbol_count && (!model->symbol_type_ids ||
	     !model->symbol_static_values || !model->symbol_types))) abort();
	for (uint32_t i = 0; i < model->node_count; i++) {
		model->node_type_ids[i] = TSTATIC_TYPE_UNKNOWN;
		model->node_static_values[i] = TSTATIC_TYPE_UNKNOWN;
	}
	for (uint32_t i = 0; i < model->symbol_count; i++) {
		model->symbol_type_ids[i] = semantic->symbols[i].kind ==
			tsemantic_symbol_import ? tstatic_builtin_library : TSTATIC_TYPE_UNKNOWN;
		model->symbol_static_values[i] = TSTATIC_TYPE_UNKNOWN;
	}
	type_analyzer analyzer = {
		.document = document,
		.ast = arena,
		.semantic = semantic,
		.model = model,
		.external_resolver = resolver,
		.external_context = resolver_context,
		.declaration_symbols = (uint32_t *)malloc(
			arena->node_count * sizeof(uint32_t))
	};
	if (arena->node_count && !analyzer.declaration_symbols) abort();
	for (uint32_t i = 0; i < arena->node_count; i++)
		analyzer.declaration_symbols[i] = TSEMANTIC_INVALID_ID;
	for (uint32_t i = 0; i < semantic->symbol_count; i++) {
		tast_id declaration = semantic->symbols[i].declaration;
		if (declaration < arena->node_count)
			analyzer.declaration_symbols[declaration] = i;
	}
	analyze_declarations(&analyzer);
	analyze_recursive_assignments(&analyzer);
	for (tast_id id = 0; id < arena->node_count; id++) {
		infer(&analyzer, id);
		model->node_static_values[id] = static_value(&analyzer, id);
	}
	for (uint32_t i = 0; i < model->node_count; i++)
		if (model->node_type_ids[i] != TSTATIC_TYPE_UNKNOWN)
			model->node_types[i] = tstatic_type_format(
				&model->arena, model->node_type_ids[i]);
	for (uint32_t i = 0; i < model->symbol_count; i++)
		if (model->symbol_type_ids[i] != TSTATIC_TYPE_UNKNOWN)
			model->symbol_types[i] = tstatic_type_format(
				&model->arena, model->symbol_type_ids[i]);
	free(analyzer.declaration_symbols);
}

void ttype_info_analyze(const tsource_document *document,
			const tast_arena *arena,
			const tsemantic_model *semantic,
			ttype_info_model *model)
{
	ttype_info_analyze_with_resolver(
		document, arena, semantic, model, NULL, NULL);
}

tstatic_type_id ttype_info_node_id(const ttype_info_model *model, tast_id node)
{
	return model && node < model->node_count ?
		model->node_type_ids[node] : TSTATIC_TYPE_UNKNOWN;
}

tstatic_type_id ttype_info_node_static_value(
	const ttype_info_model *model, tast_id node)
{
	return model && node < model->node_count ?
		model->node_static_values[node] : TSTATIC_TYPE_UNKNOWN;
}

tstatic_type_id ttype_info_symbol_id(const ttype_info_model *model,
				     const tsemantic_model *semantic,
				     const tsemantic_symbol *symbol)
{
	if (!model || !semantic || !symbol) return TSTATIC_TYPE_UNKNOWN;
	uint32_t id = (uint32_t)(symbol - semantic->symbols);
	return id < model->symbol_count ? model->symbol_type_ids[id] :
		TSTATIC_TYPE_UNKNOWN;
}

const char *ttype_info_for_node(const ttype_info_model *model, tast_id node)
{
	return model && node < model->node_count && model->node_types[node] ?
		tstring_cstr(model->node_types[node]) : NULL;
}

const char *ttype_info_for_symbol(const ttype_info_model *model,
				 const tsemantic_model *semantic,
				 const tsemantic_symbol *symbol)
{
	if (!model || !semantic || !symbol) return NULL;
	uint32_t id = (uint32_t)(symbol - semantic->symbols);
	return id < model->symbol_count && model->symbol_types[id] ?
		tstring_cstr(model->symbol_types[id]) : NULL;
}
