/** Static Type evaluation and expression inference over the reusable AST. */
#include "ast_emit_internal.h"

#include "tapas/runtime/tstr.h"

#include <stdlib.h>
#include <string.h>

static ttypeval *retain_type(ttypeval *type)
{
	ttypeval_retain(type);
	return type;
}

static tstring *node_text(const tast_emitter *emitter, tsource_span span)
{
	return tsource_document_slice(emitter->document, span);
}

static ttypeval *builtin_type(const char *name)
{
	static const char *const names[ttype_builtin_count] = {
		"AnyType", "Nil", "Bool", "Int", "Float", "String",
		"List", "Pair", "Dictionary", "Iterator", "Function",
		"Library", "RealArray", "BoolArray", "Time", "Type"
	};
	for (int i = 0; i < ttype_builtin_count; i++)
		if (strcmp(name, names[i]) == 0)
			return ttypeval_builtin((ttype_builtin)i);
	return NULL;
}

static int find_binding(tast_emitter *emitter, tsource_span span,
			tobj_ctr **owner, uint_objs *slot)
{
	tstring *name = node_text(emitter, span);
	int found = tcompile_find_binding(&emitter->cp->tmpctr, name, owner, slot);
	if (!found)
		found = tcompile_find_binding(&emitter->cp->objctr, name, owner, slot);
	tstring_free(name);
	return found;
}

static int types_member_name(tast_emitter *emitter, const tast_node *node,
			     tstring **name)
{
	if (!node || node->kind != tast_member ||
	    node->member.op != tsyntax_scope)
		return 0;
	const tast_node *receiver = tast_get(
		emitter->arena, node->member.receiver);
	if (!receiver || receiver->kind != tast_name)
		return 0;
	tstring *receiver_name = node_text(emitter, receiver->span);
	int is_types = strcmp(tstring_cstr(receiver_name), "types") == 0;
	if (!is_types) {
		tobj_ctr *owner = NULL;
		uint_objs slot = 0;
		if (find_binding(emitter, receiver->span, &owner, &slot))
			is_types = owner->bindings[slot].is_types_package;
	}
	tstring_free(receiver_name);
	if (!is_types)
		return 0;
	*name = node_text(emitter, node->member.name);
	return 1;
}

int tast_is_types_package_expression(tast_emitter *emitter, tast_id id)
{
	const tast_node *node = tast_get(emitter->arena, id);
	if (!node)
		return 0;
	if (node->kind == tast_group)
		return tast_is_types_package_expression(emitter, node->group.value);
	if (node->kind != tast_name)
		return 0;
	tstring *name = node_text(emitter, node->span);
	int result = strcmp(tstring_cstr(name), "types") == 0;
	tstring_free(name);
	if (!result) {
		tobj_ctr *owner = NULL;
		uint_objs slot = 0;
		if (find_binding(emitter, node->span, &owner, &slot))
			result = owner->bindings[slot].is_types_package;
	}
	return result;
}

static const tcompile_export *module_member_export(
	tast_emitter *emitter, const tast_node *node)
{
	if (!node || node->kind != tast_member ||
	    node->member.op != tsyntax_scope)
		return NULL;
	const tast_node *receiver = tast_get(
		emitter->arena, node->member.receiver);
	if (!receiver || receiver->kind != tast_name)
		return NULL;
	tobj_ctr *owner = NULL;
	uint_objs slot = 0;
	if (!find_binding(emitter, receiver->span, &owner, &slot) ||
	    !owner->bindings[slot].module_interface)
		return NULL;
	tstring *name = node_text(emitter, node->member.name);
	const tcompile_export *exported = tcompile_module_export(
		owner->bindings[slot].module_interface, tstring_cstr(name));
	tstring_free(name);
	return exported;
}

static ttypeval *static_type_expression(tast_emitter *emitter, tast_id id);
static tstring *string_literal_value(tast_emitter *emitter,
				     const tast_node *literal);

static int is_type_constructor(tast_emitter *emitter, const tast_node *node)
{
	tstring *name = NULL;
	if (!types_member_name(emitter, node, &name))
		return 0;
	const char *text = tstring_cstr(name);
	int result = strcmp(text, "make_type") == 0 ||
		strcmp(text, "union") == 0 || strcmp(text, "list") == 0 ||
		strcmp(text, "pair") == 0 || strcmp(text, "dictionary") == 0;
	tstring_free(name);
	return result;
}

static ttypeval *static_type_call(tast_emitter *emitter,
				  const tast_node *call)
{
	const tast_node *callee = tast_get(
		emitter->arena, call->aggregate.receiver);
	tstring *name = NULL;
	if (!types_member_name(emitter, callee, &name))
		return NULL;
	const tast_id *arguments = tast_get_children(
		emitter->arena, call->aggregate.children, call->aggregate.count);
	uint32_t count = call->aggregate.count;
	ttypeval *result = NULL;

	if (strcmp(tstring_cstr(name), "make_type") == 0) {
		if (count == 0)
			twarn(ErrCompile_Other, "types::make_type",
			      "at least one field is required");
		ttype_field *fields = (ttype_field *)calloc(count, sizeof(ttype_field));
		if (!fields)
			abort();
		for (uint32_t i = 0; i < count; i++) {
			const tast_node *pair = tast_get(emitter->arena, arguments[i]);
			if (!pair || pair->kind != tast_binary ||
			    pair->binary.op != tsyntax_colon)
				twarn(ErrCompile_Other, "types::make_type",
				      "each field must be a direct Pair");
			const tast_node *field_name = tast_get(
				emitter->arena, pair->binary.left);
			if (!field_name || field_name->kind != tast_string)
				twarn(ErrCompile_Other, "types::make_type",
				      "field name must be a String literal");
			tsource_span contents = field_name->span;
			contents.start++;
			contents.end--;
			fields[i].name = node_text(emitter, contents);
			if (tstring_empty(fields[i].name) ||
			    tstring_at(fields[i].name, 0) == '@')
				twarn(ErrCompile_Other, "types::make_type",
				      "field names beginning with @ are reserved");
			for (uint32_t previous = 0; previous < i; previous++)
				if (tstring_eq(fields[previous].name, fields[i].name))
					twarn(ErrCompile_Other, "types::make_type",
					      "duplicate field name");
			fields[i].type = static_type_expression(
				emitter, pair->binary.right);
			if (!fields[i].type)
				twarn(ErrCompile_Other, "types::make_type",
				      "field value must be a static Type");
		}
		result = ttypeval_new_fields(fields, count);
		for (uint32_t i = 0; i < count; i++) {
			tstring_free((tstring *)fields[i].name);
			ttypeval_release(fields[i].type);
		}
		free(fields);
	} else if (strcmp(tstring_cstr(name), "union") == 0) {
		if (count < 2)
			twarn(ErrCompile_Other, "types::union",
			      "at least two Type arguments are required");
		ttypeval **members = (ttypeval **)calloc(count, sizeof(ttypeval *));
		if (!members)
			abort();
		for (uint32_t i = 0; i < count; i++) {
			members[i] = static_type_expression(emitter, arguments[i]);
			if (!members[i])
				twarn(ErrCompile_Other, "types::union",
				      "argument must be a static Type");
		}
		result = ttypeval_new_union(members, count);
		for (uint32_t i = 0; i < count; i++)
			ttypeval_release(members[i]);
		free(members);
	} else if (strcmp(tstring_cstr(name), "list") == 0) {
		if (count != 1)
			twarn(ErrCompile_Other, "types::list",
			      "exactly one Type argument is required");
		ttypeval *item = static_type_expression(emitter, arguments[0]);
		if (!item)
			twarn(ErrCompile_Other, "types::list",
			      "argument must be a static Type");
		result = ttypeval_new_list(item);
		ttypeval_release(item);
	} else if (strcmp(tstring_cstr(name), "pair") == 0) {
		if (count != 2)
			twarn(ErrCompile_Other, "types::pair",
			      "exactly two Type arguments are required");
		ttypeval *first = static_type_expression(emitter, arguments[0]);
		ttypeval *second = static_type_expression(emitter, arguments[1]);
		if (!first || !second)
			twarn(ErrCompile_Other, "types::pair",
			      "arguments must be static Types");
		result = ttypeval_new_pair(first, second);
		ttypeval_release(first);
		ttypeval_release(second);
	} else if (strcmp(tstring_cstr(name), "dictionary") == 0) {
		if (count != 2)
			twarn(ErrCompile_Other, "types::dictionary",
			      "exactly two Type arguments are required");
		ttypeval *key = static_type_expression(emitter, arguments[0]);
		ttypeval *value = static_type_expression(emitter, arguments[1]);
		if (!key || !value)
			twarn(ErrCompile_Other, "types::dictionary",
			      "arguments must be static Types");
		result = ttypeval_new_dictionary(key, value);
		ttypeval_release(key);
		ttypeval_release(value);
	}
	tstring_free(name);
	return result ? retain_type(result) : NULL;
}

static ttypeval *static_type_expression(tast_emitter *emitter, tast_id id)
{
	const tast_node *node = tast_get(emitter->arena, id);
	if (!node)
		return NULL;
	if (node->kind == tast_group)
		return static_type_expression(emitter, node->group.value);
	if (node->kind == tast_name) {
		tobj_ctr *owner = NULL;
		uint_objs slot = 0;
		if (find_binding(emitter, node->span, &owner, &slot) &&
		    owner->bindings[slot].type_value)
			return retain_type(owner->bindings[slot].type_value);
		return NULL;
	}
	if (node->kind == tast_member) {
		tstring *name = NULL;
		if (types_member_name(emitter, node, &name)) {
			ttypeval *type = builtin_type(tstring_cstr(name));
			tstring_free(name);
			return type ? retain_type(type) : NULL;
		}
		const tcompile_export *exported = module_member_export(emitter, node);
		if (exported)
			return retain_type(exported->type);
		return NULL;
	}
	if (node->kind == tast_call)
		return static_type_call(emitter, node);
	return NULL;
}

static int plain_callee_name(tast_emitter *emitter, const tast_node *callee,
			     const char *expected)
{
	if (!callee || callee->kind != tast_name)
		return 0;
	tstring *name = node_text(emitter, callee->span);
	int equal = strcmp(tstring_cstr(name), expected) == 0;
	tstring_free(name);
	return equal;
}

static void validate_mutating_call(tast_emitter *emitter,
				   const tast_node *call,
				   const tast_node *callee)
{
	int is_push = plain_callee_name(emitter, callee, "push");
	int is_insert = plain_callee_name(emitter, callee, "insert");
	int is_append = plain_callee_name(emitter, callee, "append");
	int is_delete = plain_callee_name(emitter, callee, "delete");
	if (!is_push && !is_insert && !is_append && !is_delete)
		return;
	const tast_id *arguments = tast_get_children(emitter->arena,
		call->aggregate.children, call->aggregate.count);
	if (call->aggregate.count < 2)
		return; /* Runtime arity checking reports the ordinary call error. */
	const tast_node *target = tast_get(emitter->arena, arguments[0]);
	if (!target || target->kind != tast_name)
		return;
	tobj_ctr *owner = NULL;
	uint_objs slot = 0;
	if (!find_binding(emitter, target->span, &owner, &slot))
		return;
	ttypeval *container = owner->bindings[slot].value_type;
	if (!container)
		return;
	if ((is_push || is_insert) && container->kind == ttype_kind_list) {
		ttypeval *item = ttypeval_parameter(container, "item");
		if (!tast_expression_assignable_to(emitter, arguments[1], item))
			twarn(ErrCompile_Other, is_insert ? "insert" : "push",
			      "inserted value Type mismatch");
		return;
	}
	if (is_delete && container->kind == ttype_kind_fields) {
		tstring *field = string_literal_value(
			emitter, tast_get(emitter->arena, arguments[1]));
		if (field && ttypeval_field_named(container, tstring_cstr(field)))
			twarn(ErrCompile_Other, "delete",
			      "required fields cannot be deleted");
		tstring_free(field);
		return;
	}
	if (!is_append)
		return;
	const tast_node *pair = tast_get(emitter->arena, arguments[1]);
	if (container->kind == ttype_kind_list) {
		ttypeval *item = ttypeval_parameter(container, "item");
		if (!tast_expression_assignable_to(emitter, arguments[1], item))
			twarn(ErrCompile_Other, "append",
			      "appended value Type mismatch");
	} else if (container->kind == ttype_kind_dictionary) {
		if (!pair || pair->kind != tast_binary ||
		    pair->binary.op != tsyntax_colon ||
		    !tast_expression_assignable_to(emitter, pair->binary.left,
			ttypeval_parameter(container, "key")) ||
		    !tast_expression_assignable_to(emitter, pair->binary.right,
			ttypeval_parameter(container, "value")))
			twarn(ErrCompile_Other, "append",
			      "Dictionary entry Type mismatch");
	} else if (container->kind == ttype_kind_fields) {
		if (!pair || pair->kind != tast_binary ||
		    pair->binary.op != tsyntax_colon)
			twarn(ErrCompile_Other, "append",
			      "field update requires a direct Pair");
		tstring *field = string_literal_value(
			emitter, tast_get(emitter->arena, pair->binary.left));
		ttypeval *expected = field ? ttypeval_field_named(
			container, tstring_cstr(field)) : NULL;
		if (!expected)
			twarn(ErrCompile_Other, "append",
			      "field is not declared by the target Type");
		if (!tast_expression_assignable_to(
			emitter, pair->binary.right, expected))
			twarn(ErrCompile_Other, "append",
			      "field value Type mismatch");
		tstring_free(field);
	}
}

static void validate_node(tast_emitter *emitter, tast_id id,
			  int direct_constructor_callee)
{
	const tast_node *node = tast_get(emitter->arena, id);
	if (!node)
		return;
	if (node->kind == tast_member && is_type_constructor(emitter, node) &&
	    !direct_constructor_callee)
		twarn(ErrCompile_Other, "Type constructor",
		      "constructors cannot be saved or called indirectly");
	switch (node->kind) {
	case tast_group:
		validate_node(emitter, node->group.value, 0);
		break;
	case tast_unary:
		validate_node(emitter, node->unary.operand, 0);
		break;
	case tast_binary:
		validate_node(emitter, node->binary.left, 0);
		validate_node(emitter, node->binary.right, 0);
		break;
	case tast_member:
		validate_node(emitter, node->member.receiver, 0);
		break;
	case tast_call: {
		const tast_node *callee = tast_get(
			emitter->arena, node->aggregate.receiver);
		int constructor = is_type_constructor(emitter, callee);
		validate_mutating_call(emitter, node, callee);
		if (constructor) {
			ttypeval *validated = static_type_call(emitter, node);
			ttypeval_release(validated);
		}
		validate_node(emitter, node->aggregate.receiver, constructor);
		const tast_id *arguments = tast_get_children(emitter->arena,
			node->aggregate.children, node->aggregate.count);
		for (uint32_t i = 0; i < node->aggregate.count; i++)
			validate_node(emitter, arguments[i], 0);
	} break;
	case tast_index:
		validate_node(emitter, node->aggregate.receiver, 0);
		/* fall through */
	case tast_list:
	case tast_dictionary:
	case tast_structure: {
		const tast_id *children = tast_get_children(emitter->arena,
			node->aggregate.children, node->aggregate.count);
		for (uint32_t i = 0; i < node->aggregate.count; i++)
			validate_node(emitter, children[i], 0);
	} break;
	case tast_named_field:
		validate_node(emitter, node->named_field.value, 0);
		break;
	case tast_slice:
		if (node->slice.has_start)
			validate_node(emitter, node->slice.start, 0);
		if (node->slice.has_end)
			validate_node(emitter, node->slice.end, 0);
		break;
	case tast_function:
		break;
	case tast_module:
	case tast_block:
	case tast_declaration_group:
		break;
	case tast_expression_statement:
		validate_node(emitter, node->expression_statement.value, 0);
		break;
	case tast_declaration_statement:
		validate_node(emitter, node->declaration_statement.initializer, 0);
		break;
	case tast_assignment_statement:
		validate_node(emitter, node->assignment_statement.target, 0);
		validate_node(emitter, node->assignment_statement.value, 0);
		break;
	case tast_return_statement:
		if (node->return_statement.value != TAST_INVALID_ID)
			validate_node(emitter, node->return_statement.value, 0);
		break;
	case tast_if_statement:
	case tast_elif_statement:
	case tast_while_statement:
		validate_node(emitter, node->control_statement.condition, 0);
		break;
	case tast_else_statement:
		break;
	case tast_for_statement:
		validate_node(emitter, node->for_statement.iterable, 0);
		break;
	default:
		break;
	}
}

void tast_validate_node(tast_emitter *emitter, tast_id id)
{
	validate_node(emitter, id, 0);
}

void tast_free_field_order(tstring **order, uint_objs count)
{
	for (uint_objs i = 0; i < count; i++)
		tstring_free(order[i]);
	free(order);
}

static void copy_binding_field_order(const tcompile_binding *binding,
				     tstring ***order, uint_objs *count)
{
	*order = NULL;
	*count = 0;
	if (!binding || binding->field_order_count == 0)
		return;
	*order = (tstring **)calloc(binding->field_order_count,
				    sizeof(tstring *));
	if (!*order)
		abort();
	for (uint_objs i = 0; i < binding->field_order_count; i++)
		(*order)[i] = tstring_dup(binding->field_order[i]);
	*count = binding->field_order_count;
}

void tast_static_type_field_order(tast_emitter *emitter, tast_id id,
				  tstring ***order, uint_objs *count)
{
	*order = NULL;
	*count = 0;
	const tast_node *node = tast_get(emitter->arena, id);
	if (!node)
		return;
	if (node->kind == tast_group) {
		tast_static_type_field_order(emitter, node->group.value, order, count);
		return;
	}
	if (node->kind == tast_name) {
		tobj_ctr *owner = NULL;
		uint_objs slot = 0;
		if (find_binding(emitter, node->span, &owner, &slot))
			copy_binding_field_order(&owner->bindings[slot], order, count);
		return;
	}
	if (node->kind == tast_member) {
		const tcompile_export *exported = module_member_export(emitter, node);
		if (!exported || exported->field_order_count == 0)
			return;
		*order = (tstring **)calloc(exported->field_order_count,
					    sizeof(tstring *));
		if (!*order)
			abort();
		for (uint_objs i = 0; i < exported->field_order_count; i++)
			(*order)[i] = tstring_dup(exported->field_order[i]);
		*count = exported->field_order_count;
		return;
	}
	if (node->kind == tast_dictionary) {
		uint32_t entries = node->aggregate.count / 2;
		const tast_id *children = tast_get_children(emitter->arena,
			node->aggregate.children, node->aggregate.count);
		*order = (tstring **)calloc(entries, sizeof(tstring *));
		if (!*order)
			abort();
		for (uint32_t i = 0; i < entries; i++) {
			const tast_node *key = tast_get(emitter->arena, children[i * 2]);
			if (!key || key->kind != tast_string) {
				tast_free_field_order(*order, *count);
				*order = NULL;
				*count = 0;
				return;
			}
			tsource_span contents = key->span;
			contents.start++;
			contents.end--;
			(*order)[(*count)++] = node_text(emitter, contents);
		}
		return;
	}
	if (node->kind != tast_call)
		return;
	const tast_node *callee = tast_get(emitter->arena,
					  node->aggregate.receiver);
	tstring *constructor = NULL;
	if (!types_member_name(emitter, callee, &constructor))
		return;
	int is_make_type = strcmp(tstring_cstr(constructor), "make_type") == 0;
	tstring_free(constructor);
	if (!is_make_type)
		return;
	const tast_id *arguments = tast_get_children(emitter->arena,
		node->aggregate.children, node->aggregate.count);
	*order = (tstring **)calloc(node->aggregate.count, sizeof(tstring *));
	if (!*order)
		abort();
	for (uint32_t i = 0; i < node->aggregate.count; i++) {
		const tast_node *pair = tast_get(emitter->arena, arguments[i]);
		if (!pair || pair->kind != tast_binary ||
		    pair->binary.op != tsyntax_colon)
			continue;
		const tast_node *field = tast_get(emitter->arena, pair->binary.left);
		if (!field || field->kind != tast_string)
			continue;
		tsource_span contents = field->span;
		contents.start++;
		contents.end--;
		(*order)[*count] = node_text(emitter, contents);
		(*count)++;
	}
	if (*count == 0) {
		free(*order);
		*order = NULL;
	}
}

void tast_annotation_field_order(tast_emitter *emitter,
				 tsource_span annotation,
				 tstring ***order, uint_objs *count)
{
	*order = NULL;
	*count = 0;
	tstring *name = node_text(emitter, annotation);
	const char *scope = strstr(tstring_cstr(name), "::");
	if (!scope) {
		tobj_ctr *owner = NULL;
		uint_objs slot = 0;
		if (tcompile_find_binding(&emitter->cp->tmpctr, name, &owner, &slot) ||
		    tcompile_find_binding(&emitter->cp->objctr, name, &owner, &slot))
			copy_binding_field_order(&owner->bindings[slot], order, count);
	} else if (strstr(scope + 2, "::") == NULL) {
		tstring *module = tstring_new_len(tstring_cstr(name),
			(size_t)(scope - tstring_cstr(name)));
		tobj_ctr *owner = NULL;
		uint_objs slot = 0;
		if (tcompile_find_binding(&emitter->cp->tmpctr, module,
			&owner, &slot) ||
		    tcompile_find_binding(&emitter->cp->objctr, module,
			&owner, &slot)) {
			const tcompile_export *exported = tcompile_module_export(
				owner->bindings[slot].module_interface, scope + 2);
			if (exported && exported->field_order_count) {
				*order = (tstring **)calloc(
					exported->field_order_count, sizeof(tstring *));
				if (!*order)
					abort();
				for (uint_objs i = 0;
				     i < exported->field_order_count; i++)
					(*order)[i] = tstring_dup(
						exported->field_order[i]);
				*count = exported->field_order_count;
			}
		}
		tstring_free(module);
	}
	tstring_free(name);
}

ttypeval *tast_resolve_annotation(tast_emitter *emitter,
				  tsource_span annotation)
{
	tstring *text = node_text(emitter, annotation);
	const char *value = tstring_cstr(text);
	ttypeval *result = NULL;
	const char *scope = strstr(value, "::");
	if (scope && scope == value + 5 && strncmp(value, "types", 5) == 0 &&
	    strstr(scope + 2, "::") == NULL)
		result = builtin_type(scope + 2);
	else if (scope && strstr(scope + 2, "::") == NULL) {
		tstring *module = tstring_new_len(value, (size_t)(scope - value));
		tobj_ctr *owner = NULL;
		uint_objs slot = 0;
		if (tcompile_find_binding(&emitter->cp->tmpctr, module,
			&owner, &slot) ||
		    tcompile_find_binding(&emitter->cp->objctr, module,
			&owner, &slot)) {
			const tcompile_export *exported = tcompile_module_export(
				owner->bindings[slot].module_interface, scope + 2);
			if (exported)
				result = exported->type;
		}
		tstring_free(module);
	} else if (!scope) {
		tobj_ctr *owner = NULL;
		uint_objs slot = 0;
		if (tcompile_find_binding(&emitter->cp->tmpctr, text, &owner, &slot) ||
		    tcompile_find_binding(&emitter->cp->objctr, text, &owner, &slot))
			result = owner->bindings[slot].type_value;
		else
			result = builtin_type(value);
	}
	if (result)
		ttypeval_retain(result);
	else
		twarn(ErrCompile_Other, "type annotation", value);
	tstring_free(text);
	return result;
}

static ttypeval *binding_value_type(tast_emitter *emitter,
				    const tast_node *name)
{
	tobj_ctr *owner = NULL;
	uint_objs slot = 0;
	if (!find_binding(emitter, name->span, &owner, &slot) ||
	    !owner->bindings[slot].value_type)
		return NULL;
	return retain_type(owner->bindings[slot].value_type);
}

static ttypeval *union_of(ttypeval **types, uint32_t count,
			  ttypeval *fallback)
{
	if (count == 0)
		return retain_type(fallback);
	for (uint32_t i = 0; i < count; i++)
		if (!types[i])
			return retain_type(fallback);
	int all_equal = 1;
	for (uint32_t i = 1; i < count; i++)
		all_equal = all_equal && ttypeval_equal(types[0], types[i]);
	if (all_equal)
		return retain_type(types[0]);
	ttypeval *result = ttypeval_new_union(types, count);
	return retain_type(result);
}

static tstring *string_literal_value(tast_emitter *emitter,
				     const tast_node *literal)
{
	if (!literal || literal->kind != tast_string)
		return NULL;
	tsource_span contents = literal->span;
	contents.start++;
	contents.end--;
	return node_text(emitter, contents);
}

static ttypeval *infer_indexed_type(tast_emitter *emitter,
				    const tast_node *node)
{
	ttypeval *receiver = tast_infer_expression_type(
		emitter, node->aggregate.receiver, NULL);
	if (!receiver || node->aggregate.count != 1) {
		ttypeval_release(receiver);
		return NULL;
	}
	const tast_id *indices = tast_get_children(emitter->arena,
		node->aggregate.children, node->aggregate.count);
	ttypeval *result = NULL;
	const tast_node *index = tast_get(emitter->arena, indices[0]);
	if (index && index->kind == tast_slice &&
	    receiver->kind == ttype_kind_list)
		result = receiver;
	else if (receiver->kind == ttype_kind_fields) {
		tstring *field = string_literal_value(
			emitter, tast_get(emitter->arena, indices[0]));
		if (field) {
			result = ttypeval_field_named(receiver, tstring_cstr(field));
			tstring_free(field);
		}
	} else if (receiver->kind == ttype_kind_list)
		result = ttypeval_parameter(receiver, "item");
	else if (receiver->kind == ttype_kind_dictionary)
		result = ttypeval_parameter(receiver, "value");
	if (result)
		ttypeval_retain(result);
	ttypeval_release(receiver);
	return result;
}

static ttypeval *infer_member_type(tast_emitter *emitter,
				   const tast_node *node)
{
	ttypeval *receiver = tast_infer_expression_type(
		emitter, node->member.receiver, NULL);
	if (!receiver || receiver->kind != ttype_kind_fields) {
		ttypeval_release(receiver);
		return NULL;
	}
	tstring *field = node_text(emitter, node->member.name);
	ttypeval *result = ttypeval_field_named(receiver, tstring_cstr(field));
	if (result)
		ttypeval_retain(result);
	tstring_free(field);
	ttypeval_release(receiver);
	return result;
}

static ttypeval *infer_dictionary_type(tast_emitter *emitter,
				       const tast_node *node)
{
	uint32_t count = node->aggregate.count / 2;
	if (count == 0)
		return retain_type(ttypeval_builtin(ttype_builtin_dictionary));
	const tast_id *entries = tast_get_children(emitter->arena,
		node->aggregate.children, node->aggregate.count);
	ttypeval **keys = (ttypeval **)calloc(count, sizeof(ttypeval *));
	ttypeval **values = (ttypeval **)calloc(count, sizeof(ttypeval *));
	tstring **names = (tstring **)calloc(count, sizeof(tstring *));
	if (!keys || !values || !names)
		abort();
	int all_known = 1;
	int field_shape = 1;
	for (uint32_t i = 0; i < count; i++) {
		const tast_node *key_node = tast_get(emitter->arena, entries[i * 2]);
		keys[i] = tast_infer_expression_type(emitter, entries[i * 2], NULL);
		values[i] = tast_infer_expression_type(
			emitter, entries[i * 2 + 1], NULL);
		names[i] = string_literal_value(emitter, key_node);
		all_known = all_known && keys[i] && values[i];
		field_shape = field_shape && names[i];
		for (uint32_t previous = 0; previous < i && names[i]; previous++)
			if (names[previous] && tstring_eq(names[previous], names[i]))
				field_shape = 0;
	}
	ttypeval *result = NULL;
	if (all_known && field_shape) {
		ttype_field *fields = (ttype_field *)calloc(count, sizeof(ttype_field));
		if (!fields)
			abort();
		for (uint32_t i = 0; i < count; i++) {
			fields[i].name = names[i];
			fields[i].type = values[i];
		}
		result = ttypeval_new_fields(fields, count);
		free(fields);
	} else if (all_known) {
		ttypeval *key = union_of(keys, count,
			ttypeval_builtin(ttype_builtin_any));
		ttypeval *value = union_of(values, count,
			ttypeval_builtin(ttype_builtin_any));
		result = ttypeval_new_dictionary(key, value);
		ttypeval_release(key);
		ttypeval_release(value);
	}
	for (uint32_t i = 0; i < count; i++) {
		ttypeval_release(keys[i]);
		ttypeval_release(values[i]);
		tstring_free(names[i]);
	}
	free(keys);
	free(values);
	free(names);
	return result ? retain_type(result) :
		retain_type(ttypeval_builtin(ttype_builtin_dictionary));
}

ttypeval *tast_infer_expression_type(tast_emitter *emitter, tast_id id,
				     ttypeval **static_value)
{
	if (static_value)
		*static_value = NULL;
	ttypeval *type_value = static_type_expression(emitter, id);
	if (type_value) {
		if (static_value)
			*static_value = type_value;
		else
			ttypeval_release(type_value);
		return retain_type(ttypeval_builtin(ttype_builtin_type));
	}

	const tast_node *node = tast_get(emitter->arena, id);
	if (!node)
		return NULL;
	switch (node->kind) {
	case tast_nil: return retain_type(ttypeval_builtin(ttype_builtin_nil));
	case tast_bool: return retain_type(ttypeval_builtin(ttype_builtin_bool));
	case tast_integer: return retain_type(ttypeval_builtin(ttype_builtin_int));
	case tast_float: return retain_type(ttypeval_builtin(ttype_builtin_float));
	case tast_string: return retain_type(ttypeval_builtin(ttype_builtin_string));
	case tast_function:
		return retain_type(ttypeval_builtin(ttype_builtin_function));
	case tast_name:
		return binding_value_type(emitter, node);
	case tast_member:
		return infer_member_type(emitter, node);
	case tast_index:
		return infer_indexed_type(emitter, node);
	case tast_group:
		return tast_infer_expression_type(emitter, node->group.value, NULL);
	case tast_unary:
		return tast_infer_expression_type(emitter, node->unary.operand, NULL);
	case tast_binary: {
		if (node->binary.op == tsyntax_colon) {
			ttypeval *first = tast_infer_expression_type(
				emitter, node->binary.left, NULL);
			ttypeval *second = tast_infer_expression_type(
				emitter, node->binary.right, NULL);
			ttypeval *result = first && second ?
				ttypeval_new_pair(first, second) : NULL;
			ttypeval_release(first);
			ttypeval_release(second);
			return result ? retain_type(result) :
				retain_type(ttypeval_builtin(ttype_builtin_pair));
		}
		if (node->binary.op == tsyntax_eq || node->binary.op == tsyntax_ne ||
		    node->binary.op == tsyntax_gt || node->binary.op == tsyntax_ge ||
		    node->binary.op == tsyntax_lt || node->binary.op == tsyntax_le ||
		    node->binary.op == tsyntax_kw_in ||
		    node->binary.op == tsyntax_kw_and ||
		    node->binary.op == tsyntax_kw_or)
			return retain_type(ttypeval_builtin(ttype_builtin_bool));
		if (node->binary.op == tsyntax_kw_to)
			return retain_type(ttypeval_builtin(ttype_builtin_iterator));
		ttypeval *left = tast_infer_expression_type(
			emitter, node->binary.left, NULL);
		ttypeval *right = tast_infer_expression_type(
			emitter, node->binary.right, NULL);
		ttypeval *result = left && right && ttypeval_equal(left, right) ?
			retain_type(left) : NULL;
		ttypeval_release(left);
		ttypeval_release(right);
		return result;
	}
	case tast_list: {
		const tast_id *items = tast_get_children(
			emitter->arena, node->aggregate.children, node->aggregate.count);
		ttypeval **types = (ttypeval **)calloc(
			node->aggregate.count, sizeof(ttypeval *));
		for (uint32_t i = 0; i < node->aggregate.count; i++)
			types[i] = tast_infer_expression_type(emitter, items[i], NULL);
		int all_known = node->aggregate.count != 0;
		for (uint32_t i = 0; i < node->aggregate.count; i++)
			all_known = all_known && types[i];
		ttypeval *item = all_known ? union_of(types, node->aggregate.count,
			ttypeval_builtin(ttype_builtin_any)) : NULL;
		ttypeval *result = all_known ? ttypeval_new_list(item) :
			ttypeval_builtin(ttype_builtin_list);
		for (uint32_t i = 0; i < node->aggregate.count; i++)
			ttypeval_release(types[i]);
		free(types);
		ttypeval_release(item);
		return retain_type(result);
	}
	case tast_dictionary:
		return infer_dictionary_type(emitter, node);
	default:
		return NULL;
	}
}

static int string_literal_equals(tast_emitter *emitter,
				 const tast_node *literal,
				 const tobj *field_name)
{
	if (!literal || literal->kind != tast_string || !field_name ||
	    field_name->type != tcompo ||
	    tobj_compo_type(field_name) != compo_tstr)
		return 0;
	tsource_span contents = literal->span;
	contents.start++;
	contents.end--;
	tstring *name = node_text(emitter, contents);
	const tstr *expected = (const tstr *)field_name->val.v_tcompo;
	int equal = tstring_eq(name, expected->data);
	tstring_free(name);
	return equal;
}

static int dictionary_assignable_to_fields(tast_emitter *emitter,
					   const tast_node *dictionary,
					   const ttypeval *target)
{
	const tast_id *entries = tast_get_children(
		emitter->arena, dictionary->aggregate.children,
		dictionary->aggregate.count);
	for (uint_objs field = 0; field < ttypeval_field_count(target); field++) {
		const tobj *field_name = NULL;
		ttypeval *field_type = NULL;
		ttypeval_field_at(target, field, &field_name, &field_type);
		int found = 0;
		for (uint32_t i = 0; i + 1 < dictionary->aggregate.count; i += 2) {
			const tast_node *key = tast_get(emitter->arena, entries[i]);
			if (!string_literal_equals(emitter, key, field_name))
				continue;
			found = 1;
			if (!tast_expression_assignable_to(
				emitter, entries[i + 1], field_type))
				return 0;
			break;
		}
		if (!found)
			return 0;
	}
	return 1;
}

int tast_expression_assignable_to(tast_emitter *emitter, tast_id id,
				  const ttypeval *target)
{
	if (!target)
		return 1;
	if (target->kind == ttype_kind_union) {
		for (uint_objs i = 0; i < ttypeval_member_count(target); i++)
			if (tast_expression_assignable_to(
				emitter, id, ttypeval_member_at(target, i)))
				return 1;
		return 0;
	}
	const tast_node *node = tast_get(emitter->arena, id);
	if (node && node->kind == tast_group)
		return tast_expression_assignable_to(
			emitter, node->group.value, target);
	if (node && node->kind == tast_list &&
	    target->kind == ttype_kind_list) {
		ttypeval *item = ttypeval_parameter(target, "item");
		const tast_id *items = tast_get_children(
			emitter->arena, node->aggregate.children,
			node->aggregate.count);
		for (uint32_t i = 0; i < node->aggregate.count; i++)
			if (!tast_expression_assignable_to(emitter, items[i], item))
				return 0;
		return 1;
	}
	if (node && node->kind == tast_binary &&
	    node->binary.op == tsyntax_colon &&
	    target->kind == ttype_kind_pair)
		return tast_expression_assignable_to(
			emitter, node->binary.left,
			ttypeval_parameter(target, "first")) &&
		       tast_expression_assignable_to(
			emitter, node->binary.right,
			ttypeval_parameter(target, "second"));
	if (node && node->kind == tast_dictionary &&
	    target->kind == ttype_kind_fields)
		return dictionary_assignable_to_fields(emitter, node, target);
	if (node && node->kind == tast_dictionary &&
	    target->kind == ttype_kind_dictionary) {
		const tast_id *entries = tast_get_children(
			emitter->arena, node->aggregate.children,
			node->aggregate.count);
		ttypeval *key = ttypeval_parameter(target, "key");
		ttypeval *value = ttypeval_parameter(target, "value");
		for (uint32_t i = 0; i + 1 < node->aggregate.count; i += 2)
			if (!tast_expression_assignable_to(emitter, entries[i], key) ||
			    !tast_expression_assignable_to(
				emitter, entries[i + 1], value))
				return 0;
		return 1;
	}
	ttypeval *actual = tast_infer_expression_type(emitter, id, NULL);
	int assignable = !actual || compile_type_assignable(actual, target);
	ttypeval_release(actual);
	return assignable;
}
