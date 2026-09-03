#include "tapas/textension.h"

#include "../arguments.h"

#include "tapas/runtime/tpair.h"


static void builtin_pair(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("pair", len, 2);
	tobj_set_compo(result, (tcompo_v *)tpair_new(&params[0], &params[1]));
}

static const textension_symbol symbols[] = {
	{
		.name = "pair",
		.type = "Function[AnyType, AnyType] -> Pair",
		.detail = "pair(first: AnyType, second: AnyType) -> Pair",
		.kind = textension_function,
		.function = builtin_pair,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	}
};

const textension_module tstdlib_pair_functions = {
	.scope = textension_root,
	.symbols = symbols,
	.symbol_count = sizeof(symbols) / sizeof(symbols[0])
};
