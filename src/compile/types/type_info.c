#include "tapas/compile/type_info.h"
#include "tapas/compile/diagnostic.h"
#include "tapas/compile/module.h"
#include "tapas/compile/semantic.h"
#include "type_constructor.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
	const tsource_document *document;
	const tast_arena *ast;
	const tsemantic_model *semantic;
	const tcontrol_flow *flow;
	ttype_info_model *model;
	ttype_info_external_resolver external_resolver;
	void *external_context;
	uint32_t annotation_offset;
} type_analyzer;

static tstatic_type_id infer(type_analyzer *analyzer, tast_id id);
static tstatic_type_id static_value(type_analyzer *analyzer, tast_id id);

static int tunnel_member(const type_analyzer *analyzer, tast_id id)
{
	const tast_node *member = tast_get(analyzer->ast, id);
	if (!member || member->kind != tast_member ||
	    member->member.op != tsyntax_dot) return 0;
	tast_id parent = tcontrol_parent(analyzer->flow, id);
	const tast_node *call = tast_get(analyzer->ast, parent);
	return call && call->kind == tast_call &&
		tcontrol_role(analyzer->flow, id) == tcontrol_child_receiver;
}

static const tstandard_symbol *standard_callee(type_analyzer *analyzer,
						tast_id id)
{
	const tast_node *node = tast_get(analyzer->ast, id);
	if (!node) return nullptr;
	if (node->kind == tast_name) {
		const tsemantic_symbol *symbol = tsemantic_resolved_symbol(
			analyzer->semantic, id);
		if (symbol && !symbol->external) return nullptr;
	} else if (!tunnel_member(analyzer, id)) {
		return nullptr;
	}
	tsource_span span = node->kind == tast_name ? node->span : node->member.name;
	tstring *name = tsource_document_slice(analyzer->document, span);
	const tstandard_symbol *result = tstandard_symbol_find(
		nullptr, tstring_cstr(name));
	tstring_free(name);
	return result && result->kind == tmodule_symbol_function ? result : nullptr;
}

typedef struct {
	tast_id id;
	uint32_t start;
} declaration_entry;

static int compare_declarations(const void *left, const void *right)
{
	const declaration_entry *a = (const declaration_entry *)left;
	const declaration_entry *b = (const declaration_entry *)right;
	return a->start < b->start ? -1 : a->start > b->start;
}

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
	const tsemantic_symbol *symbol = tsemantic_symbol_for_declaration(
		analyzer->semantic, id);
	return symbol ? (uint32_t)(symbol - analyzer->semantic->symbols) :
		TSEMANTIC_INVALID_ID;
}

static tstatic_type_id resolve_annotation_definition(void *context, const char *name)
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

static tstatic_type_id resolve_annotation_name(void *context, const char *name)
{
	type_analyzer *analyzer = context;
	tstatic_type_id definition = resolve_annotation_definition(context, name);
	return tstatic_type_with_name(&analyzer->model->arena, definition, name);
}

static tstatic_type_id resolve_annotation_value(void *context, const char *name)
{
	type_analyzer *analyzer = context;
	if (strstr(name, "::")) return analyzer->external_resolver ?
		analyzer->external_resolver(analyzer->external_context, &analyzer->model->arena, name, 0) :
		TSTATIC_TYPE_UNKNOWN;
	uint32_t scope = tsemantic_scope_at(analyzer->semantic, analyzer->annotation_offset);
	for (uint32_t i = analyzer->semantic->symbol_count; i-- > 0;) {
		const tsemantic_symbol *symbol = &analyzer->semantic->symbols[i];
		if (tstring_eq_cstr(symbol->name, name) && symbol->span.start < analyzer->annotation_offset &&
		    tsemantic_scope_contains(analyzer->semantic, symbol->scope, scope))
			return analyzer->model->symbol_type_ids[i];
	}
	return TSTATIC_TYPE_UNKNOWN;
}

static tstatic_type_id parse_annotation(type_analyzer *analyzer,
					tsource_span span)
{
	tstring *text = tsource_document_slice(analyzer->document, span);
	analyzer->annotation_offset = span.start;
	tstatic_type_id result = tstatic_type_parse_with_values(&analyzer->model->arena,
		tstring_cstr(text), resolve_annotation_name, resolve_annotation_value, analyzer);
	tstring_free(text);
	return result == TSTATIC_TYPE_INVALID_NAME ? TSTATIC_TYPE_UNKNOWN : result;
}

static tstatic_type_id resolved_name(type_analyzer *analyzer, tast_id id)
{
	uint32_t symbol = id < analyzer->semantic->resolution_count ?
		analyzer->semantic->resolutions[id] : TSEMANTIC_INVALID_ID;
	if (symbol < analyzer->model->symbol_count &&
	    !analyzer->semantic->symbols[symbol].external)
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
	return tast_string_value(analyzer->document, node);
}

static tstring *types_member_name(type_analyzer *analyzer,
				  const tast_node *node)
{
	return tast_scoped_member_name(
		analyzer->document, analyzer->ast, node, "types");
}

static tstatic_type_id static_type_call(type_analyzer *analyzer,
					const tast_node *node)
{
	const tast_node *callee = tast_get(analyzer->ast, node->aggregate.receiver);
	tstring *member = types_member_name(analyzer, callee);
	if (!member) return TSTATIC_TYPE_UNKNOWN;
	const char *name = tstring_cstr(member);
	ttype_constructor_id constructor = ttype_constructor_named(name);
	const ttype_constructor *descriptor = ttype_constructor_get(constructor);
	const tast_id *arguments = tast_get_children(analyzer->ast,
		node->aggregate.children, node->aggregate.count);
	uint32_t count = node->aggregate.count;
	tstatic_type_id result = TSTATIC_TYPE_UNKNOWN;
	if (!descriptor || count < descriptor->minimum_arguments ||
	    count > descriptor->maximum_arguments) {
		tstring_free(member);
		return result;
	}
	if ((constructor == ttype_constructor_list ||
	     constructor == ttype_constructor_iterator)) {
		tstatic_type_id item = static_value(analyzer, arguments[0]);
		if (item != TSTATIC_TYPE_UNKNOWN)
			result = tstatic_type_make(&analyzer->model->arena,
				constructor == ttype_constructor_list ? tstatic_type_list :
				tstatic_type_iterator, &item, 1, 0);
	} else if (constructor == ttype_constructor_pair ||
		   constructor == ttype_constructor_dictionary) {
		tstatic_type_id values[2] = {
			static_value(analyzer, arguments[0]),
			static_value(analyzer, arguments[1])
		};
		if (values[0] != TSTATIC_TYPE_UNKNOWN &&
		    values[1] != TSTATIC_TYPE_UNKNOWN)
			result = tstatic_type_make(&analyzer->model->arena,
				constructor == ttype_constructor_pair ? tstatic_type_pair :
				tstatic_type_dictionary, values, 2, 0);
	} else if (constructor == ttype_constructor_union) {
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
	} else if (constructor == ttype_constructor_enum) {
		tstring **members = (tstring **)calloc(count, sizeof(*members));
		if (!members) abort();
		int valid = 1;
		for (uint32_t i = 0; i < count; i++) {
			members[i] = literal_string(analyzer,
				tast_get(analyzer->ast, arguments[i]));
			valid = valid && members[i];
		}
		if (valid) result = tstatic_type_make_enum(
			&analyzer->model->arena, members, count);
		for (uint32_t i = 0; i < count; i++) tstring_free(members[i]);
		free(members);
	} else if (constructor == ttype_constructor_rule ||
		   constructor == ttype_constructor_rule_instance) {
		tstatic_type_id *values = count ?
			(tstatic_type_id *)calloc(count, sizeof(*values)) : nullptr;
		if (count && !values) abort();
		int valid = 1;
		for (uint32_t i = 0; i < count; i++) {
			values[i] = static_value(analyzer, arguments[i]);
			valid = valid && values[i] != TSTATIC_TYPE_UNKNOWN;
		}
		if (valid) result = tstatic_type_make(&analyzer->model->arena,
			constructor == ttype_constructor_rule ? tstatic_type_rule :
				tstatic_type_rule_instance, values, count, 0);
		free(values);
	} else if (constructor == ttype_constructor_make_type) {
		tstatic_field *fields = (tstatic_field *)calloc(count, sizeof(*fields));
		if (!fields) abort();
		int valid = 1;
		for (uint32_t i = 0; i < count; i++) {
			const tast_node *pair = tast_get(analyzer->ast, arguments[i]);
			if (pair && pair->kind == tast_call && pair->aggregate.count == 1) {
				const tast_node *wrapper = tast_get(
					analyzer->ast, pair->aggregate.receiver);
				tstring *wrapper_name = types_member_name(analyzer, wrapper);
				int optional = wrapper_name && ttype_constructor_named(
					tstring_cstr(wrapper_name)) == ttype_constructor_optional;
				tstring_free(wrapper_name);
				if (optional) {
					const tast_id *wrapped = tast_get_children(analyzer->ast,
						pair->aggregate.children, pair->aggregate.count);
					pair = tast_get(analyzer->ast, wrapped[0]);
					fields[i].optional = 1;
				}
			}
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
		if (symbol->external && analyzer->external_resolver)
			return analyzer->external_resolver(
				analyzer->external_context, &analyzer->model->arena,
				tstring_cstr(symbol->name), 1);
		uint32_t sid = (uint32_t)(symbol - analyzer->semantic->symbols);
		return sid < analyzer->model->symbol_count ?
			analyzer->model->symbol_static_values[sid] : TSTATIC_TYPE_UNKNOWN;
	}
	if (node->kind == tast_member) {
		tstring *member = types_member_name(analyzer, node);
		tstatic_type_id result = member ? tstatic_type_builtin_named(
			&analyzer->model->arena, tstring_cstr(member)) :
			TSTATIC_TYPE_UNKNOWN;
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

static tstatic_type_id narrow_intersection(type_analyzer *analyzer,
					   tstatic_type_id current,
					   tstatic_type_id tested)
{
	if (tested == TSTATIC_TYPE_UNKNOWN) return current;
	if (current == TSTATIC_TYPE_UNKNOWN) return tested;
	if (tstatic_type_assignable(&analyzer->model->arena, current, tested))
		return current;
	if (tstatic_type_assignable(&analyzer->model->arena, tested, current))
		return tested;
	const tstatic_type *type = tstatic_type_get(&analyzer->model->arena, current);
	if (!type || type->kind != tstatic_type_union) return current;
	const tstatic_type_id *members = tstatic_type_children(
		&analyzer->model->arena, type);
	tstatic_type_id *kept = (tstatic_type_id *)calloc(
		type->child_count, sizeof(*kept));
	if (!kept) abort();
	uint32_t count = 0;
	for (uint32_t i = 0; i < type->child_count; i++)
		if (tstatic_type_assignable(
			&analyzer->model->arena, members[i], tested))
			kept[count++] = members[i];
	tstatic_type_id result = count ? union_of(analyzer, kept, count) : current;
	free(kept);
	return result;
}

static tstatic_type_id narrowed_name(type_analyzer *analyzer, tast_id id,
				     tstatic_type_id current)
{
	const tsemantic_symbol *symbol = tsemantic_resolved_symbol(
		analyzer->semantic, id);
	if (!symbol) return current;
	for (tast_id child = id; child != TAST_INVALID_ID;) {
		tast_id control_id = tcontrol_parent(analyzer->flow, child);
		if (control_id == TAST_INVALID_ID) break;
		if (tcontrol_role(analyzer->flow, child) !=
		    tcontrol_child_control_body) {
			child = control_id;
			continue;
		}
		const tast_node *control = tast_get(analyzer->ast, control_id);
		if (!control || control->kind != tast_conditional_branch ||
		    !control->conditional_branch.has_condition) {
			child = control_id;
			continue;
		}
		const tast_node *condition = tast_get(
			analyzer->ast, control->conditional_branch.condition);
		if (!condition || condition->kind != tast_call ||
		    condition->aggregate.count != 2) {
			child = control_id;
			continue;
		}
		const tast_node *callee = tast_get(
			analyzer->ast, condition->aggregate.receiver);
		tstring *member = types_member_name(analyzer, callee);
		int matches = member && tstring_eq_cstr(member, "matches");
		tstring_free(member);
		if (!matches) {
			child = control_id;
			continue;
		}
		const tast_id *arguments = tast_get_children(analyzer->ast,
			condition->aggregate.children, condition->aggregate.count);
		const tsemantic_symbol *tested_symbol = tsemantic_resolved_symbol(
			analyzer->semantic, arguments[0]);
		if (tested_symbol != symbol) {
			child = control_id;
			continue;
		}
		tstatic_type_id tested = static_value(analyzer, arguments[1]);
		if (tested != TSTATIC_TYPE_UNKNOWN)
			current = narrow_intersection(analyzer, current, tested);
		child = control_id;
	}
	return current;
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

static tstatic_type_id infer_rule(type_analyzer *analyzer, tast_id id,
				   const tast_node *node)
{
	uint32_t count = node->function.parameter_count;
	tstatic_type_id *parameters = count ?
		(tstatic_type_id *)calloc(count, sizeof(*parameters)) : nullptr;
	if (count && !parameters) abort();
	const tast_id *ids = tast_get_children(analyzer->ast,
		node->function.parameters, count);
	for (uint32_t i = 0; i < count; i++) {
		const tast_node *parameter = tast_get(analyzer->ast, ids[i]);
		parameters[i] = parameter ? parse_annotation(
			analyzer, parameter->parameter.annotation) : TSTATIC_TYPE_UNKNOWN;
		uint32_t sid = symbol_for_declaration(analyzer, ids[i]);
		if (sid < analyzer->model->symbol_count)
			analyzer->model->symbol_type_ids[sid] = parameters[i];
	}
	tstatic_type_id result = tstatic_type_make(&analyzer->model->arena,
		tstatic_type_rule, parameters, count, 0);
	free(parameters);
	analyzer->model->node_type_ids[id] = result;
	return result;
}

static tstatic_type_id infer_function_with_context(type_analyzer *analyzer,
					    tast_id id,
					    const tast_node *node,
					    tstatic_type_id context_id)
{
	const tstatic_type *context = tstatic_type_get(
		&analyzer->model->arena, context_id);
	if (!context || context->kind != tstatic_type_function ||
	    context->child_count != node->function.parameter_count + 1 ||
	    context->variadic != node->function.variadic)
		return infer_function(analyzer, id, node);
	const tstatic_type_id *expected = tstatic_type_children(
		&analyzer->model->arena, context);
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
			expected[i];
		uint32_t sid = symbol_for_declaration(analyzer, parameters[i]);
		if (sid < analyzer->model->symbol_count)
			analyzer->model->symbol_type_ids[sid] = signature[i];
	}
	signature[count] = node->function.has_return_annotation ?
		parse_annotation(analyzer, node->function.return_annotation) :
		expected[count];
	tstatic_type_id result = tstatic_type_make(&analyzer->model->arena,
		tstatic_type_function, signature, count + 1, node->function.variadic);
	free(signature);
	analyzer->model->node_type_ids[id] = result;
	return result;
}

static tstatic_type_id infer_index(type_analyzer *analyzer,
				   const tast_node *node)
{
	tstatic_type_id receiver_id = infer(analyzer, node->aggregate.receiver);
	const tstatic_type *receiver = tstatic_type_get(
		&analyzer->model->arena, receiver_id);
	if (receiver && receiver->kind == tstatic_type_builtin &&
	    receiver->builtin == tbuiltin_type) {
		receiver_id = static_value(analyzer, node->aggregate.receiver);
		receiver = tstatic_type_get(&analyzer->model->arena, receiver_id);
	}
	if (!receiver || node->aggregate.count != 1) return TSTATIC_TYPE_UNKNOWN;
	const tast_id *indices = tast_get_children(analyzer->ast,
		node->aggregate.children, node->aggregate.count);
	const tast_node *index = tast_get(analyzer->ast, indices[0]);
	if (receiver->kind == tstatic_type_enum) return receiver_id;
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
	case tast_nil: result = tbuiltin_nil; break;
	case tast_bool: result = tbuiltin_bool; break;
	case tast_integer: result = tbuiltin_int; break;
	case tast_float: result = tbuiltin_float; break;
	case tast_string: result = tbuiltin_string; break;
	case tast_name: {
		tstring *name = tsource_document_slice(analyzer->document, node->span);
		result = tstring_eq_cstr(name, "this") ? infer(analyzer,
			tcontrol_enclosing_function(analyzer->flow, id)) :
			resolved_name(analyzer, id);
		tstring_free(name);
		result = narrowed_name(analyzer, id, result);
	} break;
	case tast_function: return infer_function(analyzer, id, node);
	case tast_rule: return infer_rule(analyzer, id, node);
	case tast_rule_condition:
		result = infer(analyzer, node->rule_condition.value);
		break;
	case tast_rule_implication:
		infer(analyzer, node->rule_implication.antecedent);
		infer(analyzer, node->rule_implication.consequent);
		result = tbuiltin_bool;
		break;
	case tast_group: result = infer(analyzer, node->group.value); break;
	case tast_unary:
		result = infer(analyzer, node->unary.operand);
		if (node->unary.op == tsyntax_kw_not) result = tbuiltin_bool;
		break;
	case tast_binary:
        if (node->binary.op == tsyntax_kw_in) {
            tstatic_type_id left = infer(analyzer, node->binary.left), right = infer(analyzer, node->binary.right);
            int symbolic = left == tbuiltin_rule_term || left == tbuiltin_rule_parameter || left == tbuiltin_rule_capture ||
                right == tbuiltin_rule_term || right == tbuiltin_rule_parameter || right == tbuiltin_rule_capture;
            result = symbolic ? tbuiltin_rule_term : tbuiltin_bool;
            break;
        }
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
			node->binary.op == tsyntax_kw_or) result = tbuiltin_bool;
		else if (node->binary.op == tsyntax_kw_to) {
			tstatic_type_id item = tbuiltin_int;
			result = tstatic_type_make(&analyzer->model->arena,
				tstatic_type_iterator, &item, 1, 0);
		}
		else {
			tstatic_type_id left = infer(analyzer, node->binary.left);
			tstatic_type_id right = infer(analyzer, node->binary.right);
			if (left == tbuiltin_float || right == tbuiltin_float)
				result = tbuiltin_float;
			else if (tstatic_type_equal(&analyzer->model->arena, left, right))
				result = left;
		}
		break;
	case tast_list: {
		const tast_id *items = tast_get_children(analyzer->ast,
			node->aggregate.children, node->aggregate.count);
		if (!node->aggregate.count) { result = tbuiltin_list; break; }
		tstatic_type_id *types = (tstatic_type_id *)calloc(
			node->aggregate.count, sizeof(*types));
		if (!types) abort();
		for (uint32_t i = 0; i < node->aggregate.count; i++)
			types[i] = infer(analyzer, items[i]);
		tstatic_type_id item = union_of(analyzer, types, node->aggregate.count);
		free(types);
		result = item == TSTATIC_TYPE_UNKNOWN ? tbuiltin_list :
			tstatic_type_make(&analyzer->model->arena,
				tstatic_type_list, &item, 1, 0);
	} break;
	case tast_dictionary:
		/* Literal entries are data, not declared field constraints. A dictionary
		 * may gain or lose keys and change value Types after initialization. */
		result = tbuiltin_dictionary;
		break;
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
        const tast_node *domain_callee = tast_get(analyzer->ast, node->aggregate.receiver);
        if (domain_callee && domain_callee->kind == tast_member && domain_callee->member.op == tsyntax_scope) {
            const tast_node *receiver = tast_get(analyzer->ast, domain_callee->member.receiver);
            const tsemantic_symbol *symbol = receiver && receiver->kind == tast_name ?
                tsemantic_resolved_symbol(analyzer->semantic, domain_callee->member.receiver) : nullptr;
            if (symbol && symbol->external && tstring_eq_cstr(symbol->name, "rules")) {
                tstring *member = tsource_document_slice(analyzer->document, domain_callee->member.name);
                if (tstring_eq_cstr(member, "points") && node->aggregate.count >= 1) {
                    tstatic_type_id item = static_value(analyzer, arguments[0]);
                    result = item == TSTATIC_TYPE_UNKNOWN ? TSTATIC_TYPE_UNKNOWN :
                        tstatic_type_make(&analyzer->model->arena, tstatic_type_points, &item, 1, 0);
                } else if (tstring_eq_cstr(member, "range") && node->aggregate.count == 2) {
                    tstatic_type_id item = tstatic_type_builtin_id(&analyzer->model->arena, tbuiltin_int);
                    result = tstatic_type_make(&analyzer->model->arena, tstatic_type_range, &item, 1, 0);
                }
                tstring_free(member);
            }
        }
		const tstandard_symbol *standard = standard_callee(
			analyzer, node->aggregate.receiver);
		if (standard && standard->result_from_argument) {
			const tast_node *callee = tast_get(
				analyzer->ast, node->aggregate.receiver);
			uint32_t argument = standard->result_argument;
			if (callee && callee->kind == tast_member &&
			    callee->member.op == tsyntax_dot) {
				if (!argument)
					result = infer(analyzer, callee->member.receiver);
				else if (argument - 1 < node->aggregate.count)
					result = infer(analyzer, arguments[argument - 1]);
			} else if (argument < node->aggregate.count) {
				result = infer(analyzer, arguments[argument]);
			}
		}
		if (function && function->kind == tstatic_type_rule)
			result = tstatic_type_make(&analyzer->model->arena,
				tstatic_type_rule_instance,
				tstatic_type_children(&analyzer->model->arena, function),
				function->child_count, 0);
		else if (function && function->kind == tstatic_type_builtin &&
			 function->builtin == tbuiltin_rule)
			result = tbuiltin_rule_instance;
	} break;
	case tast_member: {
		tstring *member = types_member_name(analyzer, node);
		if (member && tstatic_type_builtin_named(&analyzer->model->arena,
		    tstring_cstr(member)) != TSTATIC_TYPE_UNKNOWN)
			result = tbuiltin_type;
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
		const tstandard_symbol *standard = standard_callee(analyzer, id);
		if (result == TSTATIC_TYPE_UNKNOWN && standard &&
		    standard->result_from_argument && analyzer->external_resolver) {
			tstring *name = tsource_document_slice(
				analyzer->document, node->member.name);
			result = analyzer->external_resolver(
				analyzer->external_context, &analyzer->model->arena,
				tstring_cstr(name), 0);
			tstring_free(name);
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
	declaration_entry *declarations = (declaration_entry *)calloc(
		analyzer->semantic->symbol_count, sizeof(*declarations));
	if (!declarations && analyzer->semantic->symbol_count) abort();
	uint32_t count = 0;
	for (uint32_t sid = 0; sid < analyzer->semantic->symbol_count; sid++) {
		tast_id id = analyzer->semantic->symbols[sid].declaration;
		const tast_node *node = tast_get(analyzer->ast, id);
		if (node && node->kind == tast_declaration_statement)
			declarations[count++] = (declaration_entry){ id, node->span.start };
	}
	qsort(declarations, count, sizeof(*declarations), compare_declarations);
	/* A declaration without an initializer and explicitly annotated Type is a
	 * forward Type definition. Register every placeholder before resolving any
	 * initializer so mutually recursive definitions share the same graph. */
	for (uint32_t i = 0; i < count; i++) {
		const tast_node *node = tast_get(analyzer->ast, declarations[i].id);
		if (!node || node->declaration_statement.has_initializer ||
		    node->declaration_statement.is_mutable ||
		    !node->declaration_statement.has_annotation)
			continue;
		tstatic_type_id annotation = parse_annotation(
			analyzer, node->declaration_statement.annotation);
		if (annotation != tbuiltin_type) continue;
		uint32_t sid = symbol_for_declaration(analyzer, declarations[i].id);
		if (sid < analyzer->model->symbol_count) {
			analyzer->model->symbol_type_ids[sid] = tbuiltin_type;
			analyzer->model->symbol_static_values[sid] =
				tstatic_type_make_recursive(&analyzer->model->arena);
		}
	}
	for (uint32_t i = 0; i < count; i++) {
		tast_id id = declarations[i].id;
		const tast_node *node = tast_get(analyzer->ast, id);
		tstatic_type_id target = node->declaration_statement.has_annotation ?
			parse_annotation(analyzer, node->declaration_statement.annotation) :
			TSTATIC_TYPE_UNKNOWN;
		tstatic_type_id inferred = TSTATIC_TYPE_UNKNOWN;
		if (node->declaration_statement.has_initializer) {
			const tast_node *initializer = tast_get(analyzer->ast,
				node->declaration_statement.initializer);
			inferred = initializer && initializer->kind == tast_function &&
				target != TSTATIC_TYPE_UNKNOWN ?
				infer_function_with_context(analyzer,
					node->declaration_statement.initializer,
					initializer, target) :
				infer(analyzer, node->declaration_statement.initializer);
		}
		if (!node->declaration_statement.has_annotation) target = inferred;
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
			 tbuiltin_type)
			analyzer->model->symbol_static_values[sid] = body;
	}
}

static void analyze_iteration_symbols(type_analyzer *analyzer)
{
	for (uint32_t sid = 0; sid < analyzer->semantic->symbol_count; sid++) {
		const tsemantic_symbol *symbol = &analyzer->semantic->symbols[sid];
		if (symbol->kind != tsemantic_symbol_iteration) continue;
		const tast_node *loop = tast_get(analyzer->ast, symbol->declaration);
		if (!loop || loop->kind != tast_for_statement) continue;
		tstatic_type_id iterable_id = infer(
			analyzer, loop->for_statement.iterable);
		const tstatic_type *iterable = tstatic_type_get(
			&analyzer->model->arena, iterable_id);
		if (iterable && iterable->kind == tstatic_type_builtin &&
		    iterable->builtin == tbuiltin_type) {
			iterable_id = static_value(
				analyzer, loop->for_statement.iterable);
			iterable = tstatic_type_get(
				&analyzer->model->arena, iterable_id);
		}
		if (iterable && iterable->kind == tstatic_type_enum) {
			analyzer->model->symbol_type_ids[sid] = iterable_id;
			continue;
		}
		if (!iterable || (iterable->kind != tstatic_type_iterator &&
		    iterable->kind != tstatic_type_list) || iterable->child_count != 1)
			continue;
		const tstatic_type_id *children = tstatic_type_children(
			&analyzer->model->arena, iterable);
		analyzer->model->symbol_type_ids[sid] = children[0];
	}
}

void ttype_info_analyze_with_resolver(
	const tsource_document *document, const tast_arena *arena,
	const tsemantic_model *semantic, const tcontrol_flow *flow,
	ttype_info_model *model,
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
			tsemantic_symbol_import ? tbuiltin_library : TSTATIC_TYPE_UNKNOWN;
		model->symbol_static_values[i] = TSTATIC_TYPE_UNKNOWN;
	}
	type_analyzer analyzer = {
		.document = document,
		.ast = arena,
		.semantic = semantic,
		.flow = flow,
		.model = model,
		.external_resolver = resolver,
		.external_context = resolver_context
	};
	analyze_declarations(&analyzer);
	analyze_recursive_assignments(&analyzer);
	analyze_iteration_symbols(&analyzer);
	for (tast_id id = 0; id < arena->node_count; id++) {
		infer(&analyzer, id);
		model->node_static_values[id] = static_value(&analyzer, id);
	}
	for (uint32_t i = 0; i < model->node_count; i++)
		if (model->node_type_ids[i] != TSTATIC_TYPE_UNKNOWN)
			model->node_types[i] = tstatic_type_display(
				&model->arena, model->node_type_ids[i]);
	for (uint32_t i = 0; i < model->symbol_count; i++)
		if (model->symbol_type_ids[i] != TSTATIC_TYPE_UNKNOWN)
			model->symbol_types[i] = tstatic_type_display(
				&model->arena, model->symbol_type_ids[i]);
}

void ttype_info_analyze(const tsource_document *document,
			const tast_arena *arena,
			const tsemantic_model *semantic,
			const tcontrol_flow *flow,
			ttype_info_model *model)
{
	ttype_info_analyze_with_resolver(
		document, arena, semantic, flow, model, nullptr, nullptr);
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
		tstring_cstr(model->node_types[node]) : nullptr;
}

const char *ttype_info_for_symbol(const ttype_info_model *model,
				 const tsemantic_model *semantic,
				 const tsemantic_symbol *symbol)
{
	if (!model || !semantic || !symbol) return nullptr;
	uint32_t id = (uint32_t)(symbol - semantic->symbols);
	return id < model->symbol_count && model->symbol_types[id] ?
		tstring_cstr(model->symbol_types[id]) : nullptr;
}
