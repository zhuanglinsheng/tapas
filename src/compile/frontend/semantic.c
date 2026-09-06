#include "compile/frontend/semantic.h"
#include "tapas/dsa/tstring.h"
#include "compile/frontend/diagnostic.h"

#include <stdlib.h>

typedef struct {
	const tsource_document *document;
	const tast_arena *arena;
	tsemantic_model *model;
	tdiagnostics *diagnostics;
	tsemantic_external_resolver external_resolver;
	void *external_context;
	const tsyntax_tokens *tokens;
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
	free(model->declaration_symbols);
	free(model->annotations.items);
	*model = (tsemantic_model){ 0 };
}

static uint32_t add_scope(semantic_analyzer *analyzer, uint32_t parent,
			  tsource_span span, int function_boundary,
			  int rule_boundary)
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
	model->scopes[id] = (tsemantic_scope){
		parent, span, function_boundary ? 1 : 0, rule_boundary ? 1 : 0
	};
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

const tannotation_reference *tannotation_reference_at(
	const tannotation_index *index, uint32_t offset)
{
	for (uint32_t i = 0; index && i < index->count; i++)
		if (index->items[i].span.start <= offset && offset < index->items[i].span.end)
			return &index->items[i];
	return nullptr;
}

static void index_annotation(semantic_analyzer *analyzer,
			     tsource_span span, uint32_t scope)
{
	tannotation_index *index = &analyzer->model->annotations;
	uint32_t previous = UINT32_MAX;
	int member = 0;
	for (uint32_t i = 0; i < analyzer->tokens->count; i++) {
		const tsyntax_token *token = &analyzer->tokens->items[i];
		if (token->span.start < span.start) continue;
		if (token->span.end > span.end) break;
		if (tsyntax_kind_is_trivia(token->kind)) continue;
		if (token->kind == tsyntax_scope && previous != UINT32_MAX) {
			member = 1;
			continue;
		}
		if (token->kind != tsyntax_identifier) {
			previous = UINT32_MAX;
			member = 0;
			continue;
		}
		uint32_t symbol = TSEMANTIC_INVALID_ID;
		if (!member) {
			tstring *name = tsource_document_slice(analyzer->document, token->span);
			symbol = find_in_scope(analyzer, scope, name, 1);
			tstring_free(name);
		}
		if (index->count == index->capacity) {
			index->capacity = index->capacity ? index->capacity * 2 : 16;
			index->items = realloc(index->items, index->capacity * sizeof(*index->items));
			if (!index->items) abort();
		}
		index->items[index->count] = (tannotation_reference){
			.span = token->span, .scope = scope, .symbol = symbol,
			.receiver = member ? previous : UINT32_MAX
		};
		previous = index->count++;
		member = 0;
	}
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
	if (declaration < model->declaration_count)
		model->declaration_symbols[declaration] = id;
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
	uint32_t symbol_id = find_in_scope(
		analyzer, scope, name, 1);
	if (symbol_id == TSEMANTIC_INVALID_ID && analyzer->external_resolver) {
		tsemantic_symbol_kind kind = tsemantic_symbol_var;
		int initialized = 1;
		if (analyzer->external_resolver(analyzer->external_context,
		    tstring_cstr(name), &kind, &initialized)) {
			symbol_id = declare_symbol(analyzer, 0, node->span,
				TAST_INVALID_ID, kind);
			analyzer->model->symbols[symbol_id].external = 1;
			analyzer->model->symbols[symbol_id].initially_assigned =
				initialized ? 1 : 0;
		}
	}
	analyzer->model->resolutions[id] = symbol_id;
	if (symbol_id < analyzer->model->symbol_count &&
	    (analyzer->model->symbols[symbol_id].kind == tsemantic_symbol_let ||
	     analyzer->model->symbols[symbol_id].kind ==
		tsemantic_symbol_function)) {
		tsemantic_symbol *symbol = &analyzer->model->symbols[symbol_id];
		uint32_t declaration_scope =
			analyzer->model->symbols[symbol_id].scope;
		uint32_t current = scope;
		int crosses_function = 0;
		int crosses_rule = 0;
		while (current < analyzer->model->scope_count &&
		       current != declaration_scope) {
			crosses_function |=
				analyzer->model->scopes[current].function_boundary;
			crosses_rule |= analyzer->model->scopes[current].rule_boundary;
			current = analyzer->model->scopes[current].parent;
		}
		if (symbol->kind == tsemantic_symbol_function &&
		    (crosses_function || crosses_rule))
			symbol->captured = 1;
		else if (crosses_function && crosses_rule)
			symbol->captured = 1;
		else if (crosses_function)
			tdiagnostics_add(analyzer->diagnostics, tdiagnostic_error,
				node->span, "let binding cannot be captured");
	}
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
	case tast_function:
	case tast_rule: {
		if (node->function.has_return_annotation)
			index_annotation(analyzer, node->function.return_annotation, scope);
		uint32_t function_scope = add_scope(
			analyzer, scope, node->span, 1, node->kind == tast_rule);
		const tast_id *parameters = tast_get_children(analyzer->arena,
			node->function.parameters, node->function.parameter_count);
		for (uint32_t i = 0; i < node->function.parameter_count; i++) {
			const tast_node *parameter = tast_get(
				analyzer->arena, parameters[i]);
			/* Resolve before parameter bindings: Types use the definition environment. */
			if (parameter->parameter.has_annotation)
				index_annotation(analyzer, parameter->parameter.annotation, scope);
			declare_symbol(analyzer, function_scope, parameter->span,
				parameters[i], tsemantic_symbol_parameter);
		}
		analyze_node(analyzer, node->function.body, function_scope);
	} break;
	case tast_rule_condition:
		analyze_node(analyzer, node->rule_condition.value, scope);
		break;
	case tast_rule_implication:
		analyze_node(analyzer, node->rule_implication.antecedent, scope);
		analyze_node(analyzer, node->rule_implication.consequent, scope);
		break;
	case tast_module:
	case tast_block: {
		uint32_t block_scope = node->kind == tast_module ? scope :
			add_scope(analyzer, scope, node->span, 0, 0);
		analyze_children(analyzer, node, block_scope);
	} break;
	case tast_expression_statement:
		analyze_node(analyzer, node->expression_statement.value, scope);
		break;
	case tast_declaration_statement: {
		if (node->declaration_statement.has_annotation)
			index_annotation(analyzer, node->declaration_statement.annotation, scope);
		if (node->declaration_statement.has_initializer)
			analyze_node(analyzer,
				node->declaration_statement.initializer, scope);
		tsemantic_symbol_kind kind = tsemantic_symbol_let;
		if (node->declaration_statement.is_mutable)
			kind = tsemantic_symbol_var;
		else if (node->declaration_statement.is_function_declaration)
			kind = tsemantic_symbol_function;
		declare_symbol(analyzer, scope,
			node->declaration_statement.name, id, kind);
	} break;
	case tast_declaration_group:
		analyze_children(analyzer, node, scope);
		break;
	case tast_assignment_statement:
		analyze_node(analyzer, node->assignment_statement.target, scope);
		analyze_node(analyzer, node->assignment_statement.value, scope);
		{
			const tast_node *target_node = tast_get(analyzer->arena,
				node->assignment_statement.target);
			if (target_node && target_node->kind == tast_name) {
				const tsemantic_symbol *target = tsemantic_resolved_symbol(
					analyzer->model,
					node->assignment_statement.target);
				if (target && target->kind == tsemantic_symbol_function)
					tdiagnostics_add(analyzer->diagnostics,
						tdiagnostic_error, target_node->span,
						"function binding cannot be assigned");
			}
		}
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
	case tast_if_statement: {
		const tast_id *branches = tast_get_children(analyzer->arena,
			node->conditional_statement.branches,
			node->conditional_statement.branch_count);
		for (uint32_t i = 0; branches &&
		     i < node->conditional_statement.branch_count; i++)
			analyze_node(analyzer, branches[i], scope);
	} break;
	case tast_conditional_branch:
		if (node->conditional_branch.has_condition)
			analyze_node(analyzer,
				node->conditional_branch.condition, scope);
		analyze_node(analyzer, node->conditional_branch.body, scope);
		break;
	case tast_while_statement:
		analyze_node(analyzer, node->control_statement.condition, scope);
		analyze_node(analyzer, node->control_statement.body, scope);
		break;
	case tast_for_statement: {
		analyze_node(analyzer, node->for_statement.iterable, scope);
		uint32_t loop_scope = add_scope(analyzer, scope, node->span, 0, 0);
		if (node->for_statement.declares_binding)
			declare_symbol(analyzer, loop_scope, node->for_statement.name,
				id, tsemantic_symbol_iteration);
		else {
			tast_node target = {
				.kind = tast_name,
				.span = node->for_statement.name
			};
			resolve_name(analyzer, id, &target, scope);
		}
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
	tsemantic_analyze_with_resolver(document, arena, root, model,
		diagnostics, nullptr, nullptr);
}

void tsemantic_analyze_with_resolver(
	const tsource_document *document, const tast_arena *arena, tast_id root,
	tsemantic_model *model, tdiagnostics *diagnostics,
	tsemantic_external_resolver resolver, void *resolver_context)
{
	tsemantic_model_free(model);
	model->resolution_count = arena->node_count;
	model->declaration_count = arena->node_count;
	model->resolutions = (uint32_t *)malloc(
		model->resolution_count * sizeof(uint32_t));
	model->declaration_symbols = (uint32_t *)malloc(
		model->declaration_count * sizeof(uint32_t));
	if (model->resolution_count &&
	    (!model->resolutions || !model->declaration_symbols))
		abort();
	for (uint32_t i = 0; i < model->resolution_count; i++) {
		model->resolutions[i] = TSEMANTIC_INVALID_ID;
		model->declaration_symbols[i] = TSEMANTIC_INVALID_ID;
	}
	tsyntax_tokens tokens;
	tsyntax_tokens_init(&tokens);
	tsyntax_lex(document, &tokens);
	semantic_analyzer analyzer = {
		document, arena, model, diagnostics, resolver, resolver_context, &tokens
	};
	const tast_node *root_node = tast_get(arena, root);
	uint32_t root_scope = add_scope(&analyzer, TSEMANTIC_INVALID_ID,
		root_node ? root_node->span : (tsource_span){ 0, 0 }, 0, 0);
	analyze_node(&analyzer, root, root_scope);
	tsyntax_tokens_free(&tokens);
}

const tsemantic_symbol *tsemantic_symbol_for_declaration(
	const tsemantic_model *model, tast_id declaration)
{
	if (!model || declaration >= model->declaration_count) return nullptr;
	uint32_t symbol = model->declaration_symbols[declaration];
	return symbol < model->symbol_count ? &model->symbols[symbol] : nullptr;
}

const tsemantic_symbol *tsemantic_resolved_symbol(
	const tsemantic_model *model, tast_id reference)
{
	if (!model || reference >= model->resolution_count)
		return nullptr;
	uint32_t symbol = model->resolutions[reference];
	return symbol < model->symbol_count ? &model->symbols[symbol] : nullptr;
}

const tsemantic_symbol *tsemantic_symbol_at(
	const tsemantic_model *model, const tast_arena *arena,
	uint32_t offset, tast_id *reference)
{
	if (reference)
		*reference = TAST_INVALID_ID;
	if (!model || !arena)
		return nullptr;
	const tannotation_reference *annotation = tannotation_reference_at(&model->annotations, offset);
	if (annotation)
		return annotation->symbol < model->symbol_count ?
			&model->symbols[annotation->symbol] : nullptr;
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
	return nullptr;
}

const char *tsemantic_symbol_kind_name(tsemantic_symbol_kind kind)
{
	static const char *const names[] = {
		"let", "var", "function", "parameter", "import",
		"iteration variable"
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
