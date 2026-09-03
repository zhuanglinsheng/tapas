#include "tapas/textension.h"

#include "../arguments.h"

#include "tapas/runtime/tdict.h"
#include "tapas/runtime/tlist.h"
#include "tapas/runtime/tstr.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>


static void builtin_int(tobj *params, uint_regs len, tobj *vre)
{
	tstdlib_require_arguments("int", len, 1);
	switch (params[0].type) {
	case tint:
		tobj_set_int(vre, params[0].val.v_tint);
		return;
	case tfloat:
		tobj_set_int(vre, (long)params[0].val.v_tfloat);
		return;
	case tbool:
		tobj_set_int(vre, params[0].val.v_tbool ? 1 : 0);
		return;
	case tcompo:
		if (tobj_compo_type(&params[0]) == compo_tstr) {
			tstr *s = (tstr *)params[0].val.v_tcompo;
			char *end = nullptr;
			errno = 0;
			long v = strtol(tstring_cstr(s->data), &end, 10);
			if (errno == 0 && end && *end == '\0') {
				tobj_set_int(vre, v);
				return;
			}
		}
		break;
	case tnil:
		break;
	}
	twarn(ErrRuntime_ParamsType, "int", "");
}

static void builtin_float(tobj *params, uint_regs len, tobj *vre)
{
	tstdlib_require_arguments("float", len, 1);
	switch (params[0].type) {
	case tint:
		tobj_set_float(vre, (double)params[0].val.v_tint);
		return;
	case tfloat:
		tobj_set_float(vre, params[0].val.v_tfloat);
		return;
	case tbool:
		tobj_set_float(vre, params[0].val.v_tbool ? 1.0 : 0.0);
		return;
	case tcompo:
		if (tobj_compo_type(&params[0]) == compo_tstr) {
			tstr *s = (tstr *)params[0].val.v_tcompo;
			char *end = nullptr;
			errno = 0;
			double v = strtod(tstring_cstr(s->data), &end);
			if (errno == 0 && end && *end == '\0') {
				tobj_set_float(vre, v);
				return;
			}
		}
		break;
	case tnil:
		break;
	}
	twarn(ErrRuntime_ParamsType, "float", "");
}

static void builtin_bool(tobj *params, uint_regs len, tobj *vre)
{
	tstdlib_require_arguments("bool", len, 1);
	switch (params[0].type) {
	case tnil:
		tobj_set_bool(vre, 0);
		return;
	case tbool:
		tobj_set_bool(vre, params[0].val.v_tbool);
		return;
	case tint:
		tobj_set_bool(vre, params[0].val.v_tint != 0);
		return;
	case tfloat:
		tobj_set_bool(vre, params[0].val.v_tfloat != 0.0);
		return;
	case tcompo:
		if (tobj_compo_type(&params[0]) == compo_tstr) {
			tstr *s = (tstr *)params[0].val.v_tcompo;
			const char *v = tstring_cstr(s->data);
			if (strcmp(v, "true") == 0) {
				tobj_set_bool(vre, 1);
				return;
			}
			if (strcmp(v, "false") == 0) {
				tobj_set_bool(vre, 0);
				return;
			}
		}
		tobj_set_bool(vre, params[0].val.v_tcompo->vtable->len(
				      params[0].val.v_tcompo) != 0);
		return;
	}
	twarn(ErrRuntime_ParamsType, "bool", "");
}

static void builtin_str(tobj *params, uint_regs len, tobj *vre)
{
	tstdlib_require_arguments("str", len, 1);
	tstring *s = tobj_tostring_full(&params[0]);
	tobj_set_compo(vre, (tcompo_v *)tstr_new(tstring_cstr(s)));
	tstring_free(s);
}


static const textension_symbol symbols[] = {
	{
		.name = "int",
		.type = "Function[AnyType] -> Int",
		.detail = "int(value: AnyType) -> Int",
		.kind = textension_function,
		.function = builtin_int,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "float",
		.type = "Function[AnyType] -> Float",
		.detail = "float(value: AnyType) -> Float",
		.kind = textension_function,
		.function = builtin_float,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "bool",
		.type = "Function[AnyType] -> Bool",
		.detail = "bool(value: AnyType) -> Bool",
		.kind = textension_function,
		.function = builtin_bool,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "str",
		.type = "Function[AnyType] -> String",
		.detail = "str(value: AnyType) -> String",
		.kind = textension_function,
		.function = builtin_str,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	}
};

const textension_module tstdlib_conversion_functions = {
	.scope = textension_root,
	.symbols = symbols,
	.symbol_count = sizeof(symbols) / sizeof(symbols[0])
};
