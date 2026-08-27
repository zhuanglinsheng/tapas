#include "tapas/compile/type_info.h"

#include <stdlib.h>

typedef struct {
	const tsource_document *document;
	const tast_arena *arena;
	const tsemantic_model *semantic;
	ttype_info_model *model;
} type_analyzer;

void ttype_info_model_init(ttype_info_model *model)
{
	*model = (ttype_info_model){ 0 };
}

void ttype_info_model_free(ttype_info_model *model)
{
	if (!model) return;
	for (uint32_t i = 0; i < model->node_count; i++)
		tstring_free(model->node_types[i]);
	for (uint32_t i = 0; i < model->symbol_count; i++)
		tstring_free(model->symbol_types[i]);
	free(model->node_types);
	free(model->symbol_types);
	*model = (ttype_info_model){ 0 };
}

static tstring *known(const char *name) { return tstring_new(name); }
static tstring *infer(type_analyzer *analyzer, tast_id id);

static tstring *infer_name(type_analyzer *analyzer, tast_id id)
{
	if (id >= analyzer->semantic->resolution_count) return NULL;
	uint32_t symbol = analyzer->semantic->resolutions[id];
	if (symbol >= analyzer->model->symbol_count ||
	    !analyzer->model->symbol_types[symbol]) return NULL;
	return tstring_dup(analyzer->model->symbol_types[symbol]);
}

static tstring *parameterized(const char *base, const tstring *first,
			      const tstring *second)
{
	tstring *result = tstring_new(base);
	tstring_append_c(result, '<');
	tstring_append(result, first ? tstring_cstr(first) : "AnyType");
	if (second) {
		tstring_append(result, ", ");
		tstring_append_ts(result, second);
	}
	tstring_append_c(result, '>');
	return result;
}

static tstring *infer_aggregate(type_analyzer *analyzer,
				const tast_node *node, const char *base)
{
	const tast_id *items = tast_get_children(analyzer->arena,
		node->aggregate.children, node->aggregate.count);
	if (!node->aggregate.count) return parameterized(base, NULL, NULL);
	tstring *item = infer(analyzer, items[0]);
	for (uint32_t i = 1; i < node->aggregate.count; i++) {
		tstring *next = infer(analyzer, items[i]);
		if (!item || !next || !tstring_eq(item, next)) {
			tstring_free(item);
			item = known("AnyType");
		}
		tstring_free(next);
	}
	tstring *result = parameterized(base, item, NULL);
	tstring_free(item);
	return result;
}

static tstring *infer_dictionary(type_analyzer *analyzer,
				 const tast_node *node)
{
	const tast_id *items = tast_get_children(analyzer->arena,
		node->aggregate.children, node->aggregate.count);
	if (node->aggregate.count < 2)
		return known("Dictionary<AnyType, AnyType>");
	tstring *key = infer(analyzer, items[0]);
	tstring *value = infer(analyzer, items[1]);
	for (uint32_t i = 2; i + 1 < node->aggregate.count; i += 2) {
		tstring *next_key = infer(analyzer, items[i]);
		tstring *next_value = infer(analyzer, items[i + 1]);
		if (!key || !next_key || !tstring_eq(key, next_key)) {
			tstring_free(key); key = known("AnyType");
		}
		if (!value || !next_value || !tstring_eq(value, next_value)) {
			tstring_free(value); value = known("AnyType");
		}
		tstring_free(next_key);
		tstring_free(next_value);
	}
	tstring *result = parameterized("Dictionary", key, value);
	tstring_free(key);
	tstring_free(value);
	return result;
}

static tstring *infer(type_analyzer *analyzer, tast_id id)
{
	if (id >= analyzer->model->node_count) return NULL;
	if (analyzer->model->node_types[id])
		return tstring_dup(analyzer->model->node_types[id]);
	const tast_node *node = tast_get(analyzer->arena, id);
	if (!node) return NULL;
	tstring *type = NULL;
	switch (node->kind) {
	case tast_nil: type = known("Nil"); break;
	case tast_bool: type = known("Bool"); break;
	case tast_integer: type = known("Int"); break;
	case tast_float: type = known("Float"); break;
	case tast_string: type = known("String"); break;
	case tast_name: type = infer_name(analyzer, id); break;
	case tast_function: type = known("Function"); break;
	case tast_group: type = infer(analyzer, node->group.value); break;
	case tast_unary: type = infer(analyzer, node->unary.operand); break;
	case tast_binary:
		if (node->binary.op == tsyntax_colon) {
			tstring *left = infer(analyzer, node->binary.left);
			tstring *right = infer(analyzer, node->binary.right);
			type = parameterized("Pair", left, right);
			tstring_free(left); tstring_free(right);
		} else if (node->binary.op == tsyntax_eq ||
			node->binary.op == tsyntax_ne || node->binary.op == tsyntax_gt ||
			node->binary.op == tsyntax_ge || node->binary.op == tsyntax_lt ||
			node->binary.op == tsyntax_le || node->binary.op == tsyntax_kw_in ||
			node->binary.op == tsyntax_kw_and || node->binary.op == tsyntax_kw_or)
			type = known("Bool");
		else if (node->binary.op == tsyntax_kw_to)
			type = known("Iterator");
		else {
			tstring *left = infer(analyzer, node->binary.left);
			tstring *right = infer(analyzer, node->binary.right);
			if ((left && tstring_eq_cstr(left, "Float")) ||
			    (right && tstring_eq_cstr(right, "Float"))) type = known("Float");
			else if (left && right && tstring_eq(left, right)) type = tstring_dup(left);
			tstring_free(left); tstring_free(right);
		}
		break;
	case tast_list: type = infer_aggregate(analyzer, node, "List"); break;
	case tast_dictionary: type = infer_dictionary(analyzer, node); break;
	case tast_structure: type = known("Structure"); break;
	case tast_member: {
		const tast_node *receiver = tast_get(analyzer->arena, node->member.receiver);
		if (receiver && receiver->kind == tast_name) {
			tstring *name = tsource_document_slice(analyzer->document, receiver->span);
			if (tstring_eq_cstr(name, "types")) type = known("Type");
			tstring_free(name);
		}
	} break;
	default: break;
	}
	if (type) analyzer->model->node_types[id] = tstring_dup(type);
	return type;
}

void ttype_info_analyze(const tsource_document *document,
			const tast_arena *arena,
			const tsemantic_model *semantic,
			ttype_info_model *model)
{
	ttype_info_model_free(model);
	model->node_count = arena->node_count;
	model->symbol_count = semantic->symbol_count;
	model->node_types = (tstring **)calloc(model->node_count, sizeof(tstring *));
	model->symbol_types = (tstring **)calloc(model->symbol_count, sizeof(tstring *));
	if ((model->node_count && !model->node_types) ||
	    (model->symbol_count && !model->symbol_types)) abort();
	type_analyzer analyzer = { document, arena, semantic, model };
	for (uint32_t i = 0; i < semantic->symbol_count; i++) {
		const tsemantic_symbol *symbol = &semantic->symbols[i];
		const tast_node *declaration = tast_get(arena, symbol->declaration);
		if (symbol->kind == tsemantic_symbol_import)
			model->symbol_types[i] = known("Library");
		else if (symbol->kind == tsemantic_symbol_parameter ||
			 symbol->kind == tsemantic_symbol_iteration)
			model->symbol_types[i] = known("AnyType");
		else if (declaration && declaration->kind == tast_declaration_statement) {
			if (declaration->declaration_statement.has_annotation)
				model->symbol_types[i] = tsource_document_slice(document,
					declaration->declaration_statement.annotation);
			else
				model->symbol_types[i] = infer(&analyzer,
					declaration->declaration_statement.initializer);
		}
	}
	for (tast_id id = 0; id < arena->node_count; id++) {
		tstring *discard = infer(&analyzer, id);
		tstring_free(discard);
	}
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
	for (uint32_t id = 0; id < semantic->symbol_count; id++)
		if (symbol == &semantic->symbols[id])
			return id < model->symbol_count && model->symbol_types[id] ?
				tstring_cstr(model->symbol_types[id]) : NULL;
	return NULL;
}
