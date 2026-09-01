/** Conversion boundary between shared static Type IR and runtime Type values. */
#include "internal.h"
#include "tapas/compile/static_type.h"
#include "tapas/runtime/tstr.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
	tcp *cp;
	tstatic_type_arena *arena;
	const ttypeval **runtime;
	tstatic_type_id *static_ids;
	uint32_t count;
	uint32_t capacity;
} to_static_context;

typedef struct {
	const tstatic_type_arena *arena;
	tstatic_type_id *ids;
	ttypeval **values;
	uint32_t count;
	uint32_t capacity;
} to_runtime_context;

static ttypeval *retain_type(ttypeval *type)
{
	ttypeval_retain(type);
	return type;
}

static ttypeval *builtin_named(const char *name)
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

static void reserve_to_static(to_static_context *context)
{
	if (context->count < context->capacity) return;
	uint32_t capacity = context->capacity ? context->capacity * 2 : 8;
	context->runtime = (const ttypeval **)realloc(
		context->runtime, capacity * sizeof(*context->runtime));
	context->static_ids = (tstatic_type_id *)realloc(
		context->static_ids, capacity * sizeof(*context->static_ids));
	if (!context->runtime || !context->static_ids) abort();
	context->capacity = capacity;
}

static tstatic_type_id runtime_to_static(to_static_context *context,
					 const ttypeval *type)
{
	if (!type) return TSTATIC_TYPE_UNKNOWN;
	if (type->kind == ttype_kind_recursive) {
		for (uint32_t i = 0; i < context->count; i++)
			if (context->runtime[i] == type) return context->static_ids[i];
		reserve_to_static(context);
		tstatic_type_id result = tstatic_type_make_recursive(context->arena);
		context->runtime[context->count] = type;
		context->static_ids[context->count++] = result;
		if (type->recursive_defined)
			tstatic_type_define_recursive(context->arena, result,
				runtime_to_static(context, type->recursive_body));
		return result;
	}
	if (type->kind == ttype_kind_any)
		return tstatic_type_builtin_id(context->arena, tstatic_builtin_any);
	if (type->kind == ttype_kind_builtin)
		return tstatic_type_builtin_id(
			context->arena, (tstatic_builtin)type->builtin);
	if (type->kind == ttype_kind_list || type->kind == ttype_kind_pair ||
	    type->kind == ttype_kind_dictionary) {
		uint32_t count = type->kind == ttype_kind_list ? 1 : 2;
		tstatic_type_id children[2] = {
			runtime_to_static(context, ttypeval_parameter(type,
				type->kind == ttype_kind_list ? "item" :
				type->kind == ttype_kind_pair ? "first" : "key")),
			TSTATIC_TYPE_UNKNOWN
		};
		if (count == 2)
			children[1] = runtime_to_static(context, ttypeval_parameter(type,
				type->kind == ttype_kind_pair ? "second" : "value"));
		return tstatic_type_make(context->arena,
			type->kind == ttype_kind_list ? tstatic_type_list :
			type->kind == ttype_kind_pair ? tstatic_type_pair :
			tstatic_type_dictionary, children, count, 0);
	}
	if (type->kind == ttype_kind_function || type->kind == ttype_kind_union) {
		uint32_t members = type->kind == ttype_kind_function ?
			ttypeval_function_parameter_count(type) + 1 :
			ttypeval_member_count(type);
		tstatic_type_id *children = (tstatic_type_id *)calloc(
			members, sizeof(*children));
		if (!children) abort();
		for (uint32_t i = 0; i < members; i++) {
			ttypeval *child = type->kind == ttype_kind_function ?
				(i + 1 == members ? ttypeval_function_result(type) :
				 ttypeval_function_parameter_at(type, i)) :
				ttypeval_member_at(type, i);
			children[i] = runtime_to_static(context, child);
		}
		tstatic_type_id result = tstatic_type_make(context->arena,
			type->kind == ttype_kind_function ? tstatic_type_function :
			tstatic_type_union, children, members,
			type->kind == ttype_kind_function &&
			ttypeval_function_variadic(type));
		free(children);
		return result;
	}
	if (type->kind == ttype_kind_fields) {
		uint32_t count = ttypeval_field_count(type);
		tstatic_field *fields = (tstatic_field *)calloc(count, sizeof(*fields));
		if (!fields) abort();
		for (uint32_t i = 0; i < count; i++) {
			const tobj *name = NULL;
			ttypeval *field_type = NULL;
			ttypeval_field_at(type, i, &name, &field_type);
			fields[i].name = tstring_dup(
				((const tstr *)name->val.v_tcompo)->data);
			fields[i].type = runtime_to_static(context, field_type);
		}
		tstatic_type_id result = tstatic_type_make_fields(
			context->arena, fields, count);
		for (uint32_t i = 0; i < count; i++) tstring_free(fields[i].name);
		free(fields);
		return result;
	}
	return TSTATIC_TYPE_UNKNOWN;
}

static void reserve_to_runtime(to_runtime_context *context)
{
	if (context->count < context->capacity) return;
	uint32_t capacity = context->capacity ? context->capacity * 2 : 8;
	context->ids = (tstatic_type_id *)realloc(
		context->ids, capacity * sizeof(*context->ids));
	context->values = (ttypeval **)realloc(
		context->values, capacity * sizeof(*context->values));
	if (!context->ids || !context->values) abort();
	context->capacity = capacity;
}

static ttypeval *static_to_runtime(to_runtime_context *context,
				   tstatic_type_id id)
{
	const tstatic_type *type = tstatic_type_get(context->arena, id);
	if (!type) return NULL;
	if (type->kind == tstatic_type_builtin)
		return retain_type(ttypeval_builtin((ttype_builtin)type->builtin));
	if (type->kind == tstatic_type_recursive) {
		for (uint32_t i = 0; i < context->count; i++)
			if (context->ids[i] == id) return retain_type(context->values[i]);
		reserve_to_runtime(context);
		ttypeval *result = ttypeval_new_recursive();
		context->ids[context->count] = id;
		context->values[context->count++] = result;
		const tstatic_type_id *body = tstatic_type_children(
			context->arena, type);
		if (type->child_count == 1) {
			ttypeval *resolved = static_to_runtime(context, body[0]);
			ttypeval_define_recursive(result, resolved);
			ttypeval_release(resolved);
		}
		return retain_type(result);
	}
	const tstatic_type_id *ids = tstatic_type_children(context->arena, type);
	ttypeval **children = type->child_count ? (ttypeval **)calloc(
		type->child_count, sizeof(*children)) : NULL;
	if (type->child_count && !children) abort();
	for (uint32_t i = 0; i < type->child_count; i++)
		children[i] = static_to_runtime(context, ids[i]);
	ttypeval *result = NULL;
	if (type->kind == tstatic_type_list && type->child_count == 1)
		result = ttypeval_new_list(children[0]);
	else if (type->kind == tstatic_type_pair && type->child_count == 2)
		result = ttypeval_new_pair(children[0], children[1]);
	else if (type->kind == tstatic_type_dictionary && type->child_count == 2)
		result = ttypeval_new_dictionary(children[0], children[1]);
	else if (type->kind == tstatic_type_union && type->child_count >= 2)
		result = ttypeval_new_union(children, type->child_count);
	else if (type->kind == tstatic_type_function && type->child_count >= 1)
		result = ttypeval_new_function(children, type->child_count - 1,
			children[type->child_count - 1], type->variadic);
	else if (type->kind == tstatic_type_fields) {
		const tstatic_field *source = tstatic_type_field_items(
			context->arena, type);
		ttype_field *fields = (ttype_field *)calloc(
			type->field_count, sizeof(*fields));
		if (!fields) abort();
		for (uint32_t i = 0; i < type->field_count; i++) {
			fields[i].name = source[i].name;
			fields[i].type = static_to_runtime(context, source[i].type);
		}
		result = ttypeval_new_fields(fields, type->field_count);
		for (uint32_t i = 0; i < type->field_count; i++)
			ttypeval_release(fields[i].type);
		free(fields);
	}
	for (uint32_t i = 0; i < type->child_count; i++)
		ttypeval_release(children[i]);
	free(children);
	return result ? retain_type(result) : NULL;
}

ttypeval *compile_type_from_static(const tstatic_type_arena *arena,
				   tstatic_type_id id)
{
	to_runtime_context context = { .arena = arena };
	ttypeval *result = static_to_runtime(&context, id);
	free(context.ids);
	free(context.values);
	return result;
}

int compile_runtime_types_assignable(const ttypeval *actual,
				     const ttypeval *target)
{
	if (!actual || !target) return 1;
	tstatic_type_arena arena;
	tstatic_type_arena_init(&arena);
	to_static_context context = { .arena = &arena };
	tstatic_type_id actual_id = runtime_to_static(&context, actual);
	tstatic_type_id target_id = runtime_to_static(&context, target);
	int result = tstatic_type_assignable(&arena, actual_id, target_id);
	free(context.runtime);
	free(context.static_ids);
	tstatic_type_arena_free(&arena);
	return result;
}

static tstatic_type_id external_binding_type(
	void *data, tstatic_type_arena *arena,
	const char *qualified_name, int static_value)
{
	tcp *cp = (tcp *)data;
	const ttypeval *resolved = NULL;
	const char *scope = strstr(qualified_name, "::");
	if (scope && !strstr(scope + 2, "::")) {
		tstring *owner_name = tstring_new_len(
			qualified_name, (size_t)(scope - qualified_name));
		tobj_ctr *owner = NULL;
		uint_objs slot = 0;
		if ((tcompile_find_binding(&cp->tmpctr, owner_name, &owner, &slot) ||
		     tcompile_find_binding(&cp->objctr, owner_name, &owner, &slot)) &&
		    owner->bindings[slot].module_interface) {
			const tcompile_export *exported = tcompile_module_export(
				owner->bindings[slot].module_interface, scope + 2);
			if (exported)
				resolved = static_value ? exported->type :
					ttypeval_builtin(ttype_builtin_type);
		}
		tstring_free(owner_name);
	} else if (!scope) {
		tstring *name = tstring_new(qualified_name);
		tobj_ctr *owner = NULL;
		uint_objs slot = 0;
		if (tcompile_find_binding(&cp->tmpctr, name, &owner, &slot) ||
		    tcompile_find_binding(&cp->objctr, name, &owner, &slot))
			resolved = static_value ? owner->bindings[slot].type_value :
				owner->bindings[slot].value_type;
		tstring_free(name);
	}
	if (!resolved) return TSTATIC_TYPE_UNKNOWN;
	to_static_context context = { .cp = cp, .arena = arena };
	tstatic_type_id result = runtime_to_static(&context, resolved);
	free(context.runtime);
	free(context.static_ids);
	return result;
}

void tcompile_frontend_init(tcp *cp, tfrontend *frontend,
			    const char *name, const char *source,
			    tfrontend_mode mode)
{
	if (!frontend) return;
	tfrontend_init_with_resolver(
		frontend, name, source, mode,
		cp ? external_binding_type : NULL, cp);
}

static tstatic_type_id resolve_annotation_name(void *data, const char *name)
{
	to_static_context *context = (to_static_context *)data;
	ttypeval *resolved = NULL;
	const char *scope = strstr(name, "::");
	if (scope) {
		tstring *owner_name = tstring_new_len(name, (size_t)(scope - name));
		const char *member = scope + 2;
		if (tstring_eq_cstr(owner_name, "types"))
			resolved = builtin_named(member);
		else if (!strstr(member, "::")) {
			tobj_ctr *owner = NULL;
			uint_objs slot = 0;
			if ((tcompile_find_binding(&context->cp->tmpctr, owner_name,
				&owner, &slot) || tcompile_find_binding(
				&context->cp->objctr, owner_name, &owner, &slot)) &&
			    owner->bindings[slot].module_interface) {
				const tcompile_export *exported = tcompile_module_export(
					owner->bindings[slot].module_interface, member);
				if (exported) resolved = exported->type;
			}
		}
		tstring_free(owner_name);
	} else {
		tstring *local_name = tstring_new(name);
		tobj_ctr *owner = NULL;
		uint_objs slot = 0;
		if (tcompile_find_binding(&context->cp->tmpctr, local_name,
			&owner, &slot) || tcompile_find_binding(
			&context->cp->objctr, local_name, &owner, &slot)) {
			if (!owner->bindings[slot].type_value) {
				tstring_free(local_name);
				return TSTATIC_TYPE_INVALID_NAME;
			}
			resolved = owner->bindings[slot].type_value;
		}
		tstring_free(local_name);
	}
	return resolved ? runtime_to_static(context, resolved) :
		TSTATIC_TYPE_UNKNOWN;
}

ttypeval *compile_resolve_annotation(tcp *cp, const tstring *annotation)
{
	if (tstring_empty(annotation)) return NULL;
	tstatic_type_arena arena;
	tstatic_type_arena_init(&arena);
	to_static_context context = { .cp = cp, .arena = &arena };
	tstatic_type_id id = tstatic_type_parse(&arena,
		tstring_cstr(annotation), resolve_annotation_name, &context);
	ttypeval *result = compile_type_from_static(&arena, id);
	free(context.runtime);
	free(context.static_ids);
	tstatic_type_arena_free(&arena);
	if (!result)
		twarn(ErrCompile_Other, "type annotation", tstring_cstr(annotation));
	return result;
}
