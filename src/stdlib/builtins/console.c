#include "tapas/textension.h"

#include "tapas/runtime/tstr.h"

#include <stdio.h>

static void write_value(const tobj *value, int full)
{
	tstring *text = full
		? tobj_tostring_full(value)
		: tobj_tostring_abbr(value);
	if (!text)
		twarn(ErrRuntime_Other, full ? "pprint" : "print",
		      "out of memory");
	size_t length = tstring_len(text);
	if (length > 0 && fwrite(tstring_cstr(text), 1, length, stdout) != length) {
		tstring_free(text);
		twarn(ErrSession_IO, full ? "pprint" : "print", "stdout");
	}
	tstring_free(text);
}

static void builtin_print(tobj *arguments, uint_regs argument_count,
			  tobj *result)
{
	for (uint_regs i = 0; i < argument_count; i++)
		write_value(&arguments[i], 0);
	if (fputc('\n', stdout) == EOF)
		twarn(ErrSession_IO, "print", "stdout");
	tobj_set_nil(result);
}

static void builtin_pprint(tobj *arguments, uint_regs argument_count,
			   tobj *result)
{
	for (uint_regs i = 0; i < argument_count; i++)
		write_value(&arguments[i], 1);
	if (fputc('\n', stdout) == EOF)
		twarn(ErrSession_IO, "pprint", "stdout");
	tobj_set_nil(result);
}

static tstring *read_line(void)
{
	tstring *line = tstring_new_empty();
	if (!line)
		twarn(ErrRuntime_Other, "input", "out of memory");
	int character;
	while ((character = fgetc(stdin)) != EOF && character != '\n') {
		size_t previous = tstring_len(line);
		tstring_append_c(line, (char)character);
		if (tstring_len(line) == previous) {
			tstring_free(line);
			twarn(ErrRuntime_Other, "input", "out of memory");
		}
	}
	if (character == EOF && tstring_empty(line)) {
		tstring_free(line);
		if (ferror(stdin))
			twarn(ErrSession_IO, "input", "stdin");
		return nullptr;
	}
	if (!tstring_empty(line) &&
	    tstring_at(line, tstring_len(line) - 1) == '\r')
		tstring_pop_back(line);
	return line;
}

static void builtin_input(tobj *arguments, uint_regs argument_count,
			  tobj *result)
{
	if (argument_count > 1)
		twarn(ErrRuntime_ParamsCtr, "input", "");
	if (argument_count == 1) {
		if (arguments[0].type != tcompo ||
		    tobj_compo_type(&arguments[0]) != compo_tstr)
			twarn(ErrRuntime_ParamsType, "input", "String prompt required");
		tstr *prompt = (tstr *)arguments[0].val.v_tcompo;
		size_t length = tstring_len(prompt->data);
		if (length > 0 &&
		    fwrite(tstring_cstr(prompt->data), 1, length, stdout) != length)
			twarn(ErrSession_IO, "input", "stdout");
	}
	if (fflush(stdout) == EOF)
		twarn(ErrSession_IO, "input", "stdout");

	tstring *line = read_line();
	if (!line) {
		tobj_set_nil(result);
		return;
	}
	tstr *value = tstr_new_len(tstring_cstr(line), tstring_len(line));
	tstring_free(line);
	if (!value)
		twarn(ErrRuntime_Other, "input", "out of memory");
	tobj_set_compo(result, (tcompo_v *)value);
}

static const textension_symbol symbols[] = {
	{
		.name = "print",
		.type = "Function[...] -> Nil",
		.detail = "print(...values: AnyType) -> Nil",
		.kind = textension_function,
		.function = builtin_print,
		.minimum_arguments = 0,
		.maximum_arguments = UNDEF_NPARAMS
	},
	{
		.name = "pprint",
		.type = "Function[...] -> Nil",
		.detail = "pprint(...values: AnyType) -> Nil",
		.kind = textension_function,
		.function = builtin_pprint,
		.minimum_arguments = 0,
		.maximum_arguments = UNDEF_NPARAMS
	},
	{
		.name = "input",
		.type = "Function[...] -> String | Nil",
		.detail = "input(prompt: String = '') -> String | Nil",
		.kind = textension_function,
		.function = builtin_input,
		.minimum_arguments = 0,
		.maximum_arguments = 1
	}
};

const textension_module tstdlib_console_functions = {
	.scope = textension_root,
	.symbols = symbols,
	.symbol_count = sizeof(symbols) / sizeof(symbols[0])
};
