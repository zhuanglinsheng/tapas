#define _POSIX_C_SOURCE 200809L
#include "tapas/textension.h"
#include "../system.h"
#include "../text.h"
#include <errno.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void tstdlib_os_run(tobj *params, uint_regs count, tobj *result);

static void os_getenv(tobj *params, uint_regs count, tobj *result)
{
	(void)count;
	tstr *name = tstdlib_string_arg(&params[0], "os::getenv");
	const char *text = tstring_cstr(name->data);
	size_t n = tstring_len(name->data);
	if (!n || memchr(text, 0, n) || memchr(text, '=', n))
		twarn(ErrRuntime_ParamsType, "os::getenv",
		    "nonempty environment name without NUL or '=' required");
	const char *value = getenv(text);
	tdict *record = tdict_new_sized(2);
	tobj_set_compo(result, (tcompo_v *)record);
	tstdlib_field_bool(record, "found", value != nullptr);
	tstdlib_field_string(record, "value", value ? value : "");
}

static void os_cwd(tobj *params, uint_regs count, tobj *result)
{
	(void)params;
	(void)count;
	for (size_t capacity = 256; capacity <= 16 * 1024 * 1024; capacity *= 2) {
		char *buffer = malloc(capacity);
		if (!buffer)
			twarn(ErrRuntime_Other, "os::cwd", "out of memory");
		if (getcwd(buffer, capacity)) {
			tstr *value = tstr_new(buffer);
			free(buffer);
			if (!value)
				twarn(ErrRuntime_Other, "os::cwd", "out of memory");
			tobj_set_compo(result, (tcompo_v *)value);
			return;
		}
		int saved = errno;
		free(buffer);
		if (saved != ERANGE)
			twarn(ErrSession_IO, "os::cwd", strerror(saved));
	}
	twarn(ErrSession_IO, "os::cwd", "working directory is too long");
}

static void os_home(tobj *params, uint_regs count, tobj *result)
{
	(void)params;
	(void)count;
	const char *home = getenv("HOME");
	if (home && *home) {
		tstdlib_string_result(result, home, strlen(home), "os::home");
		return;
	}
	for (size_t size = 1024; size <= 1024 * 1024; size *= 2) {
		char *buffer = malloc(size);
		if (!buffer)
			twarn(ErrRuntime_Other, "os::home", "out of memory");
		struct passwd entry, *found = nullptr;
		int error = getpwuid_r(getuid(), &entry, buffer, size, &found);
		if (!error && found && entry.pw_dir && *entry.pw_dir) {
			tstr *value = tstr_new(entry.pw_dir);
			free(buffer);
			if (!value)
				twarn(ErrRuntime_Other, "os::home", "out of memory");
			tobj_set_compo(result, (tcompo_v *)value);
			return;
		}
		free(buffer);
		if (error != ERANGE)
			twarn(ErrSession_IO, "os::home",
			    error ? strerror(error) : "home directory unavailable");
	}
	twarn(ErrSession_IO, "os::home", "user entry is too large");
}

static void os_temp_dir(tobj *params, uint_regs count, tobj *result)
{
	(void)params;
	(void)count;
	const char *directory = getenv("TMPDIR");
	if (!directory || !*directory)
		directory = "/tmp";
	tstdlib_string_result(result, directory, strlen(directory), "os::temp_dir");
}

static void os_platform(tobj *params, uint_regs count, tobj *result)
{
	(void)params;
	(void)count;
#if defined(__APPLE__)
	const char *name = "macos";
#elif defined(__linux__)
	const char *name = "linux";
#else
	const char *name = "posix";
#endif
	tstdlib_string_result(result, name, strlen(name), "os::platform");
}

static const textension_symbol symbols[] = {
    {.name = "getenv",
	.type = "Function[String] -> Dictionary",
	.detail = "os::getenv(name: String) -> Dictionary",
	.kind = textension_function,
	.function = os_getenv,
	.minimum_arguments = 1,
	.maximum_arguments = 1},
    {.name = "cwd",
	.type = "Function[] -> String",
	.detail = "os::cwd() -> String",
	.kind = textension_function,
	.function = os_cwd,
	.minimum_arguments = 0,
	.maximum_arguments = 0},
    {.name = "home",
	.type = "Function[] -> String",
	.detail = "os::home() -> String",
	.kind = textension_function,
	.function = os_home,
	.minimum_arguments = 0,
	.maximum_arguments = 0},
    {.name = "temp_dir",
	.type = "Function[] -> String",
	.detail = "os::temp_dir() -> String",
	.kind = textension_function,
	.function = os_temp_dir,
	.minimum_arguments = 0,
	.maximum_arguments = 0},
    {.name = "platform",
	.type = "Function[] -> String",
	.detail = "os::platform() -> String",
	.kind = textension_function,
	.function = os_platform,
	.minimum_arguments = 0,
	.maximum_arguments = 0},
    {.name = "run",
	.type = "Function[...] -> Dictionary",
	.detail = "os::run(arguments: List[String], *, stdin, cwd, env, inherit_env, timeout, "
		  "output_limit) -> Dictionary",
	.kind = textension_function,
	.function = tstdlib_os_run,
	.minimum_arguments = 1,
	.maximum_arguments = 7},
};
const textension_module tstdlib_os_module = {.scope = textension_package,
    .name = "os",
    .detail = "Environment and bounded program execution",
    .symbols = symbols,
    .symbol_count = sizeof(symbols) / sizeof(symbols[0])};
