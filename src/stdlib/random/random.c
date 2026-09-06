#include "tapas/textension.h"

#include "../arguments.h"

#include "tapas/objects/ttype.h"

#include "object.h"

#include <stdint.h>

static void create_source_type(tobj *result)
{
	tobj_set_compo(result,
			      (tcompo_v *)tstdlib_random_source_type());
}

static void create_generator_type(tobj *result)
{
	tobj_set_compo(result,
			      (tcompo_v *)tstdlib_random_generator_type());
}

static void create_pcg32_xsh_rr(tobj *result)
{
	tobj_set_compo(result,
		      (tcompo_v *)trandom_source_pcg32_xsh_rr());
}

static void random_generator(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("random::generator", len, 2);
	if (params[0].type != tcompo ||
	    params[0].val.v_tcompo->vtable != &trandom_source_vtable)
		twarn(ErrRuntime_ParamsType, "random::generator",
		      "Source required");
	if (params[1].type != tint)
		twarn(ErrRuntime_ParamsType, "random::generator",
		      "seed must be Int");
	tobj_set_compo(result, (tcompo_v *)trandom_generator_new(
		(trandom_source *)params[0].val.v_tcompo,
		(int64_t)params[1].val.v_tint));
}

static trandom_generator *generator_param(const tobj *value,
					  const char *where)
{
	if (value->type != tcompo ||
	    value->val.v_tcompo->vtable != &trandom_generator_vtable)
		twarn(ErrRuntime_ParamsType, where, "Generator required");
	return (trandom_generator *)value->val.v_tcompo;
}

static void random_next_int(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("random::next_int", len, 2);
	trandom_generator *generator =
		generator_param(&params[0], "random::next_int");
	if (params[1].type != tint || params[1].val.v_tint <= 0)
		twarn(ErrRuntime_ParamsType, "random::next_int",
		      "bound must be a positive Int");
	uint64_t value = trandom_generator_bounded(
		generator, (uint64_t)params[1].val.v_tint);
	tobj_set_int(result, (long)value);
}

static void random_next_float(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("random::next_float", len, 1);
	tobj_set_float(result, trandom_generator_unit(
		generator_param(&params[0], "random::next_float")));
}

static void random_next_bool(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("random::next_bool", len, 1);
	tobj_set_bool(result, trandom_generator_bounded(
		generator_param(&params[0], "random::next_bool"), 2) != 0);
}

static void random_advance(tobj *params, uint_regs len, tobj *result)
{
	tstdlib_require_arguments("random::advance", len, 2);
	if (params[1].type != tint)
		twarn(ErrRuntime_ParamsType, "random::advance",
		      "steps must be Int");
	trandom_generator_advance(
		generator_param(&params[0], "random::advance"),
		(int64_t)params[1].val.v_tint);
	tobj_set_nil(result);
}

static const textension_symbol symbols[] = {
	{
		.name = "Source",
		.type = "Type",
		.detail = "random::Source: Type",
		.kind = textension_type,
		.value_factory = create_source_type
	},
	{
		.name = "Generator",
		.type = "Type",
		.detail = "random::Generator: Type",
		.kind = textension_type,
		.value_factory = create_generator_type
	},
	{
		.name = "pcg32_xsh_rr",
		.type = "Source",
		.detail = "random::pcg32_xsh_rr: Source",
		.kind = textension_value,
		.value_factory = create_pcg32_xsh_rr
	},
	{
		.name = "generator",
		.type = "Function[Source, Int] -> Generator",
		.detail = "random::generator(source: Source, seed: Int) -> Generator",
		.kind = textension_function,
		.function = random_generator,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "next_int",
		.type = "Function[Generator, Int] -> Int",
		.detail =
			"random::next_int(rng: Generator, bound: Int) -> Int",
		.kind = textension_function,
		.function = random_next_int,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	},
	{
		.name = "next_float",
		.type = "Function[Generator] -> Float",
		.detail = "random::next_float(rng: Generator) -> Float",
		.kind = textension_function,
		.function = random_next_float,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "next_bool",
		.type = "Function[Generator] -> Bool",
		.detail = "random::next_bool(rng: Generator) -> Bool",
		.kind = textension_function,
		.function = random_next_bool,
		.minimum_arguments = 1,
		.maximum_arguments = 1
	},
	{
		.name = "advance",
		.type = "Function[Generator, Int] -> Nil",
		.detail = "random::advance(rng: Generator, steps: Int) -> Nil",
		.kind = textension_function,
		.function = random_advance,
		.minimum_arguments = 2,
		.maximum_arguments = 2
	}
};

const textension_module tstdlib_random_module = {
	.scope = textension_package,
	.name = "random",
	.detail = "Reproducible pseudorandom generators",
	.symbols = symbols,
	.symbol_count = sizeof(symbols) / sizeof(symbols[0])
};
