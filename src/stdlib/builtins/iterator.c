#include "tapas/textension.h"

#include "tapas/runtime/titer.h"


static void builtin_iter(tobj *params, uint_regs len, tobj *vre)
{
	long start, step, end;
	if (len != 2 && len != 3)
		twarn(ErrRuntime_ParamsCtr, "iter", "");
	if (params[0].type != tint || params[len - 1].type != tint)
		twarn(ErrRuntime_ParamsType, "iter", "");
	start = params[0].val.v_tint;
	end = params[len - 1].val.v_tint;
	step = (end >= start) ? 1 : -1;
	if (len == 3) {
		if (params[1].type != tint)
			twarn(ErrRuntime_ParamsType, "iter", "");
		step = params[1].val.v_tint;
	}
	tobj_set_compo(vre, (tcompo_v *)titer_new_step(start, step, end));
}

static const textension_symbol symbols[] = {
	{
		.name = "iter",
		.type = "Function[...] -> Iterator",
		.detail = "iter(start: Int, [step: Int], end: Int) -> Iterator",
		.kind = textension_function,
		.function = builtin_iter,
		.minimum_arguments = 2,
		.maximum_arguments = 3
	}
};

const textension_module tstdlib_iterator_functions = {
	.scope = textension_root,
	.symbols = symbols,
	.symbol_count = sizeof(symbols) / sizeof(symbols[0])
};
