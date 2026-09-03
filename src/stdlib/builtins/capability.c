#include "tapas/textension.h"

#include "../arguments.h"
#include "tapas/tval.h"


static tcompo_v *target(tobj *params, const char *operation)
{
	if (params[0].type != tcompo || !params[0].val.v_tcompo)
		twarn(ErrRuntime_ParamsType, operation, "object required");
	return params[0].val.v_tcompo;
}

static void builtin_idx(tobj *params, uint_regs count, tobj *result)
{
	tstdlib_require_arguments("idx", count, 2);
	tcompo_index(target(params, "idx"), &params[1], 1, result);
}

static void builtin_append(tobj *params, uint_regs count, tobj *result)
{
	tstdlib_require_arguments("append", count, 2);
	tcompo_append(target(params, "append"), &params[1]);
	tobj_set_nil(result);
}

static void builtin_delete(tobj *params, uint_regs count, tobj *result)
{
	tstdlib_require_arguments("delete", count, 2);
	tcompo_delete(target(params, "delete"), &params[1]);
	tobj_set_nil(result);
}

static const textension_symbol symbols[] = {
	{
		.name = "idx",
		.type = "Function[Indexable, AnyType] -> AnyType",
		.detail = "idx(target: Indexable, key: AnyType) -> AnyType",
		.kind = textension_function,
		.function = builtin_idx,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "append",
		.type = "Function[Appendable, AnyType] -> Nil",
		.detail = "append(target: Appendable, value: AnyType) -> Nil",
		.kind = textension_function,
		.function = builtin_append,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "delete",
		.type = "Function[Deletable, AnyType] -> Nil",
		.detail = "delete(target: Deletable, key: AnyType) -> Nil",
		.kind = textension_function,
		.function = builtin_delete,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	}
};

const textension_module tstdlib_capability_functions = {
	.scope = textension_root,
	.symbols = symbols,
	.symbol_count = sizeof(symbols) / sizeof(symbols[0])
};
