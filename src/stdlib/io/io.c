#define _POSIX_C_SOURCE 200809L

#include "tapas/textension.h"
#include "tapas/objects/tstr.h"
#include "../result.h"
#include "../system.h"
#include "../bytes/buffer.h"
#include "../text.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int read_text(const char *filename, tobj *result, tstdlib_error *error)
{
	FILE *file = fopen(filename, "rb");
	if (!file)
		return tstdlib_syserr(error, errno, "open");
	tstring *content = tstring_new_cap(8192);
	if (!content) {
		fclose(file);
		twarn(ErrRuntime_Other, "io", "out of memory");
	}
	char buffer[8192];
	for (;;) {
		size_t count = fread(buffer, 1, sizeof(buffer), file);
		if (count) {
			size_t previous = tstring_len(content);
			tstring_append_len(content, buffer, count);
			if (tstring_len(content) - previous != count) {
				tstring_free(content);
				fclose(file);
				twarn(ErrRuntime_Other, "io", "out of memory");
			}
		}
		if (count < sizeof(buffer))
			break;
	}
	if (ferror(file))
		tstdlib_syserr(error, errno, "read");
	if (fclose(file) == EOF && !error->code)
		tstdlib_syserr(error, errno, "close");
	if (error->code) {
		tstring_free(content);
		return 0;
	}
	tstr *text = tstr_new_len(tstring_cstr(content), tstring_len(content));
	tstring_free(content);
	if (!text)
		twarn(ErrRuntime_Other, "io", "out of memory");
	tobj_set_compo(result, (tcompo_v *)text);
	return 1;
}

static int write_data(
    const char *filename, const void *data, size_t length, int append, tstdlib_error *error)
{
	FILE *file = fopen(filename, append ? "ab" : "wb");
	if (!file)
		return tstdlib_syserr(error, errno, "open");
	if (length && fwrite(data, 1, length, file) != length)
		tstdlib_syserr(error, errno, "write");
	if (fclose(file) == EOF && !error->code)
		tstdlib_syserr(error, errno, "close");
	return !error->code;
}

static int replace_data(const char *filename, const void *data, size_t length, tstdlib_error *error)
{
	size_t path_length = strlen(filename);
	if (path_length > SIZE_MAX - sizeof(".tmp.XXXXXX"))
		return tstdlib_fail(error, "invalid_path", "path is too large");
	size_t capacity = path_length + sizeof(".tmp.XXXXXX");
	char *temporary = malloc(capacity);
	if (!temporary)
		twarn(ErrRuntime_Other, "io", "out of memory");
	snprintf(temporary, capacity, "%s.tmp.XXXXXX", filename);

	struct stat metadata;
	int existing = stat(filename, &metadata) == 0;
	if (!existing && errno != ENOENT) {
		tstdlib_syserr(error, errno, "stat");
		free(temporary);
		return 0;
	}
	int descriptor = mkstemp(temporary);
	if (descriptor < 0) {
		tstdlib_syserr(error, errno, "create temporary file");
		free(temporary);
		return 0;
	}
	if (existing && fchmod(descriptor, metadata.st_mode & 07777) != 0) {
		tstdlib_syserr(error, errno, "preserve permissions");
		close(descriptor);
		unlink(temporary);
		free(temporary);
		return 0;
	}
	FILE *file = fdopen(descriptor, "wb");
	if (!file) {
		tstdlib_syserr(error, errno, "open temporary stream");
		close(descriptor);
		unlink(temporary);
		free(temporary);
		return 0;
	}
	if (length && fwrite(data, 1, length, file) != length)
		tstdlib_syserr(error, errno, "write");
	if (!error->code && fflush(file) == EOF)
		tstdlib_syserr(error, errno, "flush");
	if (!error->code && fsync(descriptor) != 0)
		tstdlib_syserr(error, errno, "sync");
	if (fclose(file) == EOF && !error->code)
		tstdlib_syserr(error, errno, "close");
	if (!error->code && rename(temporary, filename) != 0)
		tstdlib_syserr(error, errno, "replace");
	if (error->code)
		unlink(temporary);
	free(temporary);
	return !error->code;
}

static int read_bytes(const char *filename, tobj *result, tstdlib_error *error)
{
	FILE *file = fopen(filename, "rb");
	if (!file)
		return tstdlib_syserr(error, errno, "open");
	unsigned char *data = nullptr;
	size_t length = 0, capacity = 0;
	for (;;) {
		if (capacity - length < 8192) {
			if (capacity > (size_t)LONG_MAX / 2) {
				fclose(file);
				free(data);
				return tstdlib_fail(error, "out_of_range", "file too large");
			}
			size_t next = capacity ? capacity * 2 : 8192;
			void *grown = realloc(data, next);
			if (!grown) {
				fclose(file);
				free(data);
				twarn(ErrRuntime_Other, "io::read_bytes", "out of memory");
			}
			data = grown;
			capacity = next;
		}
		size_t count = fread(data + length, 1, capacity - length, file);
		length += count;
		if (length < capacity)
			break;
	}
	if (ferror(file))
		tstdlib_syserr(error, errno, "read");
	if (fclose(file) == EOF && !error->code)
		tstdlib_syserr(error, errno, "close");
	if (error->code) {
		free(data);
		return 0;
	}
	tstdlib_bytes_take(result, data, length);
	return 1;
}

typedef enum { io_read, io_write, io_append, io_replace } io_operation;

static void perform(tobj *arguments, tobj *result, io_operation operation, int attempt, int binary,
    const char *where)
{
	tstdlib_error error = {0};
	const char *filename = tstdlib_path(&arguments[0], &error);
	const void *data = nullptr;
	size_t length = 0;
	if (!error.code && operation != io_read) {
		if (binary) {
			if (!tstdlib_bytes_data(&arguments[1], &data, &length))
				tstdlib_fail(&error, "invalid_type", "bytes::Buffer required");
		} else if (arguments[1].type != tcompo ||
			   tobj_compo_type(&arguments[1]) != compo_tstr)
			tstdlib_fail(&error, "invalid_type", "String text required");
		else {
			tstr *text = (tstr *)arguments[1].val.v_tcompo;
			data = tstring_cstr(text->data);
			length = tstring_len(text->data);
		}
	}
	tobj value = {0};
	if (operation == io_read) {
		if (binary)
			tstdlib_bytes_take(&value, nullptr, 0);
		else {
			tstr *empty = tstr_new_len("", 0);
			if (!empty)
				twarn(ErrRuntime_Other, where, "out of memory");
			tobj_set_compo(&value, (tcompo_v *)empty);
		}
	} else
		tobj_set_bool(&value, 0);
	if (!error.code) {
		if (operation == io_read) {
			if (binary)
				read_bytes(filename, &value, &error);
			else
				read_text(filename, &value, &error);
		} else {
			int ok = operation == io_replace
				     ? replace_data(filename, data, length, &error)
				     : write_data(
					   filename, data, length, operation == io_append, &error);
			tobj_set_bool(&value, ok);
		}
	}
	if (attempt) {
		tstdlib_make_result(result, &value, &error);
		tobj_try_clear(&value);
	} else if (error.code) {
		tobj_try_clear(&value);
		twarn(error.system_errno ? ErrSession_IO : ErrRuntime_ParamsType, where,
		    error.message);
	} else if (operation == io_read) {
		/* Transfer the temporary result rather than taking a second reference. */
		tobj_set_compo(result, value.val.v_tcompo);
		tobj_set_nil(&value);
	} else
		tobj_set_nil(result);
}

static void io_read_text(tobj *params, uint_regs len, tobj *result)
{
	(void)len;
	perform(params, result, io_read, 0, 0, "io::read_text");
}
static void io_write_text(tobj *params, uint_regs len, tobj *result)
{
	(void)len;
	perform(params, result, io_write, 0, 0, "io::write_text");
}
static void io_append_text(tobj *params, uint_regs len, tobj *result)
{
	(void)len;
	perform(params, result, io_append, 0, 0, "io::append_text");
}
static void io_replace_text(tobj *params, uint_regs len, tobj *result)
{
	(void)len;
	perform(params, result, io_replace, 0, 0, "io::replace_text");
}
static void io_try_read_text(tobj *params, uint_regs len, tobj *result)
{
	(void)len;
	perform(params, result, io_read, 1, 0, "io::try_read_text");
}
static void io_try_write_text(tobj *params, uint_regs len, tobj *result)
{
	(void)len;
	perform(params, result, io_write, 1, 0, "io::try_write_text");
}
static void io_try_append_text(tobj *params, uint_regs len, tobj *result)
{
	(void)len;
	perform(params, result, io_append, 1, 0, "io::try_append_text");
}
static void io_try_replace_text(tobj *params, uint_regs len, tobj *result)
{
	(void)len;
	perform(params, result, io_replace, 1, 0, "io::try_replace_text");
}

static void io_read_bytes(tobj *p, uint_regs n, tobj *r)
{
	(void)n;
	perform(p, r, io_read, 0, 1, "io::read_bytes");
}
static void io_write_bytes(tobj *p, uint_regs n, tobj *r)
{
	(void)n;
	perform(p, r, io_write, 0, 1, "io::write_bytes");
}
static void io_append_bytes(tobj *p, uint_regs n, tobj *r)
{
	(void)n;
	perform(p, r, io_append, 0, 1, "io::append_bytes");
}
static void io_replace_bytes(tobj *p, uint_regs n, tobj *r)
{
	(void)n;
	perform(p, r, io_replace, 0, 1, "io::replace_bytes");
}
static void io_try_read_bytes(tobj *p, uint_regs n, tobj *r)
{
	(void)n;
	perform(p, r, io_read, 1, 1, "io::try_read_bytes");
}
static void io_try_write_bytes(tobj *p, uint_regs n, tobj *r)
{
	(void)n;
	perform(p, r, io_write, 1, 1, "io::try_write_bytes");
}
static void io_try_append_bytes(tobj *p, uint_regs n, tobj *r)
{
	(void)n;
	perform(p, r, io_append, 1, 1, "io::try_append_bytes");
}
static void io_try_replace_bytes(tobj *p, uint_regs n, tobj *r)
{
	(void)n;
	perform(p, r, io_replace, 1, 1, "io::try_replace_bytes");
}

static FILE *standard_stream(const tobj *value, const char *where, int *descriptor)
{
	tstr *string = tstdlib_string_arg(value, where);
	const char *name = tstring_cstr(string->data);
	size_t n = tstring_len(string->data);
	if (n == 5 && !memcmp(name, "stdin", 5)) {
		*descriptor = STDIN_FILENO;
		return stdin;
	}
	if (n == 6 && !memcmp(name, "stdout", 6)) {
		*descriptor = STDOUT_FILENO;
		return stdout;
	}
	if (n == 6 && !memcmp(name, "stderr", 6)) {
		*descriptor = STDERR_FILENO;
		return stderr;
	}
	twarn(ErrRuntime_ParamsType, where, "stream must be stdin, stdout or stderr");
	return nullptr;
}
static void standard_write(tobj *params, tobj *result, FILE *stream, const char *where)
{
	tstr *string = tstdlib_string_arg(&params[0], where);
	size_t n = tstring_len(string->data);
	if (n && fwrite(tstring_cstr(string->data), 1, n, stream) != n)
		twarn(ErrSession_IO, where, "write failed");
	tobj_set_nil(result);
}
static void io_standard_write(tobj *p, uint_regs n, tobj *r)
{
	(void)n;
	standard_write(p, r, stdout, "io::write");
}
static void io_write_error(tobj *p, uint_regs n, tobj *r)
{
	(void)n;
	standard_write(p, r, stderr, "io::write_error");
}
static void io_flush(tobj *params, uint_regs count, tobj *result)
{
	int descriptor;
	FILE *stream = count ? standard_stream(&params[0], "io::flush", &descriptor) : stdout;
	if (stream == stdin)
		twarn(ErrRuntime_ParamsType, "io::flush", "only stdout/stderr can be flushed");
	if (fflush(stream) == EOF)
		twarn(ErrSession_IO, "io::flush", "flush failed");
	tobj_set_nil(result);
}
static void io_is_terminal(tobj *params, uint_regs count, tobj *result)
{
	(void)count;
	int descriptor;
	standard_stream(&params[0], "io::is_terminal", &descriptor);
	errno = 0;
	int terminal = isatty(descriptor);
	if (!terminal && errno && errno != ENOTTY)
		twarn(ErrSession_IO, "io::is_terminal", strerror(errno));
	tobj_set_bool(result, terminal);
}

static const textension_symbol symbols[] = {
    {.name = "read_text",
	.type = "Function[String] -> String",
	.detail = "io::read_text(path: String) -> String",
	.kind = textension_function,
	.function = io_read_text,
	.minimum_arguments = 1,
	.maximum_arguments = 1},
    {.name = "write_text",
	.type = "Function[String, String] -> Nil",
	.detail = "io::write_text(path: String, text: String) -> Nil",
	.kind = textension_function,
	.function = io_write_text,
	.minimum_arguments = 2,
	.maximum_arguments = 2},
    {.name = "append_text",
	.type = "Function[String, String] -> Nil",
	.detail = "io::append_text(path: String, text: String) -> Nil",
	.kind = textension_function,
	.function = io_append_text,
	.minimum_arguments = 2,
	.maximum_arguments = 2},
    {.name = "replace_text",
	.type = "Function[String, String] -> Nil",
	.detail = "io::replace_text(path: String, text: String) -> Nil",
	.kind = textension_function,
	.function = io_replace_text,
	.minimum_arguments = 2,
	.maximum_arguments = 2},
    {.name = "try_read_text",
	.type = "Function[AnyType] -> Dictionary",
	.detail = "io::try_read_text(path: AnyType) -> Dictionary",
	.kind = textension_function,
	.function = io_try_read_text,
	.minimum_arguments = 1,
	.maximum_arguments = 1},
    {.name = "try_write_text",
	.type = "Function[AnyType, AnyType] -> Dictionary",
	.detail = "io::try_write_text(path: AnyType, text: AnyType) -> Dictionary",
	.kind = textension_function,
	.function = io_try_write_text,
	.minimum_arguments = 2,
	.maximum_arguments = 2},
    {.name = "try_append_text",
	.type = "Function[AnyType, AnyType] -> Dictionary",
	.detail = "io::try_append_text(path: AnyType, text: AnyType) -> Dictionary",
	.kind = textension_function,
	.function = io_try_append_text,
	.minimum_arguments = 2,
	.maximum_arguments = 2},
    {.name = "try_replace_text",
	.type = "Function[AnyType, AnyType] -> Dictionary",
	.detail = "io::try_replace_text(path: AnyType, text: AnyType) -> Dictionary",
	.kind = textension_function,
	.function = io_try_replace_text,
	.minimum_arguments = 2,
	.maximum_arguments = 2},
    {.name = "read_bytes",
	.type = "Function[String] -> bytes::Buffer",
	.detail = "io::read_bytes(path) -> bytes::Buffer",
	.kind = textension_function,
	.function = io_read_bytes,
	.minimum_arguments = 1,
	.maximum_arguments = 1},
    {.name = "write_bytes",
	.type = "Function[String, bytes::Buffer] -> Nil",
	.detail = "io::write_bytes(path, buffer) -> Nil",
	.kind = textension_function,
	.function = io_write_bytes,
	.minimum_arguments = 2,
	.maximum_arguments = 2},
    {.name = "append_bytes",
	.type = "Function[String, bytes::Buffer] -> Nil",
	.detail = "io::append_bytes(path, buffer) -> Nil",
	.kind = textension_function,
	.function = io_append_bytes,
	.minimum_arguments = 2,
	.maximum_arguments = 2},
    {.name = "replace_bytes",
	.type = "Function[String, bytes::Buffer] -> Nil",
	.detail = "io::replace_bytes(path, buffer) -> Nil",
	.kind = textension_function,
	.function = io_replace_bytes,
	.minimum_arguments = 2,
	.maximum_arguments = 2},
    {.name = "try_read_bytes",
	.type = "Function[AnyType] -> Dictionary",
	.detail = "io::try_read_bytes(path) -> Dictionary",
	.kind = textension_function,
	.function = io_try_read_bytes,
	.minimum_arguments = 1,
	.maximum_arguments = 1},
    {.name = "try_write_bytes",
	.type = "Function[AnyType, AnyType] -> Dictionary",
	.detail = "io::try_write_bytes(path, buffer) -> Dictionary",
	.kind = textension_function,
	.function = io_try_write_bytes,
	.minimum_arguments = 2,
	.maximum_arguments = 2},
    {.name = "try_append_bytes",
	.type = "Function[AnyType, AnyType] -> Dictionary",
	.detail = "io::try_append_bytes(path, buffer) -> Dictionary",
	.kind = textension_function,
	.function = io_try_append_bytes,
	.minimum_arguments = 2,
	.maximum_arguments = 2},
    {.name = "try_replace_bytes",
	.type = "Function[AnyType, AnyType] -> Dictionary",
	.detail = "io::try_replace_bytes(path, buffer) -> Dictionary",
	.kind = textension_function,
	.function = io_try_replace_bytes,
	.minimum_arguments = 2,
	.maximum_arguments = 2},

    {.name = "write",
	.type = "Function[String] -> Nil",
	.detail = "io::write(text: String) -> Nil",
	.kind = textension_function,
	.function = io_standard_write,
	.minimum_arguments = 1,
	.maximum_arguments = 1},
    {.name = "write_error",
	.type = "Function[String] -> Nil",
	.detail = "io::write_error(text: String) -> Nil",
	.kind = textension_function,
	.function = io_write_error,
	.minimum_arguments = 1,
	.maximum_arguments = 1},
    {.name = "flush",
	.type = "Function[...] -> Nil",
	.detail = "io::flush([stream: String]) -> Nil",
	.kind = textension_function,
	.function = io_flush,
	.minimum_arguments = 0,
	.maximum_arguments = 1},
    {.name = "is_terminal",
	.type = "Function[String] -> Bool",
	.detail = "io::is_terminal(stream: String) -> Bool",
	.kind = textension_function,
	.function = io_is_terminal,
	.minimum_arguments = 1,
	.maximum_arguments = 1},

};

const textension_module tstdlib_io_module = {.scope = textension_package,
    .name = "io",
    .detail = "Text file I/O with recoverable operation results",
    .symbols = symbols,
    .symbol_count = sizeof(symbols) / sizeof(symbols[0])};
