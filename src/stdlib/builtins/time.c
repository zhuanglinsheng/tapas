#include "tapas/textension.h"

#include "../arguments.h"

#include "tapas/objects/ttime.h"

#include <limits.h>
#include <time.h>


static void builtin_clock(tobj *params, uint_regs len, tobj *result)
{
	(void)params;
	tstdlib_require_arguments("clock", len, 0);
	tobj_set_float(result, (double)clock() / (double)CLOCKS_PER_SEC);
}

static void builtin_clock_ns(tobj *params, uint_regs len, tobj *result)
{
	struct timespec value;
	(void)params;
	tstdlib_require_arguments("clock_ns", len, 0);
	if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &value) != 0)
		twarn(ErrRuntime_Other, "clock_ns", "process CPU clock unavailable");
	long seconds = (long)value.tv_sec;
	if (seconds > (LONG_MAX - value.tv_nsec) / 1000000000L)
		twarn(ErrRuntime_IntOutOfRange, "clock_ns", "");
	tobj_set_int(result, seconds * 1000000000L + value.tv_nsec);
}

static void builtin_now(tobj *params, uint_regs len, tobj *result)
{
	(void)params;
	tstdlib_require_arguments("now", len, 0);
	tobj_set_compo(result, (tcompo_v *)ttime_new());
}

static const textension_symbol symbols[] = {
	{
		.name = "clock",
		.type = "Function[] -> Float",
		.detail = "clock() -> Float",
		.kind = textension_function,
		.function = builtin_clock,
		.minimum_arguments = 0,
		.maximum_arguments = 0
	},
	{
		.name = "clock_ns",
		.type = "Function[] -> Int",
		.detail = "clock_ns() -> Int",
		.kind = textension_function,
		.function = builtin_clock_ns,
		.minimum_arguments = 0,
		.maximum_arguments = 0
	},
	{
		.name = "now",
		.type = "Function[] -> Time",
		.detail = "now() -> Time",
		.kind = textension_function,
		.function = builtin_now,
		.minimum_arguments = 0,
		.maximum_arguments = 0
	}
};

const textension_module tstdlib_time_functions = {
	.scope = textension_root,
	.symbols = symbols,
	.symbol_count = sizeof(symbols) / sizeof(symbols[0])
};
