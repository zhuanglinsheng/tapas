#include "json.h"
#include "tapas/dsa/tstring.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct { const char *at; const char *error; uint32_t depth; } parser;

static void skip_space(parser *p)
{
	while (isspace((unsigned char)*p->at))
		p->at++;
}

static tjson_value *value_new(tjson_kind kind)
{
	tjson_value *value = (tjson_value *)calloc(1, sizeof(*value));
	if (!value)
		abort();
	value->kind = kind;
	return value;
}

static int hex_digit(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

static void append_utf8(tstring *out, uint32_t scalar)
{
	if (scalar <= 0x7f)
		tstring_append_c(out, (char)scalar);
	else if (scalar <= 0x7ff) {
		tstring_append_c(out, (char)(0xc0 | (scalar >> 6)));
		tstring_append_c(out, (char)(0x80 | (scalar & 0x3f)));
	} else if (scalar <= 0xffff) {
		tstring_append_c(out, (char)(0xe0 | (scalar >> 12)));
		tstring_append_c(out, (char)(0x80 | ((scalar >> 6) & 0x3f)));
		tstring_append_c(out, (char)(0x80 | (scalar & 0x3f)));
	} else {
		tstring_append_c(out, (char)(0xf0 | (scalar >> 18)));
		tstring_append_c(out, (char)(0x80 | ((scalar >> 12) & 0x3f)));
		tstring_append_c(out, (char)(0x80 | ((scalar >> 6) & 0x3f)));
		tstring_append_c(out, (char)(0x80 | (scalar & 0x3f)));
	}
}

static tstring *parse_string(parser *p)
{
	if (*p->at++ != '"')
		return nullptr;
	tstring *out = tstring_new_empty();
	while (*p->at && *p->at != '"') {
		unsigned char c = (unsigned char)*p->at++;
		if (c < 0x20) {
			p->error = "control character in string";
			break;
		}
		if (c != '\\') {
			tstring_append_c(out, (char)c);
			continue;
		}
		char escaped = *p->at++;
		switch (escaped) {
		case '"': case '\\': case '/': tstring_append_c(out, escaped); break;
		case 'b': tstring_append_c(out, '\b'); break;
		case 'f': tstring_append_c(out, '\f'); break;
		case 'n': tstring_append_c(out, '\n'); break;
		case 'r': tstring_append_c(out, '\r'); break;
		case 't': tstring_append_c(out, '\t'); break;
		case 'u': {
			uint32_t scalar = 0;
			for (int i = 0; i < 4; i++) {
				int digit = hex_digit(*p->at++);
				if (digit < 0) { p->error = "invalid unicode escape"; break; }
				scalar = scalar * 16 + (uint32_t)digit;
			}
			if (!p->error)
				append_utf8(out, scalar);
		} break;
		default: p->error = "invalid string escape"; break;
		}
		if (p->error)
			break;
	}
	if (!p->error && *p->at == '"')
		p->at++;
	else if (!p->error)
		p->error = "unterminated string";
	if (p->error) {
		tstring_free(out);
		return nullptr;
	}
	return out;
}

static tjson_value *parse_value(parser *p);

static tjson_value *parse_array(parser *p)
{
	p->at++;
	tjson_value *array = value_new(tjson_array);
	skip_space(p);
	if (*p->at == ']') { p->at++; return array; }
	while (!p->error) {
		tjson_value *item = parse_value(p);
		if (!item) break;
		tjson_value **items = (tjson_value **)realloc(array->array.items,
			(array->array.count + 1) * sizeof(*items));
		if (!items) abort();
		array->array.items = items;
		array->array.items[array->array.count++] = item;
		skip_space(p);
		if (*p->at == ']') { p->at++; return array; }
		if (*p->at++ != ',') { p->error = "expected comma in array"; break; }
		skip_space(p);
	}
	tjson_free(array);
	return nullptr;
}

static tjson_value *parse_object(parser *p)
{
	p->at++;
	tjson_value *object = value_new(tjson_object);
	tjson_member **tail = &object->object;
	skip_space(p);
	if (*p->at == '}') { p->at++; return object; }
	while (!p->error) {
		if (*p->at != '"') { p->error = "expected object key"; break; }
		tstring *key = parse_string(p);
		skip_space(p);
		if (!key || *p->at++ != ':') {
			tstring_free(key);
			if (!p->error) p->error = "expected colon";
			break;
		}
		skip_space(p);
		tjson_value *item = parse_value(p);
		if (!item) { tstring_free(key); break; }
		tjson_member *member = (tjson_member *)calloc(1, sizeof(*member));
		if (!member) abort();
		member->key = key;
		member->value = item;
		*tail = member;
		tail = &member->next;
		skip_space(p);
		if (*p->at == '}') { p->at++; return object; }
		if (*p->at++ != ',') { p->error = "expected comma in object"; break; }
		skip_space(p);
	}
	tjson_free(object);
	return nullptr;
}

static tjson_value *parse_value(parser *p)
{
	if (++p->depth > 256) { p->error = "JSON nesting limit exceeded"; return nullptr; }
	skip_space(p);
	tjson_value *value = nullptr;
	if (*p->at == '{') value = parse_object(p);
	else if (*p->at == '[') value = parse_array(p);
	else if (*p->at == '"') {
		value = value_new(tjson_string);
		value->string = parse_string(p);
		if (!value->string) { free(value); value = nullptr; }
	} else if (!strncmp(p->at, "true", 4)) {
		p->at += 4; value = value_new(tjson_boolean); value->boolean = 1;
	} else if (!strncmp(p->at, "false", 5)) {
		p->at += 5; value = value_new(tjson_boolean);
	} else if (!strncmp(p->at, "null", 4)) {
		p->at += 4; value = value_new(tjson_null);
	} else {
		char *end = nullptr;
		double number = strtod(p->at, &end);
		if (end == p->at) p->error = "expected JSON value";
		else { p->at = end; value = value_new(tjson_number); value->number = number; }
	}
	p->depth--;
	return value;
}

tjson_value *tjson_parse(const char *text, const char **error)
{
	parser p = { .at = text ? text : "" };
	tjson_value *value = parse_value(&p);
	skip_space(&p);
	if (value && *p.at) { p.error = "unexpected content after JSON value"; tjson_free(value); value = nullptr; }
	if (error) *error = p.error;
	return value;
}

void tjson_free(tjson_value *value)
{
	if (!value) return;
	if (value->kind == tjson_string) tstring_free(value->string);
	else if (value->kind == tjson_array) {
		for (uint32_t i = 0; i < value->array.count; i++) tjson_free(value->array.items[i]);
		free(value->array.items);
	} else if (value->kind == tjson_object) {
		tjson_member *member = value->object;
		while (member) { tjson_member *next = member->next; tstring_free(member->key); tjson_free(member->value); free(member); member = next; }
	}
	free(value);
}

const tjson_value *tjson_get(const tjson_value *object, const char *key)
{
	if (!object || object->kind != tjson_object) return nullptr;
	for (tjson_member *member = object->object; member; member = member->next)
		if (tstring_eq_cstr(member->key, key)) return member->value;
	return nullptr;
}

const char *tjson_string_value(const tjson_value *value)
{
	return value && value->kind == tjson_string ? tstring_cstr(value->string) : nullptr;
}

int tjson_integer_value(const tjson_value *value, int fallback)
{
	return value && value->kind == tjson_number ? (int)value->number : fallback;
}

void tjson_append_escaped(tstring *output, const char *text)
{
	tstring_append_c(output, '"');
	for (const unsigned char *at = (const unsigned char *)(text ? text : ""); *at; at++) {
		switch (*at) {
		case '"': tstring_append(output, "\\\""); break;
		case '\\': tstring_append(output, "\\\\"); break;
		case '\b': tstring_append(output, "\\b"); break;
		case '\f': tstring_append(output, "\\f"); break;
		case '\n': tstring_append(output, "\\n"); break;
		case '\r': tstring_append(output, "\\r"); break;
		case '\t': tstring_append(output, "\\t"); break;
		default:
			if (*at < 0x20) tstring_append_fmt(output, "\\u%04x", *at);
			else tstring_append_c(output, (char)*at);
		}
	}
	tstring_append_c(output, '"');
}

void tjson_append_value(tstring *output, const tjson_value *value)
{
	if (!value) { tstring_append(output, "null"); return; }
	switch (value->kind) {
	case tjson_null: tstring_append(output, "null"); break;
	case tjson_boolean: tstring_append(output, value->boolean ? "true" : "false"); break;
	case tjson_number: tstring_append_fmt(output, "%.17g", value->number); break;
	case tjson_string: tjson_append_escaped(output, tstring_cstr(value->string)); break;
	default: tstring_append(output, "null"); break;
	}
}
