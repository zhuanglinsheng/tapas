/* JSON syntax belongs to yyjson; Tapas owns value mapping and diagnostics. */
#include "tapas/textension.h"
#include "tapas/dsa/thashtbl.h"
#include "tapas/dsa/tstring.h"
#include "tapas/objects/tdict.h"
#include "tapas/objects/tlist.h"
#include "tapas/objects/tstr.h"
#include "tapas/objects/ttype.h"
#include "yyjson.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define JSON_MAX_DEPTH 256

static tcompo_vtable null_vtable;

static const char *null_type(void) { return "json::Null"; }
static long null_len(void *self) { (void)self; return 1; }
static void *null_copy(void *self)
{
	(void)self;
	tcompo_v *value = calloc(1, sizeof(*value));
	if (!value)
		twarn(ErrRuntime_Other, "json::null", "out of memory");
	value->vtable = &null_vtable;
	return value;
}
static int null_identical(void *self, void *other)
{
	(void)self; (void)other;
	return 1;
}
static tstring *null_string(void *self)
{
	(void)self;
	return tstring_new("json::null");
}
static void null_eq(void *self, const tobj *other, int rhs, tobj *result)
{
	(void)self; (void)rhs;
	tobj_set_bool(result, other->type == tcompo &&
		other->val.v_tcompo->vtable == &null_vtable);
}
static void null_ne(void *self, const tobj *other, int rhs, tobj *result)
{
	null_eq(self, other, rhs, result);
	result->val.v_tbool = !result->val.v_tbool;
}
static tcompo_vtable null_vtable = {
	.get_type = null_type,
	.compo_code = compo_extension,
	.len = null_len,
	.copy = null_copy,
	.free = free,
	.identical = null_identical,
	.tostring_abbr = null_string,
	.tostring_full = null_string,
	.op_eq = null_eq,
	.op_ne = null_ne
};
static void create_null(tobj *result)
{
	tobj_set_compo(result, null_copy(nullptr));
}
static void create_null_type(tobj *result)
{
	tobj_set_compo(result, (tcompo_v *)ttypeval_new_named("json::Null"));
}

typedef struct {
	char message[256];
	long position; /* Zero-based byte offset; -1 for mapping errors. */
} json_error;

static int fail(json_error *error, const char *message)
{
	if (!error->message[0])
		snprintf(error->message, sizeof(error->message), "%s", message);
	return 0;
}

static int decode(yyjson_val *value, unsigned depth, tobj *out, json_error *error)
{
	if (yyjson_is_null(value)) {
		create_null(out);
		return 1;
	}
	if (yyjson_is_bool(value)) {
		tobj_set_bool(out, yyjson_get_bool(value));
		return 1;
	}
	if (yyjson_is_uint(value)) {
		uint64_t number = yyjson_get_uint(value);
		if (number > (uint64_t)LONG_MAX)
			return fail(error, "integer is outside Tapas Int range");
		tobj_set_int(out, (long)number);
		return 1;
	}
	if (yyjson_is_sint(value)) {
		int64_t number = yyjson_get_sint(value);
		if (number < LONG_MIN || number > LONG_MAX)
			return fail(error, "integer is outside Tapas Int range");
		tobj_set_int(out, (long)number);
		return 1;
	}
	if (yyjson_is_real(value)) {
		tobj_set_float(out, yyjson_get_real(value));
		return 1;
	}
	if (yyjson_is_raw(value))
		return fail(error, "number is outside supported numeric range");
	if (yyjson_is_str(value)) {
		tstr *string = tstr_new_len(yyjson_get_str(value), yyjson_get_len(value));
		if (!string)
			return fail(error, "out of memory");
		tobj_set_compo(out, (tcompo_v *)string);
		return 1;
	}
	if (depth >= JSON_MAX_DEPTH)
		return fail(error, "JSON nesting exceeds 256 containers");
	if (yyjson_get_len(value) > UINT32_MAX)
		return fail(error, "JSON container exceeds Tapas capacity");

	size_t index, count;
	yyjson_val *item;
	if (yyjson_is_arr(value)) {
		tlist *list = tlist_new_sized((uint_count)yyjson_arr_size(value));
		tobj_set_compo(out, (tcompo_v *)list);
		yyjson_arr_foreach(value, index, count, item) {
			tobj child = {0};
			int ok = decode(item, depth + 1, &child, error);
			if (ok)
				tobj_vec_push(&list->items, &child);
			tobj_try_clear(&child);
			if (!ok)
				return 0;
		}
		return 1;
	}
	tdict *dictionary = tdict_new_sized((uint_count)yyjson_obj_size(value));
	tobj_set_compo(out, (tcompo_v *)dictionary);
	yyjson_val *key;
	yyjson_obj_foreach(value, index, count, key, item) {
		tobj name = {0}, child = {0};
		tstr *string = tstr_new_len(yyjson_get_str(key), yyjson_get_len(key));
		if (!string)
			return fail(error, "out of memory");
		tobj_set_compo(&name, (tcompo_v *)string);
		int ok = !tdict_contains(dictionary, &name);
		if (!ok)
			fail(error, "duplicate JSON object key");
		else
			ok = decode(item, depth + 1, &child, error);
		if (ok)
			tdict_set(dictionary, &name, &child);
		tobj_try_clear(&name);
		tobj_try_clear(&child);
		if (!ok)
			return 0;
	}
	return 1;
}

static tstr *require_string(const tobj *value, const char *where)
{
	if (value->type != tcompo || tobj_compo_type(value) != compo_tstr)
		twarn(ErrRuntime_ParamsType, where, "String required");
	return (tstr *)value->val.v_tcompo;
}

static int parse_text(tstr *text, tobj *out, json_error *error)
{
	yyjson_read_err read_error;
	/* The default reader validates the entire input and UTF-8. Keep large
	 * numeric tokens raw so integer overflow never silently becomes Float. */
	yyjson_doc *document = yyjson_read_opts((char *)tstring_cstr(text->data),
		tstring_len(text->data), YYJSON_READ_BIGNUM_AS_RAW,
		nullptr, &read_error);
	if (!document) {
		error->position = read_error.pos > LONG_MAX ? LONG_MAX : (long)read_error.pos;
		return fail(error, read_error.msg);
	}
	int ok = decode(yyjson_doc_get_root(document), 0, out, error);
	yyjson_doc_free(document);
	if (!ok)
		tobj_try_clear(out);
	return ok;
}

static void json_parse(tobj *params, uint_regs len, tobj *result)
{
	(void)len;
	tstr *text = require_string(&params[0], "json::parse");
	json_error error = {.position = -1};
	if (!parse_text(text, result, &error)) {
		char message[320];
		if (error.position >= 0)
			snprintf(message, sizeof(message), "%s at byte %ld", error.message, error.position);
		else
			snprintf(message, sizeof(message), "%s", error.message);
		twarn(ErrRuntime_Other, "json::parse", message);
	}
}

static void set_field(tdict *dictionary, const char *name, const tobj *value)
{
	tobj key = {0};
	tstr *string = tstr_new(name);
	if (!string)
		twarn(ErrRuntime_Other, "json", "out of memory");
	tobj_set_compo(&key, (tcompo_v *)string);
	tdict_set(dictionary, &key, value);
	tobj_try_clear(&key);
}

static void json_try_parse(tobj *params, uint_regs len, tobj *result)
{
	(void)len;
	tstr *text = require_string(&params[0], "json::try_parse");
	json_error error = {.position = -1};
	tobj value = {0}, field = {0};
	int ok = parse_text(text, &value, &error);
	if (!ok)
		create_null(&value);
	tdict *record = tdict_new_sized(4);
	tobj_set_compo(result, (tcompo_v *)record);
	set_field(record, "value", &value);
	tobj_try_clear(&value);
	tobj_set_bool(&field, ok);
	set_field(record, "ok", &field);
	tobj_set_int(&field, error.position);
	set_field(record, "position", &field);
	tstr *message = tstr_new(error.message);
	if (!message)
		twarn(ErrRuntime_Other, "json::try_parse", "out of memory");
	tobj_set_compo(&field, (tcompo_v *)message);
	set_field(record, "error", &field);
	tobj_try_clear(&field);
}

typedef struct {
	yyjson_mut_doc *document;
	json_error error;
	const tcompo_v *ancestors[JSON_MAX_DEPTH];
} encode_context;

static yyjson_mut_val *encode(const tobj *value, unsigned depth, encode_context *context);

typedef struct {
	encode_context *context;
	yyjson_mut_val *object;
	unsigned depth;
} object_context;

static void encode_member(const tobj *key, const tobj *value, void *data)
{
	object_context *object = data;
	encode_context *context = object->context;
	if (context->error.message[0])
		return;
	if (key->type != tcompo || tobj_compo_type(key) != compo_tstr) {
		fail(&context->error, "JSON object keys must be String");
		return;
	}
	tstr *name = (tstr *)key->val.v_tcompo;
	yyjson_mut_val *member = yyjson_mut_strn(context->document,
		tstring_cstr(name->data), tstring_len(name->data));
	yyjson_mut_val *encoded = encode(value, object->depth + 1, context);
	if (!member || !encoded || !yyjson_mut_obj_add(object->object, member, encoded))
		fail(&context->error, "could not encode JSON object member");
}

static yyjson_mut_val *encode(const tobj *value, unsigned depth, encode_context *context)
{
	yyjson_mut_doc *document = context->document;
	switch (value->type) {
	case tbool: return yyjson_mut_bool(document, value->val.v_tbool);
	case tint: return yyjson_mut_sint(document, value->val.v_tint);
	case tfloat:
		if (!isfinite(value->val.v_tfloat)) {
			fail(&context->error, "JSON does not support NaN or infinity");
			return nullptr;
		}
		return yyjson_mut_real(document, value->val.v_tfloat);
	case tcompo: break;
	default:
		fail(&context->error, "unsupported value for JSON");
		return nullptr;
	}
	tcompo_v *composite = value->val.v_tcompo;
	if (composite->vtable == &null_vtable)
		return yyjson_mut_null(document);
	if (tobj_compo_type(value) == compo_tstr) {
		tstr *string = (tstr *)composite;
		return yyjson_mut_strn(document, tstring_cstr(string->data), tstring_len(string->data));
	}
	if (tobj_compo_type(value) != compo_tlist && tobj_compo_type(value) != compo_tdict) {
		fail(&context->error, "only List and Dictionary containers can be encoded as JSON");
		return nullptr;
	}
	if (depth >= JSON_MAX_DEPTH) {
		fail(&context->error, "JSON nesting exceeds 256 containers");
		return nullptr;
	}
	for (unsigned i = 0; i < depth; i++) {
		if (context->ancestors[i] == composite) {
			fail(&context->error, "cyclic container cannot be encoded as JSON");
			return nullptr;
		}
	}
	context->ancestors[depth] = composite;
	if (tobj_compo_type(value) == compo_tlist) {
		tlist *list = (tlist *)composite;
		yyjson_mut_val *array = yyjson_mut_arr(document);
		if (!array)
			return nullptr;
		for (uint_count i = 0; i < tlist_size(list); i++) {
			yyjson_mut_val *item = encode(tlist_at(list, i), depth + 1, context);
			if (!item || !yyjson_mut_arr_append(array, item))
				return nullptr;
		}
		return array;
	}
	yyjson_mut_val *object = yyjson_mut_obj(document);
	if (!object)
		return nullptr;
	object_context members = {context, object, depth};
	thashtbl_each(((tdict *)composite)->items, encode_member, &members);
	return context->error.message[0] ? nullptr : object;
}

static void json_stringify(tobj *params, uint_regs len, tobj *result)
{
	if (len == 2 && params[1].type != tbool)
		twarn(ErrRuntime_ParamsType, "json::stringify", "pretty must be Bool");
	encode_context context = {0};
	context.document = yyjson_mut_doc_new(nullptr);
	if (!context.document)
		twarn(ErrRuntime_Other, "json::stringify", "out of memory");
	yyjson_mut_val *root = encode(&params[0], 0, &context);
	char *text = nullptr;
	size_t length = 0;
	if (root) {
		yyjson_mut_doc_set_root(context.document, root);
		yyjson_write_err write_error;
		text = yyjson_mut_write_opts(context.document,
			len == 2 && params[1].val.v_tbool ? YYJSON_WRITE_PRETTY : 0,
			nullptr, &length, &write_error);
		if (!text)
			fail(&context.error, write_error.msg);
	}
	/* Strings borrowed from Tapas remain alive throughout yyjson writing. */
	yyjson_mut_doc_free(context.document);
	if (!text)
		twarn(ErrRuntime_Other, "json::stringify", context.error.message[0] ?
			context.error.message : "out of memory");
	tstr *output = tstr_new_len(text, length);
	free(text);
	if (!output)
		twarn(ErrRuntime_Other, "json::stringify", "out of memory");
	tobj_set_compo(result, (tcompo_v *)output);
}

static const textension_symbol symbols[] = {
	{
		.name = "Null", .type = "Type", .detail = "json::Null: Type",
		.kind = textension_type, .value_factory = create_null_type
	},
	{
		.name = "null", .type = "Null", .detail = "json::null: Null",
		.kind = textension_value, .value_factory = create_null
	},
	{
		.name = "parse", .type = "Function[String] -> AnyType",
		.detail = "json::parse(text: String) -> AnyType",
		.kind = textension_function, .function = json_parse,
		.minimum_arguments = 1, .maximum_arguments = 1
	},
	{
		.name = "try_parse", .type = "Function[String] -> Dictionary",
		.detail = "json::try_parse(text: String) -> Dictionary",
		.kind = textension_function, .function = json_try_parse,
		.minimum_arguments = 1, .maximum_arguments = 1
	},
	{
		.name = "stringify", .type = "Function[...] -> String",
		.detail = "json::stringify(value: AnyType, pretty: Bool?) -> String",
		.kind = textension_function, .function = json_stringify,
		.minimum_arguments = 1, .maximum_arguments = 2
	}
};

const textension_module tstdlib_json_module = {
	.scope = textension_package, .name = "json",
	.detail = "Strict JSON parsing and serialization",
	.symbols = symbols, .symbol_count = sizeof(symbols) / sizeof(symbols[0])
};
