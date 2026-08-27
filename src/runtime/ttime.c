#include "tapas/runtime/ttime.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

static const char *ttime_type(void)
{
	return "Time";
}

static tcompo_type ttime_code(void)
{
	return compo_time;
}

static long ttime_len(void *self)
{
	(void)self;
	return 0;
}

static void *ttime_copy_impl(void *self)
{
	return ttime_from_time(((ttime *)self)->value);
}

static void ttime_free_impl(void *self)
{
	free(self);
}

static int ttime_identical_impl(void *self, void *other)
{
	return ((ttime *)self)->value == ((ttime *)other)->value;
}

static tstring *ttime_string(void *self)
{
	return ttime_format((ttime *)self, "%Y-%m-%d %H:%M:%S");
}

static long ttime_integer(const tobj *value, const char *where)
{
	if (value->type != tint)
		twarn(ErrRuntime_ParamsType, where, "integer seconds required");
	return value->val.v_tint;
}

static const ttime *ttime_operand(const tobj *value, const char *where)
{
	if (value->type != tcompo || tobj_compo_type(value) != compo_time)
		twarn(ErrRuntime_ParamsType, where, "time value required");
	return (const ttime *)value->val.v_tcompo;
}

static void ttime_add(void *self, const tobj *other, int rhs, tobj *out)
{
	if (rhs)
		twarn(ErrRuntime_ParamsType, "ttime_add",
		      "the time value must be the left operand");
	long seconds = ttime_integer(other, "ttime_add");
	tobj_set_compo(out, (tcompo_v *)ttime_shift((ttime *)self, seconds));
}

static void ttime_sub(void *self, const tobj *other, int rhs, tobj *out)
{
	if (other->type == tcompo && tobj_compo_type(other) == compo_time) {
		double difference = difftime(
			((ttime *)self)->value,
			((ttime *)other->val.v_tcompo)->value);
		tobj_set_float(out, rhs ? -difference : difference);
		return;
	}
	if (rhs)
		twarn(ErrRuntime_ParamsType, "ttime_sub",
		      "an integer cannot have a time subtracted from it");
	long seconds = ttime_integer(other, "ttime_sub");
	if (seconds == LONG_MIN)
		twarn(ErrRuntime_Other, "ttime_sub", "time offset overflow");
	tobj_set_compo(out, (tcompo_v *)ttime_shift((ttime *)self, -seconds));
}

static void ttime_compare(void *self, const tobj *other, int rhs, tobj *out,
			  int operation)
{
	time_t a = ((ttime *)self)->value;
	time_t b = ttime_operand(other, "ttime_compare")->value;
	if (rhs) {
		time_t swap = a;
		a = b;
		b = swap;
	}
	int result;
	switch (operation) {
	case 0: result = a > b; break;
	case 1: result = a < b; break;
	case 2: result = a >= b; break;
	default: result = a <= b; break;
	}
	tobj_set_bool(out, result);
}

static void ttime_gt(void *s, const tobj *v, int rhs, tobj *out)
{
	ttime_compare(s, v, rhs, out, 0);
}

static void ttime_lt(void *s, const tobj *v, int rhs, tobj *out)
{
	ttime_compare(s, v, rhs, out, 1);
}

static void ttime_ge(void *s, const tobj *v, int rhs, tobj *out)
{
	ttime_compare(s, v, rhs, out, 2);
}

static void ttime_le(void *s, const tobj *v, int rhs, tobj *out)
{
	ttime_compare(s, v, rhs, out, 3);
}

tcompo_vtable ttime_vtable = {
	.get_type = ttime_type,
	.get_compo_type_code = ttime_code,
	.len = ttime_len,
	.copy = ttime_copy_impl,
	.free = ttime_free_impl,
	.identical = ttime_identical_impl,
	.tostring_abbr = ttime_string,
	.tostring_full = ttime_string,
	.op_add = ttime_add,
	.op_sub = ttime_sub,
	.op_sg = ttime_gt,
	.op_sl = ttime_lt,
	.op_ge = ttime_ge,
	.op_le = ttime_le
};

ttime *ttime_from_time(time_t value)
{
	ttime *out = (ttime *)calloc(1, sizeof(*out));
	if (!out)
		twarn(ErrRuntime_Other, "ttime_from_time", "out of memory");
	out->base.vtable = &ttime_vtable;
	out->value = value;
	return out;
}

ttime *ttime_new(void)
{
	return ttime_from_time(time(NULL));
}

time_t ttime_get(const ttime *value)
{
	return value->value;
}

ttime *ttime_from_unix(long seconds)
{
	if ((time_t)-1 > (time_t)0 && seconds < 0)
		twarn(ErrRuntime_Other, "ttime_from_unix",
		      "negative timestamps are not supported by platform time");
	time_t converted = (time_t)seconds;
	int outside = (time_t)-1 > (time_t)0
		? (uintmax_t)converted != (uintmax_t)(unsigned long)seconds
		: (intmax_t)converted != (intmax_t)seconds;
	if (outside)
		twarn(ErrRuntime_Other, "ttime_from_unix",
		      "timestamp is outside the platform time range");
	return ttime_from_time(converted);
}

long ttime_unix(const ttime *value)
{
	int outside = (time_t)-1 > (time_t)0
		? (uintmax_t)value->value > (uintmax_t)LONG_MAX
		: (intmax_t)value->value < (intmax_t)LONG_MIN ||
		  (intmax_t)value->value > (intmax_t)LONG_MAX;
	if (outside)
		twarn(ErrRuntime_Other, "ttime_unix",
		      "timestamp is outside the Tapas integer range");
	return (long)value->value;
}

ttime *ttime_shift(const ttime *value, long seconds)
{
	long base = ttime_unix(value);
	if ((seconds > 0 && base > LONG_MAX - seconds) ||
	    (seconds < 0 && base < LONG_MIN - seconds))
		twarn(ErrRuntime_Other, "ttime_shift", "time offset overflow");
	return ttime_from_unix(base + seconds);
}

tstring *ttime_format(const ttime *value, const char *pattern)
{
	if (!pattern)
		twarn(ErrRuntime_ParamsType, "ttime_format", "format is required");
	if (*pattern == '\0')
		return tstring_new_empty();
	struct tm *parts = localtime(&value->value);
	if (!parts)
		twarn(ErrRuntime_Other, "ttime_format", "invalid local time");

	size_t capacity = 128;
	while (capacity <= 65536) {
		char *buffer = (char *)malloc(capacity);
		if (!buffer)
			twarn(ErrRuntime_Other, "ttime_format", "out of memory");
		size_t length = strftime(buffer, capacity, pattern, parts);
		if (length > 0) {
			tstring *result = tstring_new_len(buffer, length);
			free(buffer);
			return result;
		}
		free(buffer);
		capacity *= 2;
	}
	twarn(ErrRuntime_Other, "ttime_format",
	      "formatted time is empty or too large");
	return NULL;
}
