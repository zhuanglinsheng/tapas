/** Conversion boundary between shared static Type IR and runtime Type values. */
#include "internal.h"

#include "tapas/runtime/tdict.h"
#include "tapas/runtime/tstr.h"
#include "tapas/compile/static_type.h"

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
		return tstatic_type_builtin_id(context->arena, tbuiltin_any);
	if (type->kind == ttype_kind_builtin)
		return tstatic_type_builtin_id(
			context->arena, (tbuiltin_id)type->builtin);
	if (type->kind == ttype_kind_list || type->kind == ttype_kind_iterator ||
	    type->kind == ttype_kind_pair ||
	    type->kind == ttype_kind_dictionary) {
		uint32_t count = (type->kind == ttype_kind_list ||
			type->kind == ttype_kind_iterator) ? 1 : 2;
		tstatic_type_id children[2] = {
			runtime_to_static(context, ttypeval_parameter(type,
				(type->kind == ttype_kind_list ||
				 type->kind == ttype_kind_iterator) ? "item" :
				type->kind == ttype_kind_pair ? "first" : "key")),
			TSTATIC_TYPE_UNKNOWN
		};
		if (count == 2)
			children[1] = runtime_to_static(context, ttypeval_parameter(type,
				type->kind == ttype_kind_pair ? "second" : "value"));
		return tstatic_type_make(context->arena,
			type->kind == ttype_kind_list ? tstatic_type_list :
			type->kind == ttype_kind_iterator ? tstatic_type_iterator :
			type->kind == ttype_kind_pair ? tstatic_type_pair :
			tstatic_type_dictionary, children, count, 0);
	}
	if (type->kind == ttype_kind_rule ||
	    type->kind == ttype_kind_rule_instance) {
		uint32_t count = ttypeval_function_parameter_count(type);
		tstatic_type_id *children = count ?
			(tstatic_type_id *)calloc(count, sizeof(*children)) : nullptr;
		if (count && !children) abort();
		for (uint32_t i = 0; i < count; i++)
			children[i] = runtime_to_static(context,
				ttypeval_function_parameter_at(type, i));
		tstatic_type_id result = tstatic_type_make(context->arena,
			type->kind == ttype_kind_rule ? tstatic_type_rule :
				tstatic_type_rule_instance, children, count, 0);
		free(children);
		return result;
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
			const tobj *name = nullptr;
			ttypeval *field_type = nullptr;
			ttypeval_field_at(type, i, &name, &field_type);
			fields[i].name = tstring_dup(
				((const tstr *)name->val.v_tcompo)->data);
			fields[i].type = runtime_to_static(context, field_type);
			fields[i].optional = ttypeval_field_optional(type,
				tstring_cstr(fields[i].name));
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
	if (!type) return nullptr;
	if (type->kind == tstatic_type_builtin)
		return ttypeval_retain(ttypeval_builtin((tbuiltin_id)type->builtin));
	if (type->kind == tstatic_type_recursive) {
		for (uint32_t i = 0; i < context->count; i++)
			if (context->ids[i] == id)
				return ttypeval_retain(context->values[i]);
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
		return ttypeval_retain(result);
	}
	const tstatic_type_id *ids = tstatic_type_children(context->arena, type);
	ttypeval **children = type->child_count ? (ttypeval **)calloc(
		type->child_count, sizeof(*children)) : nullptr;
	if (type->child_count && !children) abort();
	for (uint32_t i = 0; i < type->child_count; i++)
		children[i] = static_to_runtime(context, ids[i]);
	ttypeval *result = nullptr;
	if (type->kind == tstatic_type_list && type->child_count == 1)
		result = ttypeval_new_list(children[0]);
	else if (type->kind == tstatic_type_iterator && type->child_count == 1)
		result = ttypeval_new_iterator(children[0]);
	else if (type->kind == tstatic_type_pair && type->child_count == 2)
		result = ttypeval_new_pair(children[0], children[1]);
	else if (type->kind == tstatic_type_dictionary && type->child_count == 2)
		result = ttypeval_new_dictionary(children[0], children[1]);
	else if (type->kind == tstatic_type_union && type->child_count >= 2)
		result = ttypeval_new_union(children, type->child_count);
	else if (type->kind == tstatic_type_function && type->child_count >= 1)
		result = ttypeval_new_function(children, type->child_count - 1,
			children[type->child_count - 1], type->variadic);
	else if (type->kind == tstatic_type_rule)
		result = ttypeval_new_rule(children, type->child_count);
	else if (type->kind == tstatic_type_rule_instance)
		result = ttypeval_new_rule_instance(children, type->child_count);
	else if (type->kind == tstatic_type_fields) {
		const tstatic_field *source = tstatic_type_field_items(
			context->arena, type);
		ttype_field *fields = (ttype_field *)calloc(
			type->field_count, sizeof(*fields));
		if (!fields) abort();
		for (uint32_t i = 0; i < type->field_count; i++) {
			fields[i].name = source[i].name;
			fields[i].type = static_to_runtime(context, source[i].type);
			fields[i].optional = source[i].optional;
		}
		result = ttypeval_new_fields(fields, type->field_count);
		for (uint32_t i = 0; i < type->field_count; i++)
			ttypeval_release(fields[i].type);
		free(fields);
	}
	for (uint32_t i = 0; i < type->child_count; i++)
		ttypeval_release(children[i]);
	free(children);
	return ttypeval_retain(result);
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

static tstatic_type_id external_binding_type(
	void *data, tstatic_type_arena *arena,
	const char *qualified_name, int static_value)
{
	tcp *cp = (tcp *)data;
	const ttypeval *resolved = nullptr;
	const char *scope = strstr(qualified_name, "::");
	if (scope && !strstr(scope + 2, "::")) {
		tstring *owner_name = tstring_new_len(
			qualified_name, (size_t)(scope - qualified_name));
		tobj_ctr *owner = nullptr;
		uint_objs slot = 0;
		if ((tcompile_find_binding(&cp->tmpctr, owner_name, &owner, &slot) ||
		     tcompile_find_binding(&cp->objctr, owner_name, &owner, &slot)) &&
		    owner->bindings[slot].module_interface) {
			const tcompile_export *exported = tcompile_module_export(
				owner->bindings[slot].module_interface, scope + 2);
			if (exported)
				resolved = static_value ? exported->type :
					ttypeval_builtin(tbuiltin_type);
		}
		tstring_free(owner_name);
	} else if (!scope) {
		tstring *name = tstring_new(qualified_name);
		tobj_ctr *owner = nullptr;
		uint_objs slot = 0;
		if (tcompile_find_binding(&cp->tmpctr, name, &owner, &slot) ||
		    tcompile_find_binding(&cp->objctr, name, &owner, &slot))
			resolved = static_value ? owner->bindings[slot].type_value :
				owner->bindings[slot].value_type;
		tstring_free(name);
	}
	if (!resolved && cp && cp->preload_library) {
		const tobj *value = nullptr;
		tobj member_value;
		tobj_set_nil(&member_value);
		if (scope && !strstr(scope + 2, "::")) {
			tstring *owner_name = tstring_new_len(
				qualified_name, (size_t)(scope - qualified_name));
			const tobj *owner = tlib_find(
				cp->preload_library, tstring_cstr(owner_name));
			tstring_free(owner_name);
			if (owner && owner->type == tcompo && owner->val.v_tcompo &&
			    owner->val.v_tcompo->vtable->get_compo_type_code() == compo_tdict) {
				tobj key;
				tobj_set_nil(&key);
				tobj_set_compo(&key, (tcompo_v *)tstr_new(scope + 2));
				tdict *package = (tdict *)owner->val.v_tcompo;
				if (tdict_contains(package, &key)) {
					tdict_get(package, &key, &member_value);
					value = &member_value;
				}
				tobj_try_clear(&key);
			}
		} else if (!scope) {
			value = tlib_find(cp->preload_library, qualified_name);
		}
		if (static_value && value && value->type == tcompo &&
		    value->val.v_tcompo &&
		    value->val.v_tcompo->vtable->get_compo_type_code() ==
			    compo_ttypeval) {
			to_static_context context = { .cp = cp, .arena = arena };
			tstatic_type_id result = runtime_to_static(
				&context, (ttypeval *)value->val.v_tcompo);
			free(context.runtime);
			free(context.static_ids);
			tobj_try_clear(&member_value);
			return result;
		}
		if (!static_value && value && value->type == tcompo &&
		    value->val.v_tcompo &&
		    (value->val.v_tcompo->vtable->get_compo_type_code() == compo_cppfunc ||
		     value->val.v_tcompo->vtable->get_compo_type_code() == compo_sessfunc)) {
			tcompo_type kind =
				value->val.v_tcompo->vtable->get_compo_type_code();
			const char *signature = kind == compo_cppfunc ?
				tcppgenf_get_signature_type(
					(const tcppgenf *)value->val.v_tcompo) :
				tcppsessf_get_signature_type(
					(const tcppsessf *)value->val.v_tcompo);
			tstatic_type_id result = signature && *signature ?
				tstatic_type_parse(arena, signature, nullptr, nullptr) :
				TSTATIC_TYPE_UNKNOWN;
			tobj_try_clear(&member_value);
			return result;
		}
		tobj_try_clear(&member_value);
	}
	if (!resolved) return TSTATIC_TYPE_UNKNOWN;
	to_static_context context = { .cp = cp, .arena = arena };
	tstatic_type_id result = runtime_to_static(&context, resolved);
	free(context.runtime);
	free(context.static_ids);
	return result;
}

static int external_binding_semantic(
	void *context, const char *name,
	tsemantic_symbol_kind *kind, int *initialized)
{
	tcp *cp = (tcp *)context;
	tstring *binding_name = tstring_new(name);
	tobj_ctr *owner = nullptr;
	uint_objs slot = 0;
	int temporary = tcompile_find_binding(
		&cp->tmpctr, binding_name, &owner, &slot);
	int found = temporary || tcompile_find_binding(
		&cp->objctr, binding_name, &owner, &slot);
	tstring_free(binding_name);
	if (!found) return 0;
	*kind = temporary ? tsemantic_symbol_let : tsemantic_symbol_var;
	*initialized = owner->bindings[slot].initialized;
	return 1;
}

void tcompile_frontend_init(tcp *cp, tfrontend *frontend,
			    const char *name, const char *source,
			    tfrontend_mode mode)
{
	if (!frontend) return;
	tfrontend_init_with_environment(frontend, name, source, mode,
		cp ? external_binding_type : nullptr, cp,
		cp ? external_binding_semantic : nullptr, cp);
}

static tstatic_type_id resolve_annotation_name(void *data, const char *name)
{
	to_static_context *context = (to_static_context *)data;
	ttypeval *resolved = nullptr;
	const char *scope = strstr(name, "::");
	if (scope) {
		tstring *owner_name = tstring_new_len(name, (size_t)(scope - name));
		const char *member = scope + 2;
		if (tstring_eq_cstr(owner_name, "types"))
			resolved = ttypeval_builtin_named(member);
		else if (!strstr(member, "::")) {
			tobj_ctr *owner = nullptr;
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
		tobj_ctr *owner = nullptr;
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
	if (tstring_empty(annotation)) return nullptr;
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
