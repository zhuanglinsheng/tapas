#include "tapas/textension.h"
#include "tapas/dsa/tstring.h"

#include "../arguments.h"
#include "../result.h"

#include "tapas/objects/tdict.h"
#include "tapas/objects/tlist.h"
#include "tapas/objects/tstr.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static int conversion_error(tstdlib_error *error, const char *code,
	const char *message)
{
	error->code = code;
	snprintf(error->message, sizeof(error->message), "%s", message);
	return 0;
}

static int convert_number(const tobj *input, int integer, tobj *result,
	tstdlib_error *error)
{
	if (input->type == tint) {
		if (integer) tobj_set_int(result, input->val.v_tint);
		else tobj_set_float(result, (double)input->val.v_tint);
		return 1;
	}
	if (input->type == tbool) {
		if (integer) tobj_set_int(result, input->val.v_tbool ? 1 : 0);
		else tobj_set_float(result, input->val.v_tbool ? 1.0 : 0.0);
		return 1;
	}
	if (input->type == tfloat) {
		double number = input->val.v_tfloat;
		if (!isfinite(number))
			return conversion_error(error, "non_finite", "finite numeric value required");
		if (integer) {
			double truncated = trunc(number);
			/* LONG_MAX may round upward when converted to double. Use the
			 * exactly representable, exclusive positive power-of-two bound. */
			double limit = ldexp(1.0, sizeof(long) * CHAR_BIT - 1);
			if (truncated < -limit || truncated >= limit)
				return conversion_error(error, "out_of_range", "value is outside Int range");
			tobj_set_int(result, (long)truncated);
		} else tobj_set_float(result, number);
		return 1;
	}
	if (input->type != tcompo || tobj_compo_type(input) != compo_tstr)
		return conversion_error(error, "invalid_type", "Bool, Int, Float or String required");
	tstr *string = (tstr *)input->val.v_tcompo;
	const char *text = tstring_cstr(string->data);
	size_t length = tstring_len(string->data);
	if (!length || memchr(text, 0, length))
		return conversion_error(error, "invalid_number", "empty input or embedded NUL in numeric text");
	char *end = nullptr;
	errno = 0;
	long whole = 0;
	double real = 0;
	if (integer) whole = strtol(text, &end, 10);
	else real = strtod(text, &end);
	int saved_errno = errno;
	if (end == text || end != text + length)
		return conversion_error(error, "invalid_number", "numeric text must be consumed in full");
	if (saved_errno)
		return conversion_error(error, "out_of_range", "numeric text is outside supported range");
	if (!integer && !isfinite(real))
		return conversion_error(error, "non_finite", "finite numeric value required");
	if (integer) tobj_set_int(result, whole);
	else tobj_set_float(result, real);
	return 1;
}

static void number_conversion(tobj *params, uint_regs len, tobj *result,
	int integer, int attempt, const char *where)
{
	tstdlib_require_arguments(where, len, 1);
	tobj value = {0};
	if (integer) tobj_set_int(&value, 0);
	else tobj_set_float(&value, 0.0);
	tstdlib_error error = {0};
	convert_number(&params[0], integer, &value, &error);
	if (attempt) tstdlib_make_result(result, &value, &error);
	else if (error.code) twarn(ErrRuntime_ParamsType, where, error.message);
	else tobj_copy(result, &value);
}

static void builtin_int(tobj *params, uint_regs len, tobj *result)
{
	number_conversion(params, len, result, 1, 0, "int");
}
static void builtin_float(tobj *params, uint_regs len, tobj *result)
{
	number_conversion(params, len, result, 0, 0, "float");
}
static void builtin_try_int(tobj *params, uint_regs len, tobj *result)
{
	number_conversion(params, len, result, 1, 1, "try_int");
}
static void builtin_try_float(tobj *params, uint_regs len, tobj *result)
{
	number_conversion(params, len, result, 0, 1, "try_float");
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
			if (tstring_len(s->data) == 4 && memcmp(v, "true", 4) == 0) {
				tobj_set_bool(vre, 1);
				return;
			}
			if (tstring_len(s->data) == 5 && memcmp(v, "false", 5) == 0) {
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
	tstr *string = tstr_new_len(tstring_cstr(s), tstring_len(s));
	tstring_free(s);
	if (!string) twarn(ErrRuntime_Other, "str", "out of memory");
	tobj_set_compo(vre, (tcompo_v *)string);
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

/* Append new root names in a separate module to retain existing bytecode slots. */
static const textension_symbol try_symbols[] = {
	{
		.name = "try_int", .type = "Function[AnyType] -> Dictionary",
		.detail = "try_int(value: AnyType) -> Dictionary",
		.kind = textension_function, .function = builtin_try_int,
		.minimum_arguments = 1, .maximum_arguments = 1
	},
	{
		.name = "try_float", .type = "Function[AnyType] -> Dictionary",
		.detail = "try_float(value: AnyType) -> Dictionary",
		.kind = textension_function, .function = builtin_try_float,
		.minimum_arguments = 1, .maximum_arguments = 1
	}
};
const textension_module tstdlib_conversion_try_functions = {
	.scope = textension_root, .symbols = try_symbols,
	.symbol_count = sizeof(try_symbols) / sizeof(try_symbols[0])
};
