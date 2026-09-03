#include "ast_emit_internal.h"

#include <stdlib.h>
#include <string.h>

static int find_binding(tast_emitter *emitter, tsource_span span,
			tobj_ctr **owner, uint_objs *slot)
{
	tstring *name = tast_emitter_text(emitter, span);
	int found = tcompile_find_binding(&emitter->cp->tmpctr, name, owner, slot);
	if (!found)
		found = tcompile_find_binding(&emitter->cp->objctr, name, owner, slot);
	tstring_free(name);
	return found;
}

int tast_is_types_package_expression(tast_emitter *emitter, tast_id id)
{
	const tast_node *node = tast_get(emitter->arena, id);
	if (!node) return 0;
	if (node->kind == tast_group)
		return tast_is_types_package_expression(emitter, node->group.value);
	if (node->kind != tast_name) return 0;
	tstring *name = tast_emitter_text(emitter, node->span);
	int result = strcmp(tstring_cstr(name), "types") == 0;
	tstring_free(name);
	if (!result) {
		tobj_ctr *owner = nullptr;
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
		return nullptr;
	const tast_node *receiver = tast_get(
		emitter->arena, node->member.receiver);
	if (!receiver || receiver->kind != tast_name) return nullptr;
	tobj_ctr *owner = nullptr;
	uint_objs slot = 0;
	if (!find_binding(emitter, receiver->span, &owner, &slot) ||
	    !owner->bindings[slot].module_interface)
		return nullptr;
	tstring *name = tast_emitter_text(emitter, node->member.name);
	const tcompile_export *exported = tcompile_module_export(
		owner->bindings[slot].module_interface, tstring_cstr(name));
	tstring_free(name);
	return exported;
}

void tast_free_field_order(tstring **order, uint_objs count)
{
	for (uint_objs i = 0; i < count; i++) tstring_free(order[i]);
	free(order);
}

static void copy_binding_field_order(const tcompile_binding *binding,
				     tstring ***order, uint_objs *count)
{
	*order = nullptr;
	*count = 0;
	if (!binding || binding->field_order_count == 0) return;
	*order = (tstring **)calloc(
		binding->field_order_count, sizeof(**order));
	if (!*order) abort();
	for (uint_objs i = 0; i < binding->field_order_count; i++)
		(*order)[i] = tstring_dup(binding->field_order[i]);
	*count = binding->field_order_count;
}

static void copy_export_field_order(const tcompile_export *exported,
				    tstring ***order, uint_objs *count)
{
	*order = nullptr;
	*count = 0;
	if (!exported || exported->field_order_count == 0) return;
	*order = (tstring **)calloc(
		exported->field_order_count, sizeof(**order));
	if (!*order) abort();
	for (uint_objs i = 0; i < exported->field_order_count; i++)
		(*order)[i] = tstring_dup(exported->field_order[i]);
	*count = exported->field_order_count;
}

static void copy_static_field_order(tast_emitter *emitter, tast_id id,
				    tstring ***order, uint_objs *count)
{
	tstatic_type_id type_id = ttype_info_node_static_value(
		&emitter->frontend->types, id);
	const tstatic_type_arena *arena = &emitter->frontend->types.arena;
	const tstatic_type *type = tstatic_type_get(arena, type_id);
	while (type && type->kind == tstatic_type_recursive &&
	       type->child_count == 1) {
		const tstatic_type_id *children = tstatic_type_children(arena, type);
		type = tstatic_type_get(arena, children[0]);
	}
	if (!type || type->kind != tstatic_type_fields || !type->field_count)
		return;
	const tstatic_field *fields = tstatic_type_field_items(arena, type);
	*order = (tstring **)calloc(type->field_count, sizeof(**order));
	if (!*order) abort();
	for (uint32_t i = 0; i < type->field_count; i++)
		(*order)[i] = tstring_dup(fields[i].name);
	*count = type->field_count;
}

void tast_static_type_field_order(tast_emitter *emitter, tast_id id,
				  tstring ***order, uint_objs *count)
{
	*order = nullptr;
	*count = 0;
	const tast_node *node = tast_get(emitter->arena, id);
	if (!node) return;
	if (node->kind == tast_group) {
		tast_static_type_field_order(emitter, node->group.value, order, count);
		return;
	}
	if (node->kind == tast_name) {
		tobj_ctr *owner = nullptr;
		uint_objs slot = 0;
		if (find_binding(emitter, node->span, &owner, &slot))
			copy_binding_field_order(&owner->bindings[slot], order, count);
		if (*count) return;
	}
	if (node->kind == tast_member) {
		copy_export_field_order(
			module_member_export(emitter, node), order, count);
		if (*count) return;
	}
	copy_static_field_order(emitter, id, order, count);
}

void tast_annotation_field_order(tast_emitter *emitter,
				 tsource_span annotation,
				 tstring ***order, uint_objs *count)
{
	*order = nullptr;
	*count = 0;
	tstring *name = tast_emitter_text(emitter, annotation);
	const char *scope = strstr(tstring_cstr(name), "::");
	if (!scope) {
		tobj_ctr *owner = nullptr;
		uint_objs slot = 0;
		if (tcompile_find_binding(&emitter->cp->tmpctr, name, &owner, &slot) ||
		    tcompile_find_binding(&emitter->cp->objctr, name, &owner, &slot))
			copy_binding_field_order(&owner->bindings[slot], order, count);
	} else if (strstr(scope + 2, "::") == nullptr) {
		tstring *module = tstring_new_len(
			tstring_cstr(name), (size_t)(scope - tstring_cstr(name)));
		tobj_ctr *owner = nullptr;
		uint_objs slot = 0;
		if (tcompile_find_binding(&emitter->cp->tmpctr, module,
			&owner, &slot) ||
		    tcompile_find_binding(&emitter->cp->objctr, module,
			&owner, &slot))
			copy_export_field_order(tcompile_module_export(
				owner->bindings[slot].module_interface, scope + 2),
				order, count);
		tstring_free(module);
	}
	tstring_free(name);
}

ttypeval *tast_resolve_annotation(tast_emitter *emitter,
				  tsource_span annotation)
{
	tstring *text = tast_emitter_text(emitter, annotation);
	ttypeval *result = compile_resolve_annotation(emitter->cp, text);
	tstring_free(text);
	return result;
}

ttypeval *tast_function_signature(tast_emitter *emitter,
				  const tast_node *function)
{
	if (!function || function->kind != tast_function) return nullptr;
	tast_id id = (tast_id)(function - emitter->arena->nodes);
	tstatic_type_id signature = ttype_info_node_id(
		&emitter->frontend->types, id);
	return compile_type_from_static(
		&emitter->frontend->types.arena, signature);
}

ttypeval *tast_infer_expression_type(tast_emitter *emitter, tast_id id,
				     ttypeval **static_value)
{
	if (static_value) *static_value = nullptr;
	tstatic_type_id static_id = ttype_info_node_static_value(
		&emitter->frontend->types, id);
	if (static_id != TSTATIC_TYPE_UNKNOWN) {
		ttypeval *type_value = compile_type_from_static(
			&emitter->frontend->types.arena, static_id);
		if (static_value)
			*static_value = type_value;
		else
			ttypeval_release(type_value);
		return ttypeval_retain(ttypeval_builtin(tbuiltin_type));
	}
	tstatic_type_id shared = ttype_info_node_id(
		&emitter->frontend->types, id);
	return shared == TSTATIC_TYPE_UNKNOWN ? nullptr : compile_type_from_static(
		&emitter->frontend->types.arena, shared);
}
