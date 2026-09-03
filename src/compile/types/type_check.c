#include "tapas/compile/type_info.h"
#include "tapas/compile/diagnostic.h"
#include "tapas/compile/semantic.h"
#include "type_constructor.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
	const tsource_document *document;
	const tast_arena *ast;
	const tsemantic_model *semantic;
	const tcontrol_flow *flow;
	const ttype_info_model *types;
	tdiagnostics *diagnostics;
} type_checker;

static tstatic_type_id node_type(const type_checker *checker, tast_id id)
{
	return ttype_info_node_id(checker->types, id);
}

static const tstatic_type *type_of(const type_checker *checker,
				   tstatic_type_id id)
{
	return tstatic_type_get(&checker->types->arena, id);
}

static void report(type_checker *checker, tsource_span span,
		   const char *message)
{
	tdiagnostics_add(checker->diagnostics, tdiagnostic_error, span, message);
}

static const tsemantic_symbol *symbol_for_declaration(
	const type_checker *checker, tast_id declaration)
{
	return tsemantic_symbol_for_declaration(checker->semantic, declaration);
}

static tstatic_type_id symbol_type(const type_checker *checker,
				   const tsemantic_symbol *symbol)
{
	return symbol ? ttype_info_symbol_id(checker->types, checker->semantic,
					     symbol) : TSTATIC_TYPE_UNKNOWN;
}

static int recursive_type_symbol(const type_checker *checker,
				 const tsemantic_symbol *symbol)
{
	if (!symbol) return 0;
	uint32_t id = (uint32_t)(symbol - checker->semantic->symbols);
	if (id >= checker->types->symbol_count) return 0;
	const tstatic_type *type = type_of(
		checker, checker->types->symbol_static_values[id]);
	return type && type->kind == tstatic_type_recursive;
}

static int recursive_definition_reference(const type_checker *checker,
					  tast_id id,
					  const tsemantic_symbol *symbol)
{
	if (!recursive_type_symbol(checker, symbol)) return 0;
	for (tast_id child = id; child != TAST_INVALID_ID;) {
		tast_id parent = tcontrol_parent(checker->flow, child);
		const tast_node *assignment = tast_get(checker->ast, parent);
		if (assignment && assignment->kind == tast_assignment_statement &&
		    tcontrol_role(checker->flow, child) ==
			tcontrol_child_assignment_value) {
			const tsemantic_symbol *target = tsemantic_resolved_symbol(
				checker->semantic,
				assignment->assignment_statement.target);
			return recursive_type_symbol(checker, target);
		}
		child = parent;
	}
	return 0;
}

static int assignable(type_checker *checker, tast_id expression,
		      tstatic_type_id target)
{
	if (target == TSTATIC_TYPE_UNKNOWN) return 1;
	tstatic_type_id actual = node_type(checker, expression);
	if (actual != TSTATIC_TYPE_UNKNOWN &&
	    tstatic_type_assignable(&checker->types->arena, actual, target))
		return 1;
	const tast_node *node = tast_get(checker->ast, expression);
	const tstatic_type *expected = type_of(checker, target);
	if (!node || !expected) return actual == TSTATIC_TYPE_UNKNOWN;
	if (expected->kind == tstatic_type_recursive && expected->child_count == 1) {
		const tstatic_type_id *body = tstatic_type_children(
			&checker->types->arena, expected);
		return assignable(checker, expression, body[0]);
	}
	const tstatic_type_id *children = tstatic_type_children(
		&checker->types->arena, expected);
	if (expected->kind == tstatic_type_union) {
		for (uint32_t i = 0; i < expected->child_count; i++)
			if (assignable(checker, expression, children[i])) return 1;
		return 0;
	}
	if (node->kind == tast_group)
		return assignable(checker, node->group.value, target);
	if (node->kind == tast_list && expected->kind == tstatic_type_list &&
	    expected->child_count == 1) {
		const tast_id *items = tast_get_children(checker->ast,
			node->aggregate.children, node->aggregate.count);
		for (uint32_t i = 0; i < node->aggregate.count; i++)
			if (!assignable(checker, items[i], children[0])) return 0;
		return 1;
	}
	if (node->kind == tast_binary && node->binary.op == tsyntax_colon &&
	    expected->kind == tstatic_type_pair && expected->child_count == 2)
		return assignable(checker, node->binary.left, children[0]) &&
		       assignable(checker, node->binary.right, children[1]);
	if (node->kind == tast_dictionary &&
	    expected->kind == tstatic_type_dictionary && expected->child_count == 2) {
		const tast_id *entries = tast_get_children(checker->ast,
			node->aggregate.children, node->aggregate.count);
		for (uint32_t i = 0; i + 1 < node->aggregate.count; i += 2)
			if (!assignable(checker, entries[i], children[0]) ||
			    !assignable(checker, entries[i + 1], children[1])) return 0;
		return 1;
	}
	if (node->kind == tast_dictionary && expected->kind == tstatic_type_fields) {
		const tast_id *entries = tast_get_children(checker->ast,
			node->aggregate.children, node->aggregate.count);
		const tstatic_field *fields = tstatic_type_field_items(
			&checker->types->arena, expected);
		for (uint32_t field = 0; field < expected->field_count; field++) {
			int found = 0;
			for (uint32_t i = 0; i + 1 < node->aggregate.count; i += 2) {
				const tast_node *key = tast_get(checker->ast, entries[i]);
				if (!key || key->kind != tast_string ||
				    key->span.end < key->span.start + 2) continue;
				tstring *name = tsource_document_slice(checker->document,
					(tsource_span){ key->span.start + 1, key->span.end - 1 });
				int same = tstring_eq(name, fields[field].name);
				tstring_free(name);
				if (!same) continue;
				found = assignable(checker, entries[i + 1], fields[field].type);
				break;
			}
			if (!found && !fields[field].optional) return 0;
		}
		return 1;
	}
	if (node->kind == tast_structure && expected->kind == tstatic_type_fields) {
		const tstatic_field *fields = tstatic_type_field_items(
			&checker->types->arena, expected);
		uint8_t *assigned = (uint8_t *)calloc(
			expected->field_count, sizeof(*assigned));
		if (!assigned) abort();
		const tast_id *items = tast_get_children(checker->ast,
			node->aggregate.children, node->aggregate.count);
		uint32_t positional = 0;
		int valid = 1;
		for (uint32_t i = 0; i < node->aggregate.count && valid; i++) {
			const tast_node *item = tast_get(checker->ast, items[i]);
			tast_id value = items[i];
			uint32_t field = expected->field_count;
			if (item && item->kind == tast_named_field) {
				tstring *name = tsource_document_slice(
					checker->document, item->named_field.name);
				for (uint32_t j = 0; j < expected->field_count; j++)
					if (tstring_eq(name, fields[j].name)) { field = j; break; }
				tstring_free(name);
				value = item->named_field.value;
			} else {
				while (positional < expected->field_count && assigned[positional])
					positional++;
				field = positional++;
			}
			if (field >= expected->field_count || assigned[field] ||
			    !assignable(checker, value, fields[field].type)) valid = 0;
			else assigned[field] = 1;
		}
		for (uint32_t i = 0; i < expected->field_count; i++)
			if (!assigned[i] && !fields[i].optional) valid = 0;
		free(assigned);
		return valid;
	}
	return actual == TSTATIC_TYPE_UNKNOWN;
}

static tstring *literal_string(type_checker *checker, tast_id id)
{
	return tast_string_value(checker->document, tast_get(checker->ast, id));
}

static void validate_index(type_checker *checker, const tast_node *node,
			   int writing, tast_id value)
{
	if (node->aggregate.count != 1) return;
	tstatic_type_id receiver_id = node_type(checker, node->aggregate.receiver);
	const tstatic_type *receiver = type_of(checker, receiver_id);
	if (!receiver) return;
	const tast_id *indices = tast_get_children(checker->ast,
		node->aggregate.children, node->aggregate.count);
	const tast_node *index = tast_get(checker->ast, indices[0]);
	const tstatic_type_id *parameters = tstatic_type_children(
		&checker->types->arena, receiver);
	if (receiver->kind == tstatic_type_list && receiver->child_count == 1) {
		if (!index || index->kind != tast_slice) {
			if (!assignable(checker, indices[0], tbuiltin_int))
				report(checker, index ? index->span : node->span,
				       "List index must have Type Int");
			if (writing && !assignable(checker, value, parameters[0]))
				report(checker, tast_get(checker->ast, value)->span,
				       "List element Type mismatch");
		}
		return;
	}
	if (receiver->kind == tstatic_type_dictionary && receiver->child_count == 2) {
		if (!assignable(checker, indices[0], parameters[0]))
			report(checker, index ? index->span : node->span,
			       "Dictionary key Type mismatch");
		if (writing && !assignable(checker, value, parameters[1]))
			report(checker, tast_get(checker->ast, value)->span,
			       "Dictionary value Type mismatch");
		return;
	}
	if (receiver->kind != tstatic_type_fields) return;
	tstring *name = literal_string(checker, indices[0]);
	if (!name) {
		report(checker, index ? index->span : node->span,
		       "field index must be a String literal");
		return;
	}
	const tstatic_field *fields = tstatic_type_field_items(
		&checker->types->arena, receiver);
	for (uint32_t i = 0; i < receiver->field_count; i++) {
		if (!tstring_eq(name, fields[i].name)) continue;
		if (writing && !assignable(checker, value, fields[i].type))
			report(checker, tast_get(checker->ast, value)->span,
			       "field value Type mismatch");
		tstring_free(name);
		return;
	}
	tstring_free(name);
	report(checker, index ? index->span : node->span,
	       "field is not declared by the target Type");
}

static int plain_name(type_checker *checker, tast_id id, const char *expected)
{
	const tast_node *node = tast_get(checker->ast, id);
	if (!node || node->kind != tast_name) return 0;
	tstring *name = tsource_document_slice(checker->document, node->span);
	int result = tstring_eq_cstr(name, expected);
	tstring_free(name);
	return result;
}

static int types_package_expression(type_checker *checker, tast_id id,
				    uint32_t depth)
{
	if (depth > checker->semantic->symbol_count) return 0;
	const tast_node *node = tast_get(checker->ast, id);
	if (!node) return 0;
	if (node->kind == tast_group)
		return types_package_expression(checker, node->group.value, depth);
	if (node->kind != tast_name) return 0;
	if (plain_name(checker, id, "types")) return 1;
	const tsemantic_symbol *symbol = tsemantic_resolved_symbol(
		checker->semantic, id);
	const tast_node *declaration = symbol ? tast_get(
		checker->ast, symbol->declaration) : nullptr;
	return declaration && declaration->kind == tast_declaration_statement &&
		declaration->declaration_statement.has_initializer &&
		types_package_expression(checker,
			declaration->declaration_statement.initializer, depth + 1);
}

static ttype_constructor_id types_constructor(type_checker *checker, tast_id id)
{
	const tast_node *member = tast_get(checker->ast, id);
	if (!member || member->kind != tast_member ||
	    member->member.op != tsyntax_scope ||
	    !types_package_expression(checker, member->member.receiver, 0))
		return ttype_constructor_invalid;
	tstring *name = tsource_document_slice(
		checker->document, member->member.name);
	ttype_constructor_id result = ttype_constructor_named(tstring_cstr(name));
	tstring_free(name);
	return result;
}

static void validate_type_constructor(type_checker *checker,
				      const tast_node *call,
				      ttype_constructor_id constructor)
{
	const ttype_constructor *descriptor = ttype_constructor_get(constructor);
	if (!descriptor) return;
	uint32_t count = call->aggregate.count;
	if (count < descriptor->minimum_arguments ||
	    count > descriptor->maximum_arguments) {
		report(checker, call->span,
		       "Type constructor argument count mismatch");
		return;
	}
	const tast_id *arguments = tast_get_children(checker->ast,
		call->aggregate.children, count);
	if (constructor != ttype_constructor_make_type &&
	    constructor != ttype_constructor_optional) {
		for (uint32_t i = 0; i < count; i++)
			if (ttype_info_node_static_value(checker->types, arguments[i]) ==
			    TSTATIC_TYPE_UNKNOWN)
				report(checker, tast_get(checker->ast, arguments[i])->span,
				       "argument must be a static Type");
		return;
	}
	if (constructor != ttype_constructor_make_type) return;
	tstring **names = (tstring **)calloc(count, sizeof(*names));
	if (!names) abort();
	for (uint32_t i = 0; i < count; i++) {
		const tast_node *pair = tast_get(checker->ast, arguments[i]);
		if (pair && pair->kind == tast_call && pair->aggregate.count == 1 &&
		    types_constructor(checker, pair->aggregate.receiver) ==
			ttype_constructor_optional) {
			const tast_id *wrapped = tast_get_children(checker->ast,
				pair->aggregate.children, pair->aggregate.count);
			pair = tast_get(checker->ast, wrapped[0]);
		}
		if (!pair || pair->kind != tast_binary ||
		    pair->binary.op != tsyntax_colon) {
			report(checker, tast_get(checker->ast, arguments[i])->span,
			       "each field must be a direct Pair");
			continue;
		}
		names[i] = literal_string(checker, pair->binary.left);
		if (!names[i]) {
			report(checker, tast_get(checker->ast, pair->binary.left)->span,
			       "field name must be a String literal");
			continue;
		}
		if (tstring_empty(names[i]) || tstring_at(names[i], 0) == '@')
			report(checker, tast_get(checker->ast, pair->binary.left)->span,
			       "field name is reserved");
		for (uint32_t previous = 0; previous < i; previous++)
			if (names[previous] && tstring_eq(names[previous], names[i]))
				report(checker,
				       tast_get(checker->ast, pair->binary.left)->span,
				       "duplicate field name");
		if (ttype_info_node_static_value(checker->types,
			pair->binary.right) == TSTATIC_TYPE_UNKNOWN)
			report(checker, tast_get(checker->ast, pair->binary.right)->span,
			       "field value must be a static Type");
	}
	for (uint32_t i = 0; i < count; i++) tstring_free(names[i]);
	free(names);
}

static void validate_mutation(type_checker *checker, const tast_node *call)
{
	int push_front = plain_name(checker, call->aggregate.receiver,
				    "push_front");
	int push_back = plain_name(checker, call->aggregate.receiver,
				   "push_back");
	int insert = plain_name(checker, call->aggregate.receiver, "insert");
	int append = plain_name(checker, call->aggregate.receiver, "append");
	int delete_value = plain_name(checker, call->aggregate.receiver, "delete");
	if (!push_front && !push_back && !insert && !append && !delete_value)
		return;
	const tast_id *args = tast_get_children(checker->ast,
		call->aggregate.children, call->aggregate.count);
	if (call->aggregate.count < 2) return;
	tstatic_type_id container_id = node_type(checker, args[0]);
	const tstatic_type *container = type_of(checker, container_id);
	if (!container) return;
	const tstatic_type_id *parameters = tstatic_type_children(
		&checker->types->arena, container);
	if (container->kind == tstatic_type_list && container->child_count == 1) {
		if (delete_value) {
			if (!assignable(checker, args[1], tbuiltin_int))
				report(checker, tast_get(checker->ast, args[1])->span,
				       "List index must have Type Int");
			return;
		}
		if (push_front || push_back || insert || append) {
			tast_id inserted = args[1];
			if (!assignable(checker, inserted, parameters[0]))
				report(checker, tast_get(checker->ast, inserted)->span,
				       "List element Type mismatch");
		}
		if (insert && call->aggregate.count >= 3 &&
		    !assignable(checker, args[2], tbuiltin_int))
			report(checker, tast_get(checker->ast, args[2])->span,
			       "List index must have Type Int");
		return;
	}
	if (append && container->kind == tstatic_type_dictionary &&
	    container->child_count == 2) {
		const tast_node *pair = tast_get(checker->ast, args[1]);
		if (!pair || pair->kind != tast_binary || pair->binary.op != tsyntax_colon ||
		    !assignable(checker, pair->binary.left, parameters[0]) ||
		    !assignable(checker, pair->binary.right, parameters[1]))
			report(checker, pair ? pair->span : call->span,
			       "Dictionary entry Type mismatch");
	}
	if (delete_value && container->kind == tstatic_type_dictionary &&
	    container->child_count == 2 && !assignable(checker, args[1], parameters[0]))
		report(checker, tast_get(checker->ast, args[1])->span,
		       "Dictionary key Type mismatch");
	if (append && container->kind == tstatic_type_fields) {
		const tast_node *pair = tast_get(checker->ast, args[1]);
		if (!pair || pair->kind != tast_binary || pair->binary.op != tsyntax_colon) {
			report(checker, pair ? pair->span : call->span,
			       "field update requires a direct Pair");
			return;
		}
		tstring *name = literal_string(checker, pair->binary.left);
		const tstatic_field *fields = tstatic_type_field_items(
			&checker->types->arena, container);
		for (uint32_t i = 0; name && i < container->field_count; i++) {
			if (!tstring_eq(name, fields[i].name)) continue;
			if (!assignable(checker, pair->binary.right, fields[i].type))
				report(checker, pair->span, "field value Type mismatch");
			tstring_free(name);
			return;
		}
		tstring_free(name);
		report(checker, pair->span, "field is not declared by the target Type");
	}
	if (delete_value && container->kind == tstatic_type_fields) {
		tstring *name = literal_string(checker, args[1]);
		const tstatic_field *fields = tstatic_type_field_items(
			&checker->types->arena, container);
		for (uint32_t i = 0; name && i < container->field_count; i++)
			if (tstring_eq(name, fields[i].name)) {
				if (!fields[i].optional)
					report(checker, tast_get(checker->ast, args[1])->span,
					       "required fields cannot be deleted");
				break;
			}
		tstring_free(name);
	}
}

static void validate_node(type_checker *checker, tast_id id)
{
	const tast_node *node = tast_get(checker->ast, id);
	if (!node) return;
	if (node->kind == tast_name &&
	    tcontrol_role(checker->flow, id) != tcontrol_child_assignment_target) {
		const tsemantic_symbol *symbol = tsemantic_resolved_symbol(
			checker->semantic, id);
		if (symbol && !tcontrol_definitely_assigned(checker->flow, id) &&
		    !recursive_definition_reference(checker, id, symbol))
			report(checker, node->span,
			       "variable is read before initialization");
	}
	if (node->kind == tast_member &&
	    types_constructor(checker, id) != ttype_constructor_invalid) {
		tast_id parent = tcontrol_parent(checker->flow, id);
		const tast_node *call = tast_get(checker->ast, parent);
		if (tcontrol_role(checker->flow, id) != tcontrol_child_receiver ||
		    !call || call->kind != tast_call)
			report(checker, node->span,
			       "Type constructors cannot be saved or called indirectly");
	}
	if (node->kind == tast_declaration_statement &&
	    node->declaration_statement.has_initializer &&
	    node->declaration_statement.has_annotation) {
		const tsemantic_symbol *symbol = symbol_for_declaration(checker, id);
		if (!assignable(checker, node->declaration_statement.initializer,
				symbol_type(checker, symbol)))
			report(checker, tast_get(checker->ast,
				node->declaration_statement.initializer)->span,
			       "initializer Type mismatch");
	}
	if (node->kind == tast_assignment_statement) {
		const tast_node *target = tast_get(checker->ast,
			node->assignment_statement.target);
		if (target && target->kind == tast_name) {
			const tsemantic_symbol *symbol = tsemantic_resolved_symbol(
				checker->semantic, node->assignment_statement.target);
			const tast_node *declaration = symbol ? tast_get(
				checker->ast, symbol->declaration) : nullptr;
			if (declaration && declaration->kind == tast_declaration_statement &&
			    declaration->declaration_statement.has_annotation &&
			    !assignable(checker, node->assignment_statement.value,
					symbol_type(checker, symbol)))
				report(checker, tast_get(checker->ast,
					node->assignment_statement.value)->span,
				       "assigned Type mismatch");
		} else if (target && target->kind == tast_index)
			validate_index(checker, target, 1,
				node->assignment_statement.value);
	}
	if (node->kind == tast_index) {
		if (tcontrol_role(checker->flow, id) !=
		    tcontrol_child_assignment_target)
			validate_index(checker, node, 0, TAST_INVALID_ID);
	}
	if (node->kind == tast_for_statement &&
	    !assignable(checker, node->for_statement.iterable, tbuiltin_iterable))
		report(checker, tast_get(checker->ast,
			node->for_statement.iterable)->span,
		       "for-loop value is not Iterable");
	if (node->kind == tast_call) {
		ttype_constructor_id constructor = types_constructor(
			checker, node->aggregate.receiver);
		if (constructor != ttype_constructor_invalid)
			validate_type_constructor(checker, node, constructor);
		if (types_constructor(checker, node->aggregate.receiver) ==
		    ttype_constructor_optional) {
			tast_id parent = tcontrol_parent(checker->flow, id);
			const tast_node *candidate = tast_get(checker->ast, parent);
			int wrapped_by_make_type =
				tcontrol_role(checker->flow, id) == tcontrol_child_argument &&
				candidate && candidate->kind == tast_call &&
				types_constructor(checker,
					candidate->aggregate.receiver) ==
					ttype_constructor_make_type;
			if (!wrapped_by_make_type)
				report(checker, node->span,
				       "types::optional is only valid inside types::make_type");
		}
		validate_mutation(checker, node);
		tstatic_type_id callee_id = node_type(checker, node->aggregate.receiver);
		const tstatic_type *callee = type_of(checker, callee_id);
		if (callee && callee->kind == tstatic_type_function) {
			uint32_t parameters = callee->child_count ? callee->child_count - 1 : 0;
			const tast_node *callee_node = tast_get(
				checker->ast, node->aggregate.receiver);
			int tunnel = callee_node && callee_node->kind == tast_member &&
				callee_node->member.op == tsyntax_dot;
			const tast_id *args = tast_get_children(checker->ast,
				node->aggregate.children, node->aggregate.count);
			const tstatic_type_id *signature = tstatic_type_children(
				&checker->types->arena, callee);
			uint32_t argument_count = node->aggregate.count + (tunnel ? 1 : 0);
			if (!callee->variadic && argument_count != parameters)
				report(checker, node->span,
				       "argument count does not match function signature");
			uint32_t checked = argument_count < parameters ?
				argument_count : parameters;
			for (uint32_t i = 0; i < checked; i++) {
				tast_id argument = tunnel && !i ? callee_node->member.receiver :
					args[i - (tunnel ? 1 : 0)];
				if (!assignable(checker, argument, signature[i]))
					report(checker, tast_get(checker->ast, argument)->span,
					       "argument Type mismatch");
			}
		}
		if (callee && callee->kind == tstatic_type_rule) {
			const tast_id *args = tast_get_children(checker->ast,
				node->aggregate.children, node->aggregate.count);
			const tstatic_type_id *parameters = tstatic_type_children(
				&checker->types->arena, callee);
			if (node->aggregate.count != callee->child_count)
				report(checker, node->span,
				       "argument count does not match Rule signature");
			uint32_t checked = node->aggregate.count < callee->child_count ?
				node->aggregate.count : callee->child_count;
			for (uint32_t i = 0; i < checked; i++)
				if (!assignable(checker, args[i], parameters[i]))
					report(checker, tast_get(checker->ast, args[i])->span,
					       "argument Type mismatch");
		}
	}
	if (node->kind == tast_rule_condition &&
	    !assignable(checker, node->rule_condition.value, tbuiltin_bool))
		report(checker, node->span, "Rule condition must have Type Bool");
	if (node->kind == tast_rule_requirement) {
		const tstatic_type *required = type_of(checker,
			node_type(checker, node->expression_statement.value));
		if (!required || (required->kind != tstatic_type_rule_instance &&
		    !(required->kind == tstatic_type_builtin &&
		      required->builtin == tbuiltin_rule_instance)))
			report(checker, node->span,
			       "require expects a Rule application");
	}
	if (node->kind == tast_return_statement) {
		tast_id function_id = tcontrol_enclosing_function(checker->flow, id);
		const tstatic_type *function = type_of(checker, node_type(checker, function_id));
		if (function && function->kind == tstatic_type_function &&
		    function->child_count) {
			const tstatic_type_id *signature = tstatic_type_children(
				&checker->types->arena, function);
			tstatic_type_id result = signature[function->child_count - 1];
			int ok = node->return_statement.value == TAST_INVALID_ID ?
				tstatic_type_assignable(&checker->types->arena,
					tbuiltin_nil, result) :
				assignable(checker, node->return_statement.value, result);
			if (!ok) report(checker, node->span, "result Type mismatch");
		}
	}
	if (node->kind == tast_function) {
		const tstatic_type *function = type_of(checker, node_type(checker, id));
		if (function && function->kind == tstatic_type_function &&
		    function->child_count &&
		    !tcontrol_definitely_returns(checker->flow, node->function.body)) {
			const tstatic_type_id *signature = tstatic_type_children(
				&checker->types->arena, function);
			tstatic_type_id result = signature[function->child_count - 1];
			if (result != TSTATIC_TYPE_UNKNOWN &&
			    !tstatic_type_assignable(&checker->types->arena,
					tbuiltin_nil, result))
				report(checker, node->span,
				       "not every path returns the annotated Type");
		}
	}
}

void ttype_info_validate(const tsource_document *document,
			 const tast_arena *arena,
			 const tsemantic_model *semantic,
			 const tcontrol_flow *flow,
			 const ttype_info_model *model,
			 tdiagnostics *diagnostics)
{
	if (!document || !arena || !semantic || !flow || !model || !diagnostics) return;
	type_checker checker = { document, arena, semantic, flow, model, diagnostics };
	for (tast_id id = 0; id < arena->node_count; id++) validate_node(&checker, id);
}
