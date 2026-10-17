#define _POSIX_C_SOURCE 200809L
#include "tapas/textension.h"
#include "tapas/dsa/tstring.h"

#include "../arguments.h"
#include "../system.h"
#include "../text.h"

#include "tapas/objects/tstr.h"
#include "tapas/objects/ttime.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

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
	int outside = (time_t)-1 > (time_t)0 ? (uintmax_t)seconds > (uintmax_t)LONG_MAX
					     : (intmax_t)seconds < (intmax_t)LONG_MIN ||
						   (intmax_t)seconds > (intmax_t)LONG_MAX;
	if (outside)
		twarn(
		    ErrRuntime_Other, "time::unix", "timestamp is outside the Tapas integer range");
	return (long)seconds;
}

static tstring *format_time(const ttime *value, const char *pattern, int utc)
{
	if (*pattern == '\0')
		return tstring_new_empty();
	time_t seconds = ttime_get(value);
	struct tm storage;
	struct tm *parts = utc ? gmtime_r(&seconds, &storage) : localtime_r(&seconds, &storage);
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
	twarn(ErrRuntime_Other, "time::format", "formatted time is empty or too large");
	return nullptr;
}

static void time_from_unix(tobj *params, uint_regs len, tobj *vre)
{
	tstdlib_require_arguments("time::from_unix", len, 1);
	if (params[0].type != tint)
		twarn(ErrRuntime_ParamsType, "time::from_unix", "integer Unix seconds required");
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
	if (params[1].type != tcompo || tobj_compo_type(&params[1]) != compo_tstr)
		twarn(ErrRuntime_ParamsType, "time::format", "format string required");
	tstr *pattern = (tstr *)params[1].val.v_tcompo;
	if (memchr(tstring_cstr(pattern->data), 0, tstring_len(pattern->data)))
		twarn(ErrRuntime_ParamsType, "time::format", "pattern contains NUL");
	tstring *formatted = format_time(value, tstring_cstr(pattern->data), 0);
	tobj_set_compo(vre, (tcompo_v *)tstr_new(tstring_cstr(formatted)));
	tstring_free(formatted);
}

static void time_monotonic(tobj *params, uint_regs len, tobj *result)
{
	(void)params;
	(void)len;
	double value;
	if (!tstdlib_monotonic(&value))
		twarn(ErrSession_IO, "time::monotonic", "clock unavailable");
	tobj_set_float(result, value);
}

static long nanoseconds(clockid_t clock, const char *where)
{
	struct timespec value;
	if (clock_gettime(clock, &value))
		twarn(ErrSession_IO, where, "clock unavailable");
	if (value.tv_sec < 0 || (uintmax_t)value.tv_sec > (uintmax_t)LONG_MAX / 1000000000 ||
	    ((uintmax_t)value.tv_sec == (uintmax_t)LONG_MAX / 1000000000 &&
		value.tv_nsec > LONG_MAX % 1000000000))
		twarn(ErrRuntime_IntOutOfRange, where, "nanosecond timestamp exceeds Int");
	return (long)value.tv_sec * 1000000000 + value.tv_nsec;
}

static void time_monotonic_ns(tobj *params, uint_regs len, tobj *result)
{
	(void)params;
	(void)len;
	tobj_set_int(result, nanoseconds(CLOCK_MONOTONIC, "time::monotonic_ns"));
}

static void time_unix_ns(tobj *params, uint_regs len, tobj *result)
{
	(void)params;
	(void)len;
	tobj_set_int(result, nanoseconds(CLOCK_REALTIME, "time::unix_ns"));
}

static void time_format_utc(tobj *params, uint_regs len, tobj *result)
{
	(void)len;
	ttime *value = time_param(&params[0], "time::format_utc");
	tstr *pattern = tstdlib_string_arg(&params[1], "time::format_utc");
	if (memchr(tstring_cstr(pattern->data), 0, tstring_len(pattern->data)))
		twarn(ErrRuntime_ParamsType, "time::format_utc", "pattern contains NUL");
	tstring *formatted = format_time(value, tstring_cstr(pattern->data), 1);
	tstr *text = tstr_new_len(tstring_cstr(formatted), tstring_len(formatted));
	tstring_free(formatted);
	if (!text)
		twarn(ErrRuntime_Other, "time::format_utc", "out of memory");
	tobj_set_compo(result, (tcompo_v *)text);
}

static int parse_utc(const tobj *input, tobj *result, tstdlib_error *error)
{
	if (input->type != tcompo || tobj_compo_type(input) != compo_tstr)
		return tstdlib_fail(error, "invalid_type", "String required");
	tstr *string = (tstr *)input->val.v_tcompo;
	const char *text = tstring_cstr(string->data);
	if (tstring_len(string->data) != 20 || text[4] != '-' || text[7] != '-' ||
	    text[10] != 'T' || text[13] != ':' || text[16] != ':' || text[19] != 'Z')
		return tstdlib_fail(error, "invalid_time", "expected YYYY-MM-DDTHH:MM:SSZ");
	const int starts[] = {0, 5, 8, 11, 14, 17}, lengths[] = {4, 2, 2, 2, 2, 2};
	int parts[6] = {0};
	for (int i = 0; i < 6; i++)
		for (int j = 0; j < lengths[i]; j++) {
			char c = text[starts[i] + j];
			if (c < '0' || c > '9')
				return tstdlib_fail(error, "invalid_time", "invalid date digits");
			parts[i] = parts[i] * 10 + c - '0';
		}
	int year = parts[0], month = parts[1], day = parts[2];
	const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	int leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
	if (year < 1 || month < 1 || month > 12 || day < 1 ||
	    day > days[month - 1] + (month == 2 && leap) || parts[3] > 23 || parts[4] > 59 ||
	    parts[5] > 59)
		return tstdlib_fail(error, "invalid_time", "invalid calendar date or time");
	/* Proleptic Gregorian civil date to days since 1970-01-01. */
	year -= month <= 2;
	long era = year / 400, yoe = year - era * 400;
	long doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
	long days_since_epoch = era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
	int64_t seconds =
	    (int64_t)days_since_epoch * 86400 + parts[3] * 3600 + parts[4] * 60 + parts[5];
	if (seconds < LONG_MIN || seconds > LONG_MAX || (int64_t)(time_t)seconds != seconds)
		return tstdlib_fail(error, "out_of_range", "timestamp outside platform range");
	tobj_set_compo(result, (tcompo_v *)ttime_from_time((time_t)seconds));
	return 1;
}

static void time_parse_utc(tobj *params, uint_regs len, tobj *result)
{
	(void)len;
	tstdlib_error error = {0};
	if (!parse_utc(&params[0], result, &error))
		twarn(ErrRuntime_ParamsType, "time::parse_utc", error.message);
}

static void time_try_parse_utc(tobj *params, uint_regs len, tobj *result)
{
	(void)len;
	tstdlib_error error = {0};
	tobj value = {0};
	tobj_set_compo(&value, (tcompo_v *)ttime_from_time(0));
	parse_utc(&params[0], &value, &error);
	tstdlib_make_result(result, &value, &error);
	tobj_try_clear(&value);
}

static const textension_symbol package_symbols[] = {
    {.name = "from_unix",
	.type = "Function[Int] -> Time",
	.detail = "time::from_unix(seconds: Int) -> Time",
	.kind = textension_function,
	.function = time_from_unix,
	.minimum_arguments = 1,
	.maximum_arguments = 1},
    {.name = "unix",
	.type = "Function[Time] -> Int",
	.detail = "time::unix(value: Time) -> Int",
	.kind = textension_function,
	.function = time_unix,
	.minimum_arguments = 1,
	.maximum_arguments = 1},
    {.name = "format",
	.type = "Function[Time, String] -> String",
	.detail = "time::format(value: Time, pattern: String) -> String",
	.kind = textension_function,
	.function = time_format,
	.minimum_arguments = 2,
	.maximum_arguments = 2},
    {.name = "monotonic",
	.type = "Function[] -> Float",
	.detail = "time::monotonic() -> Float",
	.kind = textension_function,
	.function = time_monotonic,
	.minimum_arguments = 0,
	.maximum_arguments = 0},
    {.name = "monotonic_ns",
	.type = "Function[] -> Int",
	.detail = "time::monotonic_ns() -> Int",
	.kind = textension_function,
	.function = time_monotonic_ns,
	.minimum_arguments = 0,
	.maximum_arguments = 0},
    {.name = "unix_ns",
	.type = "Function[] -> Int",
	.detail = "time::unix_ns() -> Int",
	.kind = textension_function,
	.function = time_unix_ns,
	.minimum_arguments = 0,
	.maximum_arguments = 0},
    {.name = "format_utc",
	.type = "Function[Time, String] -> String",
	.detail = "time::format_utc(value: Time, pattern: String) -> String",
	.kind = textension_function,
	.function = time_format_utc,
	.minimum_arguments = 2,
	.maximum_arguments = 2},
    {.name = "parse_utc",
	.type = "Function[String] -> Time",
	.detail = "time::parse_utc(text: String) -> Time",
	.kind = textension_function,
	.function = time_parse_utc,
	.minimum_arguments = 1,
	.maximum_arguments = 1},
    {.name = "try_parse_utc",
	.type = "Function[AnyType] -> Dictionary",
	.detail = "time::try_parse_utc(text: AnyType) -> Dictionary",
	.kind = textension_function,
	.function = time_try_parse_utc,
	.minimum_arguments = 1,
	.maximum_arguments = 1},
};

const textension_module tstdlib_time_module = {.scope = textension_package,
    .name = "time",
    .detail = "Time values and conversions",
    .symbols = package_symbols,
    .symbol_count = sizeof(package_symbols) / sizeof(package_symbols[0])};
