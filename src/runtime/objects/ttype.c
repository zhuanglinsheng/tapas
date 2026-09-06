/**
 * @file ttype.c
 * @brief Implements the core Type object.
 * @details Defines type construction, canonicalization, matching, ownership,
 * formatting, capabilities, and vtable registration.
 * @note This file describes language-level types only; package-owned types
 * must be implemented and registered by their packages.
 */
#include "tapas/objects/ttype.h"
#include "tapas/dsa/tstring.h"
#include "tapas/textension.h"

#include "tapas/dsa/thashtbl.h"
#include "tapas/tformat.h"

#include "tapas/objects/tdict.h"
#include "tapas/objects/tlist.h"
#include "tapas/objects/tpair.h"
#include "tapas/objects/tstr.h"
#include "tapas/objects/trule.h"
#include "tapas/objects/trule_ir.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
	const tobj *key;
	const tobj *value;
} ttype_entry;

static ttypeval *builtin_types[tbuiltintype_count];
static int builtins_initializing;
static uint32_t next_recursive_id;
static void ttypeval_free(void *self);

static int ttype_is_value(const tobj *value)
{
	return value && value->type == tcompo && value->val.v_tcompo &&
	       value->val.v_tcompo->vtable &&
	       value->val.v_tcompo->vtable->get_compo_type_code() ==
		       compo_ttypeval;
}

static ttypeval *ttype_new(ttype_kind kind)
{
	ttypeval *type = (ttypeval *)calloc(1, sizeof(ttypeval));
	if (!type)
		twarn(ErrRuntime_Other, "ttype_new", "out of memory");
	type->base.vtable = &ttypeval_vtable;
	type->kind = kind;
	type->definition = thashtbl_new();
	return type;
}

static void ttype_include(ttypeval *owner, const ttypeval *child)
{
	if (!owner || !child) return;
	owner->contains_instance |= child->contains_instance;
	owner->contains_custom |= child->contains_custom;
	owner->contains_recursive |= child->contains_recursive;
}

static void ttype_set_definition(ttypeval *owner,
				 const char *name,
				 ttypeval *value)
{
	ttype_include(owner, value);
	tobj key;
	tobj val;
	tobj_set_nil(&key);
	tobj_set_nil(&val);
	tobj_set_compo(&key, (tcompo_v *)tstr_new(name));
	tobj_set_compo(&val, (tcompo_v *)value);
	thashtbl_set(owner->definition, &key, &val);
	tobj_try_clear(&key);
	tobj_set_nil(&val);
}

static const tobj *ttype_definition_get(const ttypeval *type,
					const char *name)
{
	tobj key;
	tobj_set_nil(&key);
	tobj_set_compo(&key, (tcompo_v *)tstr_new(name));
	const tobj *value = thashtbl_get(type->definition, &key);
	tobj_try_clear(&key);
	return value;
}

static ttypeval *ttype_definition_get_type(const ttypeval *type,
					   const char *name)
{
	const tobj *value = ttype_definition_get(type, name);
	return ttype_is_value(value) ? (ttypeval *)value->val.v_tcompo : nullptr;
}

static void ttype_collect_entry(const tobj *key,
				const tobj *value,
				void *context)
{
	ttype_entry **cursor = (ttype_entry **)context;
	(*cursor)->key = key;
	(*cursor)->value = value;
	(*cursor)++;
}

static int ttype_compare_entry(const void *left, const void *right)
{
	const ttype_entry *a = (const ttype_entry *)left;
	const ttype_entry *b = (const ttype_entry *)right;
	const tstr *ak = (const tstr *)a->key->val.v_tcompo;
	const tstr *bk = (const tstr *)b->key->val.v_tcompo;
	return tstring_cmp(ak->data, bk->data);
}

static int ttype_compare_string_ptr(const void *left, const void *right)
{
	const tstring *a = *(const tstring *const *)left;
	const tstring *b = *(const tstring *const *)right;
	return tstring_cmp(a, b);
}

static ttype_entry *ttype_sorted_entries(const ttypeval *type,
					 uint_objs *count)
{
	*count = thashtbl_len(type->definition);
	if (*count == 0)
		return nullptr;
	ttype_entry *entries =
		(ttype_entry *)calloc(*count, sizeof(ttype_entry));
	if (!entries)
		twarn(ErrRuntime_Other, "ttype_sorted_entries", "out of memory");
	ttype_entry *cursor = entries;
	thashtbl_each(type->definition, ttype_collect_entry, &cursor);
	qsort(entries, *count, sizeof(ttype_entry), ttype_compare_entry);
	return entries;
}

static void ttype_append_canonical(tstring *out, const ttypeval *type)
{
	tstring_append_fmt(out, "%zu:", tstring_len(type->canonical));
	tstring_append_ts(out, type->canonical);
}

static int ttype_contains_recursive(const ttypeval *type)
{
	return type && (type->kind == ttype_kind_recursive ||
		type->contains_recursive);
}

static void ttype_finish_canonical(ttypeval *type, tstring *canonical)
{
	type->canonical = canonical;
	type->canonical_hash = tstring_hash(canonical);
}

static void ttype_build_fields_canonical(ttypeval *type)
{
	uint_objs count;
	ttype_entry *entries = ttype_sorted_entries(type, &count);
	tstring *canonical = tstring_new("F");
	tstring_append_fmt(canonical, "%u:", (unsigned)count);
	for (uint_objs i = 0; i < count; i++) {
		const tstr *key = (const tstr *)entries[i].key->val.v_tcompo;
		const ttypeval *field =
			(const ttypeval *)entries[i].value->val.v_tcompo;
		tstring_append_c(canonical,
			ttypeval_field_optional(type, tstring_cstr(key->data)) ? '?' : '!');
		tstring_append_fmt(canonical, "%zu:", tstring_len(key->data));
		tstring_append_ts(canonical, key->data);
		ttype_append_canonical(canonical, field);
	}
	free(entries);
	ttype_finish_canonical(type, canonical);
}

static void ttype_init_builtins(void)
{
	if (builtin_types[tbuiltintype_any] || builtins_initializing)
		return;
	builtins_initializing = 1;

	ttypeval *any = ttype_new(ttype_kind_any);
	any->builtin = tbuiltintype_any;
	any->base.refctr = 1; /* Process-wide immutable singleton. */
	ttype_finish_canonical(any, tstring_new("A"));
	builtin_types[tbuiltintype_any] = any;

	for (int i = tbuiltintype_nil; i < tbuiltintype_count; i++) {
		ttypeval *type = ttype_new(ttype_kind_builtin);
		type->builtin = (tbuiltintype_id)i;
		type->base.refctr = 1;
		const char *name = tbuiltintype_name((tbuiltintype_id)i);
		char definition_key[64];
		snprintf(definition_key, sizeof(definition_key), "@builtin/%s",
			 name);
		ttype_set_definition(type, definition_key, any);
		tstring *canonical = tstring_new("B");
		tstring_append_fmt(canonical, "%zu:", strlen(name));
		tstring_append(canonical, name);
		ttype_finish_canonical(type, canonical);
		builtin_types[i] = type;
	}
	builtins_initializing = 0;
}

ttypeval *ttypeval_builtin(tbuiltintype_id builtin)
{
	ttype_init_builtins();
	if (builtin < 0 || builtin >= tbuiltintype_count)
		return nullptr;
	return builtin_types[builtin];
}

ttypeval *ttypeval_builtin_named(const char *name)
{
	for (int i = 0; i < tbuiltintype_count; i++)
		if (strcmp(name, tbuiltintype_name((tbuiltintype_id)i)) == 0)
			return ttypeval_builtin((tbuiltintype_id)i);
	return nullptr;
}

static int canonical_number(const char **cursor, uint_objs *value)
{
	uint_objs number = 0;
	if (**cursor < '0' || **cursor > '9') return 0;
	while (**cursor >= '0' && **cursor <= '9') {
		number = number * 10 + (uint_objs)(*(*cursor)++ - '0');
	}
	if (*(*cursor)++ != ':') return 0;
	*value = number;
	return 1;
}

static tstring *canonical_string(const char **cursor)
{
	uint_objs length;
	if (!canonical_number(cursor, &length) || strlen(*cursor) < length)
		return nullptr;
	tstring *value = tstring_new_len(*cursor, length);
	*cursor += length;
	return value;
}

static ttypeval *canonical_wrapped(const char **cursor)
{
	uint_objs length;
	if (!canonical_number(cursor, &length) || strlen(*cursor) < length)
		return nullptr;
	char *text = calloc(length + 1, 1);
	if (!text) abort();
	memcpy(text, *cursor, length);
	*cursor += length;
	ttypeval *type = ttypeval_from_canonical(text);
	free(text);
	return type;
}

ttypeval *ttypeval_from_canonical(const char *canonical)
{
	if (!canonical || !*canonical) return nullptr;
	if (strcmp(canonical, "A") == 0) return ttypeval_builtin(tbuiltintype_any);
	const char *cursor = canonical;
	if (*cursor == 'N') {
		cursor++;
		tstring *name = canonical_string(&cursor);
		if (!name) return nullptr;
		ttypeval *signature = ttypeval_retain(canonical_wrapped(&cursor));
		if (!signature || *cursor || signature->kind != ttype_kind_rule_instance) {
			tstring_free(name); ttypeval_release(signature); return nullptr;
		}
		uint_objs count = ttypeval_function_parameter_count(signature);
		ttypeval **parameters = count ? calloc(count, sizeof(*parameters)) : nullptr;
		if (count && !parameters) abort();
		for (uint_objs i = 0; i < count; i++) parameters[i] = ttypeval_function_parameter_at(signature, i);
		ttypeval *result = ttypeval_new_instance_reference(tstring_cstr(name), parameters, count);
		free(parameters); ttypeval_release(signature); tstring_free(name);
		return result;
	}
	if (*cursor == 'B') {
		cursor++;
		tstring *name = canonical_string(&cursor);
		if (!name || *cursor) { tstring_free(name); return nullptr; }
		ttypeval *type = ttypeval_builtin_named(tstring_cstr(name));
		tstring_free(name);
		return type;
	}
	if (*cursor == 'X') {
		cursor++;
		tstring *name = canonical_string(&cursor);
		if (!name || *cursor) { tstring_free(name); return nullptr; }
		ttypeval *type = ttypeval_new_named(tstring_cstr(name));
		tstring_free(name);
		return type;
	}
	if (*cursor == 'K') {
		cursor++;
		tstring *name = canonical_string(&cursor);
		uint_objs count;
		uint_objs capabilities;
		if (!name || !canonical_number(&cursor, &count) || !count ||
		    !canonical_number(&cursor, &capabilities)) {
			tstring_free(name);
			return nullptr;
		}
		ttype_field *parameters = calloc(count, sizeof(*parameters));
		if (!parameters) abort();
		for (uint_objs i = 0; i < count; i++) {
			parameters[i].name = canonical_string(&cursor);
			parameters[i].type = canonical_wrapped(&cursor);
			if (!parameters[i].name || !parameters[i].type) goto named_invalid;
		}
		if (*cursor) goto named_invalid;
		ttypeval *type = ttypeval_new_named_application(
			tstring_cstr(name), parameters, count, capabilities);
		for (uint_objs i = 0; i < count; i++)
			tstring_free((tstring *)parameters[i].name);
		free(parameters);
		tstring_free(name);
		return type;
	named_invalid:
		for (uint_objs i = 0; i < count; i++)
			tstring_free((tstring *)parameters[i].name);
		free(parameters);
		tstring_free(name);
		return nullptr;
	}
	if (*cursor == 'Y' || *cursor == 'V') {
		char tag = *cursor++;
		tstring *name = canonical_string(&cursor);
		if (!name || *cursor) { tstring_free(name); return nullptr; }
		ttypeval *type = tag == 'Y' ?
			ttypeval_new_parameter(tstring_cstr(name)) :
			ttypeval_new_value_parameter(tstring_cstr(name));
		tstring_free(name);
		return type;
	}
	if (*cursor == 'C') {
		cursor++;
		tobj value;
		tobj_set_nil(&value);
		char *end = nullptr;
		if (*cursor == 'n' && !cursor[1]) {
			cursor++;
		} else if (*cursor == 'b' && (cursor[1] == '0' || cursor[1] == '1') &&
			   !cursor[2]) {
			tobj_set_bool(&value, cursor[1] == '1');
			cursor += 2;
		} else if (*cursor == 'i') {
			long number = strtol(cursor + 1, &end, 10);
			if (!end || *end) return nullptr;
			tobj_set_int(&value, number);
			cursor = end;
		} else if (*cursor == 'f') {
			double number = strtod(cursor + 1, &end);
			if (!end || *end) return nullptr;
			tobj_set_float(&value, number);
			cursor = end;
		} else if (*cursor == 's') {
			cursor++;
			tstring *text = canonical_string(&cursor);
			if (!text || *cursor) { tstring_free(text); return nullptr; }
			tobj_set_compo(&value, (tcompo_v *)tstr_new(tstring_cstr(text)));
			tstring_free(text);
		} else return nullptr;
		ttypeval *type = ttypeval_new_exact_value(&value);
		tobj_try_clear(&value);
		return type;
	}
	if (*cursor == 'G') {
		cursor++;
		uint_objs type_count, value_count;
		if (!canonical_number(&cursor, &type_count)) return nullptr;
		tstring **type_names = type_count ? calloc(type_count,
			sizeof(*type_names)) : nullptr;
		if (type_count && !type_names) abort();
		for (uint_objs i = 0; i < type_count; i++) {
			type_names[i] = canonical_string(&cursor);
			if (!type_names[i]) goto template_invalid;
		}
		if (!canonical_number(&cursor, &value_count)) goto template_invalid;
		ttype_value_parameter *values = value_count ? calloc(value_count,
			sizeof(*values)) : nullptr;
		if (value_count && !values) abort();
		for (uint_objs i = 0; i < value_count; i++) {
			values[i].name = canonical_string(&cursor);
			if (!values[i].name) goto template_values_invalid;
			values[i].type = canonical_wrapped(&cursor);
			if (!values[i].type) goto template_values_invalid;
		}
		ttypeval *body = canonical_wrapped(&cursor);
		if (!body || *cursor) goto template_values_invalid;
		ttypeval *type = ttypeval_new_template(
			(const tstring *const *)type_names, type_count, values,
			value_count, body);
		for (uint_objs i = 0; i < type_count; i++) tstring_free(type_names[i]);
		for (uint_objs i = 0; i < value_count; i++)
			tstring_free((tstring *)values[i].name);
		free(type_names);
		free(values);
		return type;
	template_values_invalid:
		for (uint_objs i = 0; i < value_count; i++)
			tstring_free((tstring *)values[i].name);
		free(values);
	template_invalid:
		for (uint_objs i = 0; i < type_count; i++) tstring_free(type_names[i]);
		free(type_names);
		return nullptr;
	}
	if (*cursor == 'L' || *cursor == 'I' || *cursor == 'Z') {
		char tag = *cursor++;
		ttypeval *item = canonical_wrapped(&cursor);
		if (!item || *cursor) return nullptr;
		return tag == 'L' ? ttypeval_new_list(item) :
			tag == 'I' ? ttypeval_new_iterator(item) :
			ttypeval_new_rule_term(item);
	}
	if (strncmp(cursor, "FF", 2) == 0 || strncmp(cursor, "FV", 2) == 0) {
		int variadic = cursor[1] == 'V';
		cursor += 2;
		uint_objs count;
		if (!canonical_number(&cursor, &count)) return nullptr;
		ttypeval **parameters = count ? calloc(count, sizeof(*parameters)) : nullptr;
		for (uint_objs i = 0; i < count; i++) {
			if (*cursor == '?') { cursor++; continue; }
			if (*cursor++ != 'T' ||
			    !(parameters[i] = canonical_wrapped(&cursor))) {
				free(parameters); return nullptr;
			}
		}
		ttypeval *result = nullptr;
		if (*cursor == '?') cursor++;
		else if (*cursor++ != 'R' || !(result = canonical_wrapped(&cursor))) {
			free(parameters); return nullptr;
		}
		if (*cursor) { free(parameters); return nullptr; }
		ttypeval *type = ttypeval_new_function(parameters, count, result, variadic);
		free(parameters);
		return type;
	}
	if (*cursor == 'F') {
		cursor++;
		uint_objs count;
		if (!canonical_number(&cursor, &count) || count == 0) return nullptr;
		ttype_field *fields = calloc(count, sizeof(*fields));
		tstring **names = calloc(count, sizeof(*names));
		for (uint_objs i = 0; i < count; i++) {
			fields[i].optional = *cursor == '?';
			if (*cursor != '?' && *cursor != '!') goto fields_invalid;
			cursor++;
			uint_objs name_length;
			if (!canonical_number(&cursor, &name_length) ||
			    strlen(cursor) < name_length) goto fields_invalid;
			char *name = calloc(name_length + 1, 1);
			memcpy(name, cursor, name_length);
			cursor += name_length;
			names[i] = tstring_new(name);
			free(name);
			fields[i].name = names[i];
			if (!(fields[i].type = canonical_wrapped(&cursor)))
				goto fields_invalid;
		}
		if (*cursor) goto fields_invalid;
		ttypeval *type = ttypeval_new_fields(fields, count);
		for (uint_objs i = 0; i < count; i++) tstring_free(names[i]);
		free(names); free(fields);
		return type;
	fields_invalid:
		for (uint_objs i = 0; i < count; i++) tstring_free(names[i]);
		free(names); free(fields);
		return nullptr;
	}
	if (*cursor == 'E') {
		cursor++;
		uint_objs count;
		if (!canonical_number(&cursor, &count) || count == 0) return nullptr;
		tstring **members = calloc(count, sizeof(*members));
		if (!members) abort();
		for (uint_objs i = 0; i < count; i++) {
			members[i] = canonical_string(&cursor);
			if (!members[i]) goto enum_invalid;
		}
		if (*cursor) goto enum_invalid;
		ttypeval *type = ttypeval_new_enum(
			(const tstring *const *)members, count);
		for (uint_objs i = 0; i < count; i++) tstring_free(members[i]);
		free(members);
		return type;
	enum_invalid:
		for (uint_objs i = 0; i < count; i++) tstring_free(members[i]);
		free(members);
		return nullptr;
	}
	if (*cursor == 'M') {
		cursor++;
		uint_objs ignored;
		if (!canonical_number(&cursor, &ignored)) return nullptr;
		ttypeval *body = canonical_wrapped(&cursor);
		if (!body || *cursor) return nullptr;
		ttypeval *recursive = ttypeval_new_recursive();
		if (!ttypeval_define_recursive(recursive, body)) return nullptr;
		return recursive;
	}
	if (*cursor == 'P' || *cursor == 'D') {
		char tag = *cursor++;
		ttypeval *first = canonical_wrapped(&cursor);
		ttypeval *second = canonical_wrapped(&cursor);
		if (!first || !second || *cursor) return nullptr;
		return tag == 'P' ? ttypeval_new_pair(first, second) :
			ttypeval_new_dictionary(first, second);
	}
	int rule_instance = strncmp(cursor, "QI", 2) == 0;
	if (*cursor == 'Q') {
		cursor += rule_instance ? 2 : 1;
		uint_objs count;
		if (!canonical_number(&cursor, &count)) return nullptr;
		ttypeval **parameters = count ? calloc(count, sizeof(*parameters)) : nullptr;
		for (uint_objs i = 0; i < count; i++)
			if (!(parameters[i] = canonical_wrapped(&cursor))) {
				free(parameters);
				return nullptr;
			}
		if (*cursor) { free(parameters); return nullptr; }
		ttypeval *type = rule_instance ?
			ttypeval_new_rule_instance(parameters, count) :
			ttypeval_new_rule(parameters, count);
		free(parameters);
		return type;
	}
	if (*cursor == 'U') {
		cursor++;
		uint_objs count;
		if (!canonical_number(&cursor, &count) || count < 2) return nullptr;
		ttypeval **members = calloc(count, sizeof(*members));
		for (uint_objs i = 0; i < count; i++)
			if (!(members[i] = canonical_wrapped(&cursor))) {
				free(members);
				return nullptr;
			}
		if (*cursor) { free(members); return nullptr; }
		ttypeval *type = ttypeval_new_union(members, count);
		free(members);
		return type;
	}
	return nullptr;
}

ttypeval *ttypeval_retain(ttypeval *type)
{
	if (type)
		type->base.refctr++;
	return type;
}

void ttypeval_release(ttypeval *type)
{
	if (!type)
		return;
	/* Builtin Types are process-wide canonical singletons. Their baseline
	 * reference is a pin, not a caller-owned reference. */
	if ((type->kind == ttype_kind_any || type->kind == ttype_kind_builtin) &&
	    type->builtin >= 0 && type->builtin < tbuiltintype_count &&
	    builtin_types[type->builtin] == type && type->base.refctr <= 1)
		return;
	if (type->base.refctr > 0)
		type->base.refctr--;
	if (type->base.refctr == 0)
		ttypeval_free(type);
}

ttypeval *ttypeval_new_named(const char *qualified_name)
{
	if (!qualified_name || !*qualified_name || !strstr(qualified_name, "::"))
		twarn(ErrRuntime_ParamsType, "package Type",
		      "a qualified Type name is required");
	ttypeval *type = ttype_new(ttype_kind_named);
	type->named_identity = tstring_new(qualified_name);
	tstring *canonical = tstring_new("X");
	tstring_append_fmt(canonical, "%zu:", strlen(qualified_name));
	tstring_append(canonical, qualified_name);
	ttype_finish_canonical(type, canonical);
	return type;
}

ttypeval *ttypeval_new_named_application(const char *qualified_name,
	const ttype_field *parameters, uint_objs parameter_count,
	uint32_t capabilities)
{
	if (!qualified_name || !*qualified_name || !strstr(qualified_name, "::") ||
	    !parameters || !parameter_count)
		twarn(ErrRuntime_ParamsType, "package Type",
		      "a qualified name and Type parameters are required");
	ttypeval *type = ttype_new(ttype_kind_named);
	type->named_identity = tstring_new(qualified_name);
	type->named_parameter_count = parameter_count;
	type->capabilities = capabilities;
	type->named_parameter_names = calloc(parameter_count,
		sizeof(*type->named_parameter_names));
	type->named_parameter_types = calloc(parameter_count,
		sizeof(*type->named_parameter_types));
	if (!type->named_parameter_names || !type->named_parameter_types) abort();
	ttypeval *base = ttypeval_new_named(qualified_name);
	ttype_set_definition(type, "@base", base);
	tstring *canonical = tstring_new("K");
	tstring_append_fmt(canonical, "%zu:", strlen(qualified_name));
	tstring_append(canonical, qualified_name);
	tstring_append_fmt(canonical, "%u:", (unsigned)parameter_count);
	tstring_append_fmt(canonical, "%u:", (unsigned)capabilities);
	for (uint_objs i = 0; i < parameter_count; i++) {
		const char *name = parameters[i].name ?
			tstring_cstr(parameters[i].name) : nullptr;
		if (!name || !*name || !parameters[i].type)
			twarn(ErrRuntime_ParamsType, "package Type",
			      "named Type parameter requires a name and Type");
		for (uint_objs j = 0; j < i; j++)
			if (tstring_eq_cstr(type->named_parameter_names[j], name))
				twarn(ErrRuntime_ParamsType, "package Type",
				      "duplicate named Type parameter");
		type->named_parameter_names[i] = tstring_new(name);
		tstring *definition_name = tstring_new("@");
		tstring_append(definition_name, name);
		ttype_set_definition(type, tstring_cstr(definition_name),
			parameters[i].type);
		tstring_free(definition_name);
		type->named_parameter_types[i] = parameters[i].type;
		tstring_append_fmt(canonical, "%zu:", strlen(name));
		tstring_append(canonical, name);
		ttype_append_canonical(canonical, parameters[i].type);
	}
	type->contains_custom = 1;
	ttype_finish_canonical(type, canonical);
	return type;
}

static ttypeval *extension_constraint(const char *name)
{
	if (!name || !*name) return nullptr;
	ttypeval *builtin = ttypeval_builtin_named(name);
	return builtin ? builtin : strstr(name, "::") ?
		ttypeval_new_named(name) : nullptr;
}

ttypeval *ttypeval_new_extension_template(
	const textension_nominal_template *schema)
{
	if (!schema || !schema->identity || !strstr(schema->identity, "::") ||
	    (!schema->type_parameter_count && !schema->value_parameter_count))
		return nullptr;
	uint_objs count = schema->type_parameter_count +
		schema->value_parameter_count;
	ttype_field *body_parameters = calloc(count, sizeof(*body_parameters));
	const tstring **type_names = schema->type_parameter_count ? calloc(
		schema->type_parameter_count, sizeof(*type_names)) : nullptr;
	ttype_value_parameter *value_parameters = schema->value_parameter_count ?
		calloc(schema->value_parameter_count, sizeof(*value_parameters)) : nullptr;
	if (!body_parameters || (schema->type_parameter_count && !type_names) ||
	    (schema->value_parameter_count && !value_parameters)) abort();
	for (uint_objs i = 0; i < schema->type_parameter_count; i++) {
		const textension_template_type_parameter *parameter =
			&schema->type_parameters[i];
		if (!parameter->name || !*parameter->name ||
		    !parameter->slot || !*parameter->slot) goto invalid;
		ttypeval *placeholder = ttypeval_new_parameter(parameter->name);
		type_names[i] = placeholder->parameter_name;
		body_parameters[i] = (ttype_field){
			.name = tstring_new(parameter->slot), .type = placeholder };
	}
	for (uint_objs i = 0; i < schema->value_parameter_count; i++) {
		const textension_template_value_parameter *parameter =
			&schema->value_parameters[i];
		ttypeval *constraint = extension_constraint(parameter->constraint);
		if (!parameter->name || !*parameter->name ||
		    !parameter->slot || !*parameter->slot || !constraint)
			goto invalid;
		ttypeval *placeholder = ttypeval_new_value_parameter(parameter->name);
		value_parameters[i] = (ttype_value_parameter){
			.name = placeholder->parameter_name, .type = constraint };
		body_parameters[schema->type_parameter_count + i] = (ttype_field){
			.name = tstring_new(parameter->slot), .type = placeholder };
	}
	ttypeval *body = ttypeval_new_named_application(schema->identity,
		body_parameters, count, schema->capabilities);
	ttypeval *result = ttypeval_new_template(type_names,
		schema->type_parameter_count, value_parameters,
		schema->value_parameter_count, body);
	for (uint_objs i = 0; i < count; i++)
		tstring_free((tstring *)body_parameters[i].name);
	free(body_parameters);
	free(type_names);
	free(value_parameters);
	return result;

invalid:
	for (uint_objs i = 0; i < count; i++)
		tstring_free((tstring *)body_parameters[i].name);
	free(body_parameters);
	free(type_names);
	free(value_parameters);
	return nullptr;
}

static int template_parameter_name_valid(const char *name)
{
	if (!name || !*name || !((*name >= 'A' && *name <= 'Z') ||
	    (*name >= 'a' && *name <= 'z') || *name == '_')) return 0;
	for (name++; *name; name++)
		if (!((*name >= 'A' && *name <= 'Z') ||
		      (*name >= 'a' && *name <= 'z') ||
		      (*name >= '0' && *name <= '9') || *name == '_')) return 0;
	return 1;
}

static ttypeval *new_parameter(ttype_kind kind, const char *name, char tag)
{
	if (!template_parameter_name_valid(name))
		twarn(ErrRuntime_ParamsType, "types::parameter",
		      "parameter name must be an identifier");
	ttypeval *type = ttype_new(kind);
	type->parameter_name = tstring_new(name);
	tstring *canonical = tstring_new_empty();
	tstring_append_c(canonical, tag);
	tstring_append_fmt(canonical, "%zu:", strlen(name));
	tstring_append(canonical, name);
	ttype_finish_canonical(type, canonical);
	return type;
}

ttypeval *ttypeval_new_parameter(const char *name)
{
	return new_parameter(ttype_kind_parameter, name, 'Y');
}

ttypeval *ttypeval_new_value_parameter(const char *name)
{
	return new_parameter(ttype_kind_value_parameter, name, 'V');
}

static int template_has_name(const ttypeval *type, const char *name)
{
	for (uint_objs i = 0; i < type->template_type_parameter_count; i++)
		if (tstring_eq_cstr(type->template_type_parameters[i], name)) return 1;
	for (uint_objs i = 0; i < type->template_value_parameter_count; i++)
		if (tstring_eq_cstr(type->template_value_parameters[i].name, name)) return 1;
	return 0;
}

ttypeval *ttypeval_new_template(const tstring *const *type_parameters,
	uint_objs type_parameter_count,
	const ttype_value_parameter *value_parameters,
	uint_objs value_parameter_count, ttypeval *body)
{
	if (!body || (!type_parameter_count && !value_parameter_count))
		twarn(ErrRuntime_ParamsCtr, "types::template",
		      "parameters and a definition are required");
	ttypeval *type = ttype_new(ttype_kind_template);
	type->template_body = ttypeval_retain(body);
	ttype_include(type, body);
	type->template_type_parameter_count = type_parameter_count;
	type->template_value_parameter_count = value_parameter_count;
	if (type_parameter_count) {
		type->template_type_parameters = calloc(type_parameter_count,
			sizeof(*type->template_type_parameters));
		if (!type->template_type_parameters) abort();
	}
	if (value_parameter_count) {
		type->template_value_parameters = calloc(value_parameter_count,
			sizeof(*type->template_value_parameters));
		if (!type->template_value_parameters) abort();
	}
	for (uint_objs i = 0; i < type_parameter_count; i++) {
		const char *name = type_parameters[i] ?
			tstring_cstr(type_parameters[i]) : nullptr;
		if (!template_parameter_name_valid(name) || template_has_name(type, name))
			twarn(ErrRuntime_ParamsType, "types::template",
			      "invalid or duplicate Type parameter");
		type->template_type_parameters[i] = tstring_new(name);
	}
	for (uint_objs i = 0; i < value_parameter_count; i++) {
		const char *name = value_parameters[i].name ?
			tstring_cstr(value_parameters[i].name) : nullptr;
		if (!template_parameter_name_valid(name) || !value_parameters[i].type ||
		    template_has_name(type, name))
			twarn(ErrRuntime_ParamsType, "types::template",
			      "invalid or duplicate Value parameter");
		type->template_value_parameters[i].name = tstring_new(name);
		type->template_value_parameters[i].type =
			ttypeval_retain(value_parameters[i].type);
		ttype_include(type, value_parameters[i].type);
	}
	tstring *canonical = tstring_new("G");
	tstring_append_fmt(canonical, "%u:", (unsigned)type_parameter_count);
	for (uint_objs i = 0; i < type_parameter_count; i++) {
		tstring *name = type->template_type_parameters[i];
		tstring_append_fmt(canonical, "%zu:", tstring_len(name));
		tstring_append_ts(canonical, name);
	}
	tstring_append_fmt(canonical, "%u:", (unsigned)value_parameter_count);
	for (uint_objs i = 0; i < value_parameter_count; i++) {
		tstring *name = type->template_value_parameters[i].name;
		tstring_append_fmt(canonical, "%zu:", tstring_len(name));
		tstring_append_ts(canonical, name);
		ttype_append_canonical(canonical,
			type->template_value_parameters[i].type);
	}
	ttype_append_canonical(canonical, body);
	ttype_finish_canonical(type, canonical);
	return type;
}

int ttypeval_is_template(const ttypeval *type)
{
	return type && type->kind == ttype_kind_template;
}

ttypeval *ttypeval_new_exact_value(const tobj *value)
{
	if (!value || (value->type == tcompo && (!value->val.v_tcompo ||
	    tobj_compo_type(value) != compo_tstr)))
		twarn(ErrRuntime_ParamsType, "Type value parameter",
		      "only Nil, Bool, Int, Float, and String are canonical values");
	ttypeval *type = ttype_new(ttype_kind_exact_value);
	tobj_set_nil(&type->exact_value);
	tobj_copy(&type->exact_value, value);
	tstring *canonical = tstring_new("C");
	switch (value->type) {
	case tnil: tstring_append(canonical, "n"); break;
	case tbool: tstring_append_fmt(canonical, "b%d", value->val.v_tbool); break;
	case tint: tstring_append_fmt(canonical, "i%ld", value->val.v_tint); break;
	case tfloat: tstring_append_fmt(canonical, "f%.17g", value->val.v_tfloat); break;
	case tcompo: {
		const tstring *text = ((const tstr *)value->val.v_tcompo)->data;
		tstring_append_fmt(canonical, "s%zu:", tstring_len(text));
		tstring_append_ts(canonical, text);
	} break;
	}
	ttype_finish_canonical(type, canonical);
	return type;
}

const tobj *ttypeval_exact_value(const ttypeval *type)
{
	return type && type->kind == ttype_kind_exact_value ?
		&type->exact_value : nullptr;
}

static ttypeval *substitute_template(const ttypeval *source,
	const ttypeval *template_type, ttypeval *const *types,
	const tobj *values)
{
	if (source->kind == ttype_kind_parameter) {
		for (uint_objs i = 0; i < template_type->template_type_parameter_count; i++)
			if (tstring_eq(source->parameter_name,
			    template_type->template_type_parameters[i]))
				return ttypeval_retain(types[i]);
		twarn(ErrRuntime_ObjUnfound, "types::template",
		      "undeclared Type parameter in definition");
	}
	if (source->kind == ttype_kind_value_parameter) {
		for (uint_objs i = 0; i < template_type->template_value_parameter_count; i++)
			if (tstring_eq(source->parameter_name,
			    template_type->template_value_parameters[i].name))
				return ttypeval_retain(ttypeval_new_exact_value(&values[i]));
		twarn(ErrRuntime_ObjUnfound, "types::template",
		      "undeclared Value parameter in definition");
	}
	if (source->kind == ttype_kind_named && source->named_parameter_count) {
		ttype_field *parameters = calloc(source->named_parameter_count,
			sizeof(*parameters));
		if (!parameters) abort();
		for (uint_objs i = 0; i < source->named_parameter_count; i++) {
			parameters[i].name = source->named_parameter_names[i];
			parameters[i].type = substitute_template(
				source->named_parameter_types[i], template_type,
				types, values);
		}
		ttypeval *result = ttypeval_new_named_application(
			tstring_cstr(source->named_identity), parameters,
			source->named_parameter_count, source->capabilities);
		for (uint_objs i = 0; i < source->named_parameter_count; i++)
			ttypeval_release(parameters[i].type);
		free(parameters);
		return ttypeval_retain(result);
	}
	if (source->kind == ttype_kind_fields) {
		uint_objs count = ttypeval_field_count(source);
		ttype_field *fields = calloc(count, sizeof(*fields));
		if (!fields) abort();
		for (uint_objs i = 0; i < count; i++) {
			const tobj *name;
			ttypeval *field;
			ttypeval_field_at(source, i, &name, &field);
			fields[i].name = ((const tstr *)name->val.v_tcompo)->data;
			fields[i].type = substitute_template(field, template_type,
				types, values);
			fields[i].optional = ttypeval_field_optional(source,
				tstring_cstr(fields[i].name));
		}
		ttypeval *result = ttypeval_new_fields(fields, count);
		for (uint_objs i = 0; i < count; i++) ttypeval_release(fields[i].type);
		free(fields);
		return ttypeval_retain(result);
	}
	if (source->kind == ttype_kind_list || source->kind == ttype_kind_iterator ||
	    source->kind == ttype_kind_rule_term) {
		ttypeval *item = substitute_template(ttypeval_parameter(source, "item"),
			template_type, types, values);
		ttypeval *result = source->kind == ttype_kind_list ?
			ttypeval_new_list(item) : source->kind == ttype_kind_iterator ?
			ttypeval_new_iterator(item) : ttypeval_new_rule_term(item);
		ttypeval_release(item);
		return ttypeval_retain(result);
	}
	if (source->kind == ttype_kind_pair || source->kind == ttype_kind_dictionary) {
		ttypeval *first = substitute_template(ttypeval_parameter(source,
			source->kind == ttype_kind_pair ? "first" : "key"),
			template_type, types, values);
		ttypeval *second = substitute_template(ttypeval_parameter(source,
			source->kind == ttype_kind_pair ? "second" : "value"),
			template_type, types, values);
		ttypeval *result = source->kind == ttype_kind_pair ?
			ttypeval_new_pair(first, second) :
			ttypeval_new_dictionary(first, second);
		ttypeval_release(first); ttypeval_release(second);
		return ttypeval_retain(result);
	}
	if (source->kind == ttype_kind_union) {
		uint_objs count = ttypeval_member_count(source);
		ttypeval **members = calloc(count, sizeof(*members));
		if (!members) abort();
		for (uint_objs i = 0; i < count; i++)
			members[i] = substitute_template(ttypeval_member_at(source, i),
				template_type, types, values);
		ttypeval *result = ttypeval_new_union(members, count);
		for (uint_objs i = 0; i < count; i++) ttypeval_release(members[i]);
		free(members);
		return ttypeval_retain(result);
	}
	if (source->kind == ttype_kind_function || source->kind == ttype_kind_rule ||
	    source->kind == ttype_kind_rule_instance ||
	    source->kind == ttype_kind_instance_of) {
		uint_objs parameter_count = ttypeval_function_parameter_count(source);
		ttypeval **parameters = parameter_count ? calloc(parameter_count,
			sizeof(*parameters)) : nullptr;
		if (parameter_count && !parameters) abort();
		for (uint_objs i = 0; i < parameter_count; i++)
			parameters[i] = substitute_template(
				ttypeval_function_parameter_at(source, i), template_type,
				types, values);
		ttypeval *result = nullptr;
		if (source->kind == ttype_kind_function) {
			ttypeval *return_type = substitute_template(
				ttypeval_function_result(source), template_type, types, values);
			result = ttypeval_new_function(parameters, parameter_count,
				return_type, ttypeval_function_variadic(source));
			ttypeval_release(return_type);
		} else if (source->kind == ttype_kind_rule) {
			result = ttypeval_new_rule(parameters, parameter_count);
		} else if (source->kind == ttype_kind_rule_instance) {
			result = ttypeval_new_rule_instance(parameters, parameter_count);
		} else if (source->instance_reference) {
			result = ttypeval_new_instance_reference(
				tstring_cstr(source->instance_reference), parameters,
				parameter_count);
		} else {
			result = (ttypeval *)source;
		}
		for (uint_objs i = 0; i < parameter_count; i++)
			ttypeval_release(parameters[i]);
		free(parameters);
		return ttypeval_retain(result);
	}
	return ttypeval_retain((ttypeval *)source);
}

ttypeval *ttypeval_apply_template(ttypeval *template_type,
	ttypeval *const *type_arguments, uint_objs type_argument_count,
	const tobj *value_arguments, uint_objs value_argument_count)
{
	if (!ttypeval_is_template(template_type) ||
	    type_argument_count != template_type->template_type_parameter_count ||
	    value_argument_count != template_type->template_value_parameter_count)
		twarn(ErrRuntime_ParamsCtr, "Type template",
		      "template argument count mismatch");
	for (uint_objs i = 0; i < type_argument_count; i++)
		if (!type_arguments[i])
			twarn(ErrRuntime_ParamsType, "Type template",
			      "Type argument required");
	for (uint_objs i = 0; i < value_argument_count; i++)
		if (!ttypeval_matches(&value_arguments[i],
		    template_type->template_value_parameters[i].type))
			twarn(ErrRuntime_ParamsType, "Type template",
			      "Value argument does not match its declared Type");
	return substitute_template(template_type->template_body, template_type,
		type_arguments, value_arguments);
}

ttypeval *ttypeval_new_fields(const ttype_field *fields, uint_objs count)
{
	if (!fields || count == 0)
		twarn(ErrRuntime_ParamsCtr, "types::make_type",
		      "at least one field is required");
	ttypeval *type = ttype_new(ttype_kind_fields);
	for (uint_objs i = 0; i < count; i++) {
		const char *name = tstring_cstr(fields[i].name);
		if (!name || name[0] == '@')
			twarn(ErrRuntime_ParamsType, "types::make_type",
			      "field names beginning with @ are reserved");
		tobj key;
		tobj_set_nil(&key);
		tobj_set_compo(&key, (tcompo_v *)tstr_new(name));
		if (thashtbl_contains(type->definition, &key))
			twarn(ErrRuntime_Other, "types::make_type",
			      "duplicate field name");
		tobj_try_clear(&key);
		ttype_set_definition(type, name, fields[i].type);
		if (fields[i].optional) {
			tstring **optional = (tstring **)realloc(type->optional_fields,
				(type->optional_field_count + 1) * sizeof(*optional));
			if (!optional) abort();
			type->optional_fields = optional;
			type->optional_fields[type->optional_field_count++] =
				tstring_new(name);
		}
	}
	ttype_build_fields_canonical(type);
	return type;
}

ttypeval *ttypeval_new_enum(const tstring *const *members, uint_objs count)
{
	if (!members || count == 0)
		twarn(ErrRuntime_ParamsCtr, "types::enum",
		      "at least one String member is required");
	ttypeval *type = ttype_new(ttype_kind_enum);
	type->enum_members = (tstring **)calloc(count, sizeof(*type->enum_members));
	if (!type->enum_members)
		twarn(ErrRuntime_Other, "types::enum", "out of memory");
	type->enum_member_count = count;
	for (uint_objs i = 0; i < count; i++) {
		if (!members[i])
			twarn(ErrRuntime_ParamsType, "types::enum",
			      "String member required");
		for (uint_objs j = 0; j < i; j++)
			if (tstring_eq(members[i], members[j]))
				twarn(ErrRuntime_Other, "types::enum",
				      "duplicate enum member");
		type->enum_members[i] = tstring_dup(members[i]);
	}
	ttype_set_definition(type, "@base", ttypeval_builtin(tbuiltintype_string));
	tstring **sorted = (tstring **)calloc(count, sizeof(*sorted));
	if (!sorted) abort();
	for (uint_objs i = 0; i < count; i++) sorted[i] = type->enum_members[i];
	qsort(sorted, count, sizeof(*sorted), ttype_compare_string_ptr);
	tstring *canonical = tstring_new("E");
	tstring_append_fmt(canonical, "%u:", (unsigned)count);
	for (uint_objs i = 0; i < count; i++) {
		tstring_append_fmt(canonical, "%zu:", tstring_len(sorted[i]));
		tstring_append_ts(canonical, sorted[i]);
	}
	free(sorted);
	ttype_finish_canonical(type, canonical);
	return type;
}

static ttypeval *ttype_new_parameterized(ttype_kind kind,
					 tbuiltintype_id base,
					 const char *first_name,
					 ttypeval *first,
					 const char *second_name,
					 ttypeval *second,
					 const char *tag)
{
	ttypeval *type = ttype_new(kind);
	ttype_set_definition(type, "@base", ttypeval_builtin(base));
	ttype_set_definition(type, first_name, first);
	if (second_name)
		ttype_set_definition(type, second_name, second);
	tstring *canonical = tstring_new(tag);
		ttype_append_canonical(canonical, first);
	if (second)
		ttype_append_canonical(canonical, second);
	ttype_finish_canonical(type, canonical);
	return type;
}

ttypeval *ttypeval_new_list(ttypeval *item)
{
	return ttype_new_parameterized(ttype_kind_list, tbuiltintype_list,
				       "@item", item, nullptr, nullptr, "L");
}

ttypeval *ttypeval_new_iterator(ttypeval *item)
{
	return ttype_new_parameterized(ttype_kind_iterator,
		tbuiltintype_iterator, "@item", item, nullptr, nullptr, "I");
}

ttypeval *ttypeval_new_pair(ttypeval *first, ttypeval *second)
{
	return ttype_new_parameterized(ttype_kind_pair, tbuiltintype_pair,
				       "@first", first, "@second", second, "P");
}

ttypeval *ttypeval_new_dictionary(ttypeval *key, ttypeval *value)
{
	return ttype_new_parameterized(ttype_kind_dictionary,
				       tbuiltintype_dictionary,
				       "@key", key, "@value", value, "D");
}

ttypeval *ttypeval_new_rule_term(ttypeval *result)
{
	return ttype_new_parameterized(ttype_kind_rule_term,
		tbuiltintype_rule_term, "@item", result, nullptr, nullptr, "Z");
}

ttypeval *ttypeval_new_function(ttypeval *const *parameters,
				uint_objs parameter_count,
				ttypeval *result,
				int variadic)
{
	ttypeval *type = ttype_new(ttype_kind_function);
	type->function_parameter_count = parameter_count;
	type->function_variadic = variadic ? 1 : 0;
	ttype_set_definition(type, "@base",
			     ttypeval_builtin(tbuiltintype_function));
	tstring *canonical = tstring_new(variadic ? "FV" : "FF");
	tstring_append_fmt(canonical, "%u:", (unsigned)parameter_count);
	for (uint_objs i = 0; i < parameter_count; i++) {
		if (parameters && parameters[i]) {
			char key[32];
			snprintf(key, sizeof(key), "@parameter/%u", (unsigned)i);
			ttype_set_definition(type, key, parameters[i]);
			tstring_append_c(canonical, 'T');
			ttype_append_canonical(canonical, parameters[i]);
		} else
			tstring_append_c(canonical, '?');
	}
	if (result) {
		ttype_set_definition(type, "@result", result);
		tstring_append_c(canonical, 'R');
		ttype_append_canonical(canonical, result);
	} else
		tstring_append_c(canonical, '?');
	ttype_finish_canonical(type, canonical);
	return type;
}

static ttypeval *ttype_new_rule_type(ttype_kind kind, tbuiltintype_id base,
				     const char *tag,
				     ttypeval *const *parameters,
				     uint_objs parameter_count)
{
	ttypeval *type = ttype_new(kind);
	type->function_parameter_count = parameter_count;
	ttype_set_definition(type, "@base", ttypeval_builtin(base));
	tstring *canonical = tstring_new(tag);
	tstring_append_fmt(canonical, "%u:", (unsigned)parameter_count);
	for (uint_objs i = 0; i < parameter_count; i++) {
		char key[32];
		snprintf(key, sizeof(key), "@parameter/%u", (unsigned)i);
		ttype_set_definition(type, key, parameters[i]);
		ttype_append_canonical(canonical, parameters[i]);
	}
	ttype_finish_canonical(type, canonical);
	return type;
}

ttypeval *ttypeval_new_rule(ttypeval *const *parameters,
			    uint_objs parameter_count)
{
	return ttype_new_rule_type(ttype_kind_rule, tbuiltintype_rule, "Q",
		parameters, parameter_count);
}

ttypeval *ttypeval_new_rule_instance(ttypeval *const *parameters,
				     uint_objs parameter_count)
{
	return ttype_new_rule_type(ttype_kind_rule_instance,
		tbuiltintype_rule_instance, "QI", parameters, parameter_count);
}

ttypeval *ttypeval_new_instance_reference(const char *name,
	ttypeval *const *parameters, uint_objs count)
{
	ttypeval *type = ttype_new_rule_type(ttype_kind_instance_of,
		tbuiltintype_rule_instance, "QI", parameters, count);
	tstring *canonical = tstring_new("N");
	tstring_append_fmt(canonical, "%zu:", strlen(name));
	tstring_append(canonical, name);
	ttype_append_canonical(canonical, type);
	tstring_free(type->canonical);
	type->instance_reference = tstring_new(name);
	tobj_set_nil(&type->instance_rule);
	type->contains_instance = 1;
	ttype_finish_canonical(type, canonical);
	return type;
}

ttypeval *ttypeval_new_instance_of(const tobj *value)
{
	if (!value || value->type != tcompo || tobj_compo_type(value) != compo_trule)
		twarn(ErrRuntime_ParamsType, "InstanceOf", "Rule value required");
	trule *rule = (trule *)value->val.v_tcompo;
	uint_objs count = rule->ir->parameters.len;
	ttypeval **parameters = count ? calloc(count, sizeof(*parameters)) : nullptr;
	if (count && !parameters) abort();
	for (uint_objs i = 0; i < count; i++)
		parameters[i] = ((trule_term *)rule->ir->parameters.data[i].val.v_tcompo)->type;
	ttypeval *type = ttype_new_rule_type(ttype_kind_instance_of,
		tbuiltintype_rule_instance, "QI", parameters, count);
	free(parameters);
	tobj_set_nil(&type->instance_rule);
	tobj_copy(&type->instance_rule, value);
	type->contains_instance = 1;
	/* Process-local identity; never decoded as a portable Type definition. */
	tstring *canonical = tstring_new_empty();
	tstring_append_fmt(canonical, "OI%llu", (unsigned long long)rule->identity);
	tstring_free(type->canonical);
	ttype_finish_canonical(type, canonical);
	return type;
}

ttypeval *ttypeval_resolve_instances(ttypeval *type, ttype_instance_resolver resolver, void *context)
{
	if (!type || !type->contains_instance) return ttypeval_retain(type);
	if (type->kind == ttype_kind_instance_of) {
		if (!type->instance_reference) return ttypeval_retain(type);
		const tobj *value = resolver(context, tstring_cstr(type->instance_reference));
		return value ? ttypeval_retain(ttypeval_new_instance_of(value)) : ttypeval_retain(type);
	}
	if (type->kind == ttype_kind_template) {
		ttype_value_parameter *parameters = type->template_value_parameter_count ?
			calloc(type->template_value_parameter_count, sizeof(*parameters)) :
			nullptr;
		if (type->template_value_parameter_count && !parameters) abort();
		for (uint_objs i = 0; i < type->template_value_parameter_count; i++) {
			parameters[i].name = type->template_value_parameters[i].name;
			parameters[i].type = ttypeval_resolve_instances(
				type->template_value_parameters[i].type, resolver, context);
		}
		ttypeval *body = ttypeval_resolve_instances(type->template_body,
			resolver, context);
		ttypeval *result = ttypeval_new_template(
			(const tstring *const *)type->template_type_parameters,
			type->template_type_parameter_count, parameters,
			type->template_value_parameter_count, body);
		for (uint_objs i = 0; i < type->template_value_parameter_count; i++)
			ttypeval_release(parameters[i].type);
		ttypeval_release(body);
		free(parameters);
		return ttypeval_retain(result);
	}
	if (type->kind == ttype_kind_named && type->named_parameter_count) {
		ttype_field *parameters = calloc(type->named_parameter_count,
			sizeof(*parameters));
		if (!parameters) abort();
		for (uint_objs i = 0; i < type->named_parameter_count; i++) {
			parameters[i].name = type->named_parameter_names[i];
			parameters[i].type = ttypeval_resolve_instances(
				type->named_parameter_types[i], resolver, context);
		}
		ttypeval *result = ttypeval_new_named_application(
			tstring_cstr(type->named_identity), parameters,
			type->named_parameter_count, type->capabilities);
		for (uint_objs i = 0; i < type->named_parameter_count; i++)
			ttypeval_release(parameters[i].type);
		free(parameters);
		return ttypeval_retain(result);
	}
	uint_objs count = type->kind == ttype_kind_fields ? ttypeval_field_count(type) :
		type->kind == ttype_kind_union ? ttypeval_member_count(type) :
		type->kind == ttype_kind_function ? ttypeval_function_parameter_count(type) + 1 :
		type->kind == ttype_kind_rule || type->kind == ttype_kind_rule_instance ?
		ttypeval_function_parameter_count(type) :
		type->kind == ttype_kind_pair || type->kind == ttype_kind_dictionary ? 2 : 1;
	ttypeval **children = count ? calloc(count, sizeof(*children)) : nullptr;
	ttype_field *fields = type->kind == ttype_kind_fields && count ?
		calloc(count, sizeof(*fields)) : nullptr;
	if ((count && !children) ||
		(type->kind == ttype_kind_fields && count && !fields)) abort();
	for (uint_objs i = 0; i < count; i++) {
		ttypeval *child = nullptr;
		if (type->kind == ttype_kind_fields) {
			const tobj *name;
			ttypeval_field_at(type, i, &name, &child);
			fields[i].name = ((tstr *)name->val.v_tcompo)->data;
			fields[i].optional = ttypeval_field_optional(type, tstring_cstr(fields[i].name));
		} else if (type->kind == ttype_kind_union) child = ttypeval_member_at(type, i);
		else if (type->kind == ttype_kind_function || type->kind == ttype_kind_rule ||
		    type->kind == ttype_kind_rule_instance)
			child = type->kind == ttype_kind_function && i + 1 == count ?
				ttypeval_function_result(type) : ttypeval_function_parameter_at(type, i);
		else child = ttypeval_parameter(type,
			type->kind == ttype_kind_pair ? (i ? "second" : "first") :
			type->kind == ttype_kind_dictionary ? (i ? "value" : "key") : "item");
		children[i] = ttypeval_resolve_instances(child, resolver, context);
		if (fields) fields[i].type = children[i];
	}
	ttypeval *result = nullptr;
	switch (type->kind) {
	case ttype_kind_fields: result = ttypeval_new_fields(fields, count); break;
	case ttype_kind_union: result = ttypeval_new_union(children, count); break;
	case ttype_kind_function: result = ttypeval_new_function(children, count - 1, children[count - 1], type->function_variadic); break;
	case ttype_kind_rule: result = ttypeval_new_rule(children, count); break;
	case ttype_kind_rule_instance: result = ttypeval_new_rule_instance(children, count); break;
	case ttype_kind_list: result = ttypeval_new_list(children[0]); break;
	case ttype_kind_iterator: result = ttypeval_new_iterator(children[0]); break;
	case ttype_kind_pair: result = ttypeval_new_pair(children[0], children[1]); break;
	case ttype_kind_dictionary: result = ttypeval_new_dictionary(children[0], children[1]); break;
	default: twarn(ErrRuntime_ParamsType, "InstanceOf", "unsupported enclosing Type");
	}
	ttypeval_retain(result);
	for (uint_objs i = 0; i < count; i++) ttypeval_release(children[i]);
	free(fields); free(children);
	return result;
}

static int ttype_compare_member(const void *left, const void *right)
{
	const ttypeval *a = *(ttypeval *const *)left;
	const ttypeval *b = *(ttypeval *const *)right;
	return tstring_cmp(a->canonical, b->canonical);
}

static void ttype_union_collect(ttypeval *type,
				ttypeval ***members,
				uint_objs *count,
				uint_objs *capacity)
{
	if (type->kind == ttype_kind_union) {
		uint_objs n = ttypeval_member_count(type);
		for (uint_objs i = 0; i < n; i++)
			ttype_union_collect(ttypeval_member_at(type, i), members,
					    count, capacity);
		return;
	}
	if (*count >= *capacity) {
		uint_objs grown = *capacity ? (uint_objs)(*capacity * 2) : 8;
		ttypeval **next =
			(ttypeval **)realloc(*members, grown * sizeof(ttypeval *));
		if (!next)
			twarn(ErrRuntime_Other, "types::union", "out of memory");
		*members = next;
		*capacity = grown;
	}
	(*members)[(*count)++] = type;
}

ttypeval *ttypeval_new_union(ttypeval *const *input, uint_objs input_count)
{
	if (!input || input_count < 2)
		twarn(ErrRuntime_ParamsCtr, "types::union",
		      "at least two Type values are required");
	ttypeval **members = nullptr;
	uint_objs count = 0;
	uint_objs capacity = 0;
	for (uint_objs i = 0; i < input_count; i++)
		ttype_union_collect(input[i], &members, &count, &capacity);
	qsort(members, count, sizeof(ttypeval *), ttype_compare_member);

	uint_objs unique = 0;
	for (uint_objs i = 0; i < count; i++) {
		if (members[i]->kind == ttype_kind_any) {
			free(members);
			return ttypeval_builtin(tbuiltintype_any);
		}
		if (unique == 0 || !ttypeval_equal(members[unique - 1], members[i]))
			members[unique++] = members[i];
	}
	if (unique == 1) {
		ttypeval *only = members[0];
		free(members);
		return only;
	}

	ttypeval *type = ttype_new(ttype_kind_union);
	tstring *canonical = tstring_new("U");
	tstring_append_fmt(canonical, "%u:", (unsigned)unique);
	for (uint_objs i = 0; i < unique; i++) {
		char key[32];
		snprintf(key, sizeof(key), "@union/%u", (unsigned)i);
		ttype_set_definition(type, key, members[i]);
		ttype_append_canonical(canonical, members[i]);
	}
	free(members);
	ttype_finish_canonical(type, canonical);
	return type;
}

ttypeval *ttypeval_new_recursive(void)
{
	ttypeval *type = ttype_new(ttype_kind_recursive);
	type->recursive_id = next_recursive_id++;
	type->contains_recursive = 1;
	/* The initial reference owns the placeholder until it enters a binding. */
	type->base.refctr = 1;
	tstring *canonical = tstring_new("R");
	tstring_append_fmt(canonical, "%u", (unsigned)type->recursive_id);
	ttype_finish_canonical(type, canonical);
	return type;
}

int ttypeval_define_recursive(ttypeval *type, ttypeval *body)
{
	if (!type || type->kind != ttype_kind_recursive ||
	    type->recursive_defined || !body ||
	    body->kind == ttype_kind_recursive)
		return 0;
	type->recursive_body = body;
	ttypeval_retain(body);
	type->recursive_defined = 1;
	tstring_free(type->canonical);
	type->canonical = tstring_new("M");
	tstring_append_fmt(type->canonical, "%u:", (unsigned)type->recursive_id);
	ttype_append_canonical(type->canonical, body);
	type->canonical_hash = 0x7265637572736976ULL;
	return 1;
}

int ttypeval_is_recursive(const ttypeval *type)
{
	return type && type->kind == ttype_kind_recursive;
}

typedef struct {
	const ttypeval *left;
	const ttypeval *right;
} ttype_compare_pair;

static const ttypeval *ttype_unwrap(const ttypeval *type)
{
	return type && type->kind == ttype_kind_recursive &&
	       type->recursive_defined ? type->recursive_body : type;
}

static int ttype_equal_graph(const ttypeval *left, const ttypeval *right,
			     ttype_compare_pair **seen, uint_objs *count,
			     uint_objs *capacity)
{
	if (left == right) return left != nullptr;
	if (!left || !right) return 0;
	for (uint_objs i = 0; i < *count; i++)
		if ((*seen)[i].left == left && (*seen)[i].right == right)
			return 1;
	if (*count == *capacity) {
		uint_objs grown = *capacity ? *capacity * 2 : 16;
		ttype_compare_pair *items = (ttype_compare_pair *)realloc(
			*seen, grown * sizeof(*items));
		if (!items) abort();
		*seen = items;
		*capacity = grown;
	}
	(*seen)[(*count)++] = (ttype_compare_pair){ left, right };
	left = ttype_unwrap(left);
	right = ttype_unwrap(right);
	if (left == right) return 1;
	if (!left || !right || left->kind != right->kind) return 0;
	if (left->kind == ttype_kind_instance_of)
		return strcmp(tstring_cstr(left->canonical),
			      tstring_cstr(right->canonical)) == 0;
	if (left->kind == ttype_kind_any || left->kind == ttype_kind_builtin)
		return left->builtin == right->builtin;
	if (left->kind == ttype_kind_function &&
	    (left->function_parameter_count != right->function_parameter_count ||
	     left->function_variadic != right->function_variadic))
		return 0;
	if (left->kind == ttype_kind_fields) {
		if (ttypeval_field_count(left) != ttypeval_field_count(right)) return 0;
		for (uint_objs i = 0; i < ttypeval_field_count(left); i++) {
			const tobj *name = nullptr;
			ttypeval *member = nullptr;
			ttypeval_field_at(left, i, &name, &member);
			const tstr *key = (const tstr *)name->val.v_tcompo;
			ttypeval *other = ttypeval_field_named(
				right, tstring_cstr(key->data));
			if (!other || ttypeval_field_optional(left,
				tstring_cstr(key->data)) != ttypeval_field_optional(right,
				tstring_cstr(key->data)) || !ttype_equal_graph(
				member, other, seen, count, capacity)) return 0;
		}
		return 1;
	}
	if (left->kind == ttype_kind_union) {
		uint_objs n = ttypeval_member_count(left);
		if (n != ttypeval_member_count(right)) return 0;
		for (uint_objs i = 0; i < n; i++) {
			int found = 0;
			for (uint_objs j = 0; j < n && !found; j++) {
				uint_objs saved = *count;
				found = ttype_equal_graph(ttypeval_member_at(left, i),
					ttypeval_member_at(right, j), seen, count, capacity);
				if (!found) *count = saved;
			}
			if (!found) return 0;
		}
		return 1;
	}
	uint_objs entries = thashtbl_len(left->definition);
	if (entries != thashtbl_len(right->definition)) return 0;
	uint_objs count_left = 0;
	ttype_entry *items = ttype_sorted_entries(left, &count_left);
	for (uint_objs i = 0; i < count_left; i++) {
		const tstr *key = (const tstr *)items[i].key->val.v_tcompo;
		ttypeval *a = (ttypeval *)items[i].value->val.v_tcompo;
		ttypeval *b = ttype_definition_get_type(right, tstring_cstr(key->data));
		if (!b || !ttype_equal_graph(a, b, seen, count, capacity)) {
			free(items);
			return 0;
		}
	}
	free(items);
	return 1;
}

int ttypeval_equal(const ttypeval *left, const ttypeval *right)
{
	if (left == right) return left != nullptr;
	if (!left || !right) return 0;
	if (!ttype_contains_recursive(left) && !ttype_contains_recursive(right))
		return tstring_cmp(left->canonical, right->canonical) == 0;
	ttype_compare_pair *seen = nullptr;
	uint_objs count = 0, capacity = 0;
	int equal = ttype_equal_graph(left, right, &seen, &count, &capacity);
	free(seen);
	return equal;
}

uint64_t ttypeval_hash(const ttypeval *type)
{
	return type ? (ttype_contains_recursive(type) ?
		0x7265637572736976ULL : type->canonical_hash) : 0;
}

uint_objs ttypeval_field_count(const ttypeval *type)
{
	type = ttype_unwrap(type);
	return type && type->kind == ttype_kind_fields ?
		       thashtbl_len(type->definition) : 0;
}

int ttypeval_field_at(const ttypeval *type, uint_objs index,
		      const tobj **name, ttypeval **field_type)
{
	type = ttype_unwrap(type);
	if (!type || type->kind != ttype_kind_fields)
		return 0;
	uint_objs count;
	ttype_entry *entries = ttype_sorted_entries(type, &count);
	if (index >= count) {
		free(entries);
		return 0;
	}
	if (name)
		*name = entries[index].key;
	if (field_type)
		*field_type = (ttypeval *)entries[index].value->val.v_tcompo;
	free(entries);
	return 1;
}

ttypeval *ttypeval_field_named(const ttypeval *type, const char *name)
{
	type = ttype_unwrap(type);
	if (!type || type->kind != ttype_kind_fields || !name || name[0] == '@')
		return nullptr;
	return ttype_definition_get_type(type, name);
}

int ttypeval_field_optional(const ttypeval *type, const char *name)
{
	type = ttype_unwrap(type);
	if (!type || type->kind != ttype_kind_fields || !name) return 0;
	for (uint_objs i = 0; i < type->optional_field_count; i++)
		if (tstring_eq_cstr(type->optional_fields[i], name)) return 1;
	return 0;
}

uint_objs ttypeval_member_count(const ttypeval *type)
{
	type = ttype_unwrap(type);
	return type && type->kind == ttype_kind_union ?
		       thashtbl_len(type->definition) : 0;
}

ttypeval *ttypeval_member_at(const ttypeval *type, uint_objs index)
{
	type = ttype_unwrap(type);
	if (!type || type->kind != ttype_kind_union)
		return nullptr;
	char key[32];
	snprintf(key, sizeof(key), "@union/%u", (unsigned)index);
	return ttype_definition_get_type(type, key);
}

uint_objs ttypeval_enum_member_count(const ttypeval *type)
{
	type = ttype_unwrap(type);
	return type && type->kind == ttype_kind_enum ? type->enum_member_count : 0;
}

const tstring *ttypeval_enum_member_at(const ttypeval *type, uint_objs index)
{
	type = ttype_unwrap(type);
	return type && type->kind == ttype_kind_enum &&
	       index < type->enum_member_count ? type->enum_members[index] : nullptr;
}

int ttypeval_enum_contains(const ttypeval *type, const tstring *member)
{
	if (!member) return 0;
	for (uint_objs i = 0; i < ttypeval_enum_member_count(type); i++)
		if (tstring_eq(ttypeval_enum_member_at(type, i), member)) return 1;
	return 0;
}

ttypeval *ttypeval_base(const ttypeval *type)
{
	type = ttype_unwrap(type);
	if (!type)
		return nullptr;
	if (type->kind == ttype_kind_exact_value) {
		tbuiltintype_id builtin = type->exact_value.type == tnil ? tbuiltintype_nil :
			type->exact_value.type == tbool ? tbuiltintype_bool :
			type->exact_value.type == tint ? tbuiltintype_int :
			type->exact_value.type == tfloat ? tbuiltintype_float : tbuiltintype_string;
		return ttypeval_builtin(builtin);
	}
	if (!(type->kind == ttype_kind_named && type->named_parameter_count) &&
	    type->kind != ttype_kind_list && type->kind != ttype_kind_iterator &&
	    type->kind != ttype_kind_pair &&
	    type->kind != ttype_kind_dictionary &&
	    type->kind != ttype_kind_function &&
	    type->kind != ttype_kind_rule &&
	    type->kind != ttype_kind_rule_instance &&
	    type->kind != ttype_kind_instance_of &&
	    type->kind != ttype_kind_enum)
		return (ttypeval *)type;
	return ttype_definition_get_type(type, "@base");
}

ttypeval *ttypeval_parameter(const ttypeval *type, const char *name)
{
	type = ttype_unwrap(type);
	if (!type || !name)
		return nullptr;
	char key[32];
	snprintf(key, sizeof(key), "@%s", name);
	return ttype_definition_get_type(type, key);
}

uint_objs ttypeval_function_parameter_count(const ttypeval *type)
{
	type = ttype_unwrap(type);
	return type && (type->kind == ttype_kind_function ||
		type->kind == ttype_kind_rule ||
		type->kind == ttype_kind_rule_instance || type->kind == ttype_kind_instance_of) ?
		type->function_parameter_count : 0;
}

ttypeval *ttypeval_function_parameter_at(const ttypeval *type,
					  uint_objs index)
{
	type = ttype_unwrap(type);
	if (!type || (type->kind != ttype_kind_function &&
	    type->kind != ttype_kind_rule &&
	    type->kind != ttype_kind_rule_instance && type->kind != ttype_kind_instance_of) ||
	    index >= type->function_parameter_count)
		return nullptr;
	char key[32];
	snprintf(key, sizeof(key), "@parameter/%u", (unsigned)index);
	return ttype_definition_get_type(type, key);
}

ttypeval *ttypeval_function_result(const ttypeval *type)
{
	type = ttype_unwrap(type);
	return type && type->kind == ttype_kind_function ?
		ttype_definition_get_type(type, "@result") : nullptr;
}

int ttypeval_function_variadic(const ttypeval *type)
{
	type = ttype_unwrap(type);
	return type && type->kind == ttype_kind_function &&
	       type->function_variadic;
}

const thashtbl *ttypeval_definition(const ttypeval *type)
{
	type = ttype_unwrap(type);
	return type ? type->definition : nullptr;
}

void ttypeval_idx(ttypeval *type, const tobj *params, uint_regs np,
		  tobj *result)
{
	type = (ttypeval *)ttype_unwrap(type);
	if (np != 1)
		twarn(ErrRuntime_ParamsCtr, "Type index", "one key is required");
	if (!type || (type->kind != ttype_kind_fields &&
	    type->kind != ttype_kind_enum) ||
	    params[0].type != tcompo ||
	    tobj_compo_type(&params[0]) != compo_tstr)
		twarn(ErrRuntime_ParamsType, "Type index", "String member required");
	if (type->kind == ttype_kind_enum) {
		const tstring *member = ((const tstr *)params[0].val.v_tcompo)->data;
		if (!ttypeval_enum_contains(type, member))
			twarn(ErrRuntime_ObjUnfound, "Type index", "unknown enum member");
		tobj_set_compo(result, (tcompo_v *)tstr_new(tstring_cstr(member)));
		return;
	}
	const tobj *value = thashtbl_get(type->definition, &params[0]);
	if (!value)
		twarn(ErrRuntime_ObjUnfound, "Type index", "unknown field");
	*result = *value;
	if (result->type == tcompo && result->val.v_tcompo)
		result->val.v_tcompo->refctr++;
}

static int ttypeval_next_key(const ttypeval *type, long *position, tobj *result)
{
	if (!type || !position || *position < 0)
		return 0;
	if (type->kind == ttype_kind_enum) {
		const tstring *member = ttypeval_enum_member_at(type,
			(uint_objs)*position);
		if (!member) return 0;
		tobj_set_compo(result, (tcompo_v *)tstr_new(tstring_cstr(member)));
		result->val.v_tcompo->refctr++;
		(*position)++;
		return 1;
	}
	if (type->kind != ttype_kind_fields) return 0;
	const tobj *key = nullptr;
	if (!ttypeval_field_at(type, (uint_objs)*position, &key, nullptr))
		return 0;
	*result = *key;
	if (result->type == tcompo && result->val.v_tcompo)
		result->val.v_tcompo->refctr++;
	(*position)++;
	return 1;
}

static int tbuiltintype_matches(const tobj *value, tbuiltintype_id builtin)
{
	if (!value)
		return 0;
	switch (builtin) {
	case tbuiltintype_any:
		return 1;
	case tbuiltintype_nil:
		return value->type == tnil;
	case tbuiltintype_bool:
		return value->type == tbool;
	case tbuiltintype_int:
		return value->type == tint;
	case tbuiltintype_float:
		return value->type == tfloat;
	default:
		break;
	}
	if (value->type != tcompo || !value->val.v_tcompo)
		return 0;
	tcompo_type code = tobj_compo_type(value);
	switch (builtin) {
	case tbuiltintype_string:
		return code == compo_tstr;
	case tbuiltintype_list:
		return code == compo_tlist;
	case tbuiltintype_pair:
		return code == compo_tpair;
	case tbuiltintype_dictionary:
		return code == compo_tdict;
	case tbuiltintype_iterator:
		return code == compo_titer;
	case tbuiltintype_function:
		return code == compo_tfunc || code == compo_cppfunc ||
		       code == compo_sessfunc;
	case tbuiltintype_library:
		return code == compo_tlib;
	case tbuiltintype_real_array:
		return code == compo_tdarr;
	case tbuiltintype_bool_array:
		return code == compo_tbarr;
	case tbuiltintype_time:
		return code == compo_time;
	case tbuiltintype_type:
		return code == compo_ttypeval;
	case tbuiltintype_indexable:
		return value->val.v_tcompo->vtable->capabilities &&
		       value->val.v_tcompo->vtable->capabilities->indexable;
	case tbuiltintype_index_settable:
		return value->val.v_tcompo->vtable->capabilities &&
		       value->val.v_tcompo->vtable->capabilities->index_settable;
	case tbuiltintype_appendable:
		return value->val.v_tcompo->vtable->capabilities &&
		       value->val.v_tcompo->vtable->capabilities->appendable;
	case tbuiltintype_deletable:
		return value->val.v_tcompo->vtable->capabilities &&
		       value->val.v_tcompo->vtable->capabilities->deletable;
	case tbuiltintype_contains:
		return value->val.v_tcompo->vtable->capabilities &&
		       value->val.v_tcompo->vtable->capabilities->contains;
	case tbuiltintype_iterable:
		return value->val.v_tcompo->vtable->capabilities &&
		       value->val.v_tcompo->vtable->capabilities->iterable;
	case tbuiltintype_rule:
		return code == compo_trule;
	case tbuiltintype_rule_instance:
		return code == compo_trule_instance;
	case tbuiltintype_rule_ir:
		return code == compo_trule_ir;
	case tbuiltintype_rule_parameter:
		return code == compo_trule_term &&
		       ((trule_term *)value->val.v_tcompo)->kind == trule_term_parameter;
	case tbuiltintype_rule_capture:
		return code == compo_trule_term &&
		       ((trule_term *)value->val.v_tcompo)->kind == trule_term_capture;
	case tbuiltintype_rule_term:
		return code == compo_trule_term;
	case tbuiltintype_rule_item:
		return code == compo_trule_item;
	case tbuiltintype_rule_condition:
		return code == compo_trule_item &&
		       ((trule_item *)value->val.v_tcompo)->kind ==
		       trule_item_condition;
	case tbuiltintype_rule_requirement:
		return code == compo_trule_item &&
		       ((trule_item *)value->val.v_tcompo)->kind ==
		       trule_item_requirement;
	default:
		return 0;
	}
}

typedef struct {
	const ttypeval *key;
	const ttypeval *value;
	int matches;
	struct ttype_match_state *state;
} ttype_dictionary_match_ctx;

typedef struct {
	const tcompo_v *value;
	const ttypeval *type;
} ttype_match_pair;

typedef struct ttype_match_state {
	ttype_match_pair *seen;
	uint_objs count;
	uint_objs capacity;
} ttype_match_state;

static int ttype_matches_internal(const tobj *value,
				  const ttypeval *expected,
				  ttype_match_state *state);

static void ttype_match_dictionary_entry(const tobj *key,
					 const tobj *value,
					 void *context)
{
	ttype_dictionary_match_ctx *ctx =
		(ttype_dictionary_match_ctx *)context;
	if (ctx->matches &&
	    (!ttype_matches_internal(key, ctx->key, ctx->state) ||
	     !ttype_matches_internal(value, ctx->value, ctx->state)))
		ctx->matches = 0;
}

static int ttype_matches_internal(const tobj *value,
				  const ttypeval *expected,
				  ttype_match_state *state)
{
	if (!expected)
		return 0;
	if (expected->kind == ttype_kind_recursive) {
		if (!expected->recursive_defined) return 0;
		const tcompo_v *identity = value && value->type == tcompo ?
			value->val.v_tcompo : nullptr;
		if (identity) {
			for (uint_objs i = 0; i < state->count; i++)
				if (state->seen[i].value == identity &&
				    state->seen[i].type == expected)
					return 1;
			if (state->count == state->capacity) {
				uint_objs grown = state->capacity ? state->capacity * 2 : 16;
				ttype_match_pair *items = (ttype_match_pair *)realloc(
					state->seen, grown * sizeof(*items));
				if (!items) abort();
				state->seen = items;
				state->capacity = grown;
			}
			state->seen[state->count++] =
				(ttype_match_pair){ identity, expected };
		}
		return ttype_matches_internal(
			value, expected->recursive_body, state);
	}
	if (expected->kind == ttype_kind_any)
		return 1;
	if (expected->kind == ttype_kind_exact_value)
		return tobj_identical(value, &expected->exact_value);
	if (expected->kind == ttype_kind_parameter ||
	    expected->kind == ttype_kind_value_parameter ||
	    expected->kind == ttype_kind_template)
		return 0;
	if (expected->kind == ttype_kind_instance_of) {
		return !expected->instance_reference &&
			tbuiltintype_matches(value, tbuiltintype_rule_instance) &&
			((trule_instance *)value->val.v_tcompo)->rule.val.v_tcompo ==
				expected->instance_rule.val.v_tcompo;
	}
	if (expected->kind == ttype_kind_builtin) {
		if (value && value->type == tcompo && value->val.v_tcompo) {
			tobj type_value = { .type = tcompo,
				.val.v_tcompo = (tcompo_v *)expected };
			int package_match = tcompo_matches_type(
				value->val.v_tcompo, &type_value);
			if (package_match >= 0) return package_match;
		}
		return tbuiltintype_matches(value, expected->builtin);
	}
	if (expected->kind == ttype_kind_named) {
		if (!value || value->type != tcompo || !value->val.v_tcompo ||
		    !value->val.v_tcompo->vtable ||
		    !value->val.v_tcompo->vtable->get_type)
			return 0;
		if (expected->named_parameter_count) {
			tobj type_value = { .type = tcompo,
				.val.v_tcompo = (tcompo_v *)expected };
			return tcompo_matches_type(value->val.v_tcompo,
				&type_value) > 0;
		}
		const char *actual = value->val.v_tcompo->vtable->get_type();
		return actual && strcmp(actual,
			tstring_cstr(expected->named_identity)) == 0;
	}
	if (expected->kind == ttype_kind_function)
		return tbuiltintype_matches(value, tbuiltintype_function);
	if (expected->kind == ttype_kind_rule ||
	    expected->kind == ttype_kind_rule_instance) {
		if (!tbuiltintype_matches(value, expected->kind == ttype_kind_rule ?
		    tbuiltintype_rule : tbuiltintype_rule_instance))
			return 0;
		trule *rule = expected->kind == ttype_kind_rule ?
			(trule *)value->val.v_tcompo :
			(trule *)((trule_instance *)value->val.v_tcompo)
				->rule.val.v_tcompo;
		const char *cursor = tstring_cstr(rule->signature);
		uint_objs count = ttypeval_function_parameter_count(expected);
		for (uint_objs i = 0; i < count; i++) {
			const char *end = strchr(cursor, '\x1f');
			size_t length = end ? (size_t)(end - cursor) : strlen(cursor);
			ttypeval *parameter = ttypeval_function_parameter_at(expected, i);
			if (!parameter || tstring_len(parameter->canonical) != length ||
			    memcmp(tstring_cstr(parameter->canonical), cursor, length) != 0)
				return 0;
			if (i + 1 < count) {
				if (!end) return 0;
				cursor = end + 1;
			} else if (end)
				return 0;
		}
		return count ? *cursor != '\0' : *cursor == '\0';
	}
	if (expected->kind == ttype_kind_rule_term) {
		if (!value || value->type != tcompo || !value->val.v_tcompo ||
		    tobj_compo_type(value) != compo_trule_term)
			return 0;
		return ttypeval_equal(
			((trule_term *)value->val.v_tcompo)->type,
			ttypeval_parameter(expected, "item"));
	}
	if (expected->kind == ttype_kind_union) {
		uint_objs count = ttypeval_member_count(expected);
		for (uint_objs i = 0; i < count; i++) {
			if (ttype_matches_internal(
				value, ttypeval_member_at(expected, i), state))
				return 1;
		}
		return 0;
	}
	if (expected->kind == ttype_kind_enum) {
		return value && value->type == tcompo && value->val.v_tcompo &&
		       tobj_compo_type(value) == compo_tstr &&
		       ttypeval_enum_contains(expected,
			((const tstr *)value->val.v_tcompo)->data);
	}
	if (!value || value->type != tcompo || !value->val.v_tcompo)
		return 0;

	if (expected->kind == ttype_kind_list) {
		if (tobj_compo_type(value) != compo_tlist)
			return 0;
		tlist *list = (tlist *)value->val.v_tcompo;
		ttypeval *item = ttypeval_parameter(expected, "item");
		for (uint_objs i = 0; i < tlist_size(list); i++) {
			if (!ttype_matches_internal(tlist_at(list, i), item, state))
				return 0;
		}
		return 1;
	}
	if (expected->kind == ttype_kind_iterator) {
		if (tobj_compo_type(value) != compo_titer) return 0;
		/* Range iterators are intrinsically Iterator[Int]. */
		return ttype_matches_internal(&(tobj){ .type = tint, .val.v_tint = 0 },
			ttypeval_parameter(expected, "item"), state);
	}
	if (expected->kind == ttype_kind_pair) {
		if (tobj_compo_type(value) != compo_tpair)
			return 0;
		tpair *pair = (tpair *)value->val.v_tcompo;
		return ttype_matches_internal(
			       &pair->first, ttypeval_parameter(expected, "first"), state) &&
		       ttype_matches_internal(
			       &pair->second, ttypeval_parameter(expected, "second"), state);
	}
	if (expected->kind == ttype_kind_dictionary) {
		if (tobj_compo_type(value) != compo_tdict)
			return 0;
		ttype_dictionary_match_ctx ctx = {
			ttypeval_parameter(expected, "key"),
			ttypeval_parameter(expected, "value"),
			1, state
		};
		thashtbl_each(((tdict *)value->val.v_tcompo)->items,
			       ttype_match_dictionary_entry, &ctx);
		return ctx.matches;
	}
	if (expected->kind == ttype_kind_fields) {
		if (tobj_compo_type(value) != compo_tdict)
			return 0;
		tdict *dictionary = (tdict *)value->val.v_tcompo;
		uint_objs count = ttypeval_field_count(expected);
		for (uint_objs i = 0; i < count; i++) {
			const tobj *name;
			ttypeval *field_type;
			ttypeval_field_at(expected, i, &name, &field_type);
			const tobj *field = thashtbl_get(dictionary->items, name);
			const tstr *field_name = (const tstr *)name->val.v_tcompo;
			if (!field && ttypeval_field_optional(expected,
				tstring_cstr(field_name->data))) continue;
			if (!field || !ttype_matches_internal(field, field_type, state))
				return 0;
		}
		return 1;
	}
	return 0;
}

int ttypeval_matches(const tobj *value, const ttypeval *expected)
{
	ttype_match_state state = { 0 };
	int matches = ttype_matches_internal(value, expected, &state);
	free(state.seen);
	return matches;
}

/*----------------------- Required Vtable Operations -----------------------*/

static const char *ttypeval_get_type(void)
{
	return "Type";
}

static tcompo_type ttypeval_get_code(void)
{
	return compo_ttypeval;
}

static long ttypeval_len(void *self)
{
	ttypeval *type = (ttypeval *)self;
	return (long)(type->kind == ttype_kind_enum ?
		type->enum_member_count : ttypeval_field_count(type));
}

static void *ttypeval_copy(void *self)
{
	ttypeval *source = (ttypeval *)self;
	if (source->kind == ttype_kind_recursive) {
		ttypeval_retain(source);
		return source;
	}
	ttypeval *copy = ttype_new(source->kind);
	copy->builtin = source->builtin;
	thashtbl_free(copy->definition);
	copy->definition = thashtbl_copy(source->definition);
	copy->canonical = tstring_dup(source->canonical);
	copy->canonical_hash = source->canonical_hash;
	copy->function_parameter_count = source->function_parameter_count;
	copy->function_variadic = source->function_variadic;
	copy->named_identity = source->named_identity ?
		tstring_dup(source->named_identity) : nullptr;
	copy->named_parameter_count = source->named_parameter_count;
	copy->capabilities = source->capabilities;
	if (source->named_parameter_count) {
		copy->named_parameter_names = calloc(source->named_parameter_count,
			sizeof(*copy->named_parameter_names));
		copy->named_parameter_types = calloc(source->named_parameter_count,
			sizeof(*copy->named_parameter_types));
		if (!copy->named_parameter_names || !copy->named_parameter_types) abort();
		for (uint_objs i = 0; i < source->named_parameter_count; i++) {
			copy->named_parameter_names[i] = tstring_dup(
				source->named_parameter_names[i]);
			copy->named_parameter_types[i] = ttype_definition_get_type(
				copy, tstring_cstr(copy->named_parameter_names[i]));
		}
	}
	copy->contains_instance = source->contains_instance;
	copy->contains_custom = source->contains_custom;
	copy->contains_recursive = source->contains_recursive;
	copy->parameter_name = source->parameter_name ?
		tstring_dup(source->parameter_name) : nullptr;
	copy->template_body = ttypeval_retain(source->template_body);
	copy->template_type_parameter_count = source->template_type_parameter_count;
	if (source->template_type_parameter_count) {
		copy->template_type_parameters = calloc(
			source->template_type_parameter_count,
			sizeof(*copy->template_type_parameters));
		if (!copy->template_type_parameters) abort();
		for (uint_objs i = 0; i < source->template_type_parameter_count; i++)
			copy->template_type_parameters[i] =
				tstring_dup(source->template_type_parameters[i]);
	}
	copy->template_value_parameter_count = source->template_value_parameter_count;
	if (source->template_value_parameter_count) {
		copy->template_value_parameters = calloc(
			source->template_value_parameter_count,
			sizeof(*copy->template_value_parameters));
		if (!copy->template_value_parameters) abort();
		for (uint_objs i = 0; i < source->template_value_parameter_count; i++) {
			copy->template_value_parameters[i].name = tstring_dup(
				source->template_value_parameters[i].name);
			copy->template_value_parameters[i].type = ttypeval_retain(
				source->template_value_parameters[i].type);
		}
	}
	if (source->kind == ttype_kind_exact_value) {
		tobj_set_nil(&copy->exact_value);
		tobj_copy(&copy->exact_value, &source->exact_value);
	}
	if (source->kind == ttype_kind_instance_of) {
		tobj_set_nil(&copy->instance_rule);
		tobj_copy(&copy->instance_rule, &source->instance_rule);
		if (source->instance_reference) copy->instance_reference = tstring_dup(source->instance_reference);
	}
	if (source->enum_member_count) {
		copy->enum_members = (tstring **)calloc(source->enum_member_count,
			sizeof(*copy->enum_members));
		if (!copy->enum_members) abort();
		copy->enum_member_count = source->enum_member_count;
		for (uint_objs i = 0; i < source->enum_member_count; i++)
			copy->enum_members[i] = tstring_dup(source->enum_members[i]);
	}
	if (source->optional_field_count) {
		copy->optional_fields = (tstring **)calloc(
			source->optional_field_count, sizeof(*copy->optional_fields));
		if (!copy->optional_fields) abort();
		copy->optional_field_count = source->optional_field_count;
		for (uint_objs i = 0; i < source->optional_field_count; i++)
			copy->optional_fields[i] = tstring_dup(source->optional_fields[i]);
	}
	return copy;
}

static void ttypeval_free(void *self)
{
	ttypeval *type = (ttypeval *)self;
	if (type->kind == ttype_kind_instance_of) tobj_try_clear(&type->instance_rule);
	tstring_free(type->instance_reference);
	tstring_free(type->named_identity);
	for (uint_objs i = 0; i < type->named_parameter_count; i++)
		tstring_free(type->named_parameter_names[i]);
	free(type->named_parameter_names);
	free(type->named_parameter_types);
	tstring_free(type->parameter_name);
	ttypeval_release(type->template_body);
	for (uint_objs i = 0; i < type->template_type_parameter_count; i++)
		tstring_free(type->template_type_parameters[i]);
	free(type->template_type_parameters);
	for (uint_objs i = 0; i < type->template_value_parameter_count; i++) {
		tstring_free(type->template_value_parameters[i].name);
		ttypeval_release(type->template_value_parameters[i].type);
	}
	free(type->template_value_parameters);
	if (type->kind == ttype_kind_exact_value) tobj_try_clear(&type->exact_value);
	thashtbl_free(type->definition);
	tstring_free(type->canonical);
	for (uint_objs i = 0; i < type->optional_field_count; i++)
		tstring_free(type->optional_fields[i]);
	free(type->optional_fields);
	for (uint_objs i = 0; i < type->enum_member_count; i++)
		tstring_free(type->enum_members[i]);
	free(type->enum_members);
	free(type);
}

static int ttypeval_identical(void *self, void *other)
{
	return self == other;
}

static tstring *ttypeval_tostring(void *self)
{
	ttypeval *type = (ttypeval *)self;
	if (type->kind == ttype_kind_recursive) {
		tstring *out = tstring_new("RecursiveType(");
		tstring_append_fmt(out, "%u", (unsigned)type->recursive_id);
		tstring_append_c(out, ')');
		return out;
	}
	if (type->kind == ttype_kind_any || type->kind == ttype_kind_builtin) {
		tstring *out = tstring_new("types::");
		tstring_append(out, tbuiltintype_name(type->builtin));
		return out;
	}
	if (type->kind == ttype_kind_named) {
		tstring *out = tstring_dup(type->named_identity);
		if (type->named_parameter_count) {
			tstring_append_c(out, '[');
			for (uint_objs i = 0; i < type->named_parameter_count; i++) {
				if (i) tstring_append(out, ", ");
				tstring *parameter = ttypeval_tostring(
					type->named_parameter_types[i]);
				tstring_append_ts(out, parameter);
				tstring_free(parameter);
			}
			tstring_append_c(out, ']');
		}
		return out;
	}
	if (type->kind == ttype_kind_parameter ||
	    type->kind == ttype_kind_value_parameter) {
		tstring *out = tstring_new(type->kind == ttype_kind_parameter ?
			"TypeParameter(" : "ValueParameter(");
		tstring_append_ts(out, type->parameter_name);
		tstring_append_c(out, ')');
		return out;
	}
	if (type->kind == ttype_kind_template) {
		tstring *out = tstring_new("TypeTemplate[");
		for (uint_objs i = 0; i < type->template_type_parameter_count; i++) {
			if (i) tstring_append(out, ", ");
			tstring_append_ts(out, type->template_type_parameters[i]);
		}
		if (type->template_type_parameter_count &&
		    type->template_value_parameter_count) tstring_append(out, "; ");
		for (uint_objs i = 0; i < type->template_value_parameter_count; i++) {
			if (i) tstring_append(out, ", ");
			tstring_append_ts(out, type->template_value_parameters[i].name);
		}
		tstring_append_c(out, ']');
		return out;
	}
	tstring *out = tstring_new("Type(");
	tstring_append_ts(out, type->canonical);
	tstring_append_c(out, ')');
	return out;
}

/*------------------------------- Operators --------------------------------*/

static void ttypeval_op_eq(void *self, const tobj *other,
			   int is_rhs, tobj *result)
{
	(void)is_rhs;
	int equal = ttype_is_value(other) &&
		    ttypeval_equal((ttypeval *)self,
				   (ttypeval *)other->val.v_tcompo);
	tobj_set_bool(result, equal);
}

static void ttypeval_op_ne(void *self, const tobj *other,
			   int is_rhs, tobj *result)
{
	ttypeval_op_eq(self, other, is_rhs, result);
	result->val.v_tbool = !result->val.v_tbool;
}

/*------------------------------ Capabilities ------------------------------*/

/*
 * Type supports indexed field lookup and iteration over field names. It does
 * not allow indexed mutation because Type values are immutable after their
 * recursive definition has been completed.
 */

static void type_index(void *self, const tobj *arguments,
		       uint_regs argument_count, tobj *result)
{
	ttypeval_idx((ttypeval *)self, arguments, argument_count, result);
}

static int type_next(void *self, long *position, tobj *result)
{
	return ttypeval_next_key((ttypeval *)self, position, result);
}

static const tcompo_capabilities type_capabilities = {
	.indexable = type_index,
	.iterable = type_next
};

void tformat_type(tformat_context *context, ttypeval *type)
{
	if (!type) {
		tformat_text(context, "AnyType");
		return;
	}
	if (!tformat_begin_nested(context)) return;
	if (type->kind == ttype_kind_builtin || type->kind == ttype_kind_any) {
		tformat_text(context, tbuiltintype_name(type->builtin));
	} else if (type->kind == ttype_kind_recursive) {
		tformat_text(context, "Recursive");
	} else if (type->kind == ttype_kind_named) {
		tformat_text(context, tstring_cstr(type->named_identity));
		if (type->named_parameter_count) {
			tformat_text(context, "[");
			for (uint_objs i = 0; i < type->named_parameter_count; i++) {
				if (i) tformat_text(context, ", ");
				tformat_type(context, type->named_parameter_types[i]);
			}
			tformat_text(context, "]");
		}
	} else if (type->kind == ttype_kind_fields) {
		tformat_text(context, "{");
		for (uint_objs i = 0; i < ttypeval_field_count(type) &&
		     !tformat_stopped(context); i++) {
			const tobj *key;
			ttypeval *field;
			if (!ttypeval_field_at(type, i, &key, &field)) continue;
			if (i) tformat_text(context, ", ");
			const char *name = key->type == tcompo &&
				tobj_compo_type(key) == compo_tstr ?
				tstring_cstr(((tstr *)key->val.v_tcompo)->data) : nullptr;
			tformat_text(context, name ? name : "?");
			if (name && ttypeval_field_optional(type, name))
				tformat_text(context, "?");
			tformat_text(context, ": ");
			tformat_type(context, field);
		}
		tformat_text(context, "}");
	} else if (type->kind == ttype_kind_enum) {
		tformat_text(context, "Enum[");
		for (uint_objs i = 0; i < ttypeval_enum_member_count(type) &&
		     !tformat_stopped(context); i++) {
			if (i) tformat_text(context, ", ");
			tformat_quoted(context,
				tstring_cstr(ttypeval_enum_member_at(type, i)));
		}
		tformat_text(context, "]");
	} else if (type->kind == ttype_kind_instance_of) {
		tformat_text(context, "InstanceOf[");
		if (type->instance_reference &&
		    tstring_len(type->instance_reference))
			tformat_text(context,
				tstring_cstr(type->instance_reference));
		else
			tformat_value(context, &type->instance_rule);
		tformat_text(context, "]");
	} else if (type->kind == ttype_kind_parameter ||
		   type->kind == ttype_kind_value_parameter) {
		tformat_text(context, type->kind == ttype_kind_parameter ?
			"TypeParameter[" : "ValueParameter[");
		tformat_text(context, tstring_cstr(type->parameter_name));
		tformat_text(context, "]");
	} else if (type->kind == ttype_kind_exact_value) {
		tformat_value(context, &type->exact_value);
	} else if (type->kind == ttype_kind_template) {
		tformat_text(context, "Template[");
		for (uint_objs i = 0; i < type->template_type_parameter_count; i++) {
			if (i) tformat_text(context, ", ");
			tformat_text(context,
				tstring_cstr(type->template_type_parameters[i]));
		}
		if (type->template_type_parameter_count &&
		    type->template_value_parameter_count)
			tformat_text(context, "; ");
		for (uint_objs i = 0; i < type->template_value_parameter_count; i++) {
			if (i) tformat_text(context, ", ");
			tformat_text(context, tstring_cstr(
				type->template_value_parameters[i].name));
			tformat_text(context, ": ");
			tformat_type(context,
				type->template_value_parameters[i].type);
		}
		tformat_text(context, "]");
	} else {
		const char *name = type->kind == ttype_kind_function ? "Function" :
			type->kind == ttype_kind_rule ? "Rule" :
			type->kind == ttype_kind_rule_instance ? "RuleInstance" :
			type->kind == ttype_kind_rule_term ? "RuleTerm" :
			type->kind == ttype_kind_list ? "List" :
			type->kind == ttype_kind_iterator ? "Iterator" :
			type->kind == ttype_kind_pair ? "Pair" :
			type->kind == ttype_kind_dictionary ? "Dictionary" :
			type->kind == ttype_kind_union ? "Union" : "Type";
		tformat_text(context, name);
		tformat_text(context, "[");
		if (type->kind == ttype_kind_function ||
		    type->kind == ttype_kind_rule ||
		    type->kind == ttype_kind_rule_instance) {
			if (ttypeval_function_variadic(type))
				tformat_text(context, "...");
			else for (uint_objs i = 0;
				  i < ttypeval_function_parameter_count(type) &&
				  !tformat_stopped(context); i++) {
				if (i) tformat_text(context, ", ");
				tformat_type(context,
					ttypeval_function_parameter_at(type, i));
			}
		} else if (type->kind == ttype_kind_union) {
			for (uint_objs i = 0; i < ttypeval_member_count(type) &&
			     !tformat_stopped(context); i++) {
				if (i) tformat_text(context, ", ");
				tformat_type(context, ttypeval_member_at(type, i));
			}
		} else if (type->kind == ttype_kind_pair ||
			   type->kind == ttype_kind_dictionary) {
			tformat_type(context, ttypeval_parameter(type,
				type->kind == ttype_kind_pair ? "first" : "key"));
			tformat_text(context, ", ");
			tformat_type(context, ttypeval_parameter(type,
				type->kind == ttype_kind_pair ? "second" : "value"));
		} else {
			tformat_type(context, ttypeval_parameter(type, "item"));
		}
		tformat_text(context, "]");
		if (type->kind == ttype_kind_function) {
			tformat_text(context, " -> ");
			tformat_type(context, ttypeval_function_result(type));
		}
	}
	tformat_end_nested(context);
}

tcompo_vtable ttypeval_vtable = {
	.get_type = ttypeval_get_type,
	.get_compo_type_code = ttypeval_get_code,
	.len = ttypeval_len,
	.copy = ttypeval_copy,
	.free = ttypeval_free,
	.identical = ttypeval_identical,
	.tostring_abbr = ttypeval_tostring,
	.tostring_full = ttypeval_tostring,
	.op_eq = ttypeval_op_eq,
	.op_ne = ttypeval_op_ne,
	.capabilities = &type_capabilities
};
