/**
 * @file type_bridge.c
 * @brief Converts between static Type IR and runtime Type objects.
 * @details Preserves recursive Types, templates, placeholders, constraints,
 * and exact scalar values across compiler metadata and module boundaries.
 * @note This is a representation bridge only; it must not encode knowledge of
 * package-owned object implementations or specific package template names.
 */
#include "internal.h"
#include "tapas/dsa/tstring.h"
#include "tapas/objects/tdict.h"
#include "tapas/objects/tstr.h"
#include "compile/types/static_type.h"
#include "compile/frontend/module.h"

#include <stdlib.h>
#include <string.h>

static tstring *exact_literal(const tobj *value)
{
	tstring *literal = tstring_new_empty();
	switch (value->type) {
	case tnil: tstring_append(literal, "nil"); break;
	case tbool: tstring_append(literal, value->val.v_tbool ? "true" : "false"); break;
	case tint: tstring_append_fmt(literal, "%ld", value->val.v_tint); break;
	case tfloat: tstring_append_fmt(literal, "%.17g", value->val.v_tfloat); break;
	case tcompo: {
		tstring_append_c(literal, '\'');
		const tstring *text = ((const tstr *)value->val.v_tcompo)->data;
		for (size_t i = 0; i < tstring_len(text); i++) {
			char c = tstring_at(text, i);
			if (c == '\\' || c == '\'') tstring_append_c(literal, '\\');
			tstring_append_c(literal, c);
		}
		tstring_append_c(literal, '\'');
	} break;
	}
	return literal;
}

static int exact_object(const char *literal, tobj *value)
{
	tobj_set_nil(value);
	if (strcmp(literal, "nil") == 0) return 1;
	if (strcmp(literal, "true") == 0 || strcmp(literal, "false") == 0) {
		tobj_set_bool(value, literal[0] == 't');
		return 1;
	}
	size_t length = strlen(literal);
	if (length >= 2 && (literal[0] == '\'' || literal[0] == '"') &&
	    literal[length - 1] == literal[0]) {
		tstring *text = tstring_new_empty();
		for (size_t i = 1; i + 1 < length; i++) {
			char c = literal[i];
			if (c == '\\' && i + 2 < length) {
				c = literal[++i];
				c = c == 'n' ? '\n' : c == 'r' ? '\r' :
					c == 't' ? '\t' : c;
			}
			tstring_append_c(text, c);
		}
		tobj_set_compo(value, (tcompo_v *)tstr_new(tstring_cstr(text)));
		tstring_free(text);
		return 1;
	}
	char *end = nullptr;
	if (strchr(literal, '.') || strchr(literal, 'e') || strchr(literal, 'E')) {
		double number = strtod(literal, &end);
		if (end && *end == 0) { tobj_set_float(value, number); return 1; }
	} else {
		long number = strtol(literal, &end, 10);
		if (end && *end == 0) { tobj_set_int(value, number); return 1; }
	}
	return 0;
}

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
		return tstatic_type_builtin_id(context->arena, tbuiltintype_any);
	if (type->kind == ttype_kind_builtin)
		return tstatic_type_builtin_id(
			context->arena, (tbuiltintype_id)type->builtin);
	if (type->kind == ttype_kind_named) {
		if (!type->named_parameter_count)
			return tstatic_type_make_named(context->arena,
				tstring_cstr(type->named_identity));
		tstatic_field *parameters = calloc(type->named_parameter_count,
			sizeof(*parameters));
		if (!parameters) abort();
		for (uint_objs i = 0; i < type->named_parameter_count; i++) {
			parameters[i].name = type->named_parameter_names[i];
			parameters[i].type = runtime_to_static(context,
				type->named_parameter_types[i]);
		}
		tstatic_type_id result = tstatic_type_make_named_application(
			context->arena, tstring_cstr(type->named_identity), parameters,
			type->named_parameter_count, type->capabilities);
		free(parameters);
		return result;
	}
	if (type->kind == ttype_kind_parameter ||
	    type->kind == ttype_kind_value_parameter)
		return tstatic_type_make_parameter(context->arena,
			tstring_cstr(type->parameter_name),
			type->kind == ttype_kind_value_parameter);
	if (type->kind == ttype_kind_exact_value) {
		const tobj *value = ttypeval_exact_value(type);
		tstring *literal = exact_literal(value);
		tstatic_type_id underlying = runtime_to_static(context,
			ttypeval_base(type));
		tstatic_type_id result = tstatic_type_make_exact_literal(context->arena,
			tstring_cstr(literal), underlying);
		tstring_free(literal);
		return result;
	}
	if (type->kind == ttype_kind_template) {
		uint32_t type_count = (uint32_t)type->template_type_parameter_count;
		uint32_t value_count = (uint32_t)type->template_value_parameter_count;
		tstatic_field *parameters = calloc(type_count + value_count,
			sizeof(*parameters));
		if (!parameters) abort();
		for (uint32_t i = 0; i < type_count; i++) {
			parameters[i].name = type->template_type_parameters[i];
			parameters[i].type = tbuiltintype_any;
		}
		for (uint32_t i = 0; i < value_count; i++) {
			parameters[type_count + i].name =
				type->template_value_parameters[i].name;
			parameters[type_count + i].type = runtime_to_static(context,
				type->template_value_parameters[i].type);
		}
		tstatic_type_id body = runtime_to_static(context, type->template_body);
		tstatic_type_id result = tstatic_type_make_template(context->arena,
			parameters, type_count, value_count, body);
		free(parameters);
		return result;
	}
	if (type->kind == ttype_kind_enum) {
		uint32_t count = (uint32_t)ttypeval_enum_member_count(type);
		tstring **members = (tstring **)calloc(count, sizeof(*members));
		if (!members) abort();
		for (uint32_t i = 0; i < count; i++)
			members[i] = (tstring *)ttypeval_enum_member_at(type, i);
		tstatic_type_id result = tstatic_type_make_enum(
			context->arena, members, count);
		free(members);
		return result;
	}
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
	    type->kind == ttype_kind_rule_instance || type->kind == ttype_kind_instance_of) {
		uint32_t count = ttypeval_function_parameter_count(type);
		tstatic_type_id *children = count ?
			(tstatic_type_id *)calloc(count, sizeof(*children)) : nullptr;
		if (count && !children) abort();
		for (uint32_t i = 0; i < count; i++)
			children[i] = runtime_to_static(context,
				ttypeval_function_parameter_at(type, i));
		tstatic_type_id result = tstatic_type_make(context->arena,
			type->kind == ttype_kind_rule ? tstatic_type_rule :
				type->instance_reference ? tstatic_type_instance_of :
				tstatic_type_rule_instance, children, count, 0);
		if (type->instance_reference)
			context->arena->types[result].value_reference =
				tstring_dup(type->instance_reference);
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
		return ttypeval_retain(ttypeval_builtin((tbuiltintype_id)type->builtin));
	if (type->kind == tstatic_type_named) {
		if (!type->field_count)
			return ttypeval_retain(ttypeval_new_named(
				tstring_cstr(type->value_reference)));
		const tstatic_field *items = tstatic_type_field_items(context->arena,
			type);
		ttype_field *parameters = calloc(type->field_count,
			sizeof(*parameters));
		if (!parameters) abort();
		for (uint32_t i = 0; i < type->field_count; i++) {
			parameters[i].name = items[i].name;
			parameters[i].type = static_to_runtime(context, items[i].type);
		}
		ttypeval *result = ttypeval_new_named_application(
			tstring_cstr(type->value_reference), parameters,
			type->field_count, type->capabilities);
		for (uint32_t i = 0; i < type->field_count; i++)
			ttypeval_release(parameters[i].type);
		free(parameters);
		return ttypeval_retain(result);
	}
	if (type->kind == tstatic_type_parameter ||
	    type->kind == tstatic_type_value_parameter) {
		ttypeval *result = type->kind == tstatic_type_parameter ?
			ttypeval_new_parameter(tstring_cstr(type->value_reference)) :
			ttypeval_new_value_parameter(tstring_cstr(type->value_reference));
		return ttypeval_retain(result);
	}
	if (type->kind == tstatic_type_exact_value) {
		tobj value;
		if (!exact_object(tstring_cstr(type->value_reference), &value))
			return nullptr;
		ttypeval *result = ttypeval_new_exact_value(&value);
		tobj_try_clear(&value);
		return ttypeval_retain(result);
	}
	if (type->kind == tstatic_type_template) {
		const tstatic_field *specs = tstatic_type_field_items(context->arena,
			type);
		uint32_t type_count = type->template_type_parameter_count;
		uint32_t value_count = type->field_count - type_count;
		const tstring **type_parameters = type_count ? calloc(type_count,
			sizeof(*type_parameters)) : nullptr;
		ttype_value_parameter *value_parameters = value_count ? calloc(
			value_count, sizeof(*value_parameters)) : nullptr;
		if ((type_count && !type_parameters) ||
		    (value_count && !value_parameters)) abort();
		for (uint32_t i = 0; i < type_count; i++)
			type_parameters[i] = specs[i].name;
		for (uint32_t i = 0; i < value_count; i++) {
			value_parameters[i].name = specs[type_count + i].name;
			value_parameters[i].type = static_to_runtime(context,
				specs[type_count + i].type);
		}
		const tstatic_type_id *body_id = tstatic_type_children(context->arena,
			type);
		ttypeval *body = static_to_runtime(context, body_id[0]);
		ttypeval *result = body ? ttypeval_new_template(type_parameters,
			type_count, value_parameters, value_count, body) : nullptr;
		for (uint32_t i = 0; i < value_count; i++)
			ttypeval_release(value_parameters[i].type);
		ttypeval_release(body);
		free(type_parameters);
		free(value_parameters);
		return ttypeval_retain(result);
	}
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
	/* Imported annotations may still be unknown in the frontend snapshot.
	 * Unlike Function, Rule signatures require every parameter to be known. */
	if (type->kind == tstatic_type_rule ||
	    type->kind == tstatic_type_rule_instance) {
		for (uint32_t i = 0; i < type->child_count; i++) {
			if (children[i]) continue;
			for (uint32_t j = 0; j < type->child_count; j++)
				ttypeval_release(children[j]);
			free(children);
			return nullptr;
		}
	}
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
	else if (type->kind == tstatic_type_instance_of)
		result = ttypeval_new_instance_reference(tstring_cstr(type->value_reference), children, type->child_count);
	else if (type->kind == tstatic_type_enum) {
		const tstatic_field *source = tstatic_type_field_items(
			context->arena, type);
		const tstring **members = (const tstring **)calloc(
			type->field_count, sizeof(*members));
		if (!members) abort();
		for (uint32_t i = 0; i < type->field_count; i++)
			members[i] = source[i].name;
		result = ttypeval_new_enum(members, type->field_count);
		free(members);
	}
	else if (type->kind == tstatic_type_fields) {
		const tstatic_field *source = tstatic_type_field_items(
			context->arena, type);
		ttype_field *fields = (ttype_field *)calloc(
			type->field_count, sizeof(*fields));
		if (!fields) abort();
		for (uint32_t i = 0; i < type->field_count; i++) {
			fields[i].name = source[i].name;
			fields[i].type = static_to_runtime(context, source[i].type);
            /* Unknown fields stay dynamically checked at runtime, like unknown function parameters. */
            if (!fields[i].type) fields[i].type = ttypeval_retain(ttypeval_builtin(tbuiltintype_any));
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
					ttypeval_builtin(tbuiltintype_type);
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
            tstatic_type_id declared = tstandard_type_resolve(arena,qualified_name,1);
            if (declared != TSTATIC_TYPE_UNKNOWN) {
                tobj_try_clear(&member_value);
                return declared;
            }
			to_static_context context = { .cp = cp, .arena = arena };
			tstatic_type_id result = runtime_to_static(
				&context, (ttypeval *)value->val.v_tcompo);
			free(context.runtime);
			free(context.static_ids);
			tobj_try_clear(&member_value);
			return result;
		}
        if (!static_value && value && value->type == tcompo && value->val.v_tcompo &&
            value->val.v_tcompo->vtable->get_compo_type_code() == compo_trule_builtin) {
            tstatic_type_id result = tstandard_type_resolve(arena,qualified_name,0);
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
        external_binding_type(context->cp, context->arena, name, 1);
}

static tstatic_type_id resolve_annotation_value(void *data, const char *name)
{
	to_static_context *context = data;
	/* Imported and nested members are checked against their runtime values. */
	if (strstr(name, "::")) return TSTATIC_TYPE_UNKNOWN;
	return external_binding_type(context->cp, context->arena, name, 0);
}

ttypeval *compile_resolve_annotation(tcp *cp, const tstring *annotation)
{
	if (tstring_empty(annotation)) return nullptr;
	tstatic_type_arena arena;
	tstatic_type_arena_init(&arena);
	to_static_context context = { .cp = cp, .arena = &arena };
	tstatic_type_id id = tstatic_type_parse_with_values(&arena,
		tstring_cstr(annotation), resolve_annotation_name, resolve_annotation_value, &context);
	ttypeval *result = compile_type_from_static(&arena, id);
	free(context.runtime);
	free(context.static_ids);
	tstatic_type_arena_free(&arena);
	if (!result)
		twarn(ErrCompile_Other, "type annotation", tstring_cstr(annotation));
	return result;
}
