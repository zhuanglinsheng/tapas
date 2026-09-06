#include "tapas/objects/ttime.h"
#include "tapas/dsa/tstring.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

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
	return ttime_from_time(time(nullptr));
}

time_t ttime_get(const ttime *value)
{
	return value->value;
}

/*----------------------- Required Vtable Operations -----------------------*/

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
	char buffer[32];
	time_t value = ((ttime *)self)->value;
	struct tm *parts = localtime(&value);
	if (!parts || !strftime(buffer, sizeof(buffer),
				"%Y-%m-%d %H:%M:%S", parts))
		return tstring_new("<invalid time>");
	return tstring_new(buffer);
}

/*------------------------------- Operators --------------------------------*/

static long ttime_seconds(const ttime *value, const char *where)
{
	int outside = (time_t)-1 > (time_t)0
		? (uintmax_t)value->value > (uintmax_t)LONG_MAX
		: (intmax_t)value->value < (intmax_t)LONG_MIN ||
		  (intmax_t)value->value > (intmax_t)LONG_MAX;
	if (outside)
		twarn(ErrRuntime_Other, where,
		      "timestamp is outside the Tapas integer range");
	return (long)value->value;
}

static ttime *ttime_shifted(const ttime *value, long seconds)
{
	long base = ttime_seconds(value, "time arithmetic");
	if ((seconds > 0 && base > LONG_MAX - seconds) ||
	    (seconds < 0 && base < LONG_MIN - seconds))
		twarn(ErrRuntime_Other, "time arithmetic", "time offset overflow");
	time_t shifted = (time_t)(base + seconds);
	if ((time_t)-1 > (time_t)0 && base + seconds < 0)
		twarn(ErrRuntime_Other, "time arithmetic",
		      "negative timestamps are not supported by platform time");
	if ((time_t)-1 > (time_t)0
		? (uintmax_t)shifted != (uintmax_t)(unsigned long)(base + seconds)
		: (intmax_t)shifted != (intmax_t)(base + seconds))
		twarn(ErrRuntime_Other, "time arithmetic",
		      "timestamp is outside the platform time range");
	return ttime_from_time(shifted);
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
	tobj_set_compo(out, (tcompo_v *)ttime_shifted((ttime *)self, seconds));
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
	tobj_set_compo(out, (tcompo_v *)ttime_shifted((ttime *)self, -seconds));
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

/*------------------------------ Capabilities ------------------------------*/

/*
 * Time has no object capabilities. Arithmetic and ordering are operator
 * semantics; Unix conversion and formatting are policies of the stdlib time
 * package rather than protocols shared by unrelated runtime objects.
 */

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
