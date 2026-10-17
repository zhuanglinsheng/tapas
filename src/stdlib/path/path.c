/* Lexical POSIX paths: no filesystem access, shell expansion, or resolution. */
#include "tapas/textension.h"
#include "../text.h"

#include <stdlib.h>
#include <string.h>

typedef struct { const char *data; size_t length; } path_text;

static path_text path_argument(const tobj *value, const char *where)
{
	tstr *string = tstdlib_string_arg(value, where);
	path_text path = {tstring_cstr(string->data), tstring_len(string->data)};
	if (memchr(path.data, 0, path.length))
		twarn(ErrRuntime_ParamsType, where, "path contains an embedded NUL");
	return path;
}

static size_t trimmed_end(path_text path)
{
	size_t end = path.length;
	while (end > 1 && path.data[end - 1] == '/') end--;
	return end;
}

static void path_is_absolute(tobj *params, uint_regs len, tobj *result)
{
	(void)len;
	path_text path = path_argument(&params[0], "path::is_absolute");
	tobj_set_bool(result, path.length && path.data[0] == '/');
}

static void path_join(tobj *params, uint_regs len, tobj *result)
{
	/* Validate and size first, so bad arguments cannot leak a partial buffer. */
	size_t length = 0;
	unsigned first = 0;
	for (unsigned i = 0; i < len; i++) {
		path_text part = path_argument(&params[i], "path::join");
		if (part.length && part.data[0] == '/') { length = 0; first = i; }
		if (part.length >= SIZE_MAX - length - 1)
			twarn(ErrRuntime_Other, "path::join", "path is too large");
		length += part.length + 1;
	}
	char *output = malloc(length + 1);
	if (!output) twarn(ErrRuntime_Other, "path::join", "out of memory");
	size_t cursor = 0;
	for (unsigned i = first; i < len; i++) {
		path_text part = path_argument(&params[i], "path::join");
		if (!part.length) continue;
		if (cursor && output[cursor - 1] != '/') output[cursor++] = '/';
		memcpy(output + cursor, part.data, part.length);
		cursor += part.length;
	}
	tstr *string = tstr_new_len(output, cursor);
	free(output);
	if (!string) twarn(ErrRuntime_Other, "path::join", "out of memory");
	tobj_set_compo(result, (tcompo_v *)string);
}

static void path_basename(tobj *params, uint_regs len, tobj *result)
{
	(void)len;
	path_text path = path_argument(&params[0], "path::basename");
	size_t end = trimmed_end(path), start = end;
	while (start && path.data[start - 1] != '/') start--;
	if (end == 1 && path.data[0] == '/') start = 0;
	tstdlib_string_result(result, path.data + start, end - start, "path::basename");
}

static void path_dirname(tobj *params, uint_regs len, tobj *result)
{
	(void)len;
	path_text path = path_argument(&params[0], "path::dirname");
	size_t end = trimmed_end(path);
	while (end && path.data[end - 1] != '/') end--;
	if (!end) {
		tstdlib_string_result(result, ".", 1, "path::dirname");
		return;
	}
	while (end > 1 && path.data[end - 1] == '/') end--;
	tstdlib_string_result(result, path.data, end, "path::dirname");
}

static void path_extension(tobj *params, uint_regs len, tobj *result)
{
	(void)len;
	path_text path = path_argument(&params[0], "path::extension");
	size_t end = trimmed_end(path), start = end;
	while (start && path.data[start - 1] != '/') start--;
	size_t dot = end;
	while (dot > start && path.data[dot - 1] != '.') dot--;
	size_t first_non_dot = start;
	while (first_non_dot < end && path.data[first_non_dot] == '.') first_non_dot++;
	if (!dot || dot - 1 <= first_non_dot)
		tstdlib_string_result(result, "", 0, "path::extension");
	else
		tstdlib_string_result(result, path.data + dot - 1, end - dot + 1, "path::extension");
}

static void path_normalize(tobj *params, uint_regs len, tobj *result)
{
	(void)len;
	path_text path = path_argument(&params[0], "path::normalize");
	if (path.length > (SIZE_MAX / sizeof(size_t)) - 1)
		twarn(ErrRuntime_Other, "path::normalize", "path is too large");
	char *output = malloc(path.length + 2);
	size_t *starts = malloc((path.length + 1) * sizeof(size_t));
	if (!output || !starts) {
		free(output); free(starts);
		twarn(ErrRuntime_Other, "path::normalize", "out of memory");
	}
	int absolute = path.length && path.data[0] == '/';
	size_t cursor = absolute ? 1 : 0, count = 0, i = 0;
	if (absolute) output[0] = '/';
	while (i < path.length) {
		while (i < path.length && path.data[i] == '/') i++;
		size_t start = i;
		while (i < path.length && path.data[i] != '/') i++;
		size_t length = i - start;
		if (!length || (length == 1 && path.data[start] == '.')) continue;
		int parent = length == 2 && path.data[start] == '.' && path.data[start + 1] == '.';
		if (parent && count) {
			size_t previous = starts[count - 1];
			if (!(cursor - previous == 2 && output[previous] == '.' && output[previous + 1] == '.')) {
				cursor = previous;
				if (cursor > (absolute ? 1u : 0u)) cursor--;
				count--;
				continue;
			}
		}
		if (parent && absolute) continue;
		if (cursor && output[cursor - 1] != '/') output[cursor++] = '/';
		starts[count++] = cursor;
		memcpy(output + cursor, path.data + start, length);
		cursor += length;
	}
	if (!cursor) output[cursor++] = '.';
	tstr *string = tstr_new_len(output, cursor);
	free(output); free(starts);
	if (!string) twarn(ErrRuntime_Other, "path::normalize", "out of memory");
	tobj_set_compo(result, (tcompo_v *)string);
}

static const textension_symbol symbols[] = {
	{
		.name = "join", .type = "Function[...] -> String",
		.detail = "path::join(...parts: String) -> String",
		.kind = textension_function, .function = path_join,
		.minimum_arguments = 1, .maximum_arguments = 255
	},
	{
		.name = "normalize", .type = "Function[String] -> String",
		.detail = "path::normalize(value: String) -> String",
		.kind = textension_function, .function = path_normalize,
		.minimum_arguments = 1, .maximum_arguments = 1
	},
	{
		.name = "basename", .type = "Function[String] -> String",
		.detail = "path::basename(value: String) -> String",
		.kind = textension_function, .function = path_basename,
		.minimum_arguments = 1, .maximum_arguments = 1
	},
	{
		.name = "dirname", .type = "Function[String] -> String",
		.detail = "path::dirname(value: String) -> String",
		.kind = textension_function, .function = path_dirname,
		.minimum_arguments = 1, .maximum_arguments = 1
	},
	{
		.name = "extension", .type = "Function[String] -> String",
		.detail = "path::extension(value: String) -> String",
		.kind = textension_function, .function = path_extension,
		.minimum_arguments = 1, .maximum_arguments = 1
	},
	{
		.name = "is_absolute", .type = "Function[String] -> Bool",
		.detail = "path::is_absolute(value: String) -> Bool",
		.kind = textension_function, .function = path_is_absolute,
		.minimum_arguments = 1, .maximum_arguments = 1
	},
};

const textension_module tstdlib_path_module = {
	.scope = textension_package, .name = "path",
	.detail = "Lexical POSIX path operations",
	.symbols = symbols, .symbol_count = sizeof(symbols) / sizeof(symbols[0])
};
