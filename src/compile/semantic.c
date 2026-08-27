#include "tapas/compile/semantic.h"

#include <stdlib.h>

typedef struct {
	const tsource_document *document;
	const tast_arena *arena;
	tsemantic_model *model;
	tdiagnostics *diagnostics;
} semantic_analyzer;

void tsemantic_model_init(tsemantic_model *model)
{
	*model = (tsemantic_model){ 0 };
}

void tsemantic_model_free(tsemantic_model *model)
{
	if (!model)
		return;
	for (uint32_t i = 0; i < model->symbol_count; i++)
		tstring_free(model->symbols[i].name);
	free(model->symbols);
	free(model->scopes);
	free(model->resolutions);
	*model = (tsemantic_model){ 0 };
}

static uint32_t add_scope(semantic_analyzer *analyzer, uint32_t parent,
			  tsource_span span)
{
	tsemantic_model *model = analyzer->model;
	if (model->scope_count == model->scope_capacity) {
		uint32_t capacity = model->scope_capacity ?
			model->scope_capacity * 2 : 16;
		model->scopes = (tsemantic_scope *)realloc(
			model->scopes, capacity * sizeof(tsemantic_scope));
		if (!model->scopes)
			abort();
		model->scope_capacity = capacity;
	}
	uint32_t id = model->scope_count++;
	model->scopes[id] = (tsemantic_scope){ parent, span };
	return id;
}

static uint32_t find_in_scope(const semantic_analyzer *analyzer,
			      uint32_t scope, const tstring *name,
			      int parents)
{
	for (;;) {
		for (uint32_t i = analyzer->model->symbol_count; i > 0; i--) {
			const tsemantic_symbol *symbol =
				&analyzer->model->symbols[i - 1];
			if (symbol->scope == scope && tstring_eq(symbol->name, name))
				return i - 1;
		}
		if (!parents || scope == TSEMANTIC_INVALID_ID)
			break;
		scope = analyzer->model->scopes[scope].parent;
	}
	return TSEMANTIC_INVALID_ID;
}

static uint32_t declare_symbol(semantic_analyzer *analyzer, uint32_t scope,
			       tsource_span span, tast_id declaration,
			       tsemantic_symbol_kind kind)
{
	tstring *name = tsource_document_slice(analyzer->document, span);
	if (find_in_scope(analyzer, scope, name, 0) != TSEMANTIC_INVALID_ID)
		tdiagnostics_add(analyzer->diagnostics, tdiagnostic_error, span,
			"name is already declared in this scope");
	tsemantic_model *model = analyzer->model;
	if (model->symbol_count == model->symbol_capacity) {
		uint32_t capacity = model->symbol_capacity ?
			model->symbol_capacity * 2 : 32;
		model->symbols = (tsemantic_symbol *)realloc(
			model->symbols, capacity * sizeof(tsemantic_symbol));
		if (!model->symbols)
			abort();
		model->symbol_capacity = capacity;
	}
	uint32_t id = model->symbol_count++;
	model->symbols[id] = (tsemantic_symbol){
		.name = name, .span = span, .declaration = declaration,
		.scope = scope, .kind = kind
	};
	return id;
}

static void analyze_node(semantic_analyzer *analyzer, tast_id id,
			 uint32_t scope);

static void analyze_children(semantic_analyzer *analyzer,
			     const tast_node *node, uint32_t scope)
{
	const tast_id *children = tast_get_children(analyzer->arena,
		node->aggregate.children, node->aggregate.count);
	for (uint32_t i = 0; i < node->aggregate.count; i++)
		analyze_node(analyzer, children[i], scope);
}

static void resolve_name(semantic_analyzer *analyzer, tast_id id,
			 const tast_node *node, uint32_t scope)
{
	tstring *name = tsource_document_slice(analyzer->document, node->span);
	analyzer->model->resolutions[id] = find_in_scope(
		analyzer, scope, name, 1);
	tstring_free(name);
}

static void analyze_node(semantic_analyzer *analyzer, tast_id id,
			 uint32_t scope)
{
	const tast_node *node = tast_get(analyzer->arena, id);
	if (!node)
		return;
	switch (node->kind) {
	case tast_name:
		resolve_name(analyzer, id, node, scope);
		break;
	case tast_group:
		analyze_node(analyzer, node->group.value, scope);
		break;
	case tast_unary:
		analyze_node(analyzer, node->unary.operand, scope);
		break;
	case tast_binary:
		analyze_node(analyzer, node->binary.left, scope);
		analyze_node(analyzer, node->binary.right, scope);
		break;
	case tast_member:
		analyze_node(analyzer, node->member.receiver, scope);
		break;
	case tast_call:
	case tast_index:
		analyze_node(analyzer, node->aggregate.receiver, scope);
		analyze_children(analyzer, node, scope);
		break;
	case tast_list:
	case tast_dictionary:
	case tast_structure:
		analyze_children(analyzer, node, scope);
		break;
	case tast_named_field:
		analyze_node(analyzer, node->named_field.value, scope);
		break;
	case tast_slice:
		if (node->slice.has_start)
			analyze_node(analyzer, node->slice.start, scope);
		if (node->slice.has_end)
			analyze_node(analyzer, node->slice.end, scope);
		break;
	case tast_function: {
		uint32_t function_scope = add_scope(
			analyzer, scope, node->span);
		const tast_id *parameters = tast_get_children(analyzer->arena,
			node->function.parameters, node->function.parameter_count);
		for (uint32_t i = 0; i < node->function.parameter_count; i++) {
			const tast_node *parameter = tast_get(
				analyzer->arena, parameters[i]);
			declare_symbol(analyzer, function_scope, parameter->span,
				parameters[i], tsemantic_symbol_parameter);
		}
		analyze_node(analyzer, node->function.body, function_scope);
	} break;
	case tast_module:
	case tast_block: {
		uint32_t block_scope = node->kind == tast_module ? scope :
			add_scope(analyzer, scope, node->span);
		analyze_children(analyzer, node, block_scope);
	} break;
	case tast_expression_statement:
		analyze_node(analyzer, node->expression_statement.value, scope);
		break;
	case tast_declaration_statement:
		analyze_node(analyzer, node->declaration_statement.initializer, scope);
		declare_symbol(analyzer, scope,
			node->declaration_statement.name, id,
			node->declaration_statement.is_mutable ?
				tsemantic_symbol_var : tsemantic_symbol_let);
		break;
	case tast_declaration_group:
		analyze_children(analyzer, node, scope);
		break;
	case tast_assignment_statement:
		analyze_node(analyzer, node->assignment_statement.target, scope);
		analyze_node(analyzer, node->assignment_statement.value, scope);
		break;
	case tast_return_statement:
		if (node->return_statement.value != TAST_INVALID_ID)
			analyze_node(analyzer, node->return_statement.value, scope);
		break;
	case tast_import_statement:
		if (node->import_statement.has_alias)
			declare_symbol(analyzer, scope, node->import_statement.alias,
				id, tsemantic_symbol_import);
		break;
	case tast_if_statement:
	case tast_elif_statement:
	case tast_while_statement:
		analyze_node(analyzer, node->control_statement.condition, scope);
		analyze_node(analyzer, node->control_statement.body, scope);
		break;
	case tast_else_statement:
		analyze_node(analyzer, node->control_statement.body, scope);
		break;
	case tast_for_statement: {
		analyze_node(analyzer, node->for_statement.iterable, scope);
		uint32_t loop_scope = add_scope(analyzer, scope, node->span);
		if (node->for_statement.declares_binding)
			declare_symbol(analyzer, loop_scope, node->for_statement.name,
				id, tsemantic_symbol_iteration);
		analyze_node(analyzer, node->for_statement.body, loop_scope);
	} break;
	default:
		break;
	}
}

void tsemantic_analyze(const tsource_document *document,
		       const tast_arena *arena, tast_id root,
		       tsemantic_model *model, tdiagnostics *diagnostics)
{
	tsemantic_model_free(model);
	model->resolution_count = arena->node_count;
	model->resolutions = (uint32_t *)malloc(
		model->resolution_count * sizeof(uint32_t));
	if (model->resolution_count && !model->resolutions)
		abort();
	for (uint32_t i = 0; i < model->resolution_count; i++)
		model->resolutions[i] = TSEMANTIC_INVALID_ID;
	semantic_analyzer analyzer = { document, arena, model, diagnostics };
	const tast_node *root_node = tast_get(arena, root);
	uint32_t root_scope = add_scope(&analyzer, TSEMANTIC_INVALID_ID,
		root_node ? root_node->span : (tsource_span){ 0, 0 });
	analyze_node(&analyzer, root, root_scope);
}

const tsemantic_symbol *tsemantic_resolved_symbol(
	const tsemantic_model *model, tast_id reference)
{
	if (!model || reference >= model->resolution_count)
		return NULL;
	uint32_t symbol = model->resolutions[reference];
	return symbol < model->symbol_count ? &model->symbols[symbol] : NULL;
}

const tsemantic_symbol *tsemantic_symbol_at(
	const tsemantic_model *model, const tast_arena *arena,
	uint32_t offset, tast_id *reference)
{
	if (reference)
		*reference = TAST_INVALID_ID;
	if (!model || !arena)
		return NULL;
	for (tast_id id = 0; id < model->resolution_count; id++) {
		const tast_node *node = tast_get(arena, id);
		if (!node || node->kind != tast_name ||
		    offset < node->span.start || offset > node->span.end)
			continue;
		const tsemantic_symbol *symbol = tsemantic_resolved_symbol(model, id);
		if (symbol) {
			if (reference)
				*reference = id;
			return symbol;
		}
	}
	for (uint32_t i = 0; i < model->symbol_count; i++) {
		const tsemantic_symbol *symbol = &model->symbols[i];
		if (symbol->span.start <= offset && offset <= symbol->span.end)
			return symbol;
	}
	return NULL;
}

const char *tsemantic_symbol_kind_name(tsemantic_symbol_kind kind)
{
	static const char *const names[] = {
		"let", "var", "parameter", "import", "iteration variable"
	};
	return (unsigned)kind < sizeof(names) / sizeof(names[0]) ?
		names[kind] : "symbol";
}

uint32_t tsemantic_scope_at(const tsemantic_model *model, uint32_t offset)
{
	if (!model) return TSEMANTIC_INVALID_ID;
	uint32_t best = TSEMANTIC_INVALID_ID;
	uint32_t width = UINT32_MAX;
	for (uint32_t i = 0; i < model->scope_count; i++) {
		tsource_span span = model->scopes[i].span;
		if (span.start <= offset && offset <= span.end &&
		    span.end - span.start <= width) {
			best = i;
			width = span.end - span.start;
		}
	}
	return best;
}

int tsemantic_scope_contains(const tsemantic_model *model,
			     uint32_t scope, uint32_t descendant)
{
	if (!model || scope >= model->scope_count) return 0;
	while (descendant < model->scope_count) {
		if (descendant == scope) return 1;
		descendant = model->scopes[descendant].parent;
	}
	return 0;
}
