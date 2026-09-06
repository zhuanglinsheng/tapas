#include "tapas/textension.h"
#include "tapas/dsa/tstring.h"

#include "../arguments.h"

#include "tapas/objects/tstr.h"
#include "tapas/objects/ttime.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>


static ttime *time_param(const tobj *value, const char *where)
{
	if (value->type != tcompo || tobj_compo_type(value) != compo_time)
		twarn(ErrRuntime_ParamsType, where, "time value required");
	return (ttime *)value->val.v_tcompo;
}

static ttime *from_unix(long seconds)
{
	if ((time_t)-1 > (time_t)0 && seconds < 0)
		twarn(ErrRuntime_Other, "time::from_unix",
		      "negative timestamps are not supported by platform time");
	time_t converted = (time_t)seconds;
	int outside = (time_t)-1 > (time_t)0
		? (uintmax_t)converted != (uintmax_t)(unsigned long)seconds
		: (intmax_t)converted != (intmax_t)seconds;
	if (outside)
		twarn(ErrRuntime_Other, "time::from_unix",
		      "timestamp is outside the platform time range");
	return ttime_from_time(converted);
}

static long unix_seconds(const ttime *value)
{
	time_t seconds = ttime_get(value);
	int outside = (time_t)-1 > (time_t)0
		? (uintmax_t)seconds > (uintmax_t)LONG_MAX
		: (intmax_t)seconds < (intmax_t)LONG_MIN ||
		  (intmax_t)seconds > (intmax_t)LONG_MAX;
	if (outside)
		twarn(ErrRuntime_Other, "time::unix",
		      "timestamp is outside the Tapas integer range");
	return (long)seconds;
}

static tstring *format_time(const ttime *value, const char *pattern)
{
	if (*pattern == '\0')
		return tstring_new_empty();
	time_t seconds = ttime_get(value);
	struct tm *parts = localtime(&seconds);
	if (!parts)
		twarn(ErrRuntime_Other, "time::format", "invalid local time");

	for (size_t capacity = 128; capacity <= 65536; capacity *= 2) {
		char *buffer = (char *)malloc(capacity);
		if (!buffer)
			twarn(ErrRuntime_Other, "time::format", "out of memory");
		size_t length = strftime(buffer, capacity, pattern, parts);
		if (length > 0) {
			tstring *result = tstring_new_len(buffer, length);
			free(buffer);
			return result;
		}
		free(buffer);
	}
	twarn(ErrRuntime_Other, "time::format",
	      "formatted time is empty or too large");
	return nullptr;
}

static void time_from_unix(tobj *params, uint_regs len, tobj *vre)
{
	tstdlib_require_arguments("time::from_unix", len, 1);
	if (params[0].type != tint)
		twarn(ErrRuntime_ParamsType, "time::from_unix",
		      "integer Unix seconds required");
	tobj_set_compo(vre, (tcompo_v *)from_unix(params[0].val.v_tint));
}

static void time_unix(tobj *params, uint_regs len, tobj *vre)
{
	tstdlib_require_arguments("time::unix", len, 1);
	tobj_set_int(vre, unix_seconds(time_param(&params[0], "time::unix")));
}

static void time_format(tobj *params, uint_regs len, tobj *vre)
{
	tstdlib_require_arguments("time::format", len, 2);
	ttime *value = time_param(&params[0], "time::format");
	if (params[1].type != tcompo ||
	    tobj_compo_type(&params[1]) != compo_tstr)
		twarn(ErrRuntime_ParamsType, "time::format",
		      "format string required");
	tstr *pattern = (tstr *)params[1].val.v_tcompo;
	tstring *formatted = format_time(value, tstring_cstr(pattern->data));
	tobj_set_compo(vre,
		      (tcompo_v *)tstr_new(tstring_cstr(formatted)));
	tstring_free(formatted);
}


static const textension_symbol package_symbols[] = {
	{
		.name = "from_unix",
		.type = "Function[Int] -> Time",
		.detail = "time::from_unix(seconds: Int) -> Time",
		.kind = textension_function,
		.function = time_from_unix,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "unix",
		.type = "Function[Time] -> Int",
		.detail = "time::unix(value: Time) -> Int",
		.kind = textension_function,
		.function = time_unix,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "format",
		.type = "Function[Time, String] -> String",
		.detail = "time::format(value: Time, pattern: String) -> String",
		.kind = textension_function,
		.function = time_format,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	}
};

const textension_module tstdlib_time_module = {
	.scope = textension_package,
	.name = "time",
	.detail = "Time values and conversions",
	.symbols = package_symbols,
	.symbol_count = sizeof(package_symbols) / sizeof(package_symbols[0])
};
