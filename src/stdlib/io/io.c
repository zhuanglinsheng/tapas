#define _POSIX_C_SOURCE 200809L

#include "tapas/textension.h"
#include "tapas/dsa/tstring.h"

#include "tapas/objects/tstr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>


static tstr *string_argument(const tobj *value, const char *where,
			     const char *description)
{
	if (value->type != tcompo || tobj_compo_type(value) != compo_tstr)
		twarn(ErrRuntime_ParamsType, where, description);
	return (tstr *)value->val.v_tcompo;
}

static const char *path_argument(const tobj *value, const char *where)
{
	tstr *path = string_argument(value, where, "String path required");
	const char *filename = tstring_cstr(path->data);
	if (strlen(filename) != tstring_len(path->data))
		twarn(ErrRuntime_ParamsType, where, "path contains a null byte");
	return filename;
}

static void io_read_text(tobj *arguments, uint_regs argument_count,
			 tobj *result)
{
	(void)argument_count;
	const char *filename = path_argument(&arguments[0], "io::read_text");
	FILE *file = fopen(filename, "rb");
	if (!file)
		twarn(ErrSession_IO, "io::read_text", filename);

	tstring *content = tstring_new_cap(8192);
	if (!content) {
		fclose(file);
		twarn(ErrRuntime_Other, "io::read_text", "out of memory");
	}
	char buffer[8192];
	for (;;) {
		size_t read_count = fread(buffer, 1, sizeof(buffer), file);
		if (read_count > 0) {
			size_t previous = tstring_len(content);
			tstring_append_len(content, buffer, read_count);
			if (tstring_len(content) - previous != read_count) {
				tstring_free(content);
				fclose(file);
				twarn(ErrRuntime_Other, "io::read_text",
				      "out of memory");
			}
		}
		if (read_count < sizeof(buffer))
			break;
	}
	if (ferror(file)) {
		tstring_free(content);
		fclose(file);
		twarn(ErrSession_IO, "io::read_text", filename);
	}
	if (fclose(file) == EOF) {
		tstring_free(content);
		twarn(ErrSession_IO, "io::read_text", filename);
	}

	tstr *text = tstr_new_len(tstring_cstr(content), tstring_len(content));
	tstring_free(content);
	if (!text)
		twarn(ErrRuntime_Other, "io::read_text", "out of memory");
	tobj_set_compo(result, (tcompo_v *)text);
}

static void write_text(tobj *arguments, const char *where, const char *mode,
		       tobj *result)
{
	const char *filename = path_argument(&arguments[0], where);
	tstr *text = string_argument(&arguments[1], where, "String text required");
	FILE *file = fopen(filename, mode);
	if (!file)
		twarn(ErrSession_IO, where, filename);

	size_t length = tstring_len(text->data);
	int failed = length > 0 &&
		fwrite(tstring_cstr(text->data), 1, length, file) != length;
	if (fclose(file) == EOF)
		failed = 1;
	if (failed)
		twarn(ErrSession_IO, where, filename);
	tobj_set_nil(result);
}

static void io_write_text(tobj *arguments, uint_regs argument_count,
			  tobj *result)
{
	(void)argument_count;
	write_text(arguments, "io::write_text", "wb", result);
}

static void io_append_text(tobj *arguments, uint_regs argument_count,
			   tobj *result)
{
	(void)argument_count;
	write_text(arguments, "io::append_text", "ab", result);
}

static void io_replace_text(tobj *arguments, uint_regs argument_count,
			    tobj *result)
{
	(void)argument_count;
	const char *filename = path_argument(&arguments[0], "io::replace_text");
	tstr *text = string_argument(&arguments[1], "io::replace_text",
			     "String text required");
	size_t template_length = strlen(filename) + sizeof(".tmp.XXXXXX");
	char *temporary = (char *)malloc(template_length);
	if (!temporary)
		twarn(ErrRuntime_Other, "io::replace_text", "out of memory");
	snprintf(temporary, template_length, "%s.tmp.XXXXXX", filename);

	struct stat metadata;
	int existing = stat(filename, &metadata) == 0;
	int descriptor = mkstemp(temporary);
	if (descriptor < 0) {
		free(temporary);
		twarn(ErrSession_IO, "io::replace_text", filename);
	}
	if (existing && fchmod(descriptor, metadata.st_mode & 07777) != 0) {
		close(descriptor);
		unlink(temporary);
		free(temporary);
		twarn(ErrSession_IO, "io::replace_text", filename);
	}

	FILE *file = fdopen(descriptor, "wb");
	if (!file) {
		close(descriptor);
		unlink(temporary);
		free(temporary);
		twarn(ErrSession_IO, "io::replace_text", filename);
	}
	size_t length = tstring_len(text->data);
	int failed = length > 0 &&
		fwrite(tstring_cstr(text->data), 1, length, file) != length;
	if (!failed && fflush(file) == EOF)
		failed = 1;
	if (!failed && fsync(descriptor) != 0)
		failed = 1;
	if (fclose(file) == EOF)
		failed = 1;
	if (!failed && rename(temporary, filename) != 0)
		failed = 1;
	if (failed)
		unlink(temporary);
	free(temporary);
	if (failed)
		twarn(ErrSession_IO, "io::replace_text", filename);
	tobj_set_nil(result);
}

static const textension_symbol symbols[] = {
	{
		.name = "read_text",
		.type = "Function[String] -> String",
		.detail = "io::read_text(path: String) -> String",
		.kind = textension_function,
		.function = io_read_text,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "write_text",
		.type = "Function[String, String] -> Nil",
		.detail = "io::write_text(path: String, text: String) -> Nil",
		.kind = textension_function,
		.function = io_write_text,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "append_text",
		.type = "Function[String, String] -> Nil",
		.detail = "io::append_text(path: String, text: String) -> Nil",
		.kind = textension_function,
		.function = io_append_text,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "replace_text",
		.type = "Function[String, String] -> Nil",
		.detail = "io::replace_text(path: String, text: String) -> Nil",
		.kind = textension_function,
		.function = io_replace_text,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	}
};

const textension_module tstdlib_io_module = {
	.scope = textension_package,
	.name = "io",
	.detail = "Text input and output",
	.symbols = symbols,
	.symbol_count = sizeof(symbols) / sizeof(symbols[0])
};
