#include "tapas/textension.h"

#include "../arguments.h"

#include "tapas/objects/tstr.h"


static void builtin_len(tobj *params, uint_regs len, tobj *vre)
{
	tstdlib_require_arguments("len", len, 1);
	if (params[0].type == tnil) {
		tobj_set_int(vre, 0);
		return;
	}
	if (params[0].type != tcompo || !params[0].val.v_tcompo) {
		tobj_set_int(vre, 1);
		return;
	}
	tobj_set_int(vre, params[0].val.v_tcompo->vtable->len(params[0].val.v_tcompo));
}

static void builtin_type(tobj *params, uint_regs len, tobj *vre)
{
	tstdlib_require_arguments("type", len, 1);
	const char *name = "nil";
	switch (params[0].type) {
	case tnil:
		name = "nil";
		break;
	case tbool:
		name = "bool";
		break;
	case tint:
		name = "int";
		break;
	case tfloat:
		name = "float";
		break;
	case tcompo:
		name = params[0].val.v_tcompo->vtable->get_type();
		break;
	}
	tobj_set_compo(vre, (tcompo_v *)tstr_new(name));
}

static void builtin_copy(tobj *params, uint_regs len, tobj *vre)
{
	tstdlib_require_arguments("copy", len, 1);
	if (params[0].type == tcompo && params[0].val.v_tcompo) {
		tobj_set_compo(
			vre, (tcompo_v *)params[0].val.v_tcompo->vtable->copy(
				     params[0].val.v_tcompo));
		return;
	}
	tobj_copy(vre, &params[0]);
}

static void builtin_identical(tobj *params, uint_regs len, tobj *vre)
{
	tstdlib_require_arguments("identical", len, 2);
	tobj_set_bool(vre, tobj_identical(&params[0], &params[1]));
}


static const textension_symbol symbols[] = {
	{
		.name = "len",
		.type = "Function[AnyType] -> Int",
		.detail = "len(value: AnyType) -> Int",
		.kind = textension_function,
		.function = builtin_len,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "type",
		.type = "Function[AnyType] -> String",
		.detail = "type(value: AnyType) -> String",
		.kind = textension_function,
		.function = builtin_type,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "copy",
		.type = "Function[AnyType] -> AnyType",
		.detail = "copy(value: T) -> T",
		.kind = textension_function,
		.function = builtin_copy,
		.minimum_arguments = 1,
		.maximum_arguments = 1,
		.result_relation = tnative_result_argument,
		.result_argument = 0
	},
	{
		.name = "identical",
		.type = "Function[AnyType, AnyType] -> Bool",
		.detail = "identical(left: AnyType, right: AnyType) -> Bool",
		.kind = textension_function,
		.function = builtin_identical,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	}
};

const textension_module tstdlib_object_functions = {
	.scope = textension_root,
	.symbols = symbols,
	.symbol_count = sizeof(symbols) / sizeof(symbols[0])
};
