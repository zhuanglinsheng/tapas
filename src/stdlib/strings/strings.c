#include "tapas/textension.h"
#include "tapas/objects/tlist.h"
#include "../text.h"
#include "../arguments.h"
#include "../iterables.h"
#include "join.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* KMP retains linear worst-case scanning for repeated-prefix delimiters. */
typedef struct {
	const char *data;
	size_t length;
	size_t *prefix;
} pattern;

static pattern make_pattern(tstr *string, const char *where)
{
	pattern p = {tstring_cstr(string->data), tstring_len(string->data), nullptr};
	if (!p.length)
		twarn(ErrRuntime_ParamsType, where, "separator/search pattern must not be empty");
	if (p.length > SIZE_MAX / sizeof(size_t))
		twarn(ErrRuntime_Other, where, "pattern is too large");
	p.prefix = malloc(p.length * sizeof(size_t));
	if (!p.prefix)
		twarn(ErrRuntime_Other, where, "out of memory");
	p.prefix[0] = 0;
	for (size_t i = 1, matched = 0; i < p.length; i++) {
		while (matched && p.data[i] != p.data[matched])
			matched = p.prefix[matched - 1];
		if (p.data[i] == p.data[matched]) matched++;
		p.prefix[i] = matched;
	}
	return p;
}

static size_t find_pattern(const char *text, size_t length, size_t start,
	const pattern *p)
{
	for (size_t i = start, matched = 0; i < length; i++) {
		while (matched && text[i] != p->data[matched])
			matched = p->prefix[matched - 1];
		if (text[i] == p->data[matched]) matched++;
		if (matched == p->length) return i + 1 - matched;
	}
	return SIZE_MAX;
}

static void append_piece(tlist *list, const char *text, size_t length)
{
	tobj piece = {0};
	tstdlib_string_result(&piece, text, length, "strings");
	tobj_vec_push(&list->items, &piece);
	tobj_try_clear(&piece);
}

static void strings_split(tobj *params, uint_regs len, tobj *result)
{
	(void)len;
	tstr *source = tstdlib_string_arg(&params[0], "strings::split");
	tstr *separator = tstdlib_string_arg(&params[1], "strings::split");
	pattern p = make_pattern(separator, "strings::split");
	const char *text = tstring_cstr(source->data);
	size_t length = tstring_len(source->data), start = 0, found;
	tlist *parts = tlist_new();
	while ((found = find_pattern(text, length, start, &p)) != SIZE_MAX) {
		append_piece(parts, text + start, found - start);
		start = found + p.length;
	}
	append_piece(parts, text + start, length - start);
	free(p.prefix);
	tobj_set_compo(result, (tcompo_v *)parts);
}

static void strings_lines(tobj *params, uint_regs len, tobj *result)
{
	(void)len;
	tstr *source = tstdlib_string_arg(&params[0], "strings::lines");
	const char *text = tstring_cstr(source->data);
	size_t length = tstring_len(source->data), start = 0;
	tlist *parts = tlist_new();
	for (size_t i = 0; i < length; i++) {
		if (text[i] != '\n' && text[i] != '\r') continue;
		append_piece(parts, text + start, i - start);
		if (text[i] == '\r' && i + 1 < length && text[i + 1] == '\n') i++;
		start = i + 1;
	}
	if (start < length) append_piece(parts, text + start, length - start);
	tobj_set_compo(result, (tcompo_v *)parts);
}

static int ascii_space(unsigned char byte)
{
	return byte == ' ' || (byte >= '\t' && byte <= '\r');
}

static void strings_trim(tobj *params, uint_regs len, tobj *result)
{
	(void)len;
	tstr *source = tstdlib_string_arg(&params[0], "strings::trim");
	const char *text = tstring_cstr(source->data);
	size_t start = 0, end = tstring_len(source->data);
	while (start < end && ascii_space((unsigned char)text[start])) start++;
	while (end > start && ascii_space((unsigned char)text[end - 1])) end--;
	tstdlib_string_result(result, text + start, end - start, "strings::trim");
}

static void strings_find(tobj *params, uint_regs len, tobj *result)
{
	tstr *source = tstdlib_string_arg(&params[0], "strings::find");
	tstr *needle = tstdlib_string_arg(&params[1], "strings::find");
	size_t length = tstring_len(source->data), start = 0;
	if (len == 3) {
		if (params[2].type != tint)
			twarn(ErrRuntime_ParamsType, "strings::find", "start must be Int");
		long offset = params[2].val.v_tint;
		if (offset < 0) {
			/* Avoid negating LONG_MIN. String length is representable by len. */
			start = offset < -(long)length ? 0 : length - (size_t)(-(offset + 1)) - 1;
		} else start = (size_t)offset;
	}
	if (start > length) {
		tobj_set_int(result, -1);
		return;
	}
	if (!tstring_len(needle->data)) {
		tobj_set_int(result, (long)start);
		return;
	}
	pattern p = make_pattern(needle, "strings::find");
	size_t found = find_pattern(tstring_cstr(source->data), length, start, &p);
	free(p.prefix);
	tobj_set_int(result, found == SIZE_MAX ? -1 : (long)found);
}

static void strings_replace(tobj *params, uint_regs len, tobj *result)
{
	(void)len;
	tstr *source = tstdlib_string_arg(&params[0], "strings::replace");
	tstr *needle = tstdlib_string_arg(&params[1], "strings::replace");
	tstr *replacement = tstdlib_string_arg(&params[2], "strings::replace");
	pattern p = make_pattern(needle, "strings::replace");
	const char *text = tstring_cstr(source->data);
	size_t length = tstring_len(source->data), start = 0, found, matches = 0;
	size_t replacement_length = tstring_len(replacement->data);
	while ((found = find_pattern(text, length, start, &p)) != SIZE_MAX) {
		matches++;
		start = found + p.length;
	}
	size_t output_length = length - matches * p.length;
	if (replacement_length && matches > (SIZE_MAX - output_length - 1) / replacement_length) {
		free(p.prefix);
		twarn(ErrRuntime_Other, "strings::replace", "result is too large");
	}
	output_length += matches * replacement_length;
	char *output = malloc(output_length + 1);
	if (!output) {
		free(p.prefix);
		twarn(ErrRuntime_Other, "strings::replace", "out of memory");
	}
	size_t cursor = 0;
	start = 0;
	while ((found = find_pattern(text, length, start, &p)) != SIZE_MAX) {
		memcpy(output + cursor, text + start, found - start);
		cursor += found - start;
		memcpy(output + cursor, tstring_cstr(replacement->data), replacement_length);
		cursor += replacement_length;
		start = found + p.length;
	}
	memcpy(output + cursor, text + start, length - start);
	free(p.prefix);
	tstr *string = tstr_new_len(output, output_length);
	free(output);
	if (!string) twarn(ErrRuntime_Other, "strings::replace", "out of memory");
	tobj_set_compo(result, (tcompo_v *)string);
}

static void check_edge(tobj *params, tobj *result, int suffix)
{
	const char *where = suffix ? "strings::ends_with" : "strings::starts_with";
	tstr *source = tstdlib_string_arg(&params[0], where);
	tstr *needle = tstdlib_string_arg(&params[1], where);
	size_t length = tstring_len(source->data), n = tstring_len(needle->data);
	int match = n <= length && !memcmp(tstring_cstr(source->data) +
		(suffix ? length - n : 0), tstring_cstr(needle->data), n);
	tobj_set_bool(result, match);
}
static void strings_starts_with(tobj *params, uint_regs len, tobj *result)
{
	(void)len; check_edge(params, result, 0);
}
static void strings_ends_with(tobj *params, uint_regs len, tobj *result)
{
	(void)len; check_edge(params, result, 1);
}

static void strings_is_utf8(tobj *params, uint_regs len, tobj *result)
{
	(void)len;
	tstr *source = tstdlib_string_arg(&params[0], "strings::is_utf8");
	const unsigned char *text = (const unsigned char *)tstring_cstr(source->data);
	size_t length = tstring_len(source->data), i = 0;
	int valid = 1;
	while (i < length) {
		unsigned char first = text[i++];
		if (first < 0x80) continue;
		unsigned count, codepoint, minimum;
		if (first >= 0xC2 && first <= 0xDF) { count = 1; codepoint = first & 31; minimum = 0x80; }
		else if (first >= 0xE0 && first <= 0xEF) { count = 2; codepoint = first & 15; minimum = 0x800; }
		else if (first >= 0xF0 && first <= 0xF4) { count = 3; codepoint = first & 7; minimum = 0x10000; }
		else { valid = 0; break; }
		if (length - i < count) { valid = 0; break; }
		for (unsigned n = 0; n < count; n++) {
			unsigned char next = text[i++];
			if ((next & 0xC0) != 0x80) { valid = 0; break; }
			codepoint = (codepoint << 6) | (next & 63);
		}
		if (!valid || codepoint < minimum || codepoint > 0x10FFFF ||
			(codepoint >= 0xD800 && codepoint <= 0xDFFF)) { valid = 0; break; }
	}
	tobj_set_bool(result, valid);
}

void tstdlib_strings_join(tobj *params, uint_regs len, tobj *vre)
{
	tstdlib_require_arguments("strings::join", len, 2);
	tcompo_v *iterable;
	if (params[0].type == tcompo && params[0].val.v_tcompo &&
	    params[0].val.v_tcompo->vtable == &tlist_vtable)
		iterable = params[0].val.v_tcompo;
	else
		iterable = tstdlib_require_iterable(&params[0], "strings::join");
	if (params[1].type != tcompo ||
	    tobj_compo_type(&params[1]) != compo_tstr)
		twarn(ErrRuntime_ParamsType, "strings::join", "String separator required");
	tstring *separator = ((tstr *)params[1].val.v_tcompo)->data;
	tstring *out = tstring_new("");
	if (!out)
		twarn(ErrRuntime_Other, "strings::join", "out of memory");
	/* Select the storage path once, outside the element loop. */
	if (iterable->vtable == &tlist_vtable) {
		tlist *list = (tlist *)iterable;
		for (uint_count i = 0; i < list->items.len; i++) {
			const tobj *element = &list->items.data[i];
			if (element->type != tcompo ||
			    tobj_compo_type(element) != compo_tstr) {
				tstring_free(out);
				twarn(ErrRuntime_ParamsType, "strings::join",
				      "String elements required");
			}
			if (i > 0)
				tstring_append_ts(out, separator);
			tstring_append_ts(out,
				((tstr *)element->val.v_tcompo)->data);
		}
	} else {
		long position = 0;
		int first = 1;
		tobj item;
		tobj_set_nil(&item);
		while (tcompo_next(iterable, &position, &item)) {
			if (tobj_compo_type(&item) != compo_tstr) {
				tobj_ddc_ref_clear(&item);
				tstring_free(out);
				twarn(ErrRuntime_ParamsType, "strings::join",
				      "String elements required");
			}
			if (!first)
				tstring_append_ts(out, separator);
			tstring_append_ts(out, ((tstr *)item.val.v_tcompo)->data);
			tobj_ddc_ref_clear(&item);
			first = 0;
		}
	}
	tobj_set_compo(vre, (tcompo_v *)tstr_new_len(
		tstring_cstr(out), tstring_len(out)));
	tstring_free(out);
}


static const textension_symbol symbols[] = {
	{
		.name = "join", .type = "Function[Iterable, String] -> String",
		.detail = "strings::join(values: Iterable, separator: String) -> String",
		.kind = textension_function, .function = tstdlib_strings_join,
		.minimum_arguments = 2, .maximum_arguments = 2
	},
	{
		.name = "split", .type = "Function[String, String] -> List",
		.detail = "strings::split(text: String, separator: String) -> List",
		.kind = textension_function, .function = strings_split,
		.minimum_arguments = 2, .maximum_arguments = 2
	},
	{
		.name = "lines", .type = "Function[String] -> List",
		.detail = "strings::lines(text: String) -> List",
		.kind = textension_function, .function = strings_lines,
		.minimum_arguments = 1, .maximum_arguments = 1
	},
	{
		.name = "trim", .type = "Function[String] -> String",
		.detail = "strings::trim(text: String) -> String",
		.kind = textension_function, .function = strings_trim,
		.minimum_arguments = 1, .maximum_arguments = 1
	},
	{
		.name = "find", .type = "Function[...] -> Int",
		.detail = "strings::find(text: String, needle: String, [start: Int]) -> Int",
		.kind = textension_function, .function = strings_find,
		.minimum_arguments = 2, .maximum_arguments = 3
	},
	{
		.name = "replace", .type = "Function[String, String, String] -> String",
		.detail = "strings::replace(text: String, old: String, replacement: String) -> String",
		.kind = textension_function, .function = strings_replace,
		.minimum_arguments = 3, .maximum_arguments = 3
	},
	{
		.name = "starts_with", .type = "Function[String, String] -> Bool",
		.detail = "strings::starts_with(text: String, prefix: String) -> Bool",
		.kind = textension_function, .function = strings_starts_with,
		.minimum_arguments = 2, .maximum_arguments = 2
	},
	{
		.name = "ends_with", .type = "Function[String, String] -> Bool",
		.detail = "strings::ends_with(text: String, suffix: String) -> Bool",
		.kind = textension_function, .function = strings_ends_with,
		.minimum_arguments = 2, .maximum_arguments = 2
	},
	{
		.name = "is_utf8", .type = "Function[String] -> Bool",
		.detail = "strings::is_utf8(text: String) -> Bool",
		.kind = textension_function, .function = strings_is_utf8,
		.minimum_arguments = 1, .maximum_arguments = 1
	},
};

const textension_module tstdlib_strings_module = {
	.scope = textension_package, .name = "strings",
	.detail = "Byte-preserving string operations and UTF-8 validation",
	.symbols = symbols, .symbol_count = sizeof(symbols) / sizeof(symbols[0])
};
