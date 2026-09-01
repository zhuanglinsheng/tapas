#include "tapas/runtime/ttype.h"

#include "tapas/runtime/tdict.h"
#include "tapas/runtime/tlist.h"
#include "tapas/runtime/tpair.h"
#include "tapas/runtime/tstr.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
	const tobj *key;
	const tobj *value;
} ttype_entry;

static ttypeval *builtin_types[ttype_builtin_count];
static int builtins_initializing;
static uint32_t next_recursive_id;
static void ttypeval_free(void *self);

static const char *const builtin_names[ttype_builtin_count] = {
	"AnyType", "Nil",       "Bool",       "Int",
	"Float",   "String",    "List",       "Pair",
	"Dictionary", "Iterator", "Function",   "Library",
	"RealArray", "BoolArray", "Time",       "Type"
};

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

static void ttype_set_definition(ttypeval *owner,
				 const char *name,
				 ttypeval *value)
{
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
	return ttype_is_value(value) ? (ttypeval *)value->val.v_tcompo : NULL;
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

static ttype_entry *ttype_sorted_entries(const ttypeval *type,
					 uint_objs *count)
{
	*count = thashtbl_len(type->definition);
	if (*count == 0)
		return NULL;
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
		tstring_append_fmt(canonical, "%zu:", tstring_len(key->data));
		tstring_append_ts(canonical, key->data);
		ttype_append_canonical(canonical, field);
	}
	free(entries);
	ttype_finish_canonical(type, canonical);
}

static void ttype_init_builtins(void)
{
	if (builtin_types[ttype_builtin_any] || builtins_initializing)
		return;
	builtins_initializing = 1;

	ttypeval *any = ttype_new(ttype_kind_any);
	any->builtin = ttype_builtin_any;
	any->base.refctr = 1; /* Process-wide immutable singleton. */
	ttype_finish_canonical(any, tstring_new("A"));
	builtin_types[ttype_builtin_any] = any;

	for (int i = ttype_builtin_nil; i < ttype_builtin_count; i++) {
		ttypeval *type = ttype_new(ttype_kind_builtin);
		type->builtin = (ttype_builtin)i;
		type->base.refctr = 1;
		char definition_key[64];
		snprintf(definition_key, sizeof(definition_key), "@builtin/%s",
			 builtin_names[i]);
		ttype_set_definition(type, definition_key, any);
		tstring *canonical = tstring_new("B");
		tstring_append_fmt(canonical, "%zu:", strlen(builtin_names[i]));
		tstring_append(canonical, builtin_names[i]);
		ttype_finish_canonical(type, canonical);
		builtin_types[i] = type;
	}
	builtins_initializing = 0;
}

ttypeval *ttypeval_builtin(ttype_builtin builtin)
{
	ttype_init_builtins();
	if (builtin < 0 || builtin >= ttype_builtin_count)
		return NULL;
	return builtin_types[builtin];
}

void ttypeval_retain(ttypeval *type)
{
	if (type)
		type->base.refctr++;
}

void ttypeval_release(ttypeval *type)
{
	if (!type)
		return;
	if (type->base.refctr > 0)
		type->base.refctr--;
	if (type->base.refctr == 0)
		ttypeval_free(type);
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
		type->contains_recursive |= ttype_contains_recursive(fields[i].type);
	}
	ttype_build_fields_canonical(type);
	return type;
}

static ttypeval *ttype_new_parameterized(ttype_kind kind,
					 ttype_builtin base,
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
	type->contains_recursive = ttype_contains_recursive(first) ||
		ttype_contains_recursive(second);
	tstring *canonical = tstring_new(tag);
		ttype_append_canonical(canonical, first);
	if (second)
		ttype_append_canonical(canonical, second);
	ttype_finish_canonical(type, canonical);
	return type;
}

ttypeval *ttypeval_new_list(ttypeval *item)
{
	return ttype_new_parameterized(ttype_kind_list, ttype_builtin_list,
				       "@item", item, NULL, NULL, "L");
}

ttypeval *ttypeval_new_pair(ttypeval *first, ttypeval *second)
{
	return ttype_new_parameterized(ttype_kind_pair, ttype_builtin_pair,
				       "@first", first, "@second", second, "P");
}

ttypeval *ttypeval_new_dictionary(ttypeval *key, ttypeval *value)
{
	return ttype_new_parameterized(ttype_kind_dictionary,
				       ttype_builtin_dictionary,
				       "@key", key, "@value", value, "D");
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
			     ttypeval_builtin(ttype_builtin_function));
	tstring *canonical = tstring_new(variadic ? "FV" : "FF");
	tstring_append_fmt(canonical, "%u:", (unsigned)parameter_count);
	for (uint_objs i = 0; i < parameter_count; i++) {
		if (parameters && parameters[i]) {
			char key[32];
			snprintf(key, sizeof(key), "@parameter/%u", (unsigned)i);
			ttype_set_definition(type, key, parameters[i]);
			type->contains_recursive |=
				ttype_contains_recursive(parameters[i]);
			tstring_append_c(canonical, 'T');
			ttype_append_canonical(canonical, parameters[i]);
		} else
			tstring_append_c(canonical, '?');
	}
	if (result) {
		ttype_set_definition(type, "@result", result);
		tstring_append_c(canonical, 'R');
		ttype_append_canonical(canonical, result);
		type->contains_recursive |= ttype_contains_recursive(result);
	} else
		tstring_append_c(canonical, '?');
	ttype_finish_canonical(type, canonical);
	return type;
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
	ttypeval **members = NULL;
	uint_objs count = 0;
	uint_objs capacity = 0;
	for (uint_objs i = 0; i < input_count; i++)
		ttype_union_collect(input[i], &members, &count, &capacity);
	qsort(members, count, sizeof(ttypeval *), ttype_compare_member);

	uint_objs unique = 0;
	for (uint_objs i = 0; i < count; i++) {
		if (members[i]->kind == ttype_kind_any) {
			free(members);
			return ttypeval_builtin(ttype_builtin_any);
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
		type->contains_recursive |= ttype_contains_recursive(members[i]);
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
	if (left == right) return left != NULL;
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
	if (left->kind == ttype_kind_any || left->kind == ttype_kind_builtin)
		return left->builtin == right->builtin;
	if (left->kind == ttype_kind_function &&
	    (left->function_parameter_count != right->function_parameter_count ||
	     left->function_variadic != right->function_variadic))
		return 0;
	if (left->kind == ttype_kind_fields) {
		if (ttypeval_field_count(left) != ttypeval_field_count(right)) return 0;
		for (uint_objs i = 0; i < ttypeval_field_count(left); i++) {
			const tobj *name = NULL;
			ttypeval *member = NULL;
			ttypeval_field_at(left, i, &name, &member);
			const tstr *key = (const tstr *)name->val.v_tcompo;
			ttypeval *other = ttypeval_field_named(
				right, tstring_cstr(key->data));
			if (!other || !ttype_equal_graph(
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
	if (left == right) return left != NULL;
	if (!left || !right) return 0;
	if (!ttype_contains_recursive(left) && !ttype_contains_recursive(right))
		return tstring_cmp(left->canonical, right->canonical) == 0;
	ttype_compare_pair *seen = NULL;
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
		return NULL;
	return ttype_definition_get_type(type, name);
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
		return NULL;
	char key[32];
	snprintf(key, sizeof(key), "@union/%u", (unsigned)index);
	return ttype_definition_get_type(type, key);
}

ttypeval *ttypeval_base(const ttypeval *type)
{
	type = ttype_unwrap(type);
	if (!type)
		return NULL;
	if (type->kind != ttype_kind_list && type->kind != ttype_kind_pair &&
	    type->kind != ttype_kind_dictionary &&
	    type->kind != ttype_kind_function)
		return (ttypeval *)type;
	return ttype_definition_get_type(type, "@base");
}

ttypeval *ttypeval_parameter(const ttypeval *type, const char *name)
{
	type = ttype_unwrap(type);
	if (!type || !name)
		return NULL;
	char key[32];
	snprintf(key, sizeof(key), "@%s", name);
	return ttype_definition_get_type(type, key);
}

uint_objs ttypeval_function_parameter_count(const ttypeval *type)
{
	type = ttype_unwrap(type);
	return type && type->kind == ttype_kind_function ?
		type->function_parameter_count : 0;
}

ttypeval *ttypeval_function_parameter_at(const ttypeval *type,
					  uint_objs index)
{
	type = ttype_unwrap(type);
	if (!type || type->kind != ttype_kind_function ||
	    index >= type->function_parameter_count)
		return NULL;
	char key[32];
	snprintf(key, sizeof(key), "@parameter/%u", (unsigned)index);
	return ttype_definition_get_type(type, key);
}

ttypeval *ttypeval_function_result(const ttypeval *type)
{
	type = ttype_unwrap(type);
	return type && type->kind == ttype_kind_function ?
		ttype_definition_get_type(type, "@result") : NULL;
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
	return type ? type->definition : NULL;
}

void ttypeval_idx(ttypeval *type, const tobj *params, uint_regs np,
		  tobj *result)
{
	type = (ttypeval *)ttype_unwrap(type);
	if (np != 1)
		twarn(ErrRuntime_ParamsCtr, "Type index", "one key is required");
	if (!type || type->kind != ttype_kind_fields ||
	    params[0].type != tcompo ||
	    tobj_compo_type(&params[0]) != compo_tstr)
		twarn(ErrRuntime_ParamsType, "Type index", "String field required");
	const tobj *value = thashtbl_get(type->definition, &params[0]);
	if (!value)
		twarn(ErrRuntime_ObjUnfound, "Type index", "unknown field");
	*result = *value;
	if (result->type == tcompo && result->val.v_tcompo)
		result->val.v_tcompo->refctr++;
}

int ttypeval_next_key(const ttypeval *type, long *position, tobj *result)
{
	if (!type || type->kind != ttype_kind_fields || !position || *position < 0)
		return 0;
	const tobj *key = NULL;
	if (!ttypeval_field_at(type, (uint_objs)*position, &key, NULL))
		return 0;
	*result = *key;
	if (result->type == tcompo && result->val.v_tcompo)
		result->val.v_tcompo->refctr++;
	(*position)++;
	return 1;
}

static int ttype_builtin_matches(const tobj *value, ttype_builtin builtin)
{
	if (!value)
		return 0;
	switch (builtin) {
	case ttype_builtin_any:
		return 1;
	case ttype_builtin_nil:
		return value->type == tnil;
	case ttype_builtin_bool:
		return value->type == tbool;
	case ttype_builtin_int:
		return value->type == tint;
	case ttype_builtin_float:
		return value->type == tfloat;
	default:
		break;
	}
	if (value->type != tcompo || !value->val.v_tcompo)
		return 0;
	tcompo_type code = tobj_compo_type(value);
	switch (builtin) {
	case ttype_builtin_string:
		return code == compo_tstr;
	case ttype_builtin_list:
		return code == compo_tlist;
	case ttype_builtin_pair:
		return code == compo_tpair;
	case ttype_builtin_dictionary:
		return code == compo_tdict;
	case ttype_builtin_iterator:
		return code == compo_titer;
	case ttype_builtin_function:
		return code == compo_tfunc || code == compo_cppfunc ||
		       code == compo_sessfunc;
	case ttype_builtin_library:
		return code == compo_tlib;
	case ttype_builtin_real_array:
		return code == compo_tdarr;
	case ttype_builtin_bool_array:
		return code == compo_tbarr;
	case ttype_builtin_time:
		return code == compo_time;
	case ttype_builtin_type:
		return code == compo_ttypeval;
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
			value->val.v_tcompo : NULL;
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
	if (expected->kind == ttype_kind_builtin)
		return ttype_builtin_matches(value, expected->builtin);
	if (expected->kind == ttype_kind_function)
		return ttype_builtin_matches(value, ttype_builtin_function);
	if (expected->kind == ttype_kind_union) {
		uint_objs count = ttypeval_member_count(expected);
		for (uint_objs i = 0; i < count; i++) {
			if (ttype_matches_internal(
				value, ttypeval_member_at(expected, i), state))
				return 1;
		}
		return 0;
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
			if (!field || !ttype_matches_internal(
				field, field_type, state))
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
	return (long)ttypeval_field_count((ttypeval *)self);
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
	return copy;
}

static void ttypeval_free(void *self)
{
	ttypeval *type = (ttypeval *)self;
	thashtbl_free(type->definition);
	tstring_free(type->canonical);
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
		tstring_append(out, builtin_names[type->builtin]);
		return out;
	}
	tstring *out = tstring_new("Type(");
	tstring_append_ts(out, type->canonical);
	tstring_append_c(out, ')');
	return out;
}

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
	.op_ne = ttypeval_op_ne
};
